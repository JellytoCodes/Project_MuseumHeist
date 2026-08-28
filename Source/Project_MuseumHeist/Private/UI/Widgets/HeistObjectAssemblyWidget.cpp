#include "UI/Widgets/HeistObjectAssemblyWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Core/HeistPlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "InputCoreTypes.h"
#include "UI/ViewModels/HeistObjectAssemblyViewModel.h"

namespace
{
constexpr double AssemblyWorkAreaRatio = 0.72;
constexpr double TrayCellWidth = 136.0;
constexpr double TrayCellHeight = 88.0;

void ApplyText(UTextBlock* TextBlock, const FText& Text)
{
	if (IsValid(TextBlock))
	{
		TextBlock->SetText(Text);
	}
}

FLinearColor ResolveQualityColor(const float Score, const float MinimumScore)
{
	return Score >= MinimumScore ? FLinearColor(0.25f, 0.95f, 0.42f) : FLinearColor(1.0f, 0.40f, 0.10f);
}

FLinearColor ResolvePieceColor(const int32 CandidateIndex, const bool bPlaced)
{
	static const FLinearColor Colors[] = {FLinearColor(0.21f, 0.50f, 0.78f), FLinearColor(0.72f, 0.38f, 0.18f), FLinearColor(0.34f, 0.64f, 0.42f),
										  FLinearColor(0.62f, 0.35f, 0.70f), FLinearColor(0.74f, 0.62f, 0.20f), FLinearColor(0.28f, 0.62f, 0.66f)};
	FLinearColor Result = Colors[FMath::Abs(CandidateIndex) % UE_ARRAY_COUNT(Colors)];
	Result.A = bPlaced ? 1.0f : 0.78f;
	return Result;
}
}

UHeistObjectAssemblyWidget::UHeistObjectAssemblyWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UHeistObjectAssemblyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindActionButtons();
	RefreshObjectAssemblyPresentation();
}

void UHeistObjectAssemblyWidget::NativeDestruct()
{
	if (IsValid(ObjectAssemblyViewModel))
	{
		ObjectAssemblyViewModel->GetPresentationChangedDelegate().RemoveAll(this);
	}
	ClearPartTiles();
	Super::NativeDestruct();
}

void UHeistObjectAssemblyWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshCountdownPresentation();
	if (IsValid(ObjectAssemblyViewModel) && ObjectAssemblyViewModel->IsDataReady())
	{
		if (DisplayedSessionRevision != ObjectAssemblyViewModel->GetSessionRevision() || PartTiles.Num() != ObjectAssemblyViewModel->GetCandidatePartCount())
		{
			RebuildPartTiles();
		}
		if (DisplayedPreviewRevision != ObjectAssemblyViewModel->GetLocalPreviewRevision())
		{
			RefreshPartTilePresentation();
		}
	}
}

FReply UHeistObjectAssemblyWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!InKeyEvent.IsRepeat() && InKeyEvent.GetKey() == EKeys::Enter)
	{
		HandleSubmitClicked();
		return FReply::Handled();
	}
	if (!InKeyEvent.IsRepeat() && InKeyEvent.GetKey() == EKeys::Escape)
	{
		HandleCancelClicked();
		return FReply::Handled();
	}
	if (!InKeyEvent.IsRepeat() && InKeyEvent.GetKey() == EKeys::R && IsValid(ObjectAssemblyViewModel))
	{
		ObjectAssemblyViewModel->ResetLocalAssembly();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UHeistObjectAssemblyWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!IsValid(ObjectAssemblyViewModel) || !ObjectAssemblyViewModel->IsDataReady() || !IsValid(AssemblyCanvas))
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	const FName PartId = FindPartTileAtScreenPosition(InMouseEvent.GetScreenSpacePosition());
	if (PartId.IsNone())
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		ObjectAssemblyViewModel->RemovePart(PartId);
		return FReply::Handled();
	}
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	DraggedPartId = PartId;
	const FVector2D CanvasPoint = AssemblyCanvas->GetCachedGeometry().AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	const UBorder* Tile = PartTiles.FindRef(PartId);
	const UCanvasPanelSlot* CanvasSlot = IsValid(Tile) ? Cast<UCanvasPanelSlot>(Tile->Slot) : nullptr;
	DragOffset = IsValid(CanvasSlot) ? CanvasSlot->GetPosition() - CanvasPoint : FVector2D::ZeroVector;
	SetKeyboardFocus();
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UHeistObjectAssemblyWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (DraggedPartId.IsNone() || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !IsValid(AssemblyCanvas) || !IsValid(ObjectAssemblyViewModel))
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}

	const FName ReleasedPartId = DraggedPartId;
	DraggedPartId = NAME_None;
	const FVector2D CanvasPoint = AssemblyCanvas->GetCachedGeometry().AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	const FVector2D CanvasSize = GetAssemblyCanvasSize();
	if (CanvasPoint.Y >= CanvasSize.Y * AssemblyWorkAreaRatio)
	{
		ObjectAssemblyViewModel->RemovePart(ReleasedPartId);
	}
	else
	{
		const FName SocketId = ResolveClosestCompatibleSocket(ReleasedPartId, CanvasPoint);
		if (!SocketId.IsNone())
		{
			ObjectAssemblyViewModel->PlacePartAtSocket(ReleasedPartId, SocketId);
		}
	}
	RefreshPartTilePresentation();
	return FReply::Handled().ReleaseMouseCapture();
}

FReply UHeistObjectAssemblyWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (DraggedPartId.IsNone() || !InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) || !IsValid(AssemblyCanvas))
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}

	const FVector2D CanvasSize = GetAssemblyCanvasSize();
	const FVector2D TileSize = ResolvePartTileSize(DraggedPartId);
	const FVector2D CanvasPoint = AssemblyCanvas->GetCachedGeometry().AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	const FVector2D RequestedPosition = CanvasPoint + DragOffset;
	SetPartTilePosition(DraggedPartId, FVector2D(FMath::Clamp(RequestedPosition.X, 0.0, FMath::Max(0.0, CanvasSize.X - TileSize.X)),
												 FMath::Clamp(RequestedPosition.Y, 0.0, FMath::Max(0.0, CanvasSize.Y - TileSize.Y))));
	return FReply::Handled();
}

FReply UHeistObjectAssemblyWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsValid(ObjectAssemblyViewModel))
	{
		const FName PartId = FindPartTileAtScreenPosition(InMouseEvent.GetScreenSpacePosition());
		if (!PartId.IsNone() && ObjectAssemblyViewModel->RotatePart(PartId, InMouseEvent.GetWheelDelta() > 0.0f ? 1 : -1))
		{
			return FReply::Handled();
		}
	}
	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

void UHeistObjectAssemblyWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
	DraggedPartId = NAME_None;
	RefreshPartTilePresentation();
}

void UHeistObjectAssemblyWidget::SetupObjectAssemblyWidget(UHeistObjectAssemblyViewModel* InObjectAssemblyViewModel, AHeistPlayerController* InPlayerController)
{
	if (ObjectAssemblyViewModel != InObjectAssemblyViewModel && IsValid(ObjectAssemblyViewModel))
	{
		ObjectAssemblyViewModel->GetPresentationChangedDelegate().RemoveAll(this);
	}

	ObjectAssemblyViewModel = InObjectAssemblyViewModel;
	PlayerController = InPlayerController;
	if (IsValid(ObjectAssemblyViewModel))
	{
		ObjectAssemblyViewModel->GetPresentationChangedDelegate().RemoveAll(this);
		ObjectAssemblyViewModel->GetPresentationChangedDelegate().AddUObject(this, &UHeistObjectAssemblyWidget::RefreshObjectAssemblyPresentation);
	}

	BP_OnObjectAssemblySourcesReady();
	RefreshObjectAssemblyPresentation();
}

bool UHeistObjectAssemblyWidget::IsOwnerOnlyContractSatisfied() const
{
	return IsValid(ObjectAssemblyViewModel) && IsValid(PlayerController) && PlayerController == GetOwningPlayer() && PlayerController->IsLocalController() &&
		   ObjectAssemblyViewModel->IsOwnerOnlyContractSatisfied();
}

bool UHeistObjectAssemblyWidget::IsWidgetPresentationVisible() const
{
	return GetVisibility() != ESlateVisibility::Collapsed && GetVisibility() != ESlateVisibility::Hidden;
}

bool UHeistObjectAssemblyWidget::IsCanvasReady() const
{
	return IsValid(AssemblyCanvas) && IsValid(ObjectAssemblyViewModel) && ObjectAssemblyViewModel->IsDataReady() && PartTiles.Num() == ObjectAssemblyViewModel->GetCandidatePartCount();
}

int32 UHeistObjectAssemblyWidget::GetPartTileCount() const
{
	return PartTiles.Num();
}

void UHeistObjectAssemblyWidget::BindActionButtons()
{
	if (IsValid(SubmitButton))
	{
		SubmitButton->OnClicked.RemoveAll(this);
		SubmitButton->OnClicked.AddDynamic(this, &UHeistObjectAssemblyWidget::HandleSubmitClicked);
	}
	if (IsValid(CancelButton))
	{
		CancelButton->OnClicked.RemoveAll(this);
		CancelButton->OnClicked.AddDynamic(this, &UHeistObjectAssemblyWidget::HandleCancelClicked);
	}
}

void UHeistObjectAssemblyWidget::RefreshObjectAssemblyPresentation()
{
	const bool bVisible = IsValid(ObjectAssemblyViewModel) && ObjectAssemblyViewModel->IsPresentationVisible();
	if (!bVisible)
	{
		DraggedPartId = NAME_None;
		ClearPartTiles();
		SetVisibility(ESlateVisibility::Collapsed);
		LastDisplayedAssemblyTimeSeconds = INDEX_NONE;
		BP_RefreshObjectAssemblyPresentation(false, false, 0, 0);
		return;
	}
	const bool bDataReady = ObjectAssemblyViewModel->IsDataReady();
	SetVisibility(ESlateVisibility::Visible);
	ApplyText(TitleText, NSLOCTEXT("HeistCommonForgeryUI", "AssemblyTitle", "조각 조립"));

	const float MinimumScore = ObjectAssemblyViewModel->GetMinimumAcceptedQualityScore();
	const FText PreviewQualityText =
		ObjectAssemblyViewModel->HasPreviewQuality()
			? FText::Format(NSLOCTEXT("HeistCommonForgeryUI", "ExpectedScore", "예상 품질  {0}/100  ·  제출 가능 {1}+"),
							FText::AsNumber(FMath::RoundToInt(ObjectAssemblyViewModel->GetPreviewQualityScore())), FText::AsNumber(FMath::RoundToInt(MinimumScore)))
			: FText::Format(NSLOCTEXT("HeistCommonForgeryUI", "ExpectedScoreUnavailable", "예상 품질  --/100  ·  제출 가능 {0}+"), FText::AsNumber(FMath::RoundToInt(MinimumScore)));
	ApplyText(PreviewScoreText, PreviewQualityText);
	if (IsValid(PreviewScoreText))
	{
		PreviewScoreText->SetColorAndOpacity(ObjectAssemblyViewModel->HasPreviewQuality() ? ResolveQualityColor(ObjectAssemblyViewModel->GetPreviewQualityScore(), MinimumScore)
																						  : FLinearColor(0.72f, 0.76f, 0.82f));
	}
	ApplyText(SubmitButtonLabel, NSLOCTEXT("HeistCommonForgeryUI", "SubmitButton", "제출"));
	ApplyText(CancelButtonLabel, NSLOCTEXT("HeistCommonForgeryUI", "CancelButton", "취소"));
	ApplyText(FooterHint, NSLOCTEXT("HeistObjectAssembly", "FooterHint", "좌클릭 드래그 배치  |  휠 회전  |  우클릭 제거  |  R 초기화  |  Enter 제출  |  Esc 취소  |  주변 소리와 팀 음성 유지"));

	if (IsValid(SubmitButton))
	{
		SubmitButton->SetIsEnabled(ObjectAssemblyViewModel->CanSubmitAssembly());
	}
	if (IsValid(CancelButton))
	{
		CancelButton->SetIsEnabled(true);
	}

	RebuildPartTiles();
	RefreshPartTilePresentation();
	RefreshCountdownPresentation();
	BP_RefreshObjectAssemblyPresentation(true, bDataReady, ObjectAssemblyViewModel->GetPlacedPartCount(), ObjectAssemblyViewModel->GetRequiredPartCount());
}

void UHeistObjectAssemblyWidget::RefreshCountdownPresentation()
{
	if (!IsValid(ObjectAssemblyViewModel) || !ObjectAssemblyViewModel->IsPresentationVisible())
	{
		return;
	}

	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr;
	const bool bHasAuthoritativeTime = IsValid(GameState) && ObjectAssemblyViewModel->GetSessionEndServerTime() > 0.0f;
	const float ServerWorldTime = bHasAuthoritativeTime ? static_cast<float>(GameState->GetServerWorldTimeSeconds()) : 0.0f;
	const int32 RemainingSeconds = bHasAuthoritativeTime ? FMath::Max(0, FMath::CeilToInt(ObjectAssemblyViewModel->GetSessionEndServerTime() - ServerWorldTime)) : INDEX_NONE;
	if (RemainingSeconds == LastDisplayedAssemblyTimeSeconds)
	{
		return;
	}

	LastDisplayedAssemblyTimeSeconds = RemainingSeconds;
	const FText TimeText = RemainingSeconds == INDEX_NONE ? FText::FromString(TEXT("--:--")) : FText::FromString(FString::Printf(TEXT("%02d:%02d"), RemainingSeconds / 60, RemainingSeconds % 60));
	ApplyText(AssemblyTimeRemainingText, FText::Format(NSLOCTEXT("HeistCommonForgeryUI", "TimeRemaining", "남은 시간  {0}"), TimeText));
}

void UHeistObjectAssemblyWidget::RebuildPartTiles()
{
	if (!IsValid(AssemblyCanvas) || !IsValid(ObjectAssemblyViewModel) || !ObjectAssemblyViewModel->IsDataReady() || !IsValid(WidgetTree))
	{
		return;
	}
	if (DisplayedSessionRevision == ObjectAssemblyViewModel->GetSessionRevision() && PartTiles.Num() == ObjectAssemblyViewModel->GetCandidatePartCount())
	{
		return;
	}

	ClearPartTiles();
	const TArray<FName>& CandidatePartIds = ObjectAssemblyViewModel->GetCandidatePartIds();
	for (int32 CandidateIndex = 0; CandidateIndex < CandidatePartIds.Num(); ++CandidateIndex)
	{
		const FName PartId = CandidatePartIds[CandidateIndex];
		UBorder* PartTile = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(*FString::Printf(TEXT("AssemblyPiece_%s"), *PartId.ToString())));
		UTextBlock* PartLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*FString::Printf(TEXT("AssemblyPieceLabel_%s"), *PartId.ToString())));
		if (!IsValid(PartTile) || !IsValid(PartLabel))
		{
			continue;
		}

		PartLabel->SetText(ObjectAssemblyViewModel->GetPartDisplayText(PartId));
		if (IsValid(KoreanUIFont.Get()))
		{
			FSlateFontInfo PartFont = PartLabel->GetFont();
			PartFont.FontObject = KoreanUIFont.Get();
			PartFont.TypefaceFontName = TEXT("Regular");
			PartLabel->SetFont(PartFont);
		}
		PartLabel->SetJustification(ETextJustify::Center);
		PartLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		PartLabel->SetAutoWrapText(true);
		PartTile->SetPadding(FMargin(8.0f));
		PartTile->SetHorizontalAlignment(HAlign_Center);
		PartTile->SetVerticalAlignment(VAlign_Center);
		PartTile->SetContent(PartLabel);
		UCanvasPanelSlot* TileSlot = AssemblyCanvas->AddChildToCanvas(PartTile);
		TileSlot->SetAutoSize(false);
		TileSlot->SetSize(ResolvePartTileSize(PartId));
		PartTiles.Add(PartId, PartTile);
	}

	DisplayedSessionRevision = ObjectAssemblyViewModel->GetSessionRevision();
	DisplayedPreviewRevision = INDEX_NONE;
}

void UHeistObjectAssemblyWidget::ClearPartTiles()
{
	for (const TPair<FName, TObjectPtr<UBorder>>& Pair : PartTiles)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->RemoveFromParent();
		}
	}
	PartTiles.Reset();
	DisplayedSessionRevision = INDEX_NONE;
	DisplayedPreviewRevision = INDEX_NONE;
}

void UHeistObjectAssemblyWidget::RefreshPartTilePresentation()
{
	if (!IsValid(ObjectAssemblyViewModel) || !IsValid(AssemblyCanvas))
	{
		return;
	}

	const FVector2D CanvasSize = GetAssemblyCanvasSize();
	const TArray<FName>& CandidatePartIds = ObjectAssemblyViewModel->GetCandidatePartIds();
	for (int32 CandidateIndex = 0; CandidateIndex < CandidatePartIds.Num(); ++CandidateIndex)
	{
		const FName PartId = CandidatePartIds[CandidateIndex];
		UBorder* Tile = PartTiles.FindRef(PartId);
		if (!IsValid(Tile))
		{
			continue;
		}

		const bool bPlaced = ObjectAssemblyViewModel->IsPartPlaced(PartId);
		Tile->SetBrushColor(ResolvePieceColor(CandidateIndex, bPlaced));
		Tile->SetRenderTransformAngle(bPlaced ? static_cast<float>(ObjectAssemblyViewModel->GetPlacedPartOrientation(PartId)) * 22.5f : 0.0f);
		if (PartId == DraggedPartId)
		{
			continue;
		}

		const FVector2D TileSize = ResolvePartTileSize(PartId);
		FVector2D Position = ResolveTrayPosition(CandidateIndex, CanvasSize, TileSize);
		if (bPlaced)
		{
			for (const FHeistObjectAssemblyEntry& Entry : ObjectAssemblyViewModel->GetLocalAssemblyEntries())
			{
				if (Entry.PartId == PartId)
				{
					const FVector2D Anchor = ResolveSocketAnchorNormalized(Entry.SocketId);
					Position = FVector2D(Anchor.X * CanvasSize.X - TileSize.X * 0.5, Anchor.Y * (CanvasSize.Y * AssemblyWorkAreaRatio) - TileSize.Y * 0.5);
					break;
				}
			}
		}
		SetPartTilePosition(PartId, Position);
	}
	DisplayedPreviewRevision = ObjectAssemblyViewModel->GetLocalPreviewRevision();
}

FName UHeistObjectAssemblyWidget::FindPartTileAtScreenPosition(const FVector2D& ScreenPosition) const
{
	for (const TPair<FName, TObjectPtr<UBorder>>& Pair : PartTiles)
	{
		if (IsValid(Pair.Value) && Pair.Value->GetCachedGeometry().IsUnderLocation(ScreenPosition))
		{
			return Pair.Key;
		}
	}
	return NAME_None;
}

FName UHeistObjectAssemblyWidget::ResolveClosestCompatibleSocket(const FName PartId, const FVector2D& CanvasPoint) const
{
	if (!IsValid(ObjectAssemblyViewModel))
	{
		return NAME_None;
	}

	const FVector2D CanvasSize = GetAssemblyCanvasSize();
	const FVector2D WorkSize(CanvasSize.X, CanvasSize.Y * AssemblyWorkAreaRatio);
	const FVector2D NormalizedPoint(FMath::Clamp(CanvasPoint.X / FMath::Max(1.0, WorkSize.X), 0.0, 1.0), FMath::Clamp(CanvasPoint.Y / FMath::Max(1.0, WorkSize.Y), 0.0, 1.0));
	FName ClosestSocketId = NAME_None;
	double ClosestDistanceSquared = TNumericLimits<double>::Max();
	for (const FName SocketId : ObjectAssemblyViewModel->GetCompatibleSocketIds(PartId))
	{
		const double DistanceSquared = FVector2D::DistSquared(NormalizedPoint, ResolveSocketAnchorNormalized(SocketId));
		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			ClosestSocketId = SocketId;
		}
	}
	return ClosestSocketId;
}

FVector2D UHeistObjectAssemblyWidget::ResolveSocketAnchorNormalized(const FName SocketId) const
{
	const FString SocketName = SocketId.ToString();
	if (SocketName.Equals(TEXT("Crest"), ESearchCase::IgnoreCase))
	{
		return FVector2D(0.50, 0.10);
	}
	if (SocketName.Equals(TEXT("Head"), ESearchCase::IgnoreCase) || SocketName.Equals(TEXT("Lid"), ESearchCase::IgnoreCase))
	{
		return FVector2D(0.50, 0.23);
	}
	if (SocketName.Contains(TEXT("_L"), ESearchCase::IgnoreCase))
	{
		return FVector2D(0.27, 0.48);
	}
	if (SocketName.Equals(TEXT("Spout"), ESearchCase::IgnoreCase))
	{
		return FVector2D(0.78, 0.40);
	}
	if (SocketName.Contains(TEXT("_R"), ESearchCase::IgnoreCase))
	{
		return FVector2D(0.73, 0.48);
	}
	if (SocketName.Equals(TEXT("Foot"), ESearchCase::IgnoreCase) || SocketName.Equals(TEXT("Pedestal"), ESearchCase::IgnoreCase))
	{
		return FVector2D(0.50, 0.80);
	}

	static const FVector2D FallbackAnchors[] = {FVector2D(0.34, 0.32), FVector2D(0.66, 0.32), FVector2D(0.34, 0.66), FVector2D(0.66, 0.66)};
	return FallbackAnchors[GetTypeHash(SocketName) % UE_ARRAY_COUNT(FallbackAnchors)];
}

FVector2D UHeistObjectAssemblyWidget::ResolvePartTileSize(const FName PartId) const
{
	const FString PartName = PartId.ToString();
	if (PartName.Contains(TEXT("Arm"), ESearchCase::IgnoreCase) || PartName.Contains(TEXT("Handle"), ESearchCase::IgnoreCase) || PartName.Contains(TEXT("Spout"), ESearchCase::IgnoreCase))
	{
		return FVector2D(118.0, 54.0);
	}
	if (PartName.Contains(TEXT("Pedestal"), ESearchCase::IgnoreCase) || PartName.Contains(TEXT("Foot"), ESearchCase::IgnoreCase))
	{
		return FVector2D(140.0, 60.0);
	}
	if (PartName.Contains(TEXT("Head"), ESearchCase::IgnoreCase) || PartName.Contains(TEXT("Lid"), ESearchCase::IgnoreCase) || PartName.Contains(TEXT("Crest"), ESearchCase::IgnoreCase))
	{
		return FVector2D(78.0, 78.0);
	}
	return FVector2D(96.0, 70.0);
}

FVector2D UHeistObjectAssemblyWidget::ResolveTrayPosition(const int32 CandidateIndex, const FVector2D& CanvasSize, const FVector2D& TileSize) const
{
	const int32 ColumnCount = FMath::Max(1, FMath::FloorToInt(CanvasSize.X / TrayCellWidth));
	const int32 Row = CandidateIndex / ColumnCount;
	const int32 Column = CandidateIndex % ColumnCount;
	const double RowWidth = FMath::Min(ColumnCount, FMath::Max(1, ObjectAssemblyViewModel->GetCandidatePartCount() - Row * ColumnCount)) * TrayCellWidth;
	const double StartX = FMath::Max(12.0, (CanvasSize.X - RowWidth) * 0.5);
	return FVector2D(StartX + Column * TrayCellWidth + (TrayCellWidth - TileSize.X) * 0.5, CanvasSize.Y * AssemblyWorkAreaRatio + 10.0 + Row * TrayCellHeight + (TrayCellHeight - TileSize.Y) * 0.5);
}

FVector2D UHeistObjectAssemblyWidget::GetAssemblyCanvasSize() const
{
	if (!IsValid(AssemblyCanvas))
	{
		return FVector2D(900.0, 560.0);
	}
	const FVector2D CachedSize = AssemblyCanvas->GetCachedGeometry().GetLocalSize();
	return CachedSize.X >= 100.0 && CachedSize.Y >= 100.0 ? CachedSize : FVector2D(900.0, 560.0);
}

void UHeistObjectAssemblyWidget::SetPartTilePosition(const FName PartId, const FVector2D& Position)
{
	UBorder* Tile = PartTiles.FindRef(PartId);
	UCanvasPanelSlot* CanvasSlot = IsValid(Tile) ? Cast<UCanvasPanelSlot>(Tile->Slot) : nullptr;
	if (IsValid(CanvasSlot))
	{
		CanvasSlot->SetPosition(Position);
	}
}

void UHeistObjectAssemblyWidget::HandleSubmitClicked()
{
	if (IsValid(ObjectAssemblyViewModel))
	{
		ObjectAssemblyViewModel->RequestSubmitAssembly();
	}
}

void UHeistObjectAssemblyWidget::HandleCancelClicked()
{
	if (IsValid(ObjectAssemblyViewModel))
	{
		ObjectAssemblyViewModel->RequestCancelAssembly();
	}
}
