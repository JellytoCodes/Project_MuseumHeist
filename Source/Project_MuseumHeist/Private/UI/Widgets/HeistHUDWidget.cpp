#include "UI/Widgets/HeistHUDWidget.h"

#include "Character/Components/HeistInteractionComponent.h"
#include "Character/Components/HeistStatusComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Components/AudioComponent.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/Widget.h"
#include "Core/HeistGameplayTags.h"
#include "Core/HeistGameState.h"
#include "Core/HeistLogChannels.h"
#include "Core/HeistPlayerController.h"
#include "Engine/Texture2D.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "UI/Pool/HeistPopupWidgetPool.h"
#include "UI/ViewModels/HeistHUDViewModel.h"
#include "UI/ViewModels/HeistInventoryViewModel.h"
#include "UI/ViewModels/HeistQuickSlotViewModel.h"
#include "UI/Widgets/HeistInteractionPromptWidget.h"
#include "Blueprint/WidgetTree.h"

namespace
{
FSlateFontInfo MakeHUDTenadaFont(const int32 Size)
{
	static UObject* TenadaFont = LoadObject<UObject>(nullptr, TEXT("/Game/Blueprints/UI/Fonts/F_TENADA.F_TENADA"));
	return FSlateFontInfo(TenadaFont, Size);
}

const FName ArrestedFeedbackEvent(TEXT("Arrested"));
const FName RescuedFeedbackEvent(TEXT("Rescued"));
}

#pragma region Construction

UHeistHUDWidget::UHeistHUDWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

#pragma endregion

#pragma region Lifecycle

void UHeistHUDWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (GetVisibility() == ESlateVisibility::Collapsed || GetVisibility() == ESlateVisibility::Hidden)
	{
		return;
	}
	RefreshLockdownCountdown();
	RefreshStunCountdown();
}

void UHeistHUDWidget::NativeDestruct()
{
	if (IsValid(InteractionComponent))
	{
		InteractionComponent->GetInteractionTargetChangedDelegate().RemoveAll(this);
	}
	if (IsValid(PopupWidgetPool))
	{
		PopupWidgetPool->ShutdownPool();
	}
	if (IsValid(HUDViewModel))
	{
		HUDViewModel->GetPresentationChangedDelegate().RemoveAll(this);
	}
	if (IsValid(QuickSlotViewModel))
	{
		QuickSlotViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	}
	if (IsValid(TutorialPlayerController))
	{
		TutorialPlayerController->GetTutorialPresentationChangedDelegate().RemoveAll(this);
	}
	ResetHiddenPresentationState();
	Super::NativeDestruct();
}

#pragma endregion

#pragma region ViewModels

void UHeistHUDWidget::SetupHUDWidget(UHeistHUDViewModel* InHUDViewModel, UHeistInventoryViewModel* InInventoryViewModel, UHeistQuickSlotViewModel* InQuickSlotViewModel,
									 UHeistInteractionComponent* InInteractionComponent)
{
	checkf(IsValid(InHUDViewModel), TEXT("HeistHUDWidget requires a valid HUD ViewModel."));

	if (HUDViewModel != InHUDViewModel && IsValid(HUDViewModel))
	{
		HUDViewModel->GetPresentationChangedDelegate().RemoveAll(this);
	}
	HUDViewModel = InHUDViewModel;
	InventoryViewModel = InInventoryViewModel;
	if (QuickSlotViewModel != InQuickSlotViewModel && IsValid(QuickSlotViewModel))
	{
		QuickSlotViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	}
	QuickSlotViewModel = InQuickSlotViewModel;
	if (IsValid(QuickSlotViewModel))
	{
		QuickSlotViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
		QuickSlotViewModel->GetSnapshotChangedDelegate().AddUObject(this, &UHeistHUDWidget::RefreshToolPresentation);
	}
	if (InteractionComponent != InInteractionComponent && IsValid(InteractionComponent))
	{
		InteractionComponent->GetInteractionTargetChangedDelegate().RemoveAll(this);
	}
	InteractionComponent = InInteractionComponent;
	if (IsValid(InteractionComponent))
	{
		InteractionComponent->GetInteractionTargetChangedDelegate().RemoveAll(this);
		InteractionComponent->GetInteractionTargetChangedDelegate().AddUObject(this, &UHeistHUDWidget::RefreshCrosshairPresentation);
	}

	HUDViewModel->GetPresentationChangedDelegate().RemoveAll(this);
	HUDViewModel->GetPresentationChangedDelegate().AddUObject(this, &UHeistHUDWidget::RefreshHUDPresentation);
	ResolveInteractionChildWidgets();
	ResolveCrosshairWidgets();
	ResolveCrewPresentationWidgets();
	UE_LOG(LogHeistUI, Verbose,
		   TEXT("[%s] HUD widget setup: Class=%s HUDViewModel=%s InteractionComponent=%s InteractionPromptWidget=%s InteractionPromptClass=%s ActionProgressWidget=%s ActionProgressClass=%s"),
		   *GetName(), *GetClass()->GetName(), *GetNameSafe(HUDViewModel.Get()), *GetNameSafe(InteractionComponent.Get()), *GetNameSafe(InteractionPromptWidget.Get()),
		   IsValid(InteractionPromptWidget) ? *InteractionPromptWidget->GetClass()->GetName() : TEXT("None"), *GetNameSafe(ActionProgressWidget.Get()),
		   IsValid(ActionProgressWidget) ? *ActionProgressWidget->GetClass()->GetName() : TEXT("None"));

	BP_OnHUDSourcesReady();
	ResolveInteractionChildWidgets();
	ResolveCrosshairWidgets();
	if (IsValid(InteractionPromptWidget))
	{
		InteractionPromptWidget->SetupInteractionPresentation(InteractionComponent, HUDViewModel);
	}
	if (IsValid(ActionProgressWidget))
	{
		ActionProgressWidget->SetupInteractionPresentation(InteractionComponent, HUDViewModel);
	}
	RefreshCrosshairPresentation(IsValid(InteractionComponent) ? InteractionComponent->GetCurrentInteractionTarget() : nullptr,
								 IsValid(InteractionComponent) && InteractionComponent->HasValidInteractionTarget());
	SetupPopupFeedbackPresentation();
	SetupTutorialPresentation();
	RefreshToolPresentation();
	RefreshHUDPresentation();
}

void UHeistHUDWidget::RefreshCrewStatusPresentation()
{
	if (!IsValid(HUDViewModel))
	{
		return;
	}

	const EHeistCrewStatus LocalCrewStatus = ResolveLocalCrewStatus();
	ApplyLocalCrewStatusPresentation(LocalCrewStatus);

	if (!IsValid(TeamStatusContainer) || !IsValid(WidgetTree))
	{
		return;
	}

	TeamStatusContainer->ClearChildren();
	for (const FHeistCrewStatusEntry& Entry : HUDViewModel->GetCrewStatusEntries())
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		UBorder* StatusBadge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		UTextBlock* StatusValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		NameText->SetText(Entry.PlayerName);
		NameText->SetColorAndOpacity(FSlateColor(Entry.PlayerColor));
		NameText->SetJustification(ETextJustify::Left);
		NameText->SetFont(MakeHUDTenadaFont(16));
		StatusBadge->SetBrushColor(HeistCrewStatus::GetPresentationColor(Entry.Status));
		StatusBadge->SetPadding(FMargin(5.0f, 1.0f));
		if (UTexture2D* StatusIconTexture = ResolveStatusIconTexture(Entry.Status))
		{
			UImage* StatusIconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			StatusIconImage->SetBrushFromTexture(StatusIconTexture, true);
			StatusIconImage->SetDesiredSizeOverride(FVector2D(16.0f, 16.0f));
			StatusIconImage->SetColorAndOpacity(FLinearColor::White);
			StatusBadge->SetContent(StatusIconImage);
		}
		else
		{
			UTextBlock* StatusIconText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			StatusIconText->SetText(HeistCrewStatus::ToIconGlyph(Entry.Status));
			StatusIconText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			StatusIconText->SetJustification(ETextJustify::Center);
			StatusIconText->SetFont(MakeHUDTenadaFont(14));
			StatusBadge->SetContent(StatusIconText);
		}
		StatusValueText->SetText(HeistCrewStatus::ToDisplayText(Entry.Status));
		StatusValueText->SetColorAndOpacity(FSlateColor(HeistCrewStatus::GetPresentationColor(Entry.Status)));
		StatusValueText->SetJustification(ETextJustify::Right);
		StatusValueText->SetFont(MakeHUDTenadaFont(16));

		if (UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(NameText))
		{
			NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			NameSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* BadgeSlot = Row->AddChildToHorizontalBox(StatusBadge))
		{
			BadgeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			BadgeSlot->SetVerticalAlignment(VAlign_Center);
			BadgeSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
		}
		if (UHorizontalBoxSlot* StatusSlot = Row->AddChildToHorizontalBox(StatusValueText))
		{
			StatusSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			StatusSlot->SetVerticalAlignment(VAlign_Center);
		}
		TeamStatusContainer->AddChild(Row);
	}
}

void UHeistHUDWidget::ResolveCrewPresentationWidgets()
{
	if (!IsValid(WidgetTree))
	{
		return;
	}
	if (!IsValid(TeamStatusContainer))
	{
		TeamStatusContainer = Cast<UPanelWidget>(GetWidgetFromName(TEXT("TeamStatusContainer")));
	}
	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!IsValid(RootCanvas))
	{
		RootCanvas = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("HUDCanvas")));
	}
	if (!IsValid(TeamStatusContainer) && IsValid(RootCanvas))
	{
		UVerticalBox* TeamList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TeamStatusContainer"));
		TeamStatusContainer = TeamList;
		if (UCanvasPanelSlot* TeamSlot = RootCanvas->AddChildToCanvas(TeamList))
		{
			TeamSlot->SetAnchors(FAnchors(1.0f, 0.0f));
			TeamSlot->SetAlignment(FVector2D(1.0f, 0.0f));
			TeamSlot->SetPosition(FVector2D(-36.0f, 110.0f));
			TeamSlot->SetSize(FVector2D(280.0f, 180.0f));
		}
	}
	if (!IsValid(StunOverlay))
	{
		StunOverlay = Cast<UBorder>(GetWidgetFromName(TEXT("StunOverlay")));
	}
	if (!IsValid(StunCountdownText))
	{
		StunCountdownText = Cast<UTextBlock>(GetWidgetFromName(TEXT("StunCountdownText")));
	}
	if (!IsValid(StunOverlay) && IsValid(RootCanvas))
	{
		StunOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StunOverlay"));
		StunOverlay->SetBrushColor(FLinearColor(0.12f, 0.04f, 0.16f, 0.24f));
		StunOverlay->SetHorizontalAlignment(HAlign_Center);
		StunOverlay->SetVerticalAlignment(VAlign_Center);
		StunOverlay->SetVisibility(ESlateVisibility::Collapsed);
		StunCountdownText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StunCountdownText"));
		StunCountdownText->SetJustification(ETextJustify::Center);
		StunCountdownText->SetFont(MakeHUDTenadaFont(26));
		StunCountdownText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		StunCountdownText->SetVisibility(ESlateVisibility::Collapsed);
		StunOverlay->SetContent(StunCountdownText);
		if (UCanvasPanelSlot* StunSlot = RootCanvas->AddChildToCanvas(StunOverlay))
		{
			StunSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			StunSlot->SetOffsets(FMargin(0.0f));
		}
	}
	if (IsValid(StunOverlay) && !IsValid(StunCountdownText))
	{
		StunOverlay->SetHorizontalAlignment(HAlign_Center);
		StunOverlay->SetVerticalAlignment(VAlign_Center);
		StunCountdownText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StunCountdownText"));
		StunCountdownText->SetJustification(ETextJustify::Center);
		StunCountdownText->SetFont(MakeHUDTenadaFont(26));
		StunCountdownText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		StunCountdownText->SetVisibility(ESlateVisibility::Collapsed);
		StunOverlay->SetContent(StunCountdownText);
	}

	if (!IsValid(ArrestOverlay))
	{
		ArrestOverlay = Cast<UBorder>(GetWidgetFromName(TEXT("ArrestOverlay")));
	}
	if (!IsValid(ArrestTitleText))
	{
		ArrestTitleText = Cast<UTextBlock>(GetWidgetFromName(TEXT("ArrestTitleText")));
	}
	if (!IsValid(ArrestInstructionText))
	{
		ArrestInstructionText = Cast<UTextBlock>(GetWidgetFromName(TEXT("ArrestInstructionText")));
	}
	if (!IsValid(ArrestOverlay) && IsValid(RootCanvas))
	{
		ArrestOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ArrestOverlay"));
		ArrestOverlay->SetBrushColor(FLinearColor(0.12f, 0.01f, 0.01f, 0.58f));
		ArrestOverlay->SetHorizontalAlignment(HAlign_Center);
		ArrestOverlay->SetVerticalAlignment(VAlign_Center);
		ArrestOverlay->SetVisibility(ESlateVisibility::Collapsed);

		UVerticalBox* ArrestContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ArrestContent"));
		ArrestTitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ArrestTitleText"));
		ArrestTitleText->SetJustification(ETextJustify::Center);
		ArrestTitleText->SetFont(MakeHUDTenadaFont(42));
		ArrestTitleText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.36f, 0.30f)));
		ArrestInstructionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ArrestInstructionText"));
		ArrestInstructionText->SetJustification(ETextJustify::Center);
		ArrestInstructionText->SetFont(MakeHUDTenadaFont(20));
		ArrestInstructionText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		ArrestContent->AddChildToVerticalBox(ArrestTitleText);
		ArrestContent->AddChildToVerticalBox(ArrestInstructionText);
		ArrestOverlay->SetContent(ArrestContent);
		if (UCanvasPanelSlot* ArrestSlot = RootCanvas->AddChildToCanvas(ArrestOverlay))
		{
			ArrestSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			ArrestSlot->SetOffsets(FMargin(0.0f));
		}
	}
}

EHeistCrewStatus UHeistHUDWidget::ResolveLocalCrewStatus() const
{
	if (!IsValid(HUDViewModel))
	{
		return EHeistCrewStatus::Active;
	}
	if (HUDViewModel->IsLocalPlayerArrested())
	{
		return EHeistCrewStatus::Arrested;
	}
	if (HUDViewModel->IsLocalPlayerEscaped())
	{
		return EHeistCrewStatus::Escaped;
	}

	const int32 LocalPlayerId = HUDViewModel->GetLocalPlayerId();
	const FHeistCrewStatusEntry* LocalEntry = HUDViewModel->GetCrewStatusEntries().FindByPredicate(
		[LocalPlayerId](const FHeistCrewStatusEntry& Entry) { return Entry.PlayerId == LocalPlayerId; });
	return LocalEntry != nullptr ? LocalEntry->Status : EHeistCrewStatus::Active;
}

UTexture2D* UHeistHUDWidget::ResolveStatusIconTexture(const EHeistCrewStatus CrewStatus) const
{
	switch (CrewStatus)
	{
	case EHeistCrewStatus::Stunned:
		return StunnedStatusIcon.Get();
	case EHeistCrewStatus::Arrested:
		return ArrestedStatusIcon.Get();
	case EHeistCrewStatus::CarryingOriginal:
		return CarryingOriginalStatusIcon.Get();
	case EHeistCrewStatus::Heavy:
		return HeavyStatusIcon.Get();
	default:
		return nullptr;
	}
}

void UHeistHUDWidget::ApplyLocalCrewStatusPresentation(const EHeistCrewStatus CrewStatus)
{
	const bool bWasInitialized = bLocalCrewStatusPresentationInitialized;
	const EHeistCrewStatus PreviousStatus = LastPresentedLocalCrewStatus;
	if (!bWasInitialized)
	{
		if (CrewStatus == EHeistCrewStatus::Arrested)
		{
			PlayArrestFeedbackAudio(ArrestedSound, ArrestedFeedbackEvent);
		}
	}
	else if (PreviousStatus != CrewStatus)
	{
		if (CrewStatus == EHeistCrewStatus::Arrested)
		{
			PlayArrestFeedbackAudio(ArrestedSound, ArrestedFeedbackEvent);
		}
		else if (PreviousStatus == EHeistCrewStatus::Arrested)
		{
			PlayArrestFeedbackAudio(RescuedSound, RescuedFeedbackEvent);
		}
	}

	bLocalCrewStatusPresentationInitialized = true;
	LastPresentedLocalCrewStatus = CrewStatus;
	const bool bStunned = CrewStatus == EHeistCrewStatus::Stunned;
	const bool bArrested = CrewStatus == EHeistCrewStatus::Arrested;
	if (IsValid(StunOverlay))
	{
		StunOverlay->SetVisibility(bStunned ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (IsValid(StunCountdownText))
	{
		StunCountdownText->SetVisibility(bStunned ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (!bStunned)
		{
			StunCountdownText->SetText(FText::GetEmpty());
		}
	}
	if (IsValid(ArrestOverlay))
	{
		ArrestOverlay->SetVisibility(bArrested ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (IsValid(ArrestTitleText))
	{
		ArrestTitleText->SetText(NSLOCTEXT("HeistHUD", "ArrestTitle", "체포됨"));
		ArrestTitleText->SetVisibility(bArrested ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (IsValid(ArrestInstructionText))
	{
		ArrestInstructionText->SetText(NSLOCTEXT("HeistHUD", "ArrestInstruction", "이동할 수 없습니다 · 팀원의 구조가 필요합니다"));
		ArrestInstructionText->SetVisibility(bArrested ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (bStunned)
	{
		LastDisplayedStunSeconds = INDEX_NONE;
		RefreshStunCountdown();
	}
	else
	{
		LastDisplayedStunSeconds = INDEX_NONE;
	}
}

void UHeistHUDWidget::RefreshStunCountdown()
{
	if (ResolveLocalCrewStatus() != EHeistCrewStatus::Stunned)
	{
		LastDisplayedStunSeconds = INDEX_NONE;
		if (IsValid(StunCountdownText))
		{
			StunCountdownText->SetText(FText::GetEmpty());
			StunCountdownText->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	const APlayerController* OwningPlayerController = GetOwningPlayer();
	const AHeistPlayerCharacter* OwningCharacter = IsValid(OwningPlayerController) ? Cast<AHeistPlayerCharacter>(OwningPlayerController->GetPawn()) : nullptr;
	const UHeistStatusComponent* StatusComponent = IsValid(OwningCharacter) ? OwningCharacter->GetStatusComponent() : nullptr;
	const FGameplayTag StunnedTag = FHeistGameplayTags::Get().Event_Player_Stunned;
	const FHeistTimedTagState* StunState = IsValid(StatusComponent)
		? StatusComponent->GetStatusTags().FindByPredicate([StunnedTag](const FHeistTimedTagState& State) { return State.StateTag == StunnedTag; })
		: nullptr;
	const UWorld* World = GetWorld();
	const AGameStateBase* WorldGameState = IsValid(World) ? World->GetGameState() : nullptr;
	const int32 RemainingSeconds = StunState != nullptr && IsValid(WorldGameState) && StunState->EndServerTime > 0.0f
		? FMath::Max(0, FMath::CeilToInt(StunState->EndServerTime - static_cast<float>(WorldGameState->GetServerWorldTimeSeconds())))
		: INDEX_NONE;
	if (RemainingSeconds == LastDisplayedStunSeconds && (!IsValid(StunCountdownText) || !StunCountdownText->GetText().IsEmpty()))
	{
		return;
	}
	LastDisplayedStunSeconds = RemainingSeconds;

	if (!IsValid(StunCountdownText))
	{
		return;
	}
	StunCountdownText->SetVisibility(ESlateVisibility::HitTestInvisible);
	StunCountdownText->SetText(RemainingSeconds == INDEX_NONE
		? NSLOCTEXT("HeistHUD", "StunTimePending", "기절  --초")
		: FText::Format(NSLOCTEXT("HeistHUD", "StunTimeFormat", "기절  {0}초"), FText::AsNumber(RemainingSeconds)));
}

void UHeistHUDWidget::SetupTutorialPresentation()
{
	AHeistPlayerController* OwningPlayerController = Cast<AHeistPlayerController>(GetOwningPlayer());
	if (TutorialPlayerController != OwningPlayerController && IsValid(TutorialPlayerController))
	{
		TutorialPlayerController->GetTutorialPresentationChangedDelegate().RemoveAll(this);
	}
	TutorialPlayerController = OwningPlayerController;
	if (IsValid(TutorialPlayerController))
	{
		TutorialPlayerController->GetTutorialPresentationChangedDelegate().RemoveAll(this);
		TutorialPlayerController->GetTutorialPresentationChangedDelegate().AddUObject(this, &UHeistHUDWidget::RefreshTutorialPresentation);
	}
	RefreshTutorialPresentation();
}

void UHeistHUDWidget::RefreshTutorialPresentation()
{
	const bool bTutorialActive = IsValid(TutorialPlayerController) && TutorialPlayerController->IsLocalTutorialActive();
	if (IsValid(TutorialCardContainer))
	{
		TutorialCardContainer->SetVisibility(bTutorialActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (IsValid(TutorialTitleText))
	{
		TutorialTitleText->SetText(bTutorialActive ? TutorialPlayerController->GetLocalTutorialTitleText() : FText::GetEmpty());
	}
	if (IsValid(TutorialBodyText))
	{
		TutorialBodyText->SetText(bTutorialActive ? TutorialPlayerController->GetLocalTutorialBodyText() : FText::GetEmpty());
	}
	if (IsValid(TutorialProgressText))
	{
		TutorialProgressText->SetText(
			bTutorialActive
				? FText::Format(NSLOCTEXT("HeistTutorial", "ProgressFormat", "단계 {0}/{1}"), FText::AsNumber(TutorialPlayerController->GetLocalTutorialStepIndex() + 1),
								FText::AsNumber(TutorialPlayerController->GetLocalTutorialStepCount()))
				: FText::GetEmpty());
	}
}

void UHeistHUDWidget::SetupPopupFeedbackPresentation()
{
	AHeistPlayerController* OwningPlayerController = Cast<AHeistPlayerController>(GetOwningPlayer());
	if (!IsValid(OwningPlayerController))
	{
		UE_LOG(LogHeistUI, Warning, TEXT("[%s] Popup feedback presentation setup rejected: Reason=MissingController"), *GetName());
		return;
	}
	if (!IsValid(PopupFeedbackLayer) && !PopupFeedbackWidgetClass)
	{
		UE_LOG(LogHeistUI, Verbose, TEXT("[%s] Popup feedback presentation disabled: Layer=None Class=None"), *GetName());
		return;
	}
	if (!IsValid(PopupFeedbackLayer) || !PopupFeedbackWidgetClass)
	{
		UE_LOG(LogHeistUI, Warning, TEXT("[%s] Popup feedback presentation setup rejected: Layer=%s Class=%s"), *GetName(), *GetNameSafe(PopupFeedbackLayer),
			   *GetNameSafe(PopupFeedbackWidgetClass.Get()));
		return;
	}

	if (!IsValid(PopupWidgetPool))
	{
		PopupWidgetPool = NewObject<UHeistPopupWidgetPool>(this);
	}
	PopupWidgetPool->SetupPool(OwningPlayerController, PopupFeedbackLayer, PopupFeedbackWidgetClass, PopupFeedbackCapacity);
}

void UHeistHUDWidget::ResolveInteractionChildWidgets()
{
	InteractionPromptWidget = ResolveInteractionChildWidget(TEXT("InteractionPromptWidget"), InteractionPromptWidget);
	ActionProgressWidget = ResolveInteractionChildWidget(TEXT("ActionProgressWidget"), ActionProgressWidget);
}

void UHeistHUDWidget::ResolveCrosshairWidgets()
{
	if (!IsValid(CrosshairContainer))
	{
		CrosshairContainer = GetWidgetFromName(TEXT("CrosshairContainer"));
	}
	if (!IsValid(CrosshairIdleIndicator))
	{
		CrosshairIdleIndicator = GetWidgetFromName(TEXT("CrosshairIdleIndicator"));
	}
	if (!IsValid(CrosshairFocusIndicator))
	{
		CrosshairFocusIndicator = GetWidgetFromName(TEXT("CrosshairFocusIndicator"));
	}

	const bool bContractValid = IsValid(CrosshairContainer) && IsValid(CrosshairIdleIndicator) && IsValid(CrosshairFocusIndicator);
	if (bContractValid)
	{
		UE_LOG(LogHeistUI, Verbose, TEXT("[%s] Crosshair widget contract: Container=%s Idle=%s Focus=%s Valid=true"), *GetName(), *GetNameSafe(CrosshairContainer),
			   *GetNameSafe(CrosshairIdleIndicator), *GetNameSafe(CrosshairFocusIndicator));
	}
	else
	{
		UE_LOG(LogHeistUI, Warning, TEXT("[%s] Crosshair widget contract: Container=%s Idle=%s Focus=%s Valid=false"), *GetName(), *GetNameSafe(CrosshairContainer),
			   *GetNameSafe(CrosshairIdleIndicator), *GetNameSafe(CrosshairFocusIndicator));
	}
}

void UHeistHUDWidget::RefreshCrosshairPresentation(AActor* TargetActor, const bool bAvailable)
{
	const bool bFocused = IsValid(TargetActor) && bAvailable;
	if (IsValid(CrosshairContainer))
	{
		CrosshairContainer->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (IsValid(CrosshairIdleIndicator))
	{
		CrosshairIdleIndicator->SetVisibility(bFocused ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (IsValid(CrosshairFocusIndicator))
	{
		CrosshairFocusIndicator->SetVisibility(bFocused ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	UE_LOG(LogHeistUI, Verbose, TEXT("[%s] Crosshair presentation refreshed: Target=%s Available=%s State=%s"), *GetName(), *GetNameSafe(TargetActor), bAvailable ? TEXT("true") : TEXT("false"),
		   bFocused ? TEXT("Focus") : TEXT("Idle"));
}

UHeistInteractionPromptWidget* UHeistHUDWidget::ResolveInteractionChildWidget(const FName WidgetName, UHeistInteractionPromptWidget* ExistingWidget) const
{
	if (IsValid(ExistingWidget))
	{
		return ExistingWidget;
	}

	UWidget* FoundWidget = GetWidgetFromName(WidgetName);
	if (!IsValid(FoundWidget))
	{
		UE_LOG(LogHeistUI, Warning, TEXT("[%s] HUD child widget missing: Name=%s"), *GetName(), *WidgetName.ToString());
		return nullptr;
	}

	UHeistInteractionPromptWidget* ResolvedWidget = Cast<UHeistInteractionPromptWidget>(FoundWidget);
	if (!IsValid(ResolvedWidget))
	{
		UE_LOG(LogHeistUI, Warning, TEXT("[%s] HUD child widget type mismatch: Name=%s Found=%s FoundClass=%s Expected=HeistInteractionPromptWidget"), *GetName(), *WidgetName.ToString(),
			   *GetNameSafe(FoundWidget), *FoundWidget->GetClass()->GetName());
		return nullptr;
	}

	UE_LOG(LogHeistUI, Verbose, TEXT("[%s] HUD child widget resolved by name: Name=%s Widget=%s Class=%s"), *GetName(), *WidgetName.ToString(), *GetNameSafe(ResolvedWidget),
		   *ResolvedWidget->GetClass()->GetName());
	return ResolvedWidget;
}

void UHeistHUDWidget::RefreshToolPresentation()
{
	if (!IsValid(ToolText))
	{
		return;
	}

	const FHeistQuickSlotPresentation* CoinPresentation = nullptr;
	if (IsValid(QuickSlotViewModel))
	{
		CoinPresentation =
			QuickSlotViewModel->GetQuickSlotPresentations().FindByPredicate([](const FHeistQuickSlotPresentation& Presentation) { return Presentation.SlotType == EHeistQuickSlotType::Coin; });
	}

	if (CoinPresentation == nullptr)
	{
		ToolText->SetText(NSLOCTEXT("HeistHUD", "ToolUnavailable", "동전  --"));
	}
	else if (!CoinPresentation->bAssigned)
	{
		ToolText->SetText(FText::Format(NSLOCTEXT("HeistHUD", "ToolEmptyFormat", "동전  0  [{0}]"), CoinPresentation->KeyLabel));
	}
	else
	{
		ToolText->SetText(FText::Format(NSLOCTEXT("HeistHUD", "CoinToolFormat", "동전  x{1}  [{0}]"), CoinPresentation->KeyLabel,
									   FText::AsNumber(CoinPresentation->Quantity)));
	}

	ToolText->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UHeistHUDWidget::RefreshHUDPresentation()
{
	if (GetVisibility() == ESlateVisibility::Collapsed || GetVisibility() == ESlateVisibility::Hidden)
	{
		ResetHiddenPresentationState();
		return;
	}

	if (!IsValid(HUDViewModel))
	{
		return;
	}

	const int32 LocalLootScore = HUDViewModel->GetLocalLootScore();
	const float LocalLootWeight = HUDViewModel->GetLocalLootWeight();
	const int32 ConnectedPlayerCount = HUDViewModel->GetConnectedPlayerCount();
	const bool bLocalPlayerEscaped = HUDViewModel->IsLocalPlayerEscaped();
	const bool bLocalPlayerArrested = HUDViewModel->IsLocalPlayerArrested();
	const bool bEscapePhaseOpen = HUDViewModel->IsEscapePhaseOpen();
	const bool bEscapeCastActive = HUDViewModel->IsEscapeCastActive();
	const float EscapeCastEndServerTime = HUDViewModel->GetEscapeCastEndServerTime();
	const bool bObservationCastActive = HUDViewModel->IsObservationCastActive();

	if (IsValid(WeightText))
	{
		FNumberFormattingOptions WeightFormatting;
		WeightFormatting.MinimumFractionalDigits = 1;
		WeightFormatting.MaximumFractionalDigits = 1;
		WeightText->SetText(FText::Format(NSLOCTEXT("HeistHUD", "WeightFormat", "무게  {0}"), FText::AsNumber(LocalLootWeight, &WeightFormatting)));
	}

	if (IsValid(ActionText))
	{
		const FText ActionLabel =
			bObservationCastActive ? NSLOCTEXT("HeistHUD", "ObservationCastAction", "관찰 중")
								   : (bEscapeCastActive ? NSLOCTEXT("HeistHUD", "EscapeCastAction", "탈출 중") : NSLOCTEXT("HeistHUD", "ReadyAction", "준비"));
		ActionText->SetText(ActionLabel);
	}

	if (IsValid(ObjectiveText))
	{
		ObjectiveText->SetText(HUDViewModel->GetObjectiveStateText());
		ObjectiveText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	if (IsValid(StatusText))
	{
		const FText StatusLabel = bLocalPlayerEscaped
			? NSLOCTEXT("HeistHUD", "EscapedStatus", "탈출")
			: (bLocalPlayerArrested ? NSLOCTEXT("HeistHUD", "ArrestedStatus", "체포") : NSLOCTEXT("HeistHUD", "NormalStatus", "박물관 내부"));
		StatusText->SetText(StatusLabel);
	}

	if (IsValid(AlertText))
	{
		AlertText->SetText(HUDViewModel->GetAlertBannerText());
		AlertText->SetColorAndOpacity(HUDViewModel->GetAlertColor());
		AlertText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	RefreshAlertPresentation();
	RefreshCrewStatusPresentation();
	BP_RefreshHUDPresentation(LocalLootScore, LocalLootWeight, ConnectedPlayerCount, bLocalPlayerEscaped, bEscapePhaseOpen, bEscapeCastActive, EscapeCastEndServerTime);
	RefreshToolPresentation();
}

void UHeistHUDWidget::RefreshAlertPresentation()
{
	if (!IsValid(HUDViewModel))
	{
		return;
	}

	ApplyAlertAudioLayers();
	LastDisplayedLockdownSeconds = INDEX_NONE;
	RefreshLockdownCountdown();
}

void UHeistHUDWidget::RefreshLockdownCountdown()
{
	const bool bCountdownVisible = IsValid(HUDViewModel) && HUDViewModel->IsLockdownCountdownVisible();
	if (IsValid(LockdownCountdownText))
	{
		LockdownCountdownText->SetVisibility(bCountdownVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		LockdownCountdownText->SetColorAndOpacity(IsValid(HUDViewModel) ? HUDViewModel->GetAlertColor() : FLinearColor::White);
	}
	if (!bCountdownVisible)
	{
		LastDisplayedLockdownSeconds = INDEX_NONE;
		return;
	}

	const UWorld* World = GetWorld();
	const AGameStateBase* WorldGameState = IsValid(World) ? World->GetGameState() : nullptr;
	const float CountdownEndServerTime = HUDViewModel->GetLockdownCountdownEndServerTime();
	const int32 RemainingSeconds = IsValid(WorldGameState) && CountdownEndServerTime > 0.0f
										   ? FMath::Max(0, FMath::CeilToInt(CountdownEndServerTime - static_cast<float>(WorldGameState->GetServerWorldTimeSeconds())))
										   : INDEX_NONE;
	if (RemainingSeconds == LastDisplayedLockdownSeconds)
	{
		return;
	}
	LastDisplayedLockdownSeconds = RemainingSeconds;

	if (!IsValid(LockdownCountdownText))
	{
		return;
	}
	if (RemainingSeconds == INDEX_NONE)
	{
		LockdownCountdownText->SetText(NSLOCTEXT("HeistHUD", "LockdownTimePending", "봉쇄까지 --:--  —  탈출 경로가 제한됩니다"));
		return;
	}

	const FText TimeText = FText::FromString(FString::Printf(TEXT("%02d:%02d"), RemainingSeconds / 60, RemainingSeconds % 60));
	LockdownCountdownText->SetText(FText::Format(NSLOCTEXT("HeistHUD", "LockdownTimeFormat", "봉쇄까지 {0}  —  탈출 경로가 제한됩니다"), TimeText));
}

void UHeistHUDWidget::ApplyAlertAudioLayers()
{
	if (!IsValid(HUDViewModel))
	{
		return;
	}

	const EHeistAlertLevel NewAlertLevel = HUDViewModel->GetAlertLevel();
	if (bAlertAudioInitialized && LastAppliedAudioAlertLevel == NewAlertLevel)
	{
		return;
	}

	const bool bPlaySuspense = HUDViewModel->IsSuspenseMusicActive();
	const bool bPlayAlarm = HUDViewModel->IsAlarmMusicActive();
	if (bPlaySuspense && IsValid(SuspenseMusic) && !IsValid(SuspenseMusicComponent))
	{
		SuspenseMusicComponent = UGameplayStatics::SpawnSound2D(this, SuspenseMusic, 0.0f, 1.0f, 0.0f, nullptr, false, false);
	}
	if (bPlayAlarm && IsValid(AlarmMusic) && !IsValid(AlarmMusicComponent))
	{
		AlarmMusicComponent = UGameplayStatics::SpawnSound2D(this, AlarmMusic, 0.0f, 1.0f, 0.0f, nullptr, false, false);
	}

	if (IsValid(SuspenseMusicComponent))
	{
		if (bPlaySuspense)
		{
			SuspenseMusicComponent->FadeIn(AlertMusicFadeSeconds, SuspenseMusicVolume);
		}
		else if (SuspenseMusicComponent->IsPlaying())
		{
			SuspenseMusicComponent->FadeOut(AlertMusicFadeSeconds, 0.0f);
		}
	}
	if (IsValid(AlarmMusicComponent))
	{
		if (bPlayAlarm)
		{
			AlarmMusicComponent->FadeIn(AlertMusicFadeSeconds, AlarmMusicVolume);
		}
		else if (AlarmMusicComponent->IsPlaying())
		{
			AlarmMusicComponent->FadeOut(AlertMusicFadeSeconds, 0.0f);
		}
	}

	LastAppliedAudioAlertLevel = NewAlertLevel;
	bAlertAudioInitialized = true;
	UE_LOG(LogHeistUI, Log, TEXT("[%s] Alert audio layers applied: Level=%s Suspense=%s Alarm=%s SuspenseAsset=%s AlarmAsset=%s"), *GetName(), *UEnum::GetValueAsString(NewAlertLevel),
		   bPlaySuspense ? TEXT("active") : TEXT("inactive"), bPlayAlarm ? TEXT("active") : TEXT("inactive"), *GetNameSafe(SuspenseMusic.Get()), *GetNameSafe(AlarmMusic.Get()));
}

void UHeistHUDWidget::StopAlertAudioLayers()
{
	if (IsValid(SuspenseMusicComponent))
	{
		SuspenseMusicComponent->Stop();
		SuspenseMusicComponent->DestroyComponent();
		SuspenseMusicComponent = nullptr;
	}
	if (IsValid(AlarmMusicComponent))
	{
		AlarmMusicComponent->Stop();
		AlarmMusicComponent->DestroyComponent();
		AlarmMusicComponent = nullptr;
	}
	bAlertAudioInitialized = false;
}

void UHeistHUDWidget::PlayArrestFeedbackAudio(USoundBase* Sound, const FName FeedbackEvent)
{
	StopArrestFeedbackAudio();
	if (!IsValid(Sound))
	{
		UE_LOG(LogHeistUI, Warning, TEXT("[%s] Arrest feedback audio skipped: Event=%s Reason=MissingAsset"), *GetName(), *FeedbackEvent.ToString());
		return;
	}

	ArrestFeedbackAudioComponent = UGameplayStatics::SpawnSound2D(this, Sound, 1.0f, 1.0f, 0.0f, nullptr, false, false);
	if (!IsValid(ArrestFeedbackAudioComponent))
	{
		UE_LOG(LogHeistUI, Warning, TEXT("[%s] Arrest feedback audio skipped: Event=%s Reason=SpawnFailed Asset=%s"), *GetName(), *FeedbackEvent.ToString(), *GetNameSafe(Sound));
		return;
	}

	LastArrestFeedbackEvent = FeedbackEvent;
	if (FeedbackEvent == ArrestedFeedbackEvent)
	{
		++ArrestAudioPlayCount;
	}
	else if (FeedbackEvent == RescuedFeedbackEvent)
	{
		++RescueAudioPlayCount;
	}
	UE_LOG(LogHeistUI, Log, TEXT("[%s] Arrest feedback audio played: Event=%s Asset=%s ArrestCount=%d RescueCount=%d"), *GetName(), *FeedbackEvent.ToString(), *GetNameSafe(Sound),
		ArrestAudioPlayCount, RescueAudioPlayCount);
}

void UHeistHUDWidget::StopArrestFeedbackAudio()
{
	if (IsValid(ArrestFeedbackAudioComponent))
	{
		ArrestFeedbackAudioComponent->Stop();
		ArrestFeedbackAudioComponent->DestroyComponent();
	}
	ArrestFeedbackAudioComponent = nullptr;
}

#pragma endregion

#pragma region Debug

void UHeistHUDWidget::ResetHiddenPresentationState()
{
	StopAlertAudioLayers();
	StopArrestFeedbackAudio();
	LastAppliedAudioAlertLevel = EHeistAlertLevel::Quiet;
	LastPresentedLocalCrewStatus = EHeistCrewStatus::Active;
	bLocalCrewStatusPresentationInitialized = false;
	LastDisplayedLockdownSeconds = INDEX_NONE;
	LastDisplayedStunSeconds = INDEX_NONE;
	LastArrestFeedbackEvent = NAME_None;
	ArrestAudioPlayCount = 0;
	RescueAudioPlayCount = 0;

	if (IsValid(AlertText))
	{
		AlertText->SetText(FText::GetEmpty());
		AlertText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (IsValid(LockdownCountdownText))
	{
		LockdownCountdownText->SetText(FText::GetEmpty());
		LockdownCountdownText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (IsValid(TeamStatusContainer))
	{
		TeamStatusContainer->ClearChildren();
	}
	if (IsValid(StunOverlay))
	{
		StunOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (IsValid(StunCountdownText))
	{
		StunCountdownText->SetText(FText::GetEmpty());
		StunCountdownText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (IsValid(ArrestOverlay))
	{
		ArrestOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (IsValid(ArrestTitleText))
	{
		ArrestTitleText->SetText(FText::GetEmpty());
		ArrestTitleText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (IsValid(ArrestInstructionText))
	{
		ArrestInstructionText->SetText(FText::GetEmpty());
		ArrestInstructionText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool UHeistHUDWidget::IsHiddenPresentationStateReset() const
{
	const bool bAlertAudioStopped = !bAlertAudioInitialized && !IsValid(SuspenseMusicComponent) && !IsValid(AlarmMusicComponent);
	const bool bArrestAudioStopped = !IsValid(ArrestFeedbackAudioComponent);
	const bool bAlertTextReset = !IsValid(AlertText) || (AlertText->GetVisibility() == ESlateVisibility::Collapsed && AlertText->GetText().IsEmpty());
	const bool bCountdownReset = LastDisplayedLockdownSeconds == INDEX_NONE &&
		(!IsValid(LockdownCountdownText) || (LockdownCountdownText->GetVisibility() == ESlateVisibility::Collapsed && LockdownCountdownText->GetText().IsEmpty()));
	const bool bTeamRowsCleared = !IsValid(TeamStatusContainer) || TeamStatusContainer->GetChildrenCount() == 0;
	const bool bStunReset = LastDisplayedStunSeconds == INDEX_NONE && (!IsValid(StunOverlay) || StunOverlay->GetVisibility() == ESlateVisibility::Collapsed) &&
		(!IsValid(StunCountdownText) || (StunCountdownText->GetVisibility() == ESlateVisibility::Collapsed && StunCountdownText->GetText().IsEmpty()));
	const bool bArrestReset = (!IsValid(ArrestOverlay) || ArrestOverlay->GetVisibility() == ESlateVisibility::Collapsed) &&
		(!IsValid(ArrestTitleText) || (ArrestTitleText->GetVisibility() == ESlateVisibility::Collapsed && ArrestTitleText->GetText().IsEmpty())) &&
		(!IsValid(ArrestInstructionText) || (ArrestInstructionText->GetVisibility() == ESlateVisibility::Collapsed && ArrestInstructionText->GetText().IsEmpty()));
	const bool bTransitionStateReset = !bLocalCrewStatusPresentationInitialized && LastPresentedLocalCrewStatus == EHeistCrewStatus::Active && LastArrestFeedbackEvent.IsNone() &&
		ArrestAudioPlayCount == 0 && RescueAudioPlayCount == 0;
	return bAlertAudioStopped && bArrestAudioStopped && bAlertTextReset && bCountdownReset && bTeamRowsCleared && bStunReset && bArrestReset && bTransitionStateReset;
}

void UHeistHUDWidget::DebugDumpFirstPersonHUDState() const
{
	const bool bCrosshairReady = IsValid(CrosshairContainer) && IsValid(CrosshairIdleIndicator) && IsValid(CrosshairFocusIndicator);
	const bool bCenterPromptReady = IsValid(InteractionPromptWidget) && IsValid(ActionProgressWidget);
	const bool bToolReady = IsValid(ToolText);
	const bool bStatusReady = IsValid(StatusText) && IsValid(WeightText);
	const bool bObjectiveReady = IsValid(ObjectiveText) && IsValid(HUDViewModel) && !HUDViewModel->GetObjectiveArtifactId().IsNone() && !HUDViewModel->GetObjectiveCaseId().IsNone() &&
								 HUDViewModel->GetObjectiveState() != EHeistObjectiveState::Inactive && !ObjectiveText->GetText().IsEmpty();
	const bool bContractPass = bCrosshairReady && bCenterPromptReady && bToolReady && bStatusReady && bObjectiveReady;

	const FString ContractMessage =
		FString::Printf(TEXT("[%s] First-person HUD contract: Crosshair=%s CenterPrompt=%s Tool=%s Status=%s Objective=%s Result=%s"), *GetName(),
						bCrosshairReady ? TEXT("true") : TEXT("false"), bCenterPromptReady ? TEXT("true") : TEXT("false"), bToolReady ? TEXT("true") : TEXT("false"),
						bStatusReady ? TEXT("true") : TEXT("false"), bObjectiveReady ? TEXT("true") : TEXT("false"), bContractPass ? TEXT("PASS") : TEXT("FAIL"));
	if (bContractPass)
	{
		UE_LOG(LogHeistUI, Log, TEXT("%s"), *ContractMessage);
	}
	else
	{
		UE_LOG(LogHeistUI, Error, TEXT("%s"), *ContractMessage);
	}

	UE_LOG(
		LogHeistUI, Log,
		TEXT("[%s] Local crew presentation: Initialized=%s Last=%s Resolved=%s StunOverlay=%s/%d StunCountdown=%s/%d/'%s' ArrestOverlay=%s/%d ArrestTitle=%s/%d/'%s' ArrestInstruction=%s/%d/'%s' Result=%s"),
		*GetName(), bLocalCrewStatusPresentationInitialized ? TEXT("true") : TEXT("false"), *UEnum::GetValueAsString(LastPresentedLocalCrewStatus),
		*UEnum::GetValueAsString(ResolveLocalCrewStatus()), IsValid(StunOverlay) ? TEXT("valid") : TEXT("missing"),
		IsValid(StunOverlay) ? static_cast<int32>(StunOverlay->GetVisibility()) : INDEX_NONE, IsValid(StunCountdownText) ? TEXT("valid") : TEXT("missing"),
		IsValid(StunCountdownText) ? static_cast<int32>(StunCountdownText->GetVisibility()) : INDEX_NONE,
		IsValid(StunCountdownText) ? *StunCountdownText->GetText().ToString() : TEXT(""), IsValid(ArrestOverlay) ? TEXT("valid") : TEXT("missing"),
		IsValid(ArrestOverlay) ? static_cast<int32>(ArrestOverlay->GetVisibility()) : INDEX_NONE, IsValid(ArrestTitleText) ? TEXT("valid") : TEXT("missing"),
		IsValid(ArrestTitleText) ? static_cast<int32>(ArrestTitleText->GetVisibility()) : INDEX_NONE,
		IsValid(ArrestTitleText) ? *ArrestTitleText->GetText().ToString() : TEXT(""), IsValid(ArrestInstructionText) ? TEXT("valid") : TEXT("missing"),
		IsValid(ArrestInstructionText) ? static_cast<int32>(ArrestInstructionText->GetVisibility()) : INDEX_NONE,
		IsValid(ArrestInstructionText) ? *ArrestInstructionText->GetText().ToString() : TEXT(""),
		IsLocalCrewStatusPresentationContractSatisfied() ? TEXT("PASS") : TEXT("FAIL"));

	UE_LOG(LogHeistUI, Log, TEXT("[%s] First-person HUD state: ToolText='%s' StatusText='%s' WeightText='%s'"), *GetName(), IsValid(ToolText) ? *ToolText->GetText().ToString() : TEXT("None"),
		   IsValid(StatusText) ? *StatusText->GetText().ToString() : TEXT("None"), IsValid(WeightText) ? *WeightText->GetText().ToString() : TEXT("None"));

	if (IsValid(HUDViewModel))
	{
		UE_LOG(LogHeistUI, Log,
			TEXT(
				"[%s] Observation presentation: Active=%s ReferenceVisible=%s ReferenceArtifact=%s EndServerTime=%.2f ObjectiveArtifact=%s ObjectiveCase=%s ObjectiveState=%d ObjectiveText='%s' OwnerOnly=true ObjectiveWidget=%s"),
			*GetName(), HUDViewModel->IsObservationCastActive() ? TEXT("true") : TEXT("false"), HUDViewModel->IsObservationReferenceVisible() ? TEXT("true") : TEXT("false"),
			*HUDViewModel->GetObservationReferenceArtifactId().ToString(), HUDViewModel->GetObservationCastEndServerTime(), *HUDViewModel->GetObjectiveArtifactId().ToString(),
			*HUDViewModel->GetObjectiveCaseId().ToString(), static_cast<int32>(HUDViewModel->GetObjectiveState()), *HUDViewModel->GetObjectiveStateText().ToString(),
			IsValid(ObjectiveText) ? TEXT("true") : TEXT("false"));
	}
}

void UHeistHUDWidget::DebugDumpFeedbackState() const
{
	if (IsValid(PopupWidgetPool))
	{
		PopupWidgetPool->DebugDumpState();
	}
	else
	{
		UE_LOG(LogHeistUI, Warning, TEXT("[%s] Popup feedback pool dump failed: Reason=MissingPool"), *GetName());
	}
}

bool UHeistHUDWidget::IsTutorialPresentationContractSatisfied() const
{
	const bool bWidgetsReady = IsValid(TutorialCardContainer) && IsValid(TutorialTitleText) && IsValid(TutorialBodyText) && IsValid(TutorialProgressText);
	if (!bWidgetsReady || !IsValid(TutorialPlayerController))
	{
		return false;
	}

	const bool bTutorialActive = TutorialPlayerController->IsLocalTutorialActive();
	const bool bCardVisible = TutorialCardContainer->GetVisibility() != ESlateVisibility::Collapsed && TutorialCardContainer->GetVisibility() != ESlateVisibility::Hidden;
	const bool bVisibilityMatches = bTutorialActive == bCardVisible;
	const bool bInputTransparent = TutorialCardContainer->GetVisibility() != ESlateVisibility::Visible;
	const bool bCopyReady = !bTutorialActive || (!TutorialTitleText->GetText().IsEmpty() && !TutorialBodyText->GetText().IsEmpty() && !TutorialProgressText->GetText().IsEmpty());
	return bVisibilityMatches && bInputTransparent && bCopyReady;
}

void UHeistHUDWidget::DebugDumpTutorialPresentationState() const
{
	const bool bPassed = IsTutorialPresentationContractSatisfied();
	const FString Message = FString::Printf(
		TEXT("[%s] Tutorial presentation: Active=%s Completed=%s Step=%s StepIndex=%d StepCount=%d CardWidget=%s CardVisible=%s TitleWidget=%s BodyWidget=%s ProgressWidget=%s "
			 "Title='%s' Body='%s' Progress='%s' InputTransparent=%s Result=%s"),
		*GetName(), IsValid(TutorialPlayerController) && TutorialPlayerController->IsLocalTutorialActive() ? TEXT("true") : TEXT("false"),
		IsValid(TutorialPlayerController) && TutorialPlayerController->HasCompletedLocalTutorial() ? TEXT("true") : TEXT("false"),
		IsValid(TutorialPlayerController) ? *TutorialPlayerController->GetLocalTutorialStepId().ToString() : TEXT("None"),
		IsValid(TutorialPlayerController) ? TutorialPlayerController->GetLocalTutorialStepIndex() : INDEX_NONE,
		IsValid(TutorialPlayerController) ? TutorialPlayerController->GetLocalTutorialStepCount() : 0, IsValid(TutorialCardContainer) ? TEXT("true") : TEXT("false"),
		IsValid(TutorialCardContainer) && TutorialCardContainer->GetVisibility() != ESlateVisibility::Collapsed && TutorialCardContainer->GetVisibility() != ESlateVisibility::Hidden
			? TEXT("true")
			: TEXT("false"),
		IsValid(TutorialTitleText) ? TEXT("true") : TEXT("false"), IsValid(TutorialBodyText) ? TEXT("true") : TEXT("false"),
		IsValid(TutorialProgressText) ? TEXT("true") : TEXT("false"), IsValid(TutorialTitleText) ? *TutorialTitleText->GetText().ToString() : TEXT("None"),
		IsValid(TutorialBodyText) ? *TutorialBodyText->GetText().ToString() : TEXT("None"), IsValid(TutorialProgressText) ? *TutorialProgressText->GetText().ToString() : TEXT("None"),
		IsValid(TutorialCardContainer) && TutorialCardContainer->GetVisibility() != ESlateVisibility::Visible ? TEXT("true") : TEXT("false"), bPassed ? TEXT("PASS") : TEXT("FAIL"));
	if (bPassed)
	{
		UE_LOG(LogHeistUI, Log, TEXT("%s"), *Message);
	}
	else
	{
		UE_LOG(LogHeistUI, Error, TEXT("%s"), *Message);
	}
}

bool UHeistHUDWidget::IsAlertPresentationContractSatisfied() const
{
	if (!IsValid(HUDViewModel) || !IsValid(AlertText) || !IsValid(LockdownCountdownText) || !IsValid(SuspenseMusic) || !IsValid(AlarmMusic))
	{
		return false;
	}

	const bool bCountdownVisibilityMatches =
		HUDViewModel->IsLockdownCountdownVisible() == (LockdownCountdownText->GetVisibility() != ESlateVisibility::Collapsed && LockdownCountdownText->GetVisibility() != ESlateVisibility::Hidden);
	const bool bAudioModeMatches = HUDViewModel->IsSuspenseMusicActive() != HUDViewModel->IsAlarmMusicActive() ||
								   (!HUDViewModel->IsSuspenseMusicActive() && !HUDViewModel->IsAlarmMusicActive());
	const bool bSuspensePlaybackMatches =
		!HUDViewModel->IsSuspenseMusicActive() || (IsValid(SuspenseMusicComponent) && SuspenseMusicComponent->IsPlaying());
	const bool bAlarmPlaybackMatches = !HUDViewModel->IsAlarmMusicActive() || (IsValid(AlarmMusicComponent) && AlarmMusicComponent->IsPlaying());
	const bool bCountdownTextValid = !HUDViewModel->IsLockdownCountdownVisible() || !LockdownCountdownText->GetText().IsEmpty();
	const FString SecurityLevelFraction = FString::Printf(TEXT("%d/4"), HUDViewModel->GetSecurityLevel());
	const bool bSecurityLevelMatches = AlertText->GetText().ToString().Contains(SecurityLevelFraction);
	return bSecurityLevelMatches && bCountdownVisibilityMatches && bCountdownTextValid && bAudioModeMatches && bSuspensePlaybackMatches && bAlarmPlaybackMatches && bAlertAudioInitialized &&
		   LastAppliedAudioAlertLevel == HUDViewModel->GetAlertLevel();
}

bool UHeistHUDWidget::AreAlertAudioAssetsAssignedForDebug() const
{
	return IsValid(SuspenseMusic) && IsValid(AlarmMusic);
}

bool UHeistHUDWidget::AreAlertAudioAssetsLoopingForDebug() const
{
	const USoundWave* SuspenseWave = Cast<USoundWave>(SuspenseMusic);
	const USoundWave* AlarmWave = Cast<USoundWave>(AlarmMusic);
	return IsValid(SuspenseWave) && SuspenseWave->bLooping && IsValid(AlarmWave) && AlarmWave->bLooping;
}

bool UHeistHUDWidget::IsSuspenseMusicPlayingForDebug() const
{
	return IsValid(SuspenseMusicComponent) && SuspenseMusicComponent->IsPlaying();
}

bool UHeistHUDWidget::IsAlarmMusicPlayingForDebug() const
{
	return IsValid(AlarmMusicComponent) && AlarmMusicComponent->IsPlaying();
}

bool UHeistHUDWidget::IsLocalCrewStatusPresentationContractSatisfied() const
{
	if (!IsValid(HUDViewModel) || !IsValid(StunOverlay) || !IsValid(StunCountdownText) || !IsValid(ArrestOverlay) || !IsValid(ArrestTitleText) || !IsValid(ArrestInstructionText))
	{
		return false;
	}

	const EHeistCrewStatus CrewStatus = ResolveLocalCrewStatus();
	const bool bStunned = CrewStatus == EHeistCrewStatus::Stunned;
	const bool bArrested = CrewStatus == EHeistCrewStatus::Arrested;
	const bool bStunOverlayVisible = StunOverlay->GetVisibility() != ESlateVisibility::Collapsed && StunOverlay->GetVisibility() != ESlateVisibility::Hidden;
	const bool bStunCountdownVisible = StunCountdownText->GetVisibility() != ESlateVisibility::Collapsed && StunCountdownText->GetVisibility() != ESlateVisibility::Hidden;
	const bool bArrestOverlayVisible = ArrestOverlay->GetVisibility() != ESlateVisibility::Collapsed && ArrestOverlay->GetVisibility() != ESlateVisibility::Hidden;
	const bool bStunCopyReady = !bStunned || !StunCountdownText->GetText().IsEmpty();
	const bool bArrestCopyReady = !bArrested || (!ArrestTitleText->GetText().IsEmpty() && !ArrestInstructionText->GetText().IsEmpty());
	return bLocalCrewStatusPresentationInitialized && LastPresentedLocalCrewStatus == CrewStatus && bStunned == bStunOverlayVisible && bStunned == bStunCountdownVisible &&
		bArrested == bArrestOverlayVisible && bStunCopyReady && bArrestCopyReady;
}

bool UHeistHUDWidget::AreCrewStatusIconTexturesAssignedForDebug() const
{
	return IsValid(StunnedStatusIcon) && IsValid(ArrestedStatusIcon) && IsValid(CarryingOriginalStatusIcon) && IsValid(HeavyStatusIcon);
}

bool UHeistHUDWidget::AreArrestAudioAssetsAssignedForDebug() const
{
	return IsValid(ArrestedSound) && IsValid(RescuedSound);
}

bool UHeistHUDWidget::IsArrestFeedbackAudioActiveForDebug() const
{
	return IsValid(ArrestFeedbackAudioComponent) && ArrestFeedbackAudioComponent->IsPlaying();
}

void UHeistHUDWidget::DebugDumpAlertPresentationState() const
{
	const bool bPassed = IsAlertPresentationContractSatisfied();
	const FString Message = FString::Printf(
		TEXT("[%s] Alert HUD presentation: Level=%s SecurityLevel=%d/4 Banner='%s' BannerWidget=%s CountdownVisible=%s CountdownText='%s' SuspenseRequested=%s SuspensePlaying=%s AlarmRequested=%s "
			 "AlarmPlaying=%s SuspenseAsset=%s AlarmAsset=%s AudioApplied=%s Result=%s"),
		*GetName(), IsValid(HUDViewModel) ? *UEnum::GetValueAsString(HUDViewModel->GetAlertLevel()) : TEXT("None"),
		IsValid(HUDViewModel) ? HUDViewModel->GetSecurityLevel() : INDEX_NONE, IsValid(AlertText) ? *AlertText->GetText().ToString() : TEXT("None"), IsValid(AlertText) ? TEXT("true") : TEXT("false"),
		IsValid(LockdownCountdownText) && LockdownCountdownText->GetVisibility() != ESlateVisibility::Collapsed && LockdownCountdownText->GetVisibility() != ESlateVisibility::Hidden
			? TEXT("true")
			: TEXT("false"),
		IsValid(LockdownCountdownText) ? *LockdownCountdownText->GetText().ToString() : TEXT("None"),
		IsValid(HUDViewModel) && HUDViewModel->IsSuspenseMusicActive() ? TEXT("true") : TEXT("false"),
		IsValid(SuspenseMusicComponent) && SuspenseMusicComponent->IsPlaying() ? TEXT("true") : TEXT("false"),
		IsValid(HUDViewModel) && HUDViewModel->IsAlarmMusicActive() ? TEXT("true") : TEXT("false"),
		IsValid(AlarmMusicComponent) && AlarmMusicComponent->IsPlaying() ? TEXT("true") : TEXT("false"), *GetNameSafe(SuspenseMusic.Get()), *GetNameSafe(AlarmMusic.Get()),
		bAlertAudioInitialized ? TEXT("true") : TEXT("false"), bPassed ? TEXT("PASS") : TEXT("FAIL"));
	if (bPassed)
	{
		UE_LOG(LogHeistUI, Log, TEXT("%s"), *Message);
	}
	else
	{
		UE_LOG(LogHeistUI, Error, TEXT("%s"), *Message);
	}
}

#pragma endregion
