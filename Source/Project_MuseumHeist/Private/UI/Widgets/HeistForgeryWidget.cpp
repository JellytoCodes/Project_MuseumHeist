#include "UI/Widgets/HeistForgeryWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Core/HeistLogChannels.h"
#include "Core/HeistPlayerController.h"
#include "GameFramework/PlayerController.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElementTypes.h"
#include "UI/ViewModels/HeistForgeryViewModel.h"

namespace
{
// Slate's thick polyline renderer develops visible joint spikes when
// adjacent points are much closer than the brush width. Keep committed
// points far enough apart to absorb one-pixel mouse jitter while the live
// endpoint still follows every pointer event.
constexpr float MinimumNormalizedPointSpacing = 0.004f;
constexpr float BrushRelativePointSpacing = 0.75f;
constexpr float MinimumNormalizedEraseRadius = 0.015f;
constexpr float PreviewScoreUpdateIntervalSeconds = 0.20f;
constexpr float DrawingSurfaceSizeSlateUnits = 400.0f;
}

UHeistForgeryWidget::UHeistForgeryWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UHeistForgeryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindPaletteButtons();
	RefreshPaletteButtons();
	MarkPreviewScoreDirty();
}

void UHeistForgeryWidget::NativeDestruct()
{
	FinishPointerInteraction();
	ResetLocalStrokePreview();

	if (IsValid(ForgeryViewModel))
	{
		ForgeryViewModel->GetPresentationChangedDelegate().RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UHeistForgeryWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bPreviewScoreDirty || !IsDrawingInputEnabled())
	{
		return;
	}

	PreviewScoreUpdateAccumulator += InDeltaTime;
	if (PreviewScoreUpdateAccumulator < PreviewScoreUpdateIntervalSeconds)
	{
		return;
	}

	RefreshLocalPreviewScore();
}

int32 UHeistForgeryWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId,
									   const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 SuperLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (!IsDrawingInputEnabled() || LocalStrokes.IsEmpty())
	{
		return SuperLayer;
	}

	if (!DrawingInputWidgetGeometry.IsSet())
	{
		return SuperLayer;
	}

	const FGeometry SurfaceGeometry = DrawingSurface->GetCachedGeometry();
	// Windowed PIE reports pointer/child geometry in desktop space, while
	// NativePaint's AllottedGeometry is rooted in the PIE window. Convert
	// through the input-event geometry so DrawPoints remain widget-local.
	const FGeometry& WidgetGeometry = DrawingInputWidgetGeometry.GetValue();
	const FVector2D SurfaceLocalSize = SurfaceGeometry.GetLocalSize();
	const FVector2D WidgetLocalSize = WidgetGeometry.GetLocalSize();
	if (SurfaceLocalSize.X <= 0.0f || SurfaceLocalSize.Y <= 0.0f || WidgetLocalSize.X <= 0.0f || WidgetLocalSize.Y <= 0.0f)
	{
		return SuperLayer;
	}

	const float StrokeThickness = FMath::Max(1.0f, GetConfiguredBrushSize() * FMath::Min(SurfaceLocalSize.X, SurfaceLocalSize.Y));
	const int32 StrokeLayer = SuperLayer + 1;

	if (bPendingDrawCoordinateLog)
	{
		const FVector2D SurfaceLocal(PendingDrawNormalizedPoint.X * SurfaceLocalSize.X, PendingDrawNormalizedPoint.Y * SurfaceLocalSize.Y);
		const FVector2D PaintScreen = SurfaceGeometry.LocalToAbsolute(SurfaceLocal);
		const FVector2D PaintWidgetLocal = WidgetGeometry.AbsoluteToLocal(PaintScreen);
		const FVector2D ReprojectedPaintScreen = WidgetGeometry.LocalToAbsolute(PaintWidgetLocal);
		const FVector2D MouseToPaintDelta = ReprojectedPaintScreen - PendingDrawMouseScreen;
		const FVector2D SurfaceScreenTopLeft = SurfaceGeometry.LocalToAbsolute(FVector2D::ZeroVector);
		const FVector2D SurfaceScreenBottomRight = SurfaceGeometry.LocalToAbsolute(SurfaceLocalSize);

		UE_LOG(
			LogHeistUI, Log,
			TEXT(
				"[%s] Forgery draw paint coordinate: MouseScreen=(%.2f,%.2f) Normalized=(%.6f,%.6f) SurfaceLocal=(%.2f,%.2f) PaintWidgetLocal=(%.2f,%.2f) PaintScreen=(%.2f,%.2f) MouseToPaintDelta=(%.2f,%.2f) SurfaceScreen=[(%.2f,%.2f)->(%.2f,%.2f)] SurfaceLocalSize=(%.2f,%.2f) Result=%s"),
			*GetName(), PendingDrawMouseScreen.X, PendingDrawMouseScreen.Y, PendingDrawNormalizedPoint.X, PendingDrawNormalizedPoint.Y, SurfaceLocal.X, SurfaceLocal.Y, PaintWidgetLocal.X,
			PaintWidgetLocal.Y, ReprojectedPaintScreen.X, ReprojectedPaintScreen.Y, MouseToPaintDelta.X, MouseToPaintDelta.Y, SurfaceScreenTopLeft.X, SurfaceScreenTopLeft.Y,
			SurfaceScreenBottomRight.X, SurfaceScreenBottomRight.Y, SurfaceLocalSize.X, SurfaceLocalSize.Y, MouseToPaintDelta.IsNearlyZero(0.5) ? TEXT("MATCH") : TEXT("MISMATCH"));
		bPendingDrawCoordinateLog = false;
	}

	for (const FHeistLocalForgeryStroke& Stroke : LocalStrokes)
	{
		if (Stroke.Points.IsEmpty())
		{
			continue;
		}

		TArray<FVector2d> DrawPoints;
		DrawPoints.Reserve(FMath::Max(2, Stroke.Points.Num()));
		for (const FVector2D& NormalizedPoint : Stroke.Points)
		{
			const FVector2D SurfaceLocalPoint(NormalizedPoint.X * SurfaceLocalSize.X, NormalizedPoint.Y * SurfaceLocalSize.Y);
			const FVector2D ScreenPoint = SurfaceGeometry.LocalToAbsolute(SurfaceLocalPoint);
			DrawPoints.Add(WidgetGeometry.AbsoluteToLocal(ScreenPoint));
		}
		if (DrawPoints.Num() == 1)
		{
			DrawPoints.Add(DrawPoints[0] + FVector2d(0.01, 0.01));
		}

		const TArray<FLinearColor>& Palette = ForgeryViewModel->GetAllowedPalette();
		const FLinearColor StrokeColor = Palette.IsValidIndex(Stroke.PaletteIndex) ? Palette[Stroke.PaletteIndex] : FLinearColor::Black;
		FSlateDrawElement::MakeLines(OutDrawElements, StrokeLayer, AllottedGeometry.ToPaintGeometry(), MoveTemp(DrawPoints), ESlateDrawEffect::None, StrokeColor, true, StrokeThickness);
	}

	return StrokeLayer;
}

FReply UHeistForgeryWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!IsDrawingInputEnabled())
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	DrawingInputWidgetGeometry.Emplace(InGeometry);

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
			UE_LOG(
				LogHeistUI, Warning,
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
		UE_LOG(
			LogHeistUI, Log,
			TEXT(
				"[%s] Forgery draw input coordinate: MouseScreen=(%.2f,%.2f) ViewportMouse=(%.2f,%.2f) HasViewportMouse=%s SurfaceLocal=(%.2f,%.2f) SurfaceLocalSize=(%.2f,%.2f) Normalized=(%.6f,%.6f) WidgetScreenTopLeft=(%.2f,%.2f) WidgetLocalSize=(%.2f,%.2f) Inside=true PointCount=%d StrokeLimit=%d Result=%s"),
			*GetName(), MouseScreen.X, MouseScreen.Y, ViewportMouseX, ViewportMouseY, bHasViewportMouse ? TEXT("true") : TEXT("false"), RawSurfaceLocal.X, RawSurfaceLocal.Y, SurfaceLocalSize.X,
			SurfaceLocalSize.Y, NormalizedPoint.X, NormalizedPoint.Y, InGeometry.LocalToAbsolute(FVector2D::ZeroVector).X, InGeometry.LocalToAbsolute(FVector2D::ZeroVector).Y,
			InGeometry.GetLocalSize().X, InGeometry.GetLocalSize().Y, GetCollectedPointCount(), GetConfiguredStrokeLimit(), bStrokeBegan ? TEXT("ACCEPTED") : TEXT("REJECTED_LIMIT"));

		if (bStrokeBegan)
		{
			PendingDrawMouseScreen = MouseScreen;
			PendingDrawNormalizedPoint = NormalizedPoint;
			bPendingDrawCoordinateLog = true;
			return FReply::Handled().CaptureMouse(TakeWidget());
		}
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		ActiveStrokeIndex = INDEX_NONE;
		bErasePointerActive = true;
		EraseLocalStrokeSegments(NormalizedPoint);
		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UHeistForgeryWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FKey ReleasedButton = InMouseEvent.GetEffectingButton();
	if ((ReleasedButton == EKeys::LeftMouseButton && ActiveStrokeIndex != INDEX_NONE) || (ReleasedButton == EKeys::RightMouseButton && bErasePointerActive))
	{
		FinishPointerInteraction();
		return FReply::Handled().ReleaseMouseCapture();
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

	DrawingInputWidgetGeometry.Emplace(InGeometry);

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

	if (bLeftMouseDown)
	{
		if (ActiveStrokeIndex == INDEX_NONE)
		{
			if (BeginLocalStroke(NormalizedPoint))
			{
				return FReply::Handled().CaptureMouse(TakeWidget());
			}
			return FReply::Handled().ReleaseMouseCapture();
		}

		AppendLocalStrokePoint(NormalizedPoint);
		return ActiveStrokeIndex != INDEX_NONE ? FReply::Handled().CaptureMouse(TakeWidget()) : FReply::Handled().ReleaseMouseCapture();
	}

	if (bRightMouseDown)
	{
		ActiveStrokeIndex = INDEX_NONE;
		bErasePointerActive = true;
		EraseLocalStrokeSegments(NormalizedPoint);
		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	if (ActiveStrokeIndex != INDEX_NONE || bErasePointerActive)
	{
		FinishPointerInteraction();
		return FReply::Handled().ReleaseMouseCapture();
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UHeistForgeryWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey PressedKey = InKeyEvent.GetKey();
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

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UHeistForgeryWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
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
		   (!ForgeryViewModel->IsPresentationVisible() || ForgeryViewModel->GetVisibleStateCount() == 1);
}

bool UHeistForgeryWidget::IsWidgetPresentationVisible() const
{
	return GetVisibility() != ESlateVisibility::Collapsed && GetVisibility() != ESlateVisibility::Hidden;
}

bool UHeistForgeryWidget::IsDrawingSurfaceReady() const
{
	const FVector2D SurfaceSize = GetDrawingSurfaceSize();
	return FMath::IsNearlyEqual(SurfaceSize.X, DrawingSurfaceSizeSlateUnits, 1.0) && FMath::IsNearlyEqual(SurfaceSize.Y, DrawingSurfaceSizeSlateUnits, 1.0);
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
	return IsValid(ForgeryViewModel) ? ForgeryViewModel->GetBrushSize() : 0.0f;
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
	int32 IgnoredShortStrokeCount = 0;
	if (!BuildDrawableStrokePayload(NormalizedPoints, StrokePointCounts, StrokePaletteIndices, IgnoredShortStrokeCount))
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

	HeistPlayerController->RequestSubmitForgeryStrokes(NormalizedPoints, StrokePointCounts, StrokePaletteIndices, GetConfiguredBrushSize());
	UE_LOG(LogHeistUI, Log, TEXT("[%s] Forgery stroke payload queued: Strokes=%d Points=%d Brush=%.4f IgnoredShortStrokes=%d RenderTargetSent=false Result=REQUESTED"), *GetName(),
		   StrokePointCounts.Num(), NormalizedPoints.Num(), GetConfiguredBrushSize(), IgnoredShortStrokeCount);
	return true;
}

void UHeistForgeryWidget::RefreshForgeryPresentation()
{
	const APlayerController* OwningPlayerController = GetOwningPlayer();
	const bool bOwnerLocal = IsValid(OwningPlayerController) && OwningPlayerController->IsLocalController();
	const bool bPresentationVisible = bOwnerLocal && IsValid(ForgeryViewModel) && ForgeryViewModel->IsPresentationVisible();

	SetVisibility(bPresentationVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	const bool bObservation = bPresentationVisible && ForgeryViewModel->IsObservationVisible();
	const bool bDrawing = bPresentationVisible && ForgeryViewModel->IsDrawingVisible();
	const bool bValidation = bPresentationVisible && ForgeryViewModel->IsValidationVisible();
	const bool bResult = bPresentationVisible && ForgeryViewModel->IsResultVisible();

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

	ApplyStateVisibility(ObservationContainer, bObservation);
	ApplyStateVisibility(DrawingContainer, bDrawing);
	ApplyStateVisibility(ValidationContainer, bValidation);
	ApplyStateVisibility(ResultContainer, bResult);
	ApplyStateVisibility(ReferenceImage, bObservation || bDrawing);
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

	if (IsValid(StateText))
	{
		StateText->SetText(ForgeryViewModel->GetStateText());
	}
	if (IsValid(ReferenceText))
	{
		ReferenceText->SetText(ForgeryViewModel->GetReferenceText());
	}
	if (IsValid(ReferenceImage))
	{
		ReferenceImage->SetBrushFromTexture(ForgeryViewModel->GetReferenceImage(), true);
	}
	if (IsValid(ResultText))
	{
		ResultText->SetText(ForgeryViewModel->GetResultText());
	}
	if (IsValid(ResultScoreText))
	{
		ResultScoreText->SetText(FText::AsNumber(FMath::RoundToInt(ForgeryViewModel->GetResultScore())));
	}

	BP_RefreshForgeryPresentation(bObservation, bDrawing, bValidation, bResult, IsValid(ForgeryViewModel) ? ForgeryViewModel->GetStateEndServerTime() : 0.0f,
								  IsValid(ForgeryViewModel) ? ForgeryViewModel->GetResultScore() : 0.0f);

	UE_LOG(LogHeistUI, Verbose, TEXT("[%s] Forgery widget refreshed: LocalOwner=%s Visible=%s Observation=%s Drawing=%s Validation=%s Result=%s StateCount=%d Contract=%s"), *GetName(),
		   bOwnerLocal ? TEXT("true") : TEXT("false"), bPresentationVisible ? TEXT("true") : TEXT("false"), bObservation ? TEXT("true") : TEXT("false"), bDrawing ? TEXT("true") : TEXT("false"),
		   bValidation ? TEXT("true") : TEXT("false"), bResult ? TEXT("true") : TEXT("false"), IsValid(ForgeryViewModel) ? ForgeryViewModel->GetVisibleStateCount() : 0,
		   IsOwnerOnlyContractSatisfied() ? TEXT("PASS") : TEXT("FAIL"));
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
	if (GetCollectedPointCount() >= GetConfiguredStrokeLimit())
	{
		CompactLocalStrokesForPointBudget();
		if (GetCollectedPointCount() >= GetConfiguredStrokeLimit())
		{
			return false;
		}
	}

	FinishPointerInteraction();
	ActiveStrokeIndex = LocalStrokes.AddDefaulted();
	LocalStrokes[ActiveStrokeIndex].PaletteIndex = static_cast<uint8>(ActivePaletteIndex);
	LocalStrokes[ActiveStrokeIndex].Points.Add(NormalizedPoint);
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

	if (GetCollectedPointCount() >= GetConfiguredStrokeLimit())
	{
		CompactLocalStrokesForPointBudget();
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

	const float MinimumSpacing = FMath::Max(MinimumNormalizedPointSpacing, GetConfiguredBrushSize() * BrushRelativePointSpacing);
	const bool bPointBudgetFull = GetCollectedPointCount() >= GetConfiguredStrokeLimit();
	if (ActiveStroke.Num() == 1)
	{
		if (bPointBudgetFull)
		{
			return false;
		}

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
		if (PendingSegment.SizeSquared() < FMath::Square(MinimumSpacing) || bPointBudgetFull)
		{
			if (ActiveStroke.Last().Equals(NormalizedPoint, UE_DOUBLE_SMALL_NUMBER))
			{
				return false;
			}

			ActiveStroke.Last() = NormalizedPoint;
			MarkPreviewScoreDirty();
			InvalidateLayoutAndVolatility();
			return true;
		}
	}

	ActiveStroke.Add(NormalizedPoint);
	MarkPreviewScoreDirty();
	RefreshDrawingFeedback();
	InvalidateLayoutAndVolatility();
	return true;
}

bool UHeistForgeryWidget::CompactLocalStrokesForPointBudget()
{
	const int32 PreviousPointCount = GetCollectedPointCount();
	if (PreviousPointCount < GetConfiguredStrokeLimit())
	{
		return false;
	}

	for (FHeistLocalForgeryStroke& StrokeData : LocalStrokes)
	{
		TArray<FVector2D>& Stroke = StrokeData.Points;
		if (Stroke.Num() <= 2)
		{
			continue;
		}

		TArray<FVector2D> CompactedStroke;
		CompactedStroke.Reserve(Stroke.Num() / 2 + 2);
		CompactedStroke.Add(Stroke[0]);
		for (int32 PointIndex = 2; PointIndex < Stroke.Num() - 1; PointIndex += 2)
		{
			CompactedStroke.Add(Stroke[PointIndex]);
		}
		if (!CompactedStroke.Last().Equals(Stroke.Last()))
		{
			CompactedStroke.Add(Stroke.Last());
		}
		Stroke = MoveTemp(CompactedStroke);
	}

	const int32 CompactedPointCount = GetCollectedPointCount();
	if (CompactedPointCount >= PreviousPointCount)
	{
		return false;
	}

	UE_LOG(LogHeistUI, Log, TEXT("[%s] Forgery stroke points compacted: Previous=%d Current=%d Limit=%d Result=PASS"), *GetName(), PreviousPointCount, CompactedPointCount, GetConfiguredStrokeLimit());
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

			FHeistLocalForgeryStroke Fragment;
			Fragment.PaletteIndex = StrokeData.PaletteIndex;
			Fragment.Points = MoveTemp(CurrentFragment);
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
	MarkPreviewScoreDirty();
	RefreshDrawingFeedback();
	InvalidateLayoutAndVolatility();
	return true;
}

bool UHeistForgeryWidget::BuildDrawableStrokePayload(TArray<FVector2D>& OutNormalizedPoints, TArray<int32>& OutStrokePointCounts, TArray<uint8>& OutStrokePaletteIndices,
													 int32& OutIgnoredShortStrokeCount) const
{
	OutNormalizedPoints.Reset();
	OutStrokePointCounts.Reset();
	OutStrokePaletteIndices.Reset();
	OutIgnoredShortStrokeCount = 0;
	OutNormalizedPoints.Reserve(GetCollectedPointCount());
	OutStrokePointCounts.Reserve(LocalStrokes.Num());
	OutStrokePaletteIndices.Reserve(LocalStrokes.Num());

	for (const FHeistLocalForgeryStroke& Stroke : LocalStrokes)
	{
		if (Stroke.Points.Num() < 2)
		{
			++OutIgnoredShortStrokeCount;
			continue;
		}

		OutStrokePointCounts.Add(Stroke.Points.Num());
		OutStrokePaletteIndices.Add(Stroke.PaletteIndex);
		OutNormalizedPoints.Append(Stroke.Points);
	}

	return !OutStrokePointCounts.IsEmpty() && !OutNormalizedPoints.IsEmpty();
}

void UHeistForgeryWidget::MarkPreviewScoreDirty()
{
	bPreviewScoreDirty = true;
}

void UHeistForgeryWidget::RefreshLocalPreviewScore()
{
	PreviewScoreUpdateAccumulator = 0.0f;
	TArray<FVector2D> NormalizedPoints;
	TArray<int32> StrokePointCounts;
	TArray<uint8> StrokePaletteIndices;
	int32 IgnoredShortStrokeCount = 0;
	if (!BuildDrawableStrokePayload(NormalizedPoints, StrokePointCounts, StrokePaletteIndices, IgnoredShortStrokeCount))
	{
		bPreviewScoreDirty = false;
		if (IsValid(PreviewScoreText))
		{
			PreviewScoreText->SetText(NSLOCTEXT("HeistForgery", "EmptyPreviewScore", "PREVIEW SCORE  --"));
		}
		return;
	}

	FHeistForgeryResult ForgeryPreviewResult;
	int32 ReferenceMaskPixels = 0;
	int32 SubmittedMaskPixels = 0;
	if (!IsValid(ForgeryViewModel) ||
		!ForgeryViewModel->CalculatePreviewScore(NormalizedPoints, StrokePointCounts, StrokePaletteIndices, GetConfiguredBrushSize(), ForgeryPreviewResult, ReferenceMaskPixels, SubmittedMaskPixels))
	{
		// Owner-only template score settings may arrive one replication
		// update after the drawing state. The next presentation refresh or
		// stroke change retries without generating RPC or log traffic.
		bPreviewScoreDirty = false;
		if (IsValid(PreviewScoreText))
		{
			PreviewScoreText->SetText(NSLOCTEXT("HeistForgery", "PendingPreviewScore", "PREVIEW SCORE  ..."));
		}
		return;
	}

	bPreviewScoreDirty = false;
	if (IsValid(PreviewScoreText))
	{
		PreviewScoreText->SetText(FText::Format(NSLOCTEXT("HeistForgery", "PreviewScoreFormat", "PREVIEW SCORE  {0}"), FText::AsNumber(FMath::RoundToInt(ForgeryPreviewResult.SimilarityScore))));
	}
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
	ActiveStrokeIndex = INDEX_NONE;
	bErasePointerActive = false;
}

void UHeistForgeryWidget::ResetLocalStrokePreview()
{
	FinishPointerInteraction();
	LocalStrokes.Reset();
	ErasedStrokeCount = 0;
	bPendingDrawCoordinateLog = false;
	PendingDrawMouseScreen = FVector2D::ZeroVector;
	PendingDrawNormalizedPoint = FVector2D::ZeroVector;
	DrawingInputWidgetGeometry.Reset();
	MarkPreviewScoreDirty();
	RefreshDrawingFeedback();
	InvalidateLayoutAndVolatility();
}

void UHeistForgeryWidget::RefreshDrawingFeedback()
{
	const bool bDrawingVisible = IsValid(ForgeryViewModel) && ForgeryViewModel->IsDrawingVisible();
	const int32 PointCount = GetCollectedPointCount();
	if (IsValid(DrawingPlaceholder))
	{
		DrawingPlaceholder->SetVisibility(bDrawingVisible && PointCount == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		DrawingPlaceholder->SetText(NSLOCTEXT("HeistForgery", "EmptyDrawingCanvas", "EMPTY CANVAS"));
	}
	if (IsValid(DrawingHint) && bDrawingVisible)
	{
		DrawingHint->SetText(FText::Format(NSLOCTEXT("HeistForgery", "DrawingCanvasHint", "LMB DRAW  /  RMB ERASE  /  ENTER SUBMIT    COLOR {0}    POINTS {1}/{2}"),
										   FText::AsNumber(ActivePaletteIndex + 1), FText::AsNumber(PointCount), FText::AsNumber(GetConfiguredStrokeLimit())));
	}
	if (IsValid(PreviewScoreText))
	{
		PreviewScoreText->SetVisibility(bDrawingVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

float UHeistForgeryWidget::GetNormalizedEraseRadius() const
{
	return FMath::Max(MinimumNormalizedEraseRadius, GetConfiguredBrushSize() * 1.5f);
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
