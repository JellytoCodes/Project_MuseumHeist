#include "UI/Widgets/HeistForgeryWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Core/HeistLogChannels.h"
#include "Core/HeistPlayerController.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "RHITypes.h"
#include "Styling/SlateBrush.h"
#include "UI/ViewModels/HeistForgeryViewModel.h"

namespace
{
// The local raster follows every pointer segment. The replicated polyline is
// sampled and simplified independently so transport limits never stop local
// painting during a live stroke.
constexpr float MinimumNormalizedPointSpacing = 0.006f;
constexpr float BrushRelativePointSpacing = 0.50f;
constexpr float NormalizedEraseRadius = 0.03f;
// Full 256x256 OpenCV preview scoring runs synchronously on the game thread.
// Completed strokes refresh quickly, while a held pointer is sampled at a
// deliberately lower cadence so feedback remains useful without stalling every
// drawing event.
constexpr float PreviewScoreReleaseDelaySeconds = 0.12f;
constexpr float PreviewScoreActiveIntervalSeconds = 1.25f;
constexpr float MinimumDrawingSurfaceDimensionSlateUnits = 256.0f;
constexpr float MaximumDrawingSurfaceAspectError = 0.02f;
// The WBP layout may be 400, 800, or DPI-scaled to another visible size. Keep
// the painter on its own power-of-two supersampled canvas.
constexpr int32 DrawingRasterResolution = 1024;
constexpr int32 DrawingRasterBytesPerPixel = 4;

void ResetRasterToOpaqueBlack(TArray64<uint8>& RasterBytes)
{
	FMemory::Memzero(RasterBytes.GetData(), RasterBytes.Num());
	for (int64 AlphaOffset = 3; AlphaOffset < RasterBytes.Num(); AlphaOffset += DrawingRasterBytesPerPixel)
	{
		RasterBytes[AlphaOffset] = 255;
	}
}

double GetPolylinePointToSegmentDistanceSquared(const FVector2D& Point, const FVector2D& SegmentStart, const FVector2D& SegmentEnd)
{
	const FVector2D Segment = SegmentEnd - SegmentStart;
	const double SegmentLengthSquared = Segment.SizeSquared();
	if (SegmentLengthSquared <= UE_DOUBLE_SMALL_NUMBER)
	{
		return FVector2D::DistSquared(Point, SegmentStart);
	}

	const double Projection = FMath::Clamp(FVector2D::DotProduct(Point - SegmentStart, Segment) / SegmentLengthSquared, 0.0, 1.0);
	return FVector2D::DistSquared(Point, SegmentStart + Segment * Projection);
}

void MarkPolylinePointsToKeep(const TArray<FVector2D>& Points, const double ToleranceSquared, TArray<uint8>& KeepFlags)
{
	if (Points.Num() <= 2)
	{
		return;
	}

	TArray<FIntPoint> PendingRanges;
	PendingRanges.Reserve(Points.Num());
	PendingRanges.Emplace(0, Points.Num() - 1);
	while (!PendingRanges.IsEmpty())
	{
		const FIntPoint Range = PendingRanges.Pop(EAllowShrinking::No);
		if (Range.Y <= Range.X + 1)
		{
			continue;
		}

		double MaximumDistanceSquared = 0.0;
		int32 FarthestPointIndex = INDEX_NONE;
		for (int32 PointIndex = Range.X + 1; PointIndex < Range.Y; ++PointIndex)
		{
			const double DistanceSquared = GetPolylinePointToSegmentDistanceSquared(Points[PointIndex], Points[Range.X], Points[Range.Y]);
			if (DistanceSquared > MaximumDistanceSquared)
			{
				MaximumDistanceSquared = DistanceSquared;
				FarthestPointIndex = PointIndex;
			}
		}

		if (FarthestPointIndex == INDEX_NONE || MaximumDistanceSquared <= ToleranceSquared)
		{
			continue;
		}

		KeepFlags[FarthestPointIndex] = 1;
		PendingRanges.Emplace(Range.X, FarthestPointIndex);
		PendingRanges.Emplace(FarthestPointIndex, Range.Y);
	}
}

bool SimplifyPolyline(TArray<FVector2D>& Points, const float Tolerance)
{
	if (Points.Num() <= 2 || Tolerance <= 0.0f)
	{
		return false;
	}

	TArray<uint8> KeepFlags;
	KeepFlags.Init(0, Points.Num());
	KeepFlags[0] = 1;
	KeepFlags.Last() = 1;
	MarkPolylinePointsToKeep(Points, FMath::Square(static_cast<double>(Tolerance)), KeepFlags);

	TArray<FVector2D> SimplifiedPoints;
	SimplifiedPoints.Reserve(Points.Num());
	for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
	{
		if (KeepFlags[PointIndex] != 0)
		{
			SimplifiedPoints.Add(Points[PointIndex]);
		}
	}

	if (SimplifiedPoints.Num() >= Points.Num())
	{
		return false;
	}

	Points = MoveTemp(SimplifiedPoints);
	return true;
}

void ResamplePolylineToPointCount(TArray<FVector2D>& Points, const int32 TargetPointCount)
{
	if (TargetPointCount < 2 || Points.Num() <= TargetPointCount)
	{
		return;
	}

	TArray<FVector2D> ResampledPoints;
	ResampledPoints.Reserve(TargetPointCount);
	for (int32 TargetIndex = 0; TargetIndex < TargetPointCount; ++TargetIndex)
	{
		const double SourcePosition = static_cast<double>(TargetIndex) * (Points.Num() - 1) / (TargetPointCount - 1);
		const int32 SourceIndex = FMath::Clamp(FMath::RoundToInt32(SourcePosition), 0, Points.Num() - 1);
		ResampledPoints.Add(Points[SourceIndex]);
	}
	Points = MoveTemp(ResampledPoints);
}

FLinearColor ResolveScoreTextColor(const float Score)
{
	if (!FMath::IsFinite(Score))
	{
		return FLinearColor(0.72f, 0.76f, 0.82f);
	}
	if (Score >= 90.0f)
	{
		return FLinearColor(0.25f, 0.95f, 0.42f);
	}
	if (Score >= 70.0f)
	{
		return FLinearColor(0.65f, 0.90f, 0.25f);
	}
	if (Score >= 50.0f)
	{
		return FLinearColor(1.0f, 0.75f, 0.15f);
	}
	if (Score >= 30.0f)
	{
		return FLinearColor(1.0f, 0.40f, 0.10f);
	}
	return FLinearColor(1.0f, 0.18f, 0.15f);
}

void ApplyScorePresentation(UTextBlock* ScoreText, const TOptional<float> Score)
{
	if (!IsValid(ScoreText))
	{
		return;
	}

	if (!Score.IsSet() || !FMath::IsFinite(Score.GetValue()))
	{
		ScoreText->SetText(NSLOCTEXT("HeistForgery", "UnavailableScore", "예상 점수  --/100"));
		ScoreText->SetColorAndOpacity(FLinearColor(0.72f, 0.76f, 0.82f));
		return;
	}

	const float ClampedScore = FMath::Clamp(Score.GetValue(), 0.0f, 100.0f);
	ScoreText->SetText(
		FText::Format(NSLOCTEXT("HeistForgery", "ScoreFormat", "예상 점수  {0}/100"), FText::AsNumber(FMath::RoundToInt(ClampedScore))));
	ScoreText->SetColorAndOpacity(ResolveScoreTextColor(ClampedScore));
}
}

UHeistForgeryWidget::UHeistForgeryWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UHeistForgeryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureDrawingRasterResources();
	BindPaletteButtons();
	RefreshPaletteButtons();
	MarkPreviewScoreDirty();
	RefreshDrawingTimeRemaining();
}

void UHeistForgeryWidget::NativeDestruct()
{
	FinishPointerInteraction();
	ResetLocalStrokePreview();
	ReleaseDrawingRasterResources();

	if (IsValid(ForgeryViewModel))
	{
		ForgeryViewModel->GetPresentationChangedDelegate().RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UHeistForgeryWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshDrawingTimeRemaining();
	RefreshForgeryLockdownCountdown();
	UploadDrawingRasterTexture();

	if (!bPreviewScoreDirty || !IsDrawingInputEnabled())
	{
		return;
	}
	PreviewScoreUpdateAccumulator += InDeltaTime;
	const bool bPointerEditing = ActiveStrokeIndex != INDEX_NONE || bErasePointerActive;
	const float RequiredDelay = bPointerEditing ? PreviewScoreActiveIntervalSeconds : PreviewScoreReleaseDelaySeconds;
	if (PreviewScoreUpdateAccumulator < RequiredDelay)
	{
		return;
	}

	RefreshLocalPreviewScore();
}

FReply UHeistForgeryWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!IsDrawingInputEnabled())
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	const bool bDrawAttempt = InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
	const FGeometry SurfaceGeometry = DrawingSurface->GetCachedGeometry();
	const FVector2D SurfaceLocalSize = SurfaceGeometry.GetLocalSize();
	const FVector2D MouseScreen = InMouseEvent.GetScreenSpacePosition();
	const FVector2D RawSurfaceLocal = SurfaceGeometry.AbsoluteToLocal(MouseScreen);
	FVector2D NormalizedPoint = FVector2D::ZeroVector;
	if (!TryResolveNormalizedDrawingPoint(InMouseEvent, NormalizedPoint))
	{
		if (bDrawAttempt)
		{
			const FVector2D SurfaceScreenTopLeft = SurfaceGeometry.LocalToAbsolute(FVector2D::ZeroVector);
			const FVector2D SurfaceScreenBottomRight = SurfaceGeometry.LocalToAbsolute(SurfaceLocalSize);
			UE_LOG(LogHeistUI, Verbose,
				TEXT(
					"[%s] Forgery draw input coordinate: MouseScreen=(%.2f,%.2f) SurfaceLocal=(%.2f,%.2f) SurfaceLocalSize=(%.2f,%.2f) SurfaceScreen=[(%.2f,%.2f)->(%.2f,%.2f)] Inside=false Result=REJECTED_OUTSIDE"),
				*GetName(), MouseScreen.X, MouseScreen.Y, RawSurfaceLocal.X, RawSurfaceLocal.Y, SurfaceLocalSize.X, SurfaceLocalSize.Y, SurfaceScreenTopLeft.X, SurfaceScreenTopLeft.Y,
				SurfaceScreenBottomRight.X, SurfaceScreenBottomRight.Y);
		}
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	if (bDrawAttempt)
	{
		float ViewportMouseX = 0.0f;
		float ViewportMouseY = 0.0f;
		const APlayerController* OwningPlayerController = GetOwningPlayer();
		const bool bHasViewportMouse = IsValid(OwningPlayerController) && OwningPlayerController->GetMousePosition(ViewportMouseX, ViewportMouseY);
		const bool bStrokeBegan = BeginLocalStroke(NormalizedPoint);
		UE_LOG(LogHeistUI, Log,
			TEXT(
				"[%s] Forgery draw input coordinate: MouseScreen=(%.2f,%.2f) ViewportMouse=(%.2f,%.2f) HasViewportMouse=%s SurfaceLocal=(%.2f,%.2f) SurfaceLocalSize=(%.2f,%.2f) Normalized=(%.6f,%.6f) WidgetScreenTopLeft=(%.2f,%.2f) WidgetLocalSize=(%.2f,%.2f) Inside=true LocalPoints=%d TransportPointBudget=%d Result=%s"),
			*GetName(), MouseScreen.X, MouseScreen.Y, ViewportMouseX, ViewportMouseY, bHasViewportMouse ? TEXT("true") : TEXT("false"), RawSurfaceLocal.X, RawSurfaceLocal.Y, SurfaceLocalSize.X,
			SurfaceLocalSize.Y, NormalizedPoint.X, NormalizedPoint.Y, InGeometry.LocalToAbsolute(FVector2D::ZeroVector).X, InGeometry.LocalToAbsolute(FVector2D::ZeroVector).Y,
			InGeometry.GetLocalSize().X, InGeometry.GetLocalSize().Y, GetCollectedPointCount(), GetConfiguredStrokeLimit(), bStrokeBegan ? TEXT("ACCEPTED") : TEXT("REJECTED_RASTER"));

		if (bStrokeBegan)
		{
			return FReply::Handled().SetUserFocus(TakeWidget(), EFocusCause::Mouse).CaptureMouse(TakeWidget());
		}
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		FinishPointerInteraction();
		bErasePointerActive = true;
		EraseLocalStrokeSegments(NormalizedPoint);
		return FReply::Handled().SetUserFocus(TakeWidget(), EFocusCause::Mouse).CaptureMouse(TakeWidget());
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UHeistForgeryWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FKey ReleasedButton = InMouseEvent.GetEffectingButton();
	if ((ReleasedButton == EKeys::LeftMouseButton && ActiveStrokeIndex != INDEX_NONE) || (ReleasedButton == EKeys::RightMouseButton && bErasePointerActive))
	{
		FinishPointerInteraction();
		return FReply::Handled().SetUserFocus(TakeWidget(), EFocusCause::Mouse).ReleaseMouseCapture();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UHeistForgeryWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!IsDrawingInputEnabled())
	{
		FinishPointerInteraction();
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}

	FVector2D NormalizedPoint = FVector2D::ZeroVector;
	if (!TryResolveNormalizedDrawingPoint(InMouseEvent, NormalizedPoint))
	{
		if (ActiveStrokeIndex != INDEX_NONE || bErasePointerActive)
		{
			FinishPointerInteraction();
			return FReply::Handled().ReleaseMouseCapture();
		}

		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}

	const bool bLeftMouseDown = InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton);
	const bool bRightMouseDown = InMouseEvent.IsMouseButtonDown(EKeys::RightMouseButton);
	if (bPointerStateRequiresRevalidation)
	{
		const bool bExpectedButtonStillHeld = (ActiveStrokeIndex != INDEX_NONE && bLeftMouseDown) || (bErasePointerActive && bRightMouseDown);
		bPointerStateRequiresRevalidation = false;
		if (!bExpectedButtonStillHeld)
		{
			FinishPointerInteraction();
			return FReply::Handled().SetUserFocus(TakeWidget(), EFocusCause::Mouse).ReleaseMouseCapture();
		}
	}

	// Mouse capture owns the pointer interaction until an explicit button-up or capture-loss event.
	// Slate can transiently omit the held-button flag from a captured move event; treating that as
	// a release fragmented one drag into hundreds of two-point strokes.
	if (ActiveStrokeIndex != INDEX_NONE)
	{
		AppendLocalStrokePoint(NormalizedPoint);
		if (ActiveStrokeIndex == INDEX_NONE)
		{
			return FReply::Handled().ReleaseMouseCapture();
		}

		FReply Reply = FReply::Handled();
		if (!HasMouseCapture())
		{
			Reply.SetUserFocus(TakeWidget(), EFocusCause::Mouse).CaptureMouse(TakeWidget());
		}
		return Reply;
	}

	if (bErasePointerActive)
	{
		EraseLocalStrokeSegments(NormalizedPoint);
		FReply Reply = FReply::Handled();
		if (!HasMouseCapture())
		{
			Reply.SetUserFocus(TakeWidget(), EFocusCause::Mouse).CaptureMouse(TakeWidget());
		}
		return Reply;
	}

	if (bLeftMouseDown)
	{
		if (BeginLocalStroke(NormalizedPoint))
		{
			return FReply::Handled().SetUserFocus(TakeWidget(), EFocusCause::Mouse).CaptureMouse(TakeWidget());
		}
		return FReply::Handled().ReleaseMouseCapture();
	}

	if (bRightMouseDown)
	{
		bErasePointerActive = true;
		EraseLocalStrokeSegments(NormalizedPoint);
		return FReply::Handled().SetUserFocus(TakeWidget(), EFocusCause::Mouse).CaptureMouse(TakeWidget());
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UHeistForgeryWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey PressedKey = InKeyEvent.GetKey();
	if (IsDrawingInputEnabled() && !InKeyEvent.IsRepeat() && PressedKey == EKeys::LeftBracket && ChangeBrushPreset(-1))
	{
		return FReply::Handled();
	}
	if (IsDrawingInputEnabled() && !InKeyEvent.IsRepeat() && PressedKey == EKeys::RightBracket && ChangeBrushPreset(1))
	{
		return FReply::Handled();
	}

	const FKey PaletteKeys[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight};
	for (int32 PaletteIndex = 0; PaletteIndex < UE_ARRAY_COUNT(PaletteKeys); ++PaletteIndex)
	{
		if (PressedKey == PaletteKeys[PaletteIndex] && SelectPaletteIndex(PaletteIndex))
		{
			return FReply::Handled();
		}
	}

	if (PressedKey == EKeys::Enter && IsDrawingInputEnabled())
	{
		RequestSubmitCollectedStrokes();
		return FReply::Handled();
	}

	if (PressedKey == EKeys::R && !InKeyEvent.IsRepeat() && ResetDrawingCanvas())
	{
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UHeistForgeryWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
	const bool bPointerInteractionActive = ActiveStrokeIndex != INDEX_NONE || bErasePointerActive;
	UE_LOG(LogHeistUI, Verbose,
		TEXT("[%s] Forgery mouse capture lost: ActiveStroke=%s Eraser=%s Strokes=%d Points=%d Action=%s"), *GetName(),
		ActiveStrokeIndex != INDEX_NONE ? TEXT("true") : TEXT("false"), bErasePointerActive ? TEXT("true") : TEXT("false"), GetCollectedStrokeCount(), GetCollectedPointCount(),
		bPointerInteractionActive ? TEXT("PRESERVE_UNTIL_POINTER_EVENT") : TEXT("FINISH"));
	if (bPointerInteractionActive)
	{
		bPointerStateRequiresRevalidation = true;
		return;
	}

	FinishPointerInteraction();
	if (IsDrawingInputEnabled())
	{
		SetKeyboardFocus();
	}
}

void UHeistForgeryWidget::SetupForgeryWidget(UHeistForgeryViewModel* InForgeryViewModel)
{
	checkf(IsValid(InForgeryViewModel), TEXT("HeistForgeryWidget requires a valid Forgery ViewModel."));

	if (ForgeryViewModel != InForgeryViewModel && IsValid(ForgeryViewModel))
	{
		ForgeryViewModel->GetPresentationChangedDelegate().RemoveAll(this);
	}

	ForgeryViewModel = InForgeryViewModel;
	ForgeryViewModel->GetPresentationChangedDelegate().RemoveAll(this);
	ForgeryViewModel->GetPresentationChangedDelegate().AddUObject(this, &UHeistForgeryWidget::RefreshForgeryPresentation);

	BP_OnForgerySourcesReady();
	RefreshForgeryPresentation();
}

bool UHeistForgeryWidget::IsOwnerOnlyContractSatisfied() const
{
	const APlayerController* OwningPlayerController = GetOwningPlayer();
	return IsValid(OwningPlayerController) && OwningPlayerController->IsLocalController() && IsValid(ForgeryViewModel) &&
		   ForgeryViewModel->IsPresentationVisible() == ForgeryViewModel->IsDrawingVisible();
}

bool UHeistForgeryWidget::IsWidgetPresentationVisible() const
{
	return GetVisibility() != ESlateVisibility::Collapsed && GetVisibility() != ESlateVisibility::Hidden;
}

bool UHeistForgeryWidget::IsDrawingSurfaceReady() const
{
	const FVector2D SurfaceSize = GetDrawingSurfaceSize();
	const double MaximumDimension = FMath::Max(SurfaceSize.X, SurfaceSize.Y);
	const double MinimumDimension = FMath::Min(SurfaceSize.X, SurfaceSize.Y);
	return FMath::IsFinite(SurfaceSize.X) && FMath::IsFinite(SurfaceSize.Y) && MinimumDimension >= MinimumDrawingSurfaceDimensionSlateUnits && MaximumDimension > 0.0 &&
		   (MaximumDimension - MinimumDimension) / MaximumDimension <= MaximumDrawingSurfaceAspectError;
}

FVector2D UHeistForgeryWidget::GetDrawingSurfaceSize() const
{
	return IsValid(DrawingSurface) ? DrawingSurface->GetCachedGeometry().GetLocalSize() : FVector2D::ZeroVector;
}

bool UHeistForgeryWidget::AreCollectedPointsNormalized() const
{
	for (const FHeistLocalForgeryStroke& Stroke : LocalStrokes)
	{
		for (const FVector2D& Point : Stroke.Points)
		{
			if (!FMath::IsFinite(Point.X) || !FMath::IsFinite(Point.Y) || !FMath::IsWithinInclusive(Point.X, 0.0, 1.0) || !FMath::IsWithinInclusive(Point.Y, 0.0, 1.0))
			{
				return false;
			}
		}
	}
	return true;
}

int32 UHeistForgeryWidget::GetCollectedStrokeCount() const
{
	return LocalStrokes.Num();
}

int32 UHeistForgeryWidget::GetCollectedPointCount() const
{
	int32 PointCount = 0;
	for (const FHeistLocalForgeryStroke& Stroke : LocalStrokes)
	{
		PointCount += Stroke.Points.Num();
	}
	return PointCount;
}

int32 UHeistForgeryWidget::GetCollectedSegmentCount() const
{
	int32 SegmentCount = 0;
	for (const FHeistLocalForgeryStroke& Stroke : LocalStrokes)
	{
		SegmentCount += FMath::Max(0, Stroke.Points.Num() - 1);
	}
	return SegmentCount;
}

int32 UHeistForgeryWidget::GetErasedStrokeCount() const
{
	return ErasedStrokeCount;
}

int32 UHeistForgeryWidget::GetVisiblePaletteButtonCount() const
{
	const UButton* Buttons[] = {PaletteButton1.Get(), PaletteButton2.Get(), PaletteButton3.Get(), PaletteButton4.Get(),
								PaletteButton5.Get(), PaletteButton6.Get(), PaletteButton7.Get(), PaletteButton8.Get()};
	int32 VisibleButtonCount = 0;
	for (const UButton* Button : Buttons)
	{
		VisibleButtonCount += IsValid(Button) && Button->GetVisibility() != ESlateVisibility::Collapsed && Button->GetVisibility() != ESlateVisibility::Hidden ? 1 : 0;
	}
	return VisibleButtonCount;
}

FString UHeistForgeryWidget::GetPreviewScoreText() const
{
	return IsValid(PreviewScoreText) ? PreviewScoreText->GetText().ToString() : FString();
}

int32 UHeistForgeryWidget::GetConfiguredStrokeLimit() const
{
	return IsValid(ForgeryViewModel) ? ForgeryViewModel->GetStrokeLimit() : 0;
}

float UHeistForgeryWidget::GetConfiguredBrushSize() const
{
	return IsValid(ForgeryViewModel) ? ForgeryViewModel->GetBrushSizeForPreset(ActiveBrushPresetIndex) : 0.0f;
}

int32 UHeistForgeryWidget::GetActivePaletteIndex() const
{
	return ActivePaletteIndex;
}

FLinearColor UHeistForgeryWidget::GetActivePaletteColor() const
{
	if (!IsValid(ForgeryViewModel))
	{
		return FLinearColor::Black;
	}

	const TArray<FLinearColor>& Palette = ForgeryViewModel->GetAllowedPalette();
	return Palette.IsValidIndex(ActivePaletteIndex) ? Palette[ActivePaletteIndex] : FLinearColor::Black;
}

bool UHeistForgeryWidget::SelectPaletteIndex(const int32 PaletteIndex)
{
	if (!IsValid(ForgeryViewModel) || !ForgeryViewModel->GetAllowedPalette().IsValidIndex(PaletteIndex))
	{
		return false;
	}

	FinishPointerInteraction();
	ActivePaletteIndex = PaletteIndex;
	BP_RefreshForgeryPalette(ForgeryViewModel->GetAllowedPalette(), ActivePaletteIndex);
	RefreshPaletteButtons();
	RefreshDrawingFeedback();
	InvalidateLayoutAndVolatility();
	return true;
}

bool UHeistForgeryWidget::ResetDrawingCanvas()
{
	if (!IsDrawingInputEnabled())
	{
		return false;
	}

	const int32 PreviousStrokeCount = GetCollectedStrokeCount();
	const int32 PreviousPointCount = GetCollectedPointCount();
	ResetLocalStrokePreview();
	UE_LOG(LogHeistUI, Log, TEXT("[%s] Forgery drawing canvas reset: StrokesBefore=%d PointsBefore=%d StrokesAfter=0 PointsAfter=0 Result=RESET"), *GetName(), PreviousStrokeCount,
		   PreviousPointCount);
	return true;
}

float UHeistForgeryWidget::GetDrawingTimeRemainingSeconds() const
{
	if (!IsValid(ForgeryViewModel) || !ForgeryViewModel->IsDrawingVisible())
	{
		return 0.0f;
	}

	const float StateEndServerTime = ForgeryViewModel->GetStateEndServerTime();
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = IsValid(World) ? World->GetGameState() : nullptr;
	if (StateEndServerTime <= 0.0f || !IsValid(GameState))
	{
		return 0.0f;
	}

	const float ServerWorldTime = static_cast<float>(GameState->GetServerWorldTimeSeconds());
	return FMath::Max(0.0f, StateEndServerTime - ServerWorldTime);
}

bool UHeistForgeryWidget::RequestSubmitCollectedStrokes()
{
	if (!IsDrawingInputEnabled())
	{
		UE_LOG(LogHeistUI, Warning, TEXT("[%s] Forgery stroke submit skipped: Reason=DrawingInputDisabled"), *GetName());
		return false;
	}

	FinishPointerInteraction();
	TArray<FVector2D> NormalizedPoints;
	TArray<int32> StrokePointCounts;
	TArray<uint8> StrokePaletteIndices;
	TArray<uint8> StrokeBrushPresetIndices;
	int32 IgnoredShortStrokeCount = 0;
	if (!BuildDrawableStrokePayload(NormalizedPoints, StrokePointCounts, StrokePaletteIndices, StrokeBrushPresetIndices, IgnoredShortStrokeCount, GetConfiguredStrokeLimit()))
	{
		UE_LOG(LogHeistUI, Warning, TEXT("[%s] Forgery stroke submit skipped: Strokes=%d Points=%d IgnoredShortStrokes=%d Reason=EmptyDrawablePayload"), *GetName(), StrokePointCounts.Num(),
			   NormalizedPoints.Num(), IgnoredShortStrokeCount);
		return false;
	}

	AHeistPlayerController* HeistPlayerController = Cast<AHeistPlayerController>(GetOwningPlayer());
	if (!IsValid(HeistPlayerController))
	{
		UE_LOG(LogHeistUI, Warning, TEXT("[%s] Forgery stroke submit skipped: Reason=MissingOwningHeistController"), *GetName());
		return false;
	}

	HeistPlayerController->RequestSubmitForgeryStrokes(NormalizedPoints, StrokePointCounts, StrokePaletteIndices, StrokeBrushPresetIndices);
	UE_LOG(LogHeistUI, Log, TEXT("[%s] Forgery stroke payload queued: Strokes=%d Points=%d BrushPresets=%d IgnoredShortStrokes=%d RenderTargetSent=false Result=REQUESTED"), *GetName(),
		   StrokePointCounts.Num(), NormalizedPoints.Num(), StrokeBrushPresetIndices.Num(), IgnoredShortStrokeCount);
	return true;
}

void UHeistForgeryWidget::RefreshForgeryPresentation()
{
	const APlayerController* OwningPlayerController = GetOwningPlayer();
	const bool bOwnerLocal = IsValid(OwningPlayerController) && OwningPlayerController->IsLocalController();
	const bool bPresentationVisible = bOwnerLocal && IsValid(ForgeryViewModel) && ForgeryViewModel->IsPresentationVisible();

	SetVisibility(bPresentationVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	const bool bDrawing = bPresentationVisible && ForgeryViewModel->IsDrawingVisible();

	if (bDrawing && !bWasDrawingVisible)
	{
		ResetLocalStrokePreview();
		SetKeyboardFocus();
	}
	else if (!bDrawing && bWasDrawingVisible)
	{
		FinishPointerInteraction();
	}
	bWasDrawingVisible = bDrawing;

	ApplyStateVisibility(DrawingContainer, bDrawing);
	if (IsValid(ReferenceImage))
	{
		UTexture2D* ReferenceTexture = bDrawing ? ForgeryViewModel->GetReferenceImage() : nullptr;
		ReferenceImage->SetVisibility(IsValid(ReferenceTexture) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (IsValid(ReferenceTexture))
		{
			// Preserve the WBP-authored responsive presentation size instead of
			// replacing it with the source texture's native dimensions.
			ReferenceImage->SetBrushFromTexture(ReferenceTexture, false);
		}
	}
	RefreshDrawingFeedback();
	if (IsValid(ForgeryViewModel))
	{
		const TArray<FLinearColor>& Palette = ForgeryViewModel->GetAllowedPalette();
		if (!Palette.IsValidIndex(ActivePaletteIndex))
		{
			ActivePaletteIndex = 0;
		}
		BP_RefreshForgeryPalette(Palette, ActivePaletteIndex);
	}
	RefreshPaletteButtons();
	MarkPreviewScoreDirty();
	if (bDrawing)
	{
		LastDisplayedDrawingTimeSeconds = INDEX_NONE;
	}

	RefreshDrawingTimeRemaining();
	RefreshAlertWarningPresentation();

	UE_LOG(LogHeistUI, Verbose, TEXT("[%s] Forgery widget refreshed: LocalOwner=%s Visible=%s Drawing=%s Contract=%s"), *GetName(),
		   bOwnerLocal ? TEXT("true") : TEXT("false"), bPresentationVisible ? TEXT("true") : TEXT("false"), bDrawing ? TEXT("true") : TEXT("false"),
		   IsOwnerOnlyContractSatisfied() ? TEXT("PASS") : TEXT("FAIL"));
}

void UHeistForgeryWidget::RefreshAlertWarningPresentation()
{
	const bool bWarningVisible = IsValid(ForgeryViewModel) && ForgeryViewModel->IsDangerWarningVisible() && IsWidgetPresentationVisible();
	if (IsValid(ForgeryAlertWarningText))
	{
		ForgeryAlertWarningText->SetText(bWarningVisible ? ForgeryViewModel->GetDangerWarningText() : FText::GetEmpty());
		ForgeryAlertWarningText->SetColorAndOpacity(bWarningVisible ? ForgeryViewModel->GetDangerWarningColor() : FLinearColor::White);
		ForgeryAlertWarningText->SetVisibility(bWarningVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	LastDisplayedLockdownSeconds = INDEX_NONE;
	RefreshForgeryLockdownCountdown();
}

void UHeistForgeryWidget::RefreshForgeryLockdownCountdown()
{
	const bool bCountdownVisible = IsValid(ForgeryViewModel) && ForgeryViewModel->IsLockdownCountdownVisible() && IsWidgetPresentationVisible();
	if (IsValid(ForgeryLockdownCountdownText))
	{
		ForgeryLockdownCountdownText->SetVisibility(bCountdownVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		ForgeryLockdownCountdownText->SetColorAndOpacity(IsValid(ForgeryViewModel) ? ForgeryViewModel->GetDangerWarningColor() : FLinearColor::White);
	}
	if (!bCountdownVisible)
	{
		LastDisplayedLockdownSeconds = INDEX_NONE;
		return;
	}

	const UWorld* World = GetWorld();
	const AGameStateBase* WorldGameState = IsValid(World) ? World->GetGameState() : nullptr;
	const float CountdownEndServerTime = ForgeryViewModel->GetLockdownCountdownEndServerTime();
	const int32 RemainingSeconds = IsValid(WorldGameState) && CountdownEndServerTime > 0.0f
										   ? FMath::Max(0, FMath::CeilToInt(CountdownEndServerTime - static_cast<float>(WorldGameState->GetServerWorldTimeSeconds())))
										   : INDEX_NONE;
	if (RemainingSeconds == LastDisplayedLockdownSeconds)
	{
		return;
	}
	LastDisplayedLockdownSeconds = RemainingSeconds;

	if (!IsValid(ForgeryLockdownCountdownText))
	{
		return;
	}
	if (RemainingSeconds == INDEX_NONE)
	{
		ForgeryLockdownCountdownText->SetText(
			NSLOCTEXT("HeistForgery", "LockdownTimePending", "봉쇄까지 --:--  —  탈출 경로가 제한됩니다"));
		return;
	}

	const FText TimeText = FText::FromString(FString::Printf(TEXT("%02d:%02d"), RemainingSeconds / 60, RemainingSeconds % 60));
	ForgeryLockdownCountdownText->SetText(
		FText::Format(NSLOCTEXT("HeistForgery", "LockdownTimeFormat", "봉쇄까지 {0}  —  탈출 경로가 제한됩니다"), TimeText));
}

bool UHeistForgeryWidget::IsAlertWarningContractSatisfied() const
{
	if (!IsValid(ForgeryViewModel) || !IsValid(ForgeryAlertWarningText) || !IsValid(ForgeryLockdownCountdownText))
	{
		return false;
	}

	const bool bWarningVisible = ForgeryAlertWarningText->GetVisibility() != ESlateVisibility::Collapsed && ForgeryAlertWarningText->GetVisibility() != ESlateVisibility::Hidden;
	const bool bCountdownVisible =
		ForgeryLockdownCountdownText->GetVisibility() != ESlateVisibility::Collapsed && ForgeryLockdownCountdownText->GetVisibility() != ESlateVisibility::Hidden;
	return bWarningVisible == ForgeryViewModel->IsDangerWarningVisible() && bCountdownVisible == ForgeryViewModel->IsLockdownCountdownVisible() &&
		   (!bWarningVisible || !ForgeryAlertWarningText->GetText().IsEmpty());
}

void UHeistForgeryWidget::DebugDumpAlertWarningState() const
{
	const bool bPassed = IsAlertWarningContractSatisfied();
	const FString Message =
		FString::Printf(TEXT("[%s] Forgery alert warning: Level=%s PresentationVisible=%s WarningRequested=%s WarningVisible=%s Warning='%s' CountdownRequested=%s CountdownVisible=%s "
							 "Countdown='%s' Result=%s"),
						*GetName(), IsValid(ForgeryViewModel) ? *UEnum::GetValueAsString(ForgeryViewModel->GetAlertLevel()) : TEXT("None"),
						IsWidgetPresentationVisible() ? TEXT("true") : TEXT("false"), IsValid(ForgeryViewModel) && ForgeryViewModel->IsDangerWarningVisible() ? TEXT("true") : TEXT("false"),
						IsValid(ForgeryAlertWarningText) && ForgeryAlertWarningText->GetVisibility() != ESlateVisibility::Collapsed &&
								ForgeryAlertWarningText->GetVisibility() != ESlateVisibility::Hidden
							? TEXT("true")
							: TEXT("false"),
						IsValid(ForgeryAlertWarningText) ? *ForgeryAlertWarningText->GetText().ToString() : TEXT("None"),
						IsValid(ForgeryViewModel) && ForgeryViewModel->IsLockdownCountdownVisible() ? TEXT("true") : TEXT("false"),
						IsValid(ForgeryLockdownCountdownText) && ForgeryLockdownCountdownText->GetVisibility() != ESlateVisibility::Collapsed &&
								ForgeryLockdownCountdownText->GetVisibility() != ESlateVisibility::Hidden
							? TEXT("true")
							: TEXT("false"),
						IsValid(ForgeryLockdownCountdownText) ? *ForgeryLockdownCountdownText->GetText().ToString() : TEXT("None"), bPassed ? TEXT("PASS") : TEXT("FAIL"));
	if (bPassed)
	{
		UE_LOG(LogHeistUI, Log, TEXT("%s"), *Message);
	}
	else
	{
		UE_LOG(LogHeistUI, Error, TEXT("%s"), *Message);
	}
}

bool UHeistForgeryWidget::IsDrawingInputEnabled() const
{
	return IsValid(ForgeryViewModel) && ForgeryViewModel->IsDrawingVisible() && IsWidgetPresentationVisible() && IsValid(DrawingSurface) &&
		   FMath::IsWithinInclusive(ForgeryViewModel->GetAllowedPalette().Num(), 2, 8) && GetConfiguredStrokeLimit() > 0 && FMath::IsWithinInclusive(GetConfiguredBrushSize(), 0.001f, 0.25f);
}

bool UHeistForgeryWidget::TryResolveNormalizedDrawingPoint(const FPointerEvent& PointerEvent, FVector2D& OutNormalizedPoint) const
{
	if (!IsValid(DrawingSurface))
	{
		return false;
	}

	const FGeometry SurfaceGeometry = DrawingSurface->GetCachedGeometry();
	const FVector2D SurfaceSize = SurfaceGeometry.GetLocalSize();
	if (SurfaceSize.X <= 0.0f || SurfaceSize.Y <= 0.0f)
	{
		return false;
	}

	const FVector2D SurfacePoint = SurfaceGeometry.AbsoluteToLocal(PointerEvent.GetScreenSpacePosition());
	if (SurfacePoint.X < 0.0f || SurfacePoint.Y < 0.0f || SurfacePoint.X > SurfaceSize.X || SurfacePoint.Y > SurfaceSize.Y)
	{
		return false;
	}

	OutNormalizedPoint = FVector2D(FMath::Clamp(SurfacePoint.X / SurfaceSize.X, 0.0, 1.0), FMath::Clamp(SurfacePoint.Y / SurfaceSize.Y, 0.0, 1.0));
	return true;
}

bool UHeistForgeryWidget::BeginLocalStroke(const FVector2D& NormalizedPoint)
{
	FinishPointerInteraction();
	if (!EnsureDrawingRasterResources())
	{
		return false;
	}

	ActiveStrokeIndex = LocalStrokes.AddDefaulted();
	LocalStrokes[ActiveStrokeIndex].PaletteIndex = static_cast<uint8>(ActivePaletteIndex);
	LocalStrokes[ActiveStrokeIndex].BrushPresetIndex = static_cast<uint8>(ActiveBrushPresetIndex);
	LocalStrokes[ActiveStrokeIndex].Points.Add(NormalizedPoint);
	LocalStrokes[ActiveStrokeIndex].LastPaintedPoint = NormalizedPoint;
	LocalStrokes[ActiveStrokeIndex].bHasPaintedPoint = true;
	const TArray<FLinearColor>& Palette = ForgeryViewModel->GetAllowedPalette();
	const FLinearColor StrokeColor = Palette.IsValidIndex(ActivePaletteIndex) ? Palette[ActivePaletteIndex] : FLinearColor::Black;
	PaintDrawingRasterSegment(NormalizedPoint, NormalizedPoint, ForgeryViewModel->GetBrushSizeForPreset(static_cast<uint8>(ActiveBrushPresetIndex)), StrokeColor);
	MarkPreviewScoreDirty();
	RefreshDrawingFeedback();
	InvalidateLayoutAndVolatility();
	return true;
}

bool UHeistForgeryWidget::AppendLocalStrokePoint(const FVector2D& NormalizedPoint)
{
	if (!LocalStrokes.IsValidIndex(ActiveStrokeIndex))
	{
		FinishPointerInteraction();
		return false;
	}

	TArray<FVector2D>& ActiveStroke = LocalStrokes[ActiveStrokeIndex].Points;
	if (ActiveStroke.IsEmpty())
	{
		return false;
	}

	FVector2D CanvasAspectScale(1.0, 1.0);
	if (IsValid(DrawingSurface))
	{
		const FVector2D SurfaceSize = DrawingSurface->GetCachedGeometry().GetLocalSize();
		const double MinimumSurfaceDimension = FMath::Min(SurfaceSize.X, SurfaceSize.Y);
		if (MinimumSurfaceDimension > UE_DOUBLE_SMALL_NUMBER)
		{
			CanvasAspectScale = FVector2D(SurfaceSize.X / MinimumSurfaceDimension, SurfaceSize.Y / MinimumSurfaceDimension);
		}
	}

	const float ActiveStrokeBrushSize = IsValid(ForgeryViewModel) ? ForgeryViewModel->GetBrushSizeForPreset(LocalStrokes[ActiveStrokeIndex].BrushPresetIndex) : 0.0f;
	const TArray<FLinearColor>* Palette = IsValid(ForgeryViewModel) ? &ForgeryViewModel->GetAllowedPalette() : nullptr;
	const FLinearColor StrokeColor = Palette != nullptr && Palette->IsValidIndex(LocalStrokes[ActiveStrokeIndex].PaletteIndex)
		? (*Palette)[LocalStrokes[ActiveStrokeIndex].PaletteIndex]
		: FLinearColor::Black;
	auto PaintToCurrentPoint = [this, &NormalizedPoint, ActiveStrokeBrushSize, StrokeColor]()
	{
		FHeistLocalForgeryStroke& Stroke = LocalStrokes[ActiveStrokeIndex];
		const FVector2D SegmentStart = Stroke.bHasPaintedPoint ? Stroke.LastPaintedPoint : NormalizedPoint;
		PaintDrawingRasterSegment(SegmentStart, NormalizedPoint, ActiveStrokeBrushSize, StrokeColor);
		Stroke.LastPaintedPoint = NormalizedPoint;
		Stroke.bHasPaintedPoint = true;
	};
	const float MinimumSpacing = FMath::Max(MinimumNormalizedPointSpacing, ActiveStrokeBrushSize * BrushRelativePointSpacing);
	if (ActiveStroke.Num() == 1)
	{
		const FVector2D InitialDelta = (NormalizedPoint - ActiveStroke[0]) * CanvasAspectScale;
		if (InitialDelta.IsNearlyZero())
		{
			return false;
		}
	}
	else
	{
		// Keep the last point live so slow turns follow the cursor every
		// event, then commit another point once the pending segment is long
		// enough in aspect-corrected canvas space.
		const FVector2D PendingSegment = (NormalizedPoint - ActiveStroke[ActiveStroke.Num() - 2]) * CanvasAspectScale;
		if (PendingSegment.SizeSquared() < FMath::Square(MinimumSpacing))
		{
			if (ActiveStroke.Last().Equals(NormalizedPoint, UE_DOUBLE_SMALL_NUMBER))
			{
				return false;
			}

			ActiveStroke.Last() = NormalizedPoint;
			PaintToCurrentPoint();
			MarkPreviewScoreDirty();
			InvalidateLayoutAndVolatility();
			return true;
		}
	}

	ActiveStroke.Add(NormalizedPoint);
	PaintToCurrentPoint();
	MarkPreviewScoreDirty();
	RefreshDrawingFeedback();
	InvalidateLayoutAndVolatility();
	return true;
}

bool UHeistForgeryWidget::EraseLocalStrokeSegments(const FVector2D& NormalizedPoint)
{
	if (!IsValid(DrawingSurface) || LocalStrokes.IsEmpty())
	{
		return false;
	}

	const FVector2D SurfaceSize = DrawingSurface->GetCachedGeometry().GetLocalSize();
	const double MinimumSurfaceDimension = FMath::Min(SurfaceSize.X, SurfaceSize.Y);
	if (MinimumSurfaceDimension <= UE_DOUBLE_SMALL_NUMBER)
	{
		return false;
	}

	const FVector2D CanvasAspectScale(SurfaceSize.X / MinimumSurfaceDimension, SurfaceSize.Y / MinimumSurfaceDimension);
	const FVector2D EraserPoint = NormalizedPoint * CanvasAspectScale;
	const double EraseRadiusSquared = FMath::Square(GetNormalizedEraseRadius());
	TArray<FHeistLocalForgeryStroke> UpdatedStrokes;
	UpdatedStrokes.Reserve(LocalStrokes.Num());
	int32 AffectedStrokeCount = 0;

	for (const FHeistLocalForgeryStroke& StrokeData : LocalStrokes)
	{
		const TArray<FVector2D>& Stroke = StrokeData.Points;
		if (Stroke.IsEmpty())
		{
			continue;
		}

		if (Stroke.Num() == 1)
		{
			const FVector2D EraserSpacePoint = Stroke[0] * CanvasAspectScale;
			if (FVector2D::DistSquared(EraserPoint, EraserSpacePoint) <= EraseRadiusSquared)
			{
				++AffectedStrokeCount;
			}
			else
			{
				UpdatedStrokes.Add(StrokeData);
			}
			continue;
		}

		TArray<FVector2D> CurrentFragment;
		CurrentFragment.Reserve(Stroke.Num());
		bool bStrokeAffected = false;
		const auto CommitFragment = [&UpdatedStrokes, &CurrentFragment, &StrokeData]()
		{
			if (CurrentFragment.IsEmpty())
			{
				return;
			}
			if (CurrentFragment.Num() < 2)
			{
				CurrentFragment.Reset();
				return;
			}

			FHeistLocalForgeryStroke Fragment;
			Fragment.PaletteIndex = StrokeData.PaletteIndex;
			Fragment.BrushPresetIndex = StrokeData.BrushPresetIndex;
			Fragment.Points = MoveTemp(CurrentFragment);
			Fragment.LastPaintedPoint = Fragment.Points.Last();
			Fragment.bHasPaintedPoint = true;
			UpdatedStrokes.Add(MoveTemp(Fragment));
			CurrentFragment.Reset();
		};

		const FVector2D FirstEraserSpacePoint = Stroke[0] * CanvasAspectScale;
		const bool bFirstPointInside = FVector2D::DistSquared(EraserPoint, FirstEraserSpacePoint) <= EraseRadiusSquared;
		if (bFirstPointInside)
		{
			bStrokeAffected = true;
		}
		else
		{
			CurrentFragment.Add(Stroke[0]);
		}

		for (int32 PointIndex = 1; PointIndex < Stroke.Num(); ++PointIndex)
		{
			const FVector2D SegmentStart = Stroke[PointIndex - 1] * CanvasAspectScale;
			const FVector2D SegmentEnd = Stroke[PointIndex] * CanvasAspectScale;
			const bool bSegmentIntersectsEraser = GetPointToSegmentDistanceSquared(EraserPoint, SegmentStart, SegmentEnd) <= EraseRadiusSquared;
			const bool bEndPointInside = FVector2D::DistSquared(EraserPoint, SegmentEnd) <= EraseRadiusSquared;

			if (bSegmentIntersectsEraser)
			{
				bStrokeAffected = true;
				if (!CurrentFragment.IsEmpty())
				{
					CommitFragment();
				}
				if (!bEndPointInside)
				{
					CurrentFragment.Add(Stroke[PointIndex]);
				}
			}
			else if (!bEndPointInside)
			{
				if (CurrentFragment.IsEmpty())
				{
					CurrentFragment.Add(Stroke[PointIndex - 1]);
				}
				CurrentFragment.Add(Stroke[PointIndex]);
			}
			else
			{
				bStrokeAffected = true;
				if (!CurrentFragment.IsEmpty())
				{
					CommitFragment();
				}
			}
		}

		if (!CurrentFragment.IsEmpty())
		{
			CommitFragment();
		}
		if (bStrokeAffected)
		{
			++AffectedStrokeCount;
		}
	}

	if (AffectedStrokeCount == 0)
	{
		return false;
	}

	LocalStrokes = MoveTemp(UpdatedStrokes);
	ErasedStrokeCount += AffectedStrokeCount;
	RebuildDrawingRaster();
	MarkPreviewScoreDirty();
	RefreshDrawingFeedback();
	InvalidateLayoutAndVolatility();
	return true;
}

bool UHeistForgeryWidget::BuildDrawableStrokePayload(TArray<FVector2D>& OutNormalizedPoints, TArray<int32>& OutStrokePointCounts, TArray<uint8>& OutStrokePaletteIndices,
												 TArray<uint8>& OutStrokeBrushPresetIndices, int32& OutIgnoredShortStrokeCount, const int32 MaximumTransportPointCount) const
{
	OutNormalizedPoints.Reset();
	OutStrokePointCounts.Reset();
	OutStrokePaletteIndices.Reset();
	OutStrokeBrushPresetIndices.Reset();
	OutIgnoredShortStrokeCount = 0;

	TArray<FHeistLocalForgeryStroke> PayloadStrokes;
	PayloadStrokes.Reserve(LocalStrokes.Num());

	for (const FHeistLocalForgeryStroke& Stroke : LocalStrokes)
	{
		if (Stroke.Points.Num() < 2)
		{
			++OutIgnoredShortStrokeCount;
			continue;
		}

		PayloadStrokes.Add(Stroke);
	}
	if (PayloadStrokes.IsEmpty())
	{
		return false;
	}
	if (MaximumTransportPointCount > 0 && PayloadStrokes.Num() * 2 > MaximumTransportPointCount)
	{
		UE_LOG(LogHeistUI, Warning,
			TEXT("[%s] Forgery transport copy rejected: Strokes=%d MinimumPoints=%d TransportLimit=%d LocalRasterChanged=false Reason=InsufficientPointBudget Result=FAIL"),
			*GetName(), PayloadStrokes.Num(), PayloadStrokes.Num() * 2, MaximumTransportPointCount);
		return false;
	}

	const auto CountPayloadPoints = [&PayloadStrokes]()
	{
		int32 PointCount = 0;
		for (const FHeistLocalForgeryStroke& Stroke : PayloadStrokes)
		{
			PointCount += Stroke.Points.Num();
		}
		return PointCount;
	};

	const int32 OriginalPointCount = CountPayloadPoints();
	if (MaximumTransportPointCount > 0 && OriginalPointCount > MaximumTransportPointCount)
	{
		constexpr float SimplificationTolerances[] = {0.00125f, 0.0025f, 0.004f, 0.006f, 0.01f, 0.015f, 0.025f};
		for (const float Tolerance : SimplificationTolerances)
		{
			for (FHeistLocalForgeryStroke& Stroke : PayloadStrokes)
			{
				SimplifyPolyline(Stroke.Points, Tolerance);
			}
			if (CountPayloadPoints() <= MaximumTransportPointCount)
			{
				break;
			}
		}

		int32 CurrentPointCount = CountPayloadPoints();
		if (CurrentPointCount > MaximumTransportPointCount)
		{
			int32 RemainingPointBudget = MaximumTransportPointCount;
			int32 RemainingStrokeCount = PayloadStrokes.Num();
			for (FHeistLocalForgeryStroke& Stroke : PayloadStrokes)
			{
				const int32 MinimumBudgetForRemainingStrokes = FMath::Max(0, RemainingStrokeCount - 1) * 2;
				const int32 ProportionalTarget = FMath::RoundToInt32(static_cast<double>(Stroke.Points.Num()) * MaximumTransportPointCount / CurrentPointCount);
				const int32 MaximumTarget = FMath::Max(2, RemainingPointBudget - MinimumBudgetForRemainingStrokes);
				const int32 TargetPointCount = FMath::Clamp(ProportionalTarget, 2, MaximumTarget);
				ResamplePolylineToPointCount(Stroke.Points, TargetPointCount);
				RemainingPointBudget -= Stroke.Points.Num();
				--RemainingStrokeCount;
			}
		}

		const int32 TransportPointCount = CountPayloadPoints();
		UE_LOG(LogHeistUI, Log,
			TEXT("[%s] Forgery transport copy simplified: LocalPoints=%d TransportPoints=%d TransportLimit=%d LocalRasterChanged=false Result=%s"), *GetName(), OriginalPointCount,
			TransportPointCount, MaximumTransportPointCount, TransportPointCount <= MaximumTransportPointCount ? TEXT("PASS") : TEXT("FAIL"));
		if (TransportPointCount > MaximumTransportPointCount)
		{
			return false;
		}
	}

	OutNormalizedPoints.Reserve(CountPayloadPoints());
	OutStrokePointCounts.Reserve(PayloadStrokes.Num());
	OutStrokePaletteIndices.Reserve(PayloadStrokes.Num());
	OutStrokeBrushPresetIndices.Reserve(PayloadStrokes.Num());
	for (const FHeistLocalForgeryStroke& Stroke : PayloadStrokes)
	{
		OutStrokePointCounts.Add(Stroke.Points.Num());
		OutStrokePaletteIndices.Add(Stroke.PaletteIndex);
		OutStrokeBrushPresetIndices.Add(Stroke.BrushPresetIndex);
		OutNormalizedPoints.Append(Stroke.Points);
	}

	return !OutStrokePointCounts.IsEmpty() && !OutNormalizedPoints.IsEmpty();
}

bool UHeistForgeryWidget::EnsureDrawingRasterResources()
{
	const int64 ExpectedByteCount = static_cast<int64>(DrawingRasterResolution) * DrawingRasterResolution * DrawingRasterBytesPerPixel;
	if (IsValid(DrawingRasterTexture) && DrawingRasterBrush.IsValid() && DrawingRasterBytes.Num() == ExpectedByteCount)
	{
		return bDrawingRasterBoundToSurface || ApplyDrawingRasterToSurfaceWidget();
	}

	ReleaseDrawingRasterResources();
	DrawingRasterBytes.SetNumZeroed(ExpectedByteCount);
	ResetRasterToOpaqueBlack(DrawingRasterBytes);
	DrawingRasterTexture = UTexture2D::CreateTransient(DrawingRasterResolution, DrawingRasterResolution, PF_B8G8R8A8, NAME_None, DrawingRasterBytes);
	if (!IsValid(DrawingRasterTexture))
	{
		DrawingRasterBytes.Reset();
		return false;
	}

	DrawingRasterTexture->SRGB = true;
	DrawingRasterTexture->Filter = TF_Bilinear;
	DrawingRasterTexture->AddressX = TA_Clamp;
	DrawingRasterTexture->AddressY = TA_Clamp;
	DrawingRasterTexture->NeverStream = true;
	DrawingRasterTexture->UpdateResource();

	DrawingRasterBrush = MakeShared<FSlateBrush>();
	DrawingRasterBrush->DrawAs = ESlateBrushDrawType::Image;
	DrawingRasterBrush->SetImageSize(FVector2D(DrawingRasterResolution, DrawingRasterResolution));
	DrawingRasterBrush->SetResourceObject(DrawingRasterTexture);
	DrawingRasterDirtyMinimumX = 0;
	DrawingRasterDirtyMinimumY = 0;
	DrawingRasterDirtyMaximumX = INDEX_NONE;
	DrawingRasterDirtyMaximumY = INDEX_NONE;
	return ApplyDrawingRasterToSurfaceWidget();
}

bool UHeistForgeryWidget::ApplyDrawingRasterToSurfaceWidget()
{
	if (!IsValid(DrawingSurface) || !DrawingRasterBrush.IsValid() || !IsValid(DrawingRasterTexture))
	{
		return false;
	}

	if (UImage* SurfaceImage = Cast<UImage>(DrawingSurface))
	{
		SurfaceImage->SetBrush(*DrawingRasterBrush);
		SurfaceImage->SetColorAndOpacity(FLinearColor::White);
		bDrawingRasterBoundToSurface = true;
		UE_LOG(LogHeistUI, Log, TEXT("[%s] Forgery raster bound to UMG surface: Surface=%s Class=%s Mode=Image Resolution=%d Result=PASS"), *GetName(), *GetNameSafe(DrawingSurface),
			*GetNameSafe(DrawingSurface->GetClass()), DrawingRasterResolution);
		return true;
	}

	if (UBorder* SurfaceBorder = Cast<UBorder>(DrawingSurface))
	{
		SurfaceBorder->SetBrush(*DrawingRasterBrush);
		SurfaceBorder->SetBrushColor(FLinearColor::White);
		bDrawingRasterBoundToSurface = true;
		UE_LOG(LogHeistUI, Log, TEXT("[%s] Forgery raster bound to UMG surface: Surface=%s Class=%s Mode=Border Resolution=%d Result=PASS"), *GetName(), *GetNameSafe(DrawingSurface),
			*GetNameSafe(DrawingSurface->GetClass()), DrawingRasterResolution);
		return true;
	}

	bDrawingRasterBoundToSurface = false;
	UE_LOG(LogHeistUI, Error, TEXT("[%s] Forgery raster surface unsupported: Surface=%s Class=%s Expected=ImageOrBorder Result=FAIL"), *GetName(), *GetNameSafe(DrawingSurface),
		*GetNameSafe(DrawingSurface->GetClass()));
	return false;
}

void UHeistForgeryWidget::ReleaseDrawingRasterResources()
{
	bDrawingRasterBoundToSurface = false;
	if (DrawingRasterBrush.IsValid())
	{
		DrawingRasterBrush->SetResourceObject(nullptr);
	}
	DrawingRasterBrush.Reset();
	DrawingRasterTexture = nullptr;
	DrawingRasterBytes.Reset();
	DrawingRasterDirtyMinimumX = 0;
	DrawingRasterDirtyMinimumY = 0;
	DrawingRasterDirtyMaximumX = INDEX_NONE;
	DrawingRasterDirtyMaximumY = INDEX_NONE;
}

void UHeistForgeryWidget::ClearDrawingRaster()
{
	if (!EnsureDrawingRasterResources())
	{
		return;
	}

	ResetRasterToOpaqueBlack(DrawingRasterBytes);
	MarkDrawingRasterDirty(0, 0, DrawingRasterResolution - 1, DrawingRasterResolution - 1);
}

void UHeistForgeryWidget::RebuildDrawingRaster()
{
	ClearDrawingRaster();
	if (!IsValid(ForgeryViewModel))
	{
		return;
	}

	const TArray<FLinearColor>& Palette = ForgeryViewModel->GetAllowedPalette();
	for (const FHeistLocalForgeryStroke& Stroke : LocalStrokes)
	{
		if (Stroke.Points.IsEmpty())
		{
			continue;
		}

		const FLinearColor StrokeColor = Palette.IsValidIndex(Stroke.PaletteIndex) ? Palette[Stroke.PaletteIndex] : FLinearColor::Black;
		const float BrushSize = ForgeryViewModel->GetBrushSizeForPreset(Stroke.BrushPresetIndex);
		PaintDrawingRasterSegment(Stroke.Points[0], Stroke.Points[0], BrushSize, StrokeColor);
		for (int32 PointIndex = 1; PointIndex < Stroke.Points.Num(); ++PointIndex)
		{
			PaintDrawingRasterSegment(Stroke.Points[PointIndex - 1], Stroke.Points[PointIndex], BrushSize, StrokeColor);
		}
	}
}

void UHeistForgeryWidget::PaintDrawingRasterSegment(const FVector2D& Start, const FVector2D& End, const float BrushSize, const FLinearColor& Color)
{
	if (BrushSize <= 0.0f || !EnsureDrawingRasterResources())
	{
		return;
	}

	FColor RasterColor = Color.ToFColorSRGB();
	RasterColor.A = 255;
	const FVector2D StartPixels(FMath::Clamp(Start.X, 0.0, 1.0) * (DrawingRasterResolution - 1), FMath::Clamp(Start.Y, 0.0, 1.0) * (DrawingRasterResolution - 1));
	const FVector2D EndPixels(FMath::Clamp(End.X, 0.0, 1.0) * (DrawingRasterResolution - 1), FMath::Clamp(End.Y, 0.0, 1.0) * (DrawingRasterResolution - 1));
	const float RadiusPixels = FMath::Max(1.0f, BrushSize * DrawingRasterResolution * 0.5f);
	if (StartPixels.Equals(EndPixels, UE_KINDA_SMALL_NUMBER))
	{
		StampDrawingRasterBrush(Start, BrushSize, RasterColor);
		return;
	}

	// Rasterize one continuous capsule instead of a row of circular dabs. This
	// removes the scalloped/dotted edge visible when fast pointer events are far
	// apart while preserving ordinary painter overwrite order.
	const int32 MinimumX = FMath::Max(0, FMath::FloorToInt(FMath::Min(StartPixels.X, EndPixels.X) - RadiusPixels));
	const int32 MaximumX = FMath::Min(DrawingRasterResolution - 1, FMath::CeilToInt(FMath::Max(StartPixels.X, EndPixels.X) + RadiusPixels));
	const int32 MinimumY = FMath::Max(0, FMath::FloorToInt(FMath::Min(StartPixels.Y, EndPixels.Y) - RadiusPixels));
	const int32 MaximumY = FMath::Min(DrawingRasterResolution - 1, FMath::CeilToInt(FMath::Max(StartPixels.Y, EndPixels.Y) + RadiusPixels));
	const double RadiusSquared = FMath::Square(static_cast<double>(RadiusPixels));
	for (int32 Y = MinimumY; Y <= MaximumY; ++Y)
	{
		for (int32 X = MinimumX; X <= MaximumX; ++X)
		{
			if (GetPolylinePointToSegmentDistanceSquared(FVector2D(X, Y), StartPixels, EndPixels) > RadiusSquared)
			{
				continue;
			}

			const int64 ByteOffset = (static_cast<int64>(Y) * DrawingRasterResolution + X) * DrawingRasterBytesPerPixel;
			DrawingRasterBytes[ByteOffset] = RasterColor.B;
			DrawingRasterBytes[ByteOffset + 1] = RasterColor.G;
			DrawingRasterBytes[ByteOffset + 2] = RasterColor.R;
			DrawingRasterBytes[ByteOffset + 3] = RasterColor.A;
		}
	}
	MarkDrawingRasterDirty(MinimumX, MinimumY, MaximumX, MaximumY);
}

void UHeistForgeryWidget::StampDrawingRasterBrush(const FVector2D& NormalizedPoint, const float BrushSize, const FColor& Color)
{
	if (DrawingRasterBytes.Num() != static_cast<int64>(DrawingRasterResolution) * DrawingRasterResolution * DrawingRasterBytesPerPixel)
	{
		return;
	}

	const float CenterX = FMath::Clamp(NormalizedPoint.X, 0.0, 1.0) * (DrawingRasterResolution - 1);
	const float CenterY = FMath::Clamp(NormalizedPoint.Y, 0.0, 1.0) * (DrawingRasterResolution - 1);
	const float RadiusPixels = FMath::Max(1.0f, BrushSize * DrawingRasterResolution * 0.5f);
	const int32 MinimumX = FMath::Max(0, FMath::FloorToInt(CenterX - RadiusPixels));
	const int32 MaximumX = FMath::Min(DrawingRasterResolution - 1, FMath::CeilToInt(CenterX + RadiusPixels));
	const int32 MinimumY = FMath::Max(0, FMath::FloorToInt(CenterY - RadiusPixels));
	const int32 MaximumY = FMath::Min(DrawingRasterResolution - 1, FMath::CeilToInt(CenterY + RadiusPixels));
	const float RadiusSquared = FMath::Square(RadiusPixels);
	for (int32 Y = MinimumY; Y <= MaximumY; ++Y)
	{
		for (int32 X = MinimumX; X <= MaximumX; ++X)
		{
			if (FMath::Square(X - CenterX) + FMath::Square(Y - CenterY) > RadiusSquared)
			{
				continue;
			}

			const int64 ByteOffset = (static_cast<int64>(Y) * DrawingRasterResolution + X) * DrawingRasterBytesPerPixel;
			DrawingRasterBytes[ByteOffset] = Color.B;
			DrawingRasterBytes[ByteOffset + 1] = Color.G;
			DrawingRasterBytes[ByteOffset + 2] = Color.R;
			DrawingRasterBytes[ByteOffset + 3] = Color.A;
		}
	}
	MarkDrawingRasterDirty(MinimumX, MinimumY, MaximumX, MaximumY);
}

void UHeistForgeryWidget::MarkDrawingRasterDirty(const int32 MinimumX, const int32 MinimumY, const int32 MaximumX, const int32 MaximumY)
{
	if (MaximumX < MinimumX || MaximumY < MinimumY)
	{
		return;
	}

	if (DrawingRasterDirtyMaximumX == INDEX_NONE || DrawingRasterDirtyMaximumY == INDEX_NONE)
	{
		DrawingRasterDirtyMinimumX = MinimumX;
		DrawingRasterDirtyMinimumY = MinimumY;
		DrawingRasterDirtyMaximumX = MaximumX;
		DrawingRasterDirtyMaximumY = MaximumY;
		return;
	}

	DrawingRasterDirtyMinimumX = FMath::Min(DrawingRasterDirtyMinimumX, MinimumX);
	DrawingRasterDirtyMinimumY = FMath::Min(DrawingRasterDirtyMinimumY, MinimumY);
	DrawingRasterDirtyMaximumX = FMath::Max(DrawingRasterDirtyMaximumX, MaximumX);
	DrawingRasterDirtyMaximumY = FMath::Max(DrawingRasterDirtyMaximumY, MaximumY);
}

void UHeistForgeryWidget::UploadDrawingRasterTexture()
{
	if (!IsValid(DrawingRasterTexture) || DrawingRasterTexture->GetResource() == nullptr || DrawingRasterDirtyMaximumX == INDEX_NONE || DrawingRasterDirtyMaximumY == INDEX_NONE)
	{
		return;
	}

	const int32 RegionWidth = DrawingRasterDirtyMaximumX - DrawingRasterDirtyMinimumX + 1;
	const int32 RegionHeight = DrawingRasterDirtyMaximumY - DrawingRasterDirtyMinimumY + 1;
	if (RegionWidth <= 0 || RegionHeight <= 0)
	{
		return;
	}

	const uint32 SourcePitch = static_cast<uint32>(RegionWidth * DrawingRasterBytesPerPixel);
	uint8* UploadBytes = new uint8[static_cast<int64>(SourcePitch) * RegionHeight];
	for (int32 RowIndex = 0; RowIndex < RegionHeight; ++RowIndex)
	{
		const int64 SourceOffset = (static_cast<int64>(DrawingRasterDirtyMinimumY + RowIndex) * DrawingRasterResolution + DrawingRasterDirtyMinimumX) * DrawingRasterBytesPerPixel;
		FMemory::Memcpy(UploadBytes + static_cast<int64>(RowIndex) * SourcePitch, DrawingRasterBytes.GetData() + SourceOffset, SourcePitch);
	}

	FUpdateTextureRegion2D* UpdateRegion = new FUpdateTextureRegion2D(DrawingRasterDirtyMinimumX, DrawingRasterDirtyMinimumY, 0, 0, RegionWidth, RegionHeight);
	DrawingRasterTexture->UpdateTextureRegions(0, 1, UpdateRegion, SourcePitch, DrawingRasterBytesPerPixel, UploadBytes,
		[](uint8* SourceData, const FUpdateTextureRegion2D* Regions)
		{
			delete[] SourceData;
			delete Regions;
		});
	DrawingRasterDirtyMinimumX = 0;
	DrawingRasterDirtyMinimumY = 0;
	DrawingRasterDirtyMaximumX = INDEX_NONE;
	DrawingRasterDirtyMaximumY = INDEX_NONE;
}

void UHeistForgeryWidget::MarkPreviewScoreDirty()
{
	const bool bWasAlreadyDirty = bPreviewScoreDirty;
	bPreviewScoreDirty = true;
	if (!bWasAlreadyDirty)
	{
		PreviewScoreUpdateAccumulator = 0.0f;
	}
}

void UHeistForgeryWidget::RefreshLocalPreviewScore()
{
	PreviewScoreUpdateAccumulator = 0.0f;
	TArray<FVector2D> NormalizedPoints;
	TArray<int32> StrokePointCounts;
	TArray<uint8> StrokePaletteIndices;
	TArray<uint8> StrokeBrushPresetIndices;
	int32 IgnoredShortStrokeCount = 0;
	if (!BuildDrawableStrokePayload(NormalizedPoints, StrokePointCounts, StrokePaletteIndices, StrokeBrushPresetIndices, IgnoredShortStrokeCount))
	{
		bPreviewScoreDirty = false;
		ApplyScorePresentation(PreviewScoreText, TOptional<float>());
		return;
	}

	FHeistForgeryResult ForgeryPreviewResult;
	int32 ReferenceMaskPixels = 0;
	int32 SubmittedMaskPixels = 0;
	if (!IsValid(ForgeryViewModel) ||
		!ForgeryViewModel->CalculatePreviewScore(NormalizedPoints, StrokePointCounts, StrokePaletteIndices, StrokeBrushPresetIndices, ForgeryPreviewResult, ReferenceMaskPixels,
			SubmittedMaskPixels))
	{
		// Owner-only template score settings may arrive one replication
		// update after the drawing state. The next presentation refresh or
		// stroke change retries without generating RPC or log traffic.
		bPreviewScoreDirty = false;
		ApplyScorePresentation(PreviewScoreText, TOptional<float>());
		return;
	}

	bPreviewScoreDirty = false;
	ApplyScorePresentation(PreviewScoreText, TOptional<float>(ForgeryPreviewResult.SimilarityScore));
}

void UHeistForgeryWidget::BindPaletteButtons()
{
	if (IsValid(PaletteButton1))
	{
		PaletteButton1->OnClicked.RemoveAll(this);
		PaletteButton1->OnClicked.AddDynamic(this, &UHeistForgeryWidget::HandlePaletteButton1Clicked);
	}
	if (IsValid(PaletteButton2))
	{
		PaletteButton2->OnClicked.RemoveAll(this);
		PaletteButton2->OnClicked.AddDynamic(this, &UHeistForgeryWidget::HandlePaletteButton2Clicked);
	}
	if (IsValid(PaletteButton3))
	{
		PaletteButton3->OnClicked.RemoveAll(this);
		PaletteButton3->OnClicked.AddDynamic(this, &UHeistForgeryWidget::HandlePaletteButton3Clicked);
	}
	if (IsValid(PaletteButton4))
	{
		PaletteButton4->OnClicked.RemoveAll(this);
		PaletteButton4->OnClicked.AddDynamic(this, &UHeistForgeryWidget::HandlePaletteButton4Clicked);
	}
	if (IsValid(PaletteButton5))
	{
		PaletteButton5->OnClicked.RemoveAll(this);
		PaletteButton5->OnClicked.AddDynamic(this, &UHeistForgeryWidget::HandlePaletteButton5Clicked);
	}
	if (IsValid(PaletteButton6))
	{
		PaletteButton6->OnClicked.RemoveAll(this);
		PaletteButton6->OnClicked.AddDynamic(this, &UHeistForgeryWidget::HandlePaletteButton6Clicked);
	}
	if (IsValid(PaletteButton7))
	{
		PaletteButton7->OnClicked.RemoveAll(this);
		PaletteButton7->OnClicked.AddDynamic(this, &UHeistForgeryWidget::HandlePaletteButton7Clicked);
	}
	if (IsValid(PaletteButton8))
	{
		PaletteButton8->OnClicked.RemoveAll(this);
		PaletteButton8->OnClicked.AddDynamic(this, &UHeistForgeryWidget::HandlePaletteButton8Clicked);
	}
}

void UHeistForgeryWidget::RefreshPaletteButtons()
{
	UButton* Buttons[] = {PaletteButton1.Get(), PaletteButton2.Get(), PaletteButton3.Get(), PaletteButton4.Get(),
						  PaletteButton5.Get(), PaletteButton6.Get(), PaletteButton7.Get(), PaletteButton8.Get()};
	UTextBlock* Labels[] = {PaletteButtonText1.Get(), PaletteButtonText2.Get(), PaletteButtonText3.Get(), PaletteButtonText4.Get(),
							PaletteButtonText5.Get(), PaletteButtonText6.Get(), PaletteButtonText7.Get(), PaletteButtonText8.Get()};
	const TArray<FLinearColor>* Palette = IsValid(ForgeryViewModel) ? &ForgeryViewModel->GetAllowedPalette() : nullptr;

	for (int32 PaletteIndex = 0; PaletteIndex < UE_ARRAY_COUNT(Buttons); ++PaletteIndex)
	{
		UButton* Button = Buttons[PaletteIndex];
		UTextBlock* Label = Labels[PaletteIndex];
		const bool bAvailable = Palette != nullptr && Palette->IsValidIndex(PaletteIndex);
		if (IsValid(Button))
		{
			Button->SetVisibility(bAvailable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			if (bAvailable)
			{
				const bool bSelected = PaletteIndex == ActivePaletteIndex;
				Button->SetBackgroundColor((*Palette)[PaletteIndex]);
				Button->SetRenderOpacity(bSelected ? 1.0f : 0.65f);
				Button->SetRenderScale(bSelected ? FVector2D(1.10, 1.10) : FVector2D(1.0, 1.0));
			}
		}
		if (IsValid(Label))
		{
			Label->SetText(FText::AsNumber(PaletteIndex + 1));
			if (bAvailable)
			{
				const FLinearColor& Color = (*Palette)[PaletteIndex];
				const float Luminance = Color.R * 0.2126f + Color.G * 0.7152f + Color.B * 0.0722f;
				Label->SetColorAndOpacity(Luminance > 0.45f ? FSlateColor(FLinearColor::Black) : FSlateColor(FLinearColor::White));
			}
		}
	}
}

bool UHeistForgeryWidget::ChangeBrushPreset(const int32 Direction)
{
	if (!IsDrawingInputEnabled() || Direction == 0)
	{
		return false;
	}

	const int32 NewPresetIndex = FMath::Clamp(ActiveBrushPresetIndex + (Direction > 0 ? 1 : -1), 0, 2);
	if (NewPresetIndex == ActiveBrushPresetIndex)
	{
		return false;
	}

	FinishPointerInteraction();
	ActiveBrushPresetIndex = NewPresetIndex;
	RefreshDrawingFeedback();
	InvalidateLayoutAndVolatility();
	UE_LOG(LogHeistUI, Log, TEXT("[%s] Forgery brush preset changed: Preset=%d Brush=%.4f"), *GetName(), ActiveBrushPresetIndex, GetConfiguredBrushSize());
	return true;
}

void UHeistForgeryWidget::HandlePaletteButton1Clicked()
{
	SelectPaletteIndex(0);
}

void UHeistForgeryWidget::HandlePaletteButton2Clicked()
{
	SelectPaletteIndex(1);
}

void UHeistForgeryWidget::HandlePaletteButton3Clicked()
{
	SelectPaletteIndex(2);
}

void UHeistForgeryWidget::HandlePaletteButton4Clicked()
{
	SelectPaletteIndex(3);
}

void UHeistForgeryWidget::HandlePaletteButton5Clicked()
{
	SelectPaletteIndex(4);
}

void UHeistForgeryWidget::HandlePaletteButton6Clicked()
{
	SelectPaletteIndex(5);
}

void UHeistForgeryWidget::HandlePaletteButton7Clicked()
{
	SelectPaletteIndex(6);
}

void UHeistForgeryWidget::HandlePaletteButton8Clicked()
{
	SelectPaletteIndex(7);
}

void UHeistForgeryWidget::FinishPointerInteraction()
{
	if (LocalStrokes.IsValidIndex(ActiveStrokeIndex))
	{
		TArray<FVector2D>& StrokePoints = LocalStrokes[ActiveStrokeIndex].Points;
		if (StrokePoints.Num() == 1)
		{
			const FVector2D DotPoint = StrokePoints[0];
			StrokePoints.Add(DotPoint);
		}

		MarkPreviewScoreDirty();
		RefreshDrawingFeedback();
		InvalidateLayoutAndVolatility();
	}
	ActiveStrokeIndex = INDEX_NONE;
	bErasePointerActive = false;
	bPointerStateRequiresRevalidation = false;
}

void UHeistForgeryWidget::ResetLocalStrokePreview()
{
	FinishPointerInteraction();
	LocalStrokes.Reset();
	ErasedStrokeCount = 0;
	ClearDrawingRaster();
	PreviewScoreUpdateAccumulator = 0.0f;
	bPreviewScoreDirty = false;
	ApplyScorePresentation(PreviewScoreText, TOptional<float>());
	RefreshDrawingFeedback();
	InvalidateLayoutAndVolatility();
}

void UHeistForgeryWidget::RefreshDrawingTimeRemaining()
{
	const bool bDrawingVisible = IsValid(ForgeryViewModel) && ForgeryViewModel->IsDrawingVisible();
	if (IsValid(DrawingTimeRemainingText))
	{
		DrawingTimeRemainingText->SetVisibility(bDrawingVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (!bDrawingVisible)
	{
		LastDisplayedDrawingTimeSeconds = INDEX_NONE;
		return;
	}

	const float StateEndServerTime = ForgeryViewModel->GetStateEndServerTime();
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = IsValid(World) ? World->GetGameState() : nullptr;
	const bool bHasAuthoritativeTime = StateEndServerTime > 0.0f && IsValid(GameState);
	const float ServerWorldTime = bHasAuthoritativeTime ? static_cast<float>(GameState->GetServerWorldTimeSeconds()) : 0.0f;
	const int32 RemainingSeconds = bHasAuthoritativeTime ? FMath::Max(0, FMath::CeilToInt32(StateEndServerTime - ServerWorldTime)) : INDEX_NONE;
	if (RemainingSeconds == LastDisplayedDrawingTimeSeconds)
	{
		return;
	}
	LastDisplayedDrawingTimeSeconds = RemainingSeconds;

	if (!IsValid(DrawingTimeRemainingText))
	{
		return;
	}
	if (RemainingSeconds == INDEX_NONE)
	{
		DrawingTimeRemainingText->SetText(NSLOCTEXT("HeistForgery", "DrawingTimePending", "제출까지 --:--"));
		return;
	}

	const FText TimeText = FText::FromString(FString::Printf(TEXT("%02d:%02d"), RemainingSeconds / 60, RemainingSeconds % 60));
	DrawingTimeRemainingText->SetText(FText::Format(NSLOCTEXT("HeistForgery", "DrawingTimeFormat", "제출까지 {0}"), TimeText));
}

void UHeistForgeryWidget::RefreshDrawingFeedback()
{
	const bool bDrawingVisible = IsValid(ForgeryViewModel) && ForgeryViewModel->IsDrawingVisible();
	const int32 PointCount = GetCollectedPointCount();
	if (IsValid(DrawingPlaceholder))
	{
		DrawingPlaceholder->SetVisibility(bDrawingVisible && PointCount == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		DrawingPlaceholder->SetText(NSLOCTEXT("HeistForgery", "EmptyDrawingCanvas", "그림을 그리세요"));
	}
	if (IsValid(DrawingHint) && bDrawingVisible)
	{
		const FText BrushPresetText = ActiveBrushPresetIndex == 0 ? NSLOCTEXT("HeistForgery", "BrushPresetSmall", "소")
			: ActiveBrushPresetIndex == 2 ? NSLOCTEXT("HeistForgery", "BrushPresetLarge", "대")
			                              : NSLOCTEXT("HeistForgery", "BrushPresetMedium", "중");
		DrawingHint->SetText(FText::Format(
			NSLOCTEXT("HeistForgery", "DrawingCanvasHint", "좌클릭 그리기  |  우클릭 지우기  |  [ ] 붓 크기  |  R 초기화  |  Enter 제출  |  색상 {0}  |  붓 {1}"),
			FText::AsNumber(ActivePaletteIndex + 1), BrushPresetText));
	}
	if (IsValid(PreviewScoreText))
	{
		PreviewScoreText->SetVisibility(bDrawingVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

float UHeistForgeryWidget::GetNormalizedEraseRadius() const
{
	return NormalizedEraseRadius;
}

float UHeistForgeryWidget::GetPointToSegmentDistanceSquared(const FVector2D& Point, const FVector2D& SegmentStart, const FVector2D& SegmentEnd)
{
	const FVector2D Segment = SegmentEnd - SegmentStart;
	const double SegmentLengthSquared = Segment.SizeSquared();
	if (SegmentLengthSquared <= UE_DOUBLE_SMALL_NUMBER)
	{
		return FVector2D::DistSquared(Point, SegmentStart);
	}

	const double Projection = FMath::Clamp(FVector2D::DotProduct(Point - SegmentStart, Segment) / SegmentLengthSquared, 0.0, 1.0);
	const FVector2D ClosestPoint = SegmentStart + Segment * Projection;
	return FVector2D::DistSquared(Point, ClosestPoint);
}

void UHeistForgeryWidget::ApplyStateVisibility(UWidget* TargetWidget, const bool bVisible) const
{
	if (IsValid(TargetWidget))
	{
		TargetWidget->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}
