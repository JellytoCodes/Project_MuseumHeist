#include "UI/Widgets/HeistFloorPlanMapWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/HeistGameState.h"
#include "Core/HeistLogChannels.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistPlayerState.h"
#include "Data/HeistGameBalanceDataAsset.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "World/Actors/Loot/HeistDroppedOriginalActor.h"
#include "World/Actors/Loot/HeistObjectDisplayCaseActor.h"
#include "World/Actors/Loot/HeistPaintingDisplayCaseActor.h"

namespace
{
FSlateFontInfo MakeFloorPlanTenadaFont(const int32 Size)
{
	static UObject* TenadaFont = LoadObject<UObject>(nullptr, TEXT("/Game/Blueprints/UI/Fonts/F_TENADA.F_TENADA"));
	return FSlateFontInfo(TenadaFont, Size);
}

bool HasRequiredMapWidgets(const UImage* FloorPlanImage, const UOverlay* MapOverlay, const UCanvasPanel* StaticMarkerContainer,
	const UCanvasPanel* DynamicMarkerContainer)
{
	return IsValid(FloorPlanImage) && IsValid(MapOverlay) && IsValid(StaticMarkerContainer) && IsValid(DynamicMarkerContainer);
}
}

TSharedRef<SWidget> UHeistFloorPlanMapWidget::RebuildWidget()
{
	if (IsValid(WidgetTree) && !IsValid(WidgetTree->RootWidget))
	{
		UBorder* RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MapRootBorder"));
		RootBorder->SetBrushColor(FLinearColor(0.015f, 0.025f, 0.045f, 0.97f));
		RootBorder->SetPadding(FMargin(48.0f, 30.0f));

		UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MapLayout"));
		MapTitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MapTitleText"));
		MapOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("MapOverlay"));
		FloorPlanImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("FloorPlanImage"));
		StaticMarkerContainer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("StaticMarkerContainer"));
		DynamicMarkerContainer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DynamicMarkerContainer"));
		LegendText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LegendText"));
		MapHintText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MapHintText"));

		MapTitleText->SetJustification(ETextJustify::Center);
		LegendText->SetJustification(ETextJustify::Center);
		MapHintText->SetJustification(ETextJustify::Center);
		MapTitleText->SetFont(MakeFloorPlanTenadaFont(34));
		LegendText->SetFont(MakeFloorPlanTenadaFont(17));
		MapHintText->SetFont(MakeFloorPlanTenadaFont(16));
		LegendText->SetAutoWrapText(true);
		MapHintText->SetAutoWrapText(true);

		const auto AddFillLayer = [this](UWidget* Layer)
		{
			if (UOverlaySlot* LayerSlot = MapOverlay->AddChildToOverlay(Layer))
			{
				LayerSlot->SetHorizontalAlignment(HAlign_Fill);
				LayerSlot->SetVerticalAlignment(VAlign_Fill);
			}
		};
		AddFillLayer(FloorPlanImage);
		AddFillLayer(StaticMarkerContainer);
		AddFillLayer(DynamicMarkerContainer);

		Layout->AddChildToVerticalBox(MapTitleText);
		if (UVerticalBoxSlot* MapAreaSlot = Layout->AddChildToVerticalBox(MapOverlay))
		{
			MapAreaSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		Layout->AddChildToVerticalBox(LegendText);
		Layout->AddChildToVerticalBox(MapHintText);
		RootBorder->SetContent(Layout);
		WidgetTree->RootWidget = RootBorder;
	}

	return Super::RebuildWidget();
}

bool UHeistFloorPlanMapWidget::SetupMap(AHeistGameState* InGameState, AHeistPlayerController* InPlayerController, const bool bLogFailure)
{
	GameState = InGameState;
	PlayerController = InPlayerController;
	const bool bResolved = ResolveMapPresentation(bLogFailure);
	if (bResolved)
	{
		RefreshMapPresentation();
	}
	return bResolved;
}

void UHeistFloorPlanMapWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bMapPresentationReady || GetVisibility() == ESlateVisibility::Collapsed || GetVisibility() == ESlateVisibility::Hidden)
	{
		return;
	}

	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator >= 0.1f)
	{
		RefreshAccumulator = 0.0f;
		RefreshMapPresentation();
	}
}

bool UHeistFloorPlanMapWidget::ResolveMapPresentation(const bool bLogFailure)
{
	FString FailureReason;
	FName RequestedMapId = NAME_None;
	if (!IsValid(GameState) || !IsValid(PlayerController))
	{
		FailureReason = TEXT("MissingRuntimeContext");
	}
	else
	{
		RequestedMapId = GameState->GetContractSnapshot().MapId;
		if (RequestedMapId.IsNone())
		{
			FailureReason = TEXT("MissingContractMapId");
		}
		else if (!HasRequiredMapWidgets(FloorPlanImage, MapOverlay, StaticMarkerContainer, DynamicMarkerContainer))
		{
			FailureReason = TEXT("MissingMapWidgetBindings");
		}
	}

	const UHeistGameBalanceDataAsset* BalanceData = GetDefault<UHeistGameBalanceDataAsset>();
	UDataTable* PresentationTable = FailureReason.IsEmpty() && IsValid(BalanceData) && !BalanceData->MapPresentationDataTable.IsNull()
		? BalanceData->MapPresentationDataTable.LoadSynchronous()
		: nullptr;
	if (FailureReason.IsEmpty() && !IsValid(PresentationTable))
	{
		FailureReason = TEXT("MissingMapPresentationDataTable");
	}
	if (FailureReason.IsEmpty() && PresentationTable->GetRowStruct() != FHeistMapPresentationRow::StaticStruct())
	{
		FailureReason = TEXT("InvalidMapPresentationRowStruct");
	}

	const FHeistMapPresentationRow* Row = FailureReason.IsEmpty()
		? PresentationTable->FindRow<FHeistMapPresentationRow>(RequestedMapId, TEXT("FloorPlanMap"), false)
		: nullptr;
	if (FailureReason.IsEmpty() && Row == nullptr)
	{
		FailureReason = TEXT("MissingMapPresentationRow");
	}
	if (FailureReason.IsEmpty() && Row->MapId != RequestedMapId)
	{
		FailureReason = TEXT("MapPresentationRowIdMismatch");
	}
	if (FailureReason.IsEmpty() && !Row->IsRuntimeDefinitionValid(&FailureReason))
	{
		// FailureReason is supplied by the row contract.
	}

	UTexture2D* FloorPlanTexture = FailureReason.IsEmpty() ? Row->FloorPlanTexture.LoadSynchronous() : nullptr;
	if (FailureReason.IsEmpty() && !IsValid(FloorPlanTexture))
	{
		FailureReason = TEXT("FloorPlanTextureLoadFailed");
	}

	if (!FailureReason.IsEmpty())
	{
		ResetMapPresentation();
		if (bLogFailure)
		{
			UE_LOG(LogHeistUI, Error, TEXT("Floor Plan open rejected: Map=%s Reason=%s Result=FAIL"), *RequestedMapId.ToString(), *FailureReason);
		}
		return false;
	}

	const bool bStaticDefinitionChanged = ResolvedMapId != RequestedMapId ||
		MapPresentation.FloorPlanTexture.ToSoftObjectPath() != Row->FloorPlanTexture.ToSoftObjectPath() || StaticMarkerWidgets.IsEmpty();
	MapPresentation = *Row;
	ResolvedMapId = RequestedMapId;
	ResolvedFloorPlanTexture = FloorPlanTexture;
	bMapPresentationReady = true;
	FloorPlanImage->SetBrushFromTexture(ResolvedFloorPlanTexture, true);
	if (bStaticDefinitionChanged)
	{
		RebuildStaticMarkers();
	}
	return true;
}

void UHeistFloorPlanMapWidget::ResetMapPresentation()
{
	bMapPresentationReady = false;
	ResolvedMapId = NAME_None;
	ResolvedFloorPlanTexture = nullptr;
	MapPresentation = FHeistMapPresentationRow();
	StaticMarkerWidgets.Reset();
	StaticMarkerWorldLocations.Reset();
	DynamicMarkerPool.Reset();
	ActiveDynamicMarkerCount = 0;
	if (IsValid(StaticMarkerContainer))
	{
		StaticMarkerContainer->ClearChildren();
	}
	if (IsValid(DynamicMarkerContainer))
	{
		DynamicMarkerContainer->ClearChildren();
	}
	if (IsValid(FloorPlanImage))
	{
		FloorPlanImage->SetBrushFromTexture(nullptr, false);
	}
}

void UHeistFloorPlanMapWidget::RebuildStaticMarkers()
{
	StaticMarkerWidgets.Reset();
	StaticMarkerWorldLocations.Reset();
	if (!IsValid(StaticMarkerContainer))
	{
		return;
	}
	StaticMarkerContainer->ClearChildren();

	for (const FHeistMapZoneAnchor& ZoneAnchor : MapPresentation.ZoneAnchors)
	{
		const bool bTargetGallery = ZoneAnchor.ZoneId == MapPresentation.ContractTargetGalleryZoneId;
		const FText Label = bTargetGallery
			? FText::Format(NSLOCTEXT("HeistMap", "TargetGalleryMarker", "{0} · 목표 전시관"), ZoneAnchor.DisplayName)
			: ZoneAnchor.DisplayName;
		AddStaticMarker(ZoneAnchor.WorldLocation, Label,
			bTargetGallery ? EHeistFloorPlanMarkerType::TargetGallery : EHeistFloorPlanMarkerType::Zone);
	}
	for (const FHeistMapExitAnchor& ExitAnchor : MapPresentation.DefaultExitAnchors)
	{
		AddStaticMarker(ExitAnchor.WorldLocation, ExitAnchor.DisplayName, EHeistFloorPlanMarkerType::Exit);
	}
}

void UHeistFloorPlanMapWidget::RefreshStaticMarkerPositions()
{
	const int32 MarkerCount = FMath::Min(StaticMarkerWidgets.Num(), StaticMarkerWorldLocations.Num());
	for (int32 MarkerIndex = 0; MarkerIndex < MarkerCount; ++MarkerIndex)
	{
		if (UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(StaticMarkerWidgets[MarkerIndex]->Slot))
		{
			MarkerSlot->SetPosition(ProjectWorldLocation(StaticMarkerWorldLocations[MarkerIndex], StaticMarkerContainer));
		}
	}
}

void UHeistFloorPlanMapWidget::RefreshMapPresentation()
{
	if (!bMapPresentationReady || !IsValid(GameState) || !IsValid(PlayerController))
	{
		return;
	}

	const FHeistContractSnapshot Contract = GameState->GetContractSnapshot();
	if (Contract.MapId != ResolvedMapId)
	{
		if (!ResolveMapPresentation(false))
		{
			return;
		}
	}

	if (IsValid(MapTitleText))
	{
		MapTitleText->SetText(FText::Format(NSLOCTEXT("HeistMap", "Title", "박물관 도면 · {0}"), MapPresentation.MapDisplayName));
	}
	if (IsValid(LegendText))
	{
		LegendText->SetText(NSLOCTEXT("HeistMap", "Legend", "나 · 팀원 · 출구 · 구역 · 목표 전시관 · 발견한 목표 · 떨어진 원본"));
	}
	if (IsValid(MapHintText))
	{
		MapHintText->SetText(NSLOCTEXT("HeistMap", "Hint", "경비·시야·소리·미발견 전리품은 표시하지 않습니다.  M: 닫기"));
	}

	RefreshStaticMarkerPositions();
	BeginDynamicMarkerRefresh();
	for (APlayerState* PlayerStateBase : GameState->PlayerArray)
	{
		AHeistPlayerState* HeistPlayerState = Cast<AHeistPlayerState>(PlayerStateBase);
		APawn* Pawn = IsValid(HeistPlayerState) ? HeistPlayerState->GetPawn() : nullptr;
		if (!IsValid(HeistPlayerState) || !IsValid(Pawn))
		{
			continue;
		}

		const bool bLocal = Pawn == PlayerController->GetPawn();
		FText MarkerLabel = bLocal ? NSLOCTEXT("HeistMap", "LocalMarker", "나") : HeistPlayerState->GetHeistDisplayName();
		EHeistFloorPlanMarkerType MarkerType = bLocal ? EHeistFloorPlanMarkerType::LocalPlayer : EHeistFloorPlanMarkerType::Teammate;
		if (HeistPlayerState->IsEscaped())
		{
			MarkerLabel = FText::Format(NSLOCTEXT("HeistMap", "EscapedMarker", "{0} · 탈출"), MarkerLabel);
			MarkerType = EHeistFloorPlanMarkerType::EscapedTeammate;
		}
		else if (HeistPlayerState->IsArrested())
		{
			MarkerLabel = FText::Format(NSLOCTEXT("HeistMap", "ArrestedMarker", "{0} · 체포"), MarkerLabel);
			MarkerType = EHeistFloorPlanMarkerType::ArrestedTeammate;
		}

		AddDynamicMarker(Pawn->GetActorLocation(), MarkerLabel, MarkerType,
			MarkerType == EHeistFloorPlanMarkerType::Teammate ? HeistPlayerState->PlayerColor : FLinearColor::Transparent);
	}

	if (!Contract.RequiredTargetCaseId.IsNone() && !Contract.bRequiredTargetSecured)
	{
		bool bFoundExactTarget = false;
		for (TActorIterator<AHeistPaintingDisplayCaseActor> It(GetWorld()); It && !bFoundExactTarget; ++It)
		{
			if (IsValid(*It) && It->GetDisplayCaseId() == Contract.RequiredTargetCaseId &&
				ShouldShowExactPaintingTarget(It->GetDisplayCaseState(), Contract.bRequiredTargetSecured))
			{
				AddDynamicMarker(It->GetActorLocation(), NSLOCTEXT("HeistMap", "DiscoveredTargetMarker", "발견한 목표"),
					EHeistFloorPlanMarkerType::DiscoveredTarget);
				bFoundExactTarget = true;
			}
		}
		for (TActorIterator<AHeistObjectDisplayCaseActor> It(GetWorld()); It && !bFoundExactTarget; ++It)
		{
			if (IsValid(*It) && It->GetObjectCaseId() == Contract.RequiredTargetCaseId &&
				ShouldShowExactObjectTarget(It->GetAssemblyState(), Contract.bRequiredTargetSecured))
			{
				AddDynamicMarker(It->GetActorLocation(), NSLOCTEXT("HeistMap", "DiscoveredTargetMarker", "발견한 목표"),
					EHeistFloorPlanMarkerType::DiscoveredTarget);
				bFoundExactTarget = true;
			}
		}
	}

	for (TActorIterator<AHeistDroppedOriginalActor> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && It->IsDropAvailable())
		{
			AddDynamicMarker(It->GetActorLocation(),
				It->IsRequiredTarget() ? NSLOCTEXT("HeistMap", "RequiredDroppedOriginalMarker", "떨어진 필수 원본")
									   : NSLOCTEXT("HeistMap", "DroppedOriginalMarker", "떨어진 원본"),
				EHeistFloorPlanMarkerType::DroppedOriginal);
		}
	}
	FinishDynamicMarkerRefresh();
}

void UHeistFloorPlanMapWidget::BeginDynamicMarkerRefresh()
{
	ActiveDynamicMarkerCount = 0;
}

void UHeistFloorPlanMapWidget::FinishDynamicMarkerRefresh()
{
	for (int32 MarkerIndex = ActiveDynamicMarkerCount; MarkerIndex < DynamicMarkerPool.Num(); ++MarkerIndex)
	{
		if (IsValid(DynamicMarkerPool[MarkerIndex]))
		{
			DynamicMarkerPool[MarkerIndex]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UHeistFloorPlanMapWidget::AddStaticMarker(const FVector2D& WorldLocation, const FText& Label, const EHeistFloorPlanMarkerType MarkerType)
{
	if (!IsAllowedMarkerType(MarkerType))
	{
		return;
	}
	if (UTextBlock* Marker = CreateMarkerWidget(StaticMarkerContainer))
	{
		StaticMarkerWidgets.Add(Marker);
		StaticMarkerWorldLocations.Add(WorldLocation);
		ApplyMarkerPresentation(Marker, StaticMarkerContainer, WorldLocation, Label, MarkerType);
	}
}

void UHeistFloorPlanMapWidget::AddDynamicMarker(const FVector& WorldLocation, const FText& Label, const EHeistFloorPlanMarkerType MarkerType,
	const FLinearColor& CustomColor)
{
	if (!IsAllowedMarkerType(MarkerType) || !IsValid(DynamicMarkerContainer))
	{
		return;
	}

	UTextBlock* Marker = DynamicMarkerPool.IsValidIndex(ActiveDynamicMarkerCount) ? DynamicMarkerPool[ActiveDynamicMarkerCount].Get() : nullptr;
	if (!IsValid(Marker))
	{
		Marker = CreateMarkerWidget(DynamicMarkerContainer);
		if (!IsValid(Marker))
		{
			return;
		}
		if (DynamicMarkerPool.IsValidIndex(ActiveDynamicMarkerCount))
		{
			DynamicMarkerPool[ActiveDynamicMarkerCount] = Marker;
		}
		else
		{
			DynamicMarkerPool.Add(Marker);
		}
	}
	++ActiveDynamicMarkerCount;
	ApplyMarkerPresentation(Marker, DynamicMarkerContainer, FVector2D(WorldLocation.X, WorldLocation.Y), Label, MarkerType, CustomColor);
}

UTextBlock* UHeistFloorPlanMapWidget::CreateMarkerWidget(UCanvasPanel* ParentContainer)
{
	if (!IsValid(ParentContainer))
	{
		return nullptr;
	}

	UTextBlock* Marker = NewObject<UTextBlock>(ParentContainer);
	Marker->SetJustification(ETextJustify::Center);
	Marker->SetFont(MakeFloorPlanTenadaFont(17));
	if (UCanvasPanelSlot* MarkerSlot = ParentContainer->AddChildToCanvas(Marker))
	{
		MarkerSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		MarkerSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		MarkerSlot->SetAutoSize(true);
	}
	return Marker;
}

void UHeistFloorPlanMapWidget::ApplyMarkerPresentation(UTextBlock* Marker, const UCanvasPanel* ParentContainer, const FVector2D& WorldLocation,
	const FText& Label, const EHeistFloorPlanMarkerType MarkerType, const FLinearColor& CustomColor) const
{
	if (!IsValid(Marker))
	{
		return;
	}
	Marker->SetText(Label);
	Marker->SetColorAndOpacity(FSlateColor(ResolveMarkerColor(MarkerType, CustomColor)));
	Marker->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(Marker->Slot))
	{
		MarkerSlot->SetPosition(ProjectWorldLocation(WorldLocation, ParentContainer));
	}
}

FVector2D UHeistFloorPlanMapWidget::ProjectWorldLocation(const FVector2D& WorldLocation, const UCanvasPanel* TargetContainer) const
{
	const FVector2D UV = MapPresentation.ProjectWorldLocationToMapUV(WorldLocation);
	FVector2D CanvasSize = IsValid(TargetContainer) ? TargetContainer->GetCachedGeometry().GetLocalSize() : FVector2D::ZeroVector;
	if (CanvasSize.X <= 1.0 || CanvasSize.Y <= 1.0)
	{
		CanvasSize = FVector2D(800.0, 560.0);
	}
	return FVector2D(UV.X * CanvasSize.X, UV.Y * CanvasSize.Y);
}

FLinearColor UHeistFloorPlanMapWidget::ResolveMarkerColor(const EHeistFloorPlanMarkerType MarkerType, const FLinearColor& CustomColor)
{
	if (CustomColor != FLinearColor::Transparent)
	{
		return CustomColor;
	}
	switch (MarkerType)
	{
	case EHeistFloorPlanMarkerType::LocalPlayer:
		return FLinearColor(0.2f, 0.95f, 1.0f);
	case EHeistFloorPlanMarkerType::Teammate:
		return FLinearColor(0.45f, 0.78f, 1.0f);
	case EHeistFloorPlanMarkerType::Exit:
		return FLinearColor(0.25f, 1.0f, 0.35f);
	case EHeistFloorPlanMarkerType::TargetGallery:
	case EHeistFloorPlanMarkerType::DiscoveredTarget:
		return FLinearColor(1.0f, 0.72f, 0.18f);
	case EHeistFloorPlanMarkerType::DroppedOriginal:
		return FLinearColor(0.9f, 0.4f, 1.0f);
	case EHeistFloorPlanMarkerType::EscapedTeammate:
		return FLinearColor(0.2f, 1.0f, 0.55f);
	case EHeistFloorPlanMarkerType::ArrestedTeammate:
		return FLinearColor(1.0f, 0.22f, 0.18f);
	case EHeistFloorPlanMarkerType::Zone:
	default:
		return FLinearColor(0.82f, 0.88f, 0.95f);
	}
}

bool UHeistFloorPlanMapWidget::ShouldShowExactPaintingTarget(const EHeistDisplayCaseState DisplayCaseState, const bool bRequiredTargetSecured)
{
	return !bRequiredTargetSecured && DisplayCaseState != EHeistDisplayCaseState::Secured;
}

bool UHeistFloorPlanMapWidget::ShouldShowExactObjectTarget(const EHeistObjectAssemblyState AssemblyState, const bool bRequiredTargetSecured)
{
	return !bRequiredTargetSecured && AssemblyState != EHeistObjectAssemblyState::Secured;
}

bool UHeistFloorPlanMapWidget::IsAllowedMarkerType(const EHeistFloorPlanMarkerType MarkerType)
{
	switch (MarkerType)
	{
	case EHeistFloorPlanMarkerType::LocalPlayer:
	case EHeistFloorPlanMarkerType::Teammate:
	case EHeistFloorPlanMarkerType::Exit:
	case EHeistFloorPlanMarkerType::Zone:
	case EHeistFloorPlanMarkerType::TargetGallery:
	case EHeistFloorPlanMarkerType::DiscoveredTarget:
	case EHeistFloorPlanMarkerType::DroppedOriginal:
	case EHeistFloorPlanMarkerType::EscapedTeammate:
	case EHeistFloorPlanMarkerType::ArrestedTeammate:
		return true;
	case EHeistFloorPlanMarkerType::MAX:
	default:
		return false;
	}
}

bool UHeistFloorPlanMapWidget::IsMapPresentationReady() const
{
	return bMapPresentationReady;
}
