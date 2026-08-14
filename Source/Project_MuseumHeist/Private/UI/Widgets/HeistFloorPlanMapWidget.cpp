#include "UI/Widgets/HeistFloorPlanMapWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistPlayerState.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "World/Actors/Escape/HeistVentActor.h"
#include "World/Actors/Loot/HeistObjectDisplayCaseActor.h"
#include "World/Actors/Loot/HeistPaintingDisplayCaseActor.h"

namespace
{
FSlateFontInfo MakeFloorPlanTenadaFont(const int32 Size)
{
	static UObject* TenadaFont = LoadObject<UObject>(nullptr, TEXT("/Game/Blueprints/UI/Fonts/F_TENADA.F_TENADA"));
	return FSlateFontInfo(TenadaFont, Size);
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
		MapHintText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MapHintText"));
		MarkerContainer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MarkerContainer"));
		MapTitleText->SetJustification(ETextJustify::Center);
		MapHintText->SetJustification(ETextJustify::Center);
		MapTitleText->SetFont(MakeFloorPlanTenadaFont(34));
		MapHintText->SetFont(MakeFloorPlanTenadaFont(18));
		Layout->AddChildToVerticalBox(MapTitleText);
		if (UVerticalBoxSlot* MarkerAreaSlot = Layout->AddChildToVerticalBox(MarkerContainer))
		{
			MarkerAreaSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		Layout->AddChildToVerticalBox(MapHintText);
		RootBorder->SetContent(Layout);
		WidgetTree->RootWidget = RootBorder;
	}
	return Super::RebuildWidget();
}

void UHeistFloorPlanMapWidget::SetupMap(AHeistGameState* InGameState, AHeistPlayerController* InPlayerController)
{
	GameState = InGameState;
	PlayerController = InPlayerController;
	ResolveWorldBounds();
	RefreshMapPresentation();
}

void UHeistFloorPlanMapWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (GetVisibility() == ESlateVisibility::Collapsed || GetVisibility() == ESlateVisibility::Hidden)
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

void UHeistFloorPlanMapWidget::RefreshMapPresentation()
{
	if (!IsValid(GameState) || !IsValid(PlayerController) || !IsValid(MarkerContainer))
	{
		return;
	}
	const FHeistContractSnapshot Contract = GameState->GetContractSnapshot();
	if (IsValid(MapTitleText))
	{
		MapTitleText->SetText(FText::Format(NSLOCTEXT("HeistMap", "Title", "박물관 도면 · {0}"), FText::FromName(Contract.MapId)));
	}
	if (IsValid(MapHintText))
	{
		MapHintText->SetText(FText::Format(NSLOCTEXT("HeistMap", "Hint", "계약 목표: {0}  |  구역: 중앙 전시관 · 동관 · 서관  |  M: 닫기"),
			FText::FromName(Contract.RequiredTargetArtifactId)));
	}

	MarkerContainer->ClearChildren();
	for (APlayerState* PlayerStateBase : GameState->PlayerArray)
	{
		AHeistPlayerState* PlayerState = Cast<AHeistPlayerState>(PlayerStateBase);
		APawn* Pawn = IsValid(PlayerState) ? PlayerState->GetPawn() : nullptr;
		if (!IsValid(PlayerState) || !IsValid(Pawn) || PlayerState->IsEscaped())
		{
			continue;
		}
		const bool bLocal = Pawn == PlayerController->GetPawn();
		const FText Label = bLocal ? NSLOCTEXT("HeistMap", "LocalMarker", "나") : PlayerState->GetHeistDisplayName();
		AddMarker(Pawn->GetActorLocation(), Label, bLocal ? FLinearColor(0.2f, 0.95f, 1.0f) : PlayerState->PlayerColor);
	}
	for (TActorIterator<AHeistVentActor> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It))
		{
			AddMarker(It->GetActorLocation(), NSLOCTEXT("HeistMap", "ExitMarker", "출구"), FLinearColor(0.25f, 1.0f, 0.35f));
		}
	}

	if (!Contract.RequiredTargetCaseId.IsNone())
	{
		for (TActorIterator<AHeistPaintingDisplayCaseActor> It(GetWorld()); It; ++It)
		{
			if (IsValid(*It) && It->GetDisplayCaseId() == Contract.RequiredTargetCaseId && It->GetDisplayCaseState() != EHeistDisplayCaseState::Secured)
			{
				AddMarker(It->GetActorLocation(), NSLOCTEXT("HeistMap", "DiscoveredTargetMarker", "발견한 목표"), FLinearColor(1.0f, 0.72f, 0.18f));
				break;
			}
		}
		for (TActorIterator<AHeistObjectDisplayCaseActor> It(GetWorld()); It; ++It)
		{
			if (IsValid(*It) && It->GetObjectCaseId() == Contract.RequiredTargetCaseId && It->GetAssemblyState() != EHeistObjectAssemblyState::Secured)
			{
				AddMarker(It->GetActorLocation(), NSLOCTEXT("HeistMap", "DiscoveredTargetMarker", "발견한 목표"), FLinearColor(1.0f, 0.72f, 0.18f));
				break;
			}
		}
	}
}

void UHeistFloorPlanMapWidget::AddMarker(const FVector& WorldLocation, const FText& Label, const FLinearColor& Color)
{
	UTextBlock* Marker = NewObject<UTextBlock>(MarkerContainer);
	Marker->SetText(Label);
	Marker->SetColorAndOpacity(FSlateColor(Color));
	Marker->SetJustification(ETextJustify::Center);
	Marker->SetFont(MakeFloorPlanTenadaFont(17));
	if (UCanvasPanelSlot* MarkerSlot = MarkerContainer->AddChildToCanvas(Marker))
	{
		MarkerSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		MarkerSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		MarkerSlot->SetAutoSize(true);
		MarkerSlot->SetPosition(ProjectWorldLocation(WorldLocation));
	}
}

FVector2D UHeistFloorPlanMapWidget::ProjectWorldLocation(const FVector& WorldLocation) const
{
	const FVector2D Size = WorldBounds.GetSize();
	const FVector2D Normalized((WorldLocation.X - WorldBounds.Min.X) / FMath::Max(1.0, Size.X), (WorldLocation.Y - WorldBounds.Min.Y) / FMath::Max(1.0, Size.Y));
	const FVector2D CanvasSize = IsValid(MarkerContainer) ? MarkerContainer->GetCachedGeometry().GetLocalSize() : FVector2D(800.0f, 560.0f);
	return FVector2D(FMath::Clamp(Normalized.X, 0.0, 1.0) * CanvasSize.X, (1.0 - FMath::Clamp(Normalized.Y, 0.0, 1.0)) * CanvasSize.Y);
}

void UHeistFloorPlanMapWidget::ResolveWorldBounds()
{
	FBox2D Bounds(ForceInit);
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		Bounds += FVector2D(It->GetActorLocation());
	}
	for (TActorIterator<AHeistVentActor> It(GetWorld()); It; ++It)
	{
		Bounds += FVector2D(It->GetActorLocation());
	}
	if (Bounds.bIsValid)
	{
		const FVector2D WorldPadding(800.0, 800.0);
		WorldBounds = FBox2D(Bounds.Min - WorldPadding, Bounds.Max + WorldPadding);
	}
}
