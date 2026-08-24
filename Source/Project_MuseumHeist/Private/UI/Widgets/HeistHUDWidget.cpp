#include "UI/Widgets/HeistHUDWidget.h"

#include "Character/Components/HeistInteractionComponent.h"
#include "Character/Components/HeistNoiseEmitterComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Components/AudioComponent.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
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
#include "UI/Widgets/HeistQuickSlotWidget.h"
#include "UI/HUD/Widgets/HeistTeamCardWidget.h"
#include "Blueprint/WidgetTree.h"

namespace
{
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
	RefreshMissionPresentation();
	RefreshTransientEvent();
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

	AHeistPlayerController* OwningHeistController = Cast<AHeistPlayerController>(GetOwningPlayer());
	const TArray<UHeistTeamCardWidget*> TeamCards = {TeamCard1.Get(), TeamCard2.Get(), TeamCard3.Get(), TeamCard4.Get()};
	for (int32 SlotIndex = 0; SlotIndex < TeamCards.Num(); ++SlotIndex)
	{
		UHeistTeamCardWidget* TeamCard = TeamCards[SlotIndex];
		if (!IsValid(TeamCard))
		{
			continue;
		}
		const int32 PlayerSlot = SlotIndex + 1;
		const FHeistCrewStatusEntry* Entry = HUDViewModel->GetCrewStatusEntries().FindByPredicate(
			[PlayerSlot](const FHeistCrewStatusEntry& Candidate) { return Candidate.PlayerId == PlayerSlot; });
		if (Entry != nullptr)
		{
			TeamCard->ApplyCrewData(*Entry, OwningHeistController);
		}
		else
		{
			TeamCard->ApplyEmptySlot(PlayerSlot);
		}
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
	if (!IsValid(TeamCard1))
	{
		TeamCard1 = Cast<UHeistTeamCardWidget>(GetWidgetFromName(TEXT("TeamCard1")));
	}
	if (!IsValid(TeamCard2))
	{
		TeamCard2 = Cast<UHeistTeamCardWidget>(GetWidgetFromName(TEXT("TeamCard2")));
	}
	if (!IsValid(TeamCard3))
	{
		TeamCard3 = Cast<UHeistTeamCardWidget>(GetWidgetFromName(TEXT("TeamCard3")));
	}
	if (!IsValid(TeamCard4))
	{
		TeamCard4 = Cast<UHeistTeamCardWidget>(GetWidgetFromName(TEXT("TeamCard4")));
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
			ShowTransientEvent(NSLOCTEXT("HeistHUD", "EventArrested", "경비에게 체포되었습니다"));
		}
		else if (CrewStatus == EHeistCrewStatus::Stunned)
		{
			ShowTransientEvent(NSLOCTEXT("HeistHUD", "EventStunned", "기절 상태입니다"));
		}
	}
	else if (PreviousStatus != CrewStatus)
	{
		if (CrewStatus == EHeistCrewStatus::Arrested)
		{
			PlayArrestFeedbackAudio(ArrestedSound, ArrestedFeedbackEvent);
			ShowTransientEvent(NSLOCTEXT("HeistHUD", "EventArrested", "경비에게 체포되었습니다"));
		}
		else if (CrewStatus == EHeistCrewStatus::Stunned)
		{
			ShowTransientEvent(NSLOCTEXT("HeistHUD", "EventStunned", "기절 상태입니다"));
		}
		else if (PreviousStatus == EHeistCrewStatus::Arrested)
		{
			PlayArrestFeedbackAudio(RescuedSound, RescuedFeedbackEvent);
			ShowTransientEvent(NSLOCTEXT("HeistHUD", "EventRescued", "팀원이 구조했습니다"));
		}
		else if (CrewStatus == EHeistCrewStatus::Escaped)
		{
			ShowTransientEvent(NSLOCTEXT("HeistHUD", "EventEscaped", "탈출 지점에 도착했습니다"));
		}
	}

	bLocalCrewStatusPresentationInitialized = true;
	LastPresentedLocalCrewStatus = CrewStatus;
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
	if (IsValid(ToolText))
	{
		ToolText->SetVisibility(ESlateVisibility::Collapsed);
	}
	RefreshHUDQuickSlots();
}

void UHeistHUDWidget::RefreshMissionPresentation()

{
	if (!IsValid(HUDViewModel))
	{
		return;
	}
	if (IsValid(MissionTitleText))
	{
		MissionTitleText->SetText(NSLOCTEXT("HeistHUD", "MissionTitle", "미션"));
	}
	if (IsValid(RequiredTargetNameText))
	{
		const FText TargetName = HUDViewModel->GetRequiredTargetDisplayName().IsEmpty()
			? NSLOCTEXT("HeistHUD", "RequiredTargetPending", "필수 목표 확인 중")
			: HUDViewModel->GetRequiredTargetDisplayName();
		RequiredTargetNameText->SetText(TargetName);
		RequiredTargetNameText->SetColorAndOpacity(FSlateColor(HUDViewModel->IsRequiredTargetAcquired()
			? FLinearColor(0.25f, 0.78f, 0.34f)
			: FLinearColor(0.52f, 0.52f, 0.52f)));
	}

	const UWorld* World = GetWorld();
	const AGameStateBase* WorldGameState = IsValid(World) ? World->GetGameState() : nullptr;
	const float EndServerTime = HUDViewModel->GetMissionEndServerTime();
	const int32 RemainingSeconds = IsValid(WorldGameState) && EndServerTime > 0.0f
		? FMath::Max(0, FMath::CeilToInt(EndServerTime - static_cast<float>(WorldGameState->GetServerWorldTimeSeconds())))
		: INDEX_NONE;
	if (RemainingSeconds == LastDisplayedMissionSeconds)
	{
		return;
	}
	LastDisplayedMissionSeconds = RemainingSeconds;

	if (IsValid(MissionTimeText))
	{
		MissionTimeText->SetText(RemainingSeconds == INDEX_NONE
			? FText::FromString(TEXT("-- : --"))
			: FText::FromString(FString::Printf(TEXT("%02d : %02d"), RemainingSeconds / 60, RemainingSeconds % 60)));
		MissionTimeText->SetColorAndOpacity(FSlateColor(RemainingSeconds != INDEX_NONE && RemainingSeconds < 60
			? FLinearColor(0.90f, 0.12f, 0.10f)
			: FLinearColor::White));
	}
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
		WeightText->SetVisibility(ESlateVisibility::Collapsed);
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
		ObjectiveText->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (IsValid(StatusText))
	{
		StatusText->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (IsValid(AlertText))
	{
		AlertText->SetVisibility(ESlateVisibility::Collapsed);
	}

	RefreshMissionPresentation();
	RefreshAlertPresentation();
	RefreshCrewStatusPresentation();
	RefreshInventoryShortcutPresentation();
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
	RefreshAlertStars();
	const FName AlertTriggerId = HUDViewModel->GetLastAlertTriggerId();
	if (!AlertTriggerId.IsNone() && AlertTriggerId != LastPresentedAlertTriggerId)
	{
		LastPresentedAlertTriggerId = AlertTriggerId;
		const FText EventText = ResolveAlertEventText(AlertTriggerId);
		if (!EventText.IsEmpty())
		{
			ShowTransientEvent(EventText);
		}
	}
}

void UHeistHUDWidget::RefreshAlertStars()
{
	if (!IsValid(HUDViewModel))
	{
		return;
	}
	if (IsValid(AlertTitleText))
	{
		AlertTitleText->SetText(NSLOCTEXT("HeistHUD", "AlertTitle", "경계도"));
	}

	const TArray<UImage*> AlertStars = {AlertStar01.Get(), AlertStar02.Get(), AlertStar03.Get(), AlertStar04.Get(), AlertStar05.Get(), AlertStar06.Get(), AlertStar07.Get(),
		AlertStar08.Get(), AlertStar09.Get(), AlertStar10.Get()};
	const float MeterValue = FMath::Clamp(HUDViewModel->GetAlertMeterValue(), 0.0f, 10.0f);
	for (int32 StarIndex = 0; StarIndex < AlertStars.Num(); ++StarIndex)
	{
		UImage* StarImage = AlertStars[StarIndex];
		if (!IsValid(StarImage))
		{
			continue;
		}
		const float FilledAmount = MeterValue - static_cast<float>(StarIndex);
		UTexture2D* StarTexture = FilledAmount >= 1.0f ? FullAlertStarTexture.Get() : (FilledAmount >= 0.5f ? HalfAlertStarTexture.Get() : EmptyAlertStarTexture.Get());
		if (IsValid(StarTexture))
		{
			StarImage->SetBrushFromTexture(StarTexture, true);
		}
		StarImage->SetColorAndOpacity(FLinearColor::White);
		StarImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UHeistHUDWidget::ShowTransientEvent(const FText& EventText)

{
	if (EventText.IsEmpty() || !IsValid(AlertEventText))
	{
		return;
	}
	AlertEventText->SetText(EventText);
	AlertEventText->SetVisibility(ESlateVisibility::HitTestInvisible);
	TransientEventHideWorldTime = IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() + 3.0f : 0.0f;
}

void UHeistHUDWidget::RefreshTransientEvent()
{
	if (!IsValid(AlertEventText) || AlertEventText->GetVisibility() == ESlateVisibility::Collapsed || TransientEventHideWorldTime <= 0.0f || !IsValid(GetWorld()))
	{
		return;
	}
	if (GetWorld()->GetTimeSeconds() >= TransientEventHideWorldTime)
	{
		AlertEventText->SetText(FText::GetEmpty());
		AlertEventText->SetVisibility(ESlateVisibility::Collapsed);
		TransientEventHideWorldTime = 0.0f;
	}
}

FText UHeistHUDWidget::ResolveAlertEventText(const FName TriggerId) const
{
	const FString Trigger = TriggerId.ToString();
	if (Trigger.StartsWith(TEXT("CCTV_")))
	{
		return NSLOCTEXT("HeistHUD", "EventCCTV", "CCTV에 발각되었습니다");
	}
	if (Trigger.StartsWith(TEXT("Laser_")))
	{
		return NSLOCTEXT("HeistHUD", "EventLaser", "레이저 경보가 작동했습니다");
	}
	if (Trigger.StartsWith(TEXT("GuardCapture_")))
	{
		return NSLOCTEXT("HeistHUD", "EventGuardCapture", "경비에게 붙잡혔습니다");
	}
	return FText::GetEmpty();
}

void UHeistHUDWidget::RefreshInventoryShortcutPresentation()
{
	if (IsValid(InventoryShortcutKeyText))
	{
		InventoryShortcutKeyText->SetText(NSLOCTEXT("HeistHUD", "InventoryShortcut", "TAB"));
	}
	if (!IsValid(InventoryShortcutIcon) || !IsValid(HUDViewModel))
	{
		return;
	}

	const AHeistPlayerCharacter* OwningCharacter = IsValid(GetOwningPlayer()) ? Cast<AHeistPlayerCharacter>(GetOwningPlayer()->GetPawn()) : nullptr;
	const UHeistNoiseEmitterComponent* NoiseEmitter = IsValid(OwningCharacter) ? OwningCharacter->GetNoiseEmitterComponent() : nullptr;
	const float HeavyThreshold = IsValid(NoiseEmitter) ? FMath::Max(1.0f, NoiseEmitter->GetHeavyWeightThreshold()) : 10.0f;
	const float WeightRatio = FMath::Clamp(HUDViewModel->GetLocalLootWeight() / HeavyThreshold, 0.0f, 1.0f);
	const FLinearColor LightColor(0.22f, 0.78f, 0.34f);
	const FLinearColor MediumColor(0.94f, 0.72f, 0.18f);
	const FLinearColor HeavyColor(0.90f, 0.14f, 0.10f);
	const FLinearColor InventoryColor = WeightRatio < 0.5f
		? FLinearColor::LerpUsingHSV(LightColor, MediumColor, WeightRatio * 2.0f)
		: FLinearColor::LerpUsingHSV(MediumColor, HeavyColor, (WeightRatio - 0.5f) * 2.0f);
	InventoryShortcutIcon->SetColorAndOpacity(InventoryColor);
}

void UHeistHUDWidget::RefreshHUDQuickSlots()
{
	const TArray<UHeistQuickSlotWidget*> QuickSlotWidgets = {HUDQuickSlot1.Get(), HUDQuickSlot2.Get(), HUDQuickSlot3.Get()};
	const FHeistQuickSlotPresentation* CoinPresentation = IsValid(QuickSlotViewModel)
		? QuickSlotViewModel->GetQuickSlotPresentations().FindByPredicate(
			[](const FHeistQuickSlotPresentation& Presentation) { return Presentation.SlotType == EHeistQuickSlotType::Coin; })
		: nullptr;
	for (int32 SlotIndex = 0; SlotIndex < QuickSlotWidgets.Num(); ++SlotIndex)
	{
		UHeistQuickSlotWidget* QuickSlotWidget = QuickSlotWidgets[SlotIndex];
		if (!IsValid(QuickSlotWidget))
		{
			continue;
		}
		if (SlotIndex == 0 && CoinPresentation != nullptr)
		{
			QuickSlotWidget->SetupHUDQuickSlot(*CoinPresentation, CoinQuickSlotIcon);
		}
		else
		{
			FHeistQuickSlotPresentation EmptyPresentation;
			QuickSlotWidget->SetupHUDQuickSlot(EmptyPresentation, nullptr);
		}
	}
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
	LastDisplayedMissionSeconds = INDEX_NONE;
	LastPresentedAlertTriggerId = NAME_None;
	TransientEventHideWorldTime = 0.0f;
	LastArrestFeedbackEvent = NAME_None;
	ArrestAudioPlayCount = 0;
	RescueAudioPlayCount = 0;

	if (IsValid(AlertText))
	{
		AlertText->SetText(FText::GetEmpty());
		AlertText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (IsValid(MissionTimeText))
	{
		MissionTimeText->SetText(FText::GetEmpty());
	}
	if (IsValid(AlertEventText))
	{
		AlertEventText->SetText(FText::GetEmpty());
		AlertEventText->SetVisibility(ESlateVisibility::Collapsed);
	}
	const TArray<UHeistTeamCardWidget*> TeamCards = {TeamCard1.Get(), TeamCard2.Get(), TeamCard3.Get(), TeamCard4.Get()};
	for (int32 SlotIndex = 0; SlotIndex < TeamCards.Num(); ++SlotIndex)
	{
		if (IsValid(TeamCards[SlotIndex]))
		{
			TeamCards[SlotIndex]->ApplyEmptySlot(SlotIndex + 1);
		}
	}
}

bool UHeistHUDWidget::IsHiddenPresentationStateReset() const
{
	const bool bAlertAudioStopped = !bAlertAudioInitialized && !IsValid(SuspenseMusicComponent) && !IsValid(AlarmMusicComponent);
	const bool bArrestAudioStopped = !IsValid(ArrestFeedbackAudioComponent);
	const bool bAlertTextReset = !IsValid(AlertText) || (AlertText->GetVisibility() == ESlateVisibility::Collapsed && AlertText->GetText().IsEmpty());
	const bool bMissionTimerReset = LastDisplayedMissionSeconds == INDEX_NONE && (!IsValid(MissionTimeText) || MissionTimeText->GetText().IsEmpty());
	const bool bEventReset = TransientEventHideWorldTime <= 0.0f &&
		(!IsValid(AlertEventText) || (AlertEventText->GetVisibility() == ESlateVisibility::Collapsed && AlertEventText->GetText().IsEmpty()));
	const bool bTransitionStateReset = !bLocalCrewStatusPresentationInitialized && LastPresentedLocalCrewStatus == EHeistCrewStatus::Active && LastArrestFeedbackEvent.IsNone() &&
		ArrestAudioPlayCount == 0 && RescueAudioPlayCount == 0;
	return bAlertAudioStopped && bArrestAudioStopped && bAlertTextReset && bMissionTimerReset && bEventReset && bTransitionStateReset;
}

void UHeistHUDWidget::DebugDumpFirstPersonHUDState() const
{
	const bool bCrosshairReady = IsValid(CrosshairContainer) && IsValid(CrosshairIdleIndicator) && IsValid(CrosshairFocusIndicator);
	const bool bCenterPromptReady = IsValid(InteractionPromptWidget) && IsValid(ActionProgressWidget);
	const bool bToolReady = IsValid(HUDQuickSlot1) && IsValid(HUDQuickSlot2) && IsValid(HUDQuickSlot3);
	const bool bStatusReady = IsValid(InventoryShortcutIcon) && IsValid(InventoryShortcutKeyText);
	const bool bObjectiveReady = IsValid(MissionTitleText) && IsValid(MissionTimeText) && IsValid(RequiredTargetNameText) && IsValid(HUDViewModel) &&
		!HUDViewModel->GetRequiredTargetDisplayName().IsEmpty();
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

	UE_LOG(LogHeistUI, Log, TEXT("[%s] Local crew presentation: Initialized=%s Last=%s Resolved=%s EventText='%s' Result=%s"), *GetName(),
		bLocalCrewStatusPresentationInitialized ? TEXT("true") : TEXT("false"), *UEnum::GetValueAsString(LastPresentedLocalCrewStatus),
		*UEnum::GetValueAsString(ResolveLocalCrewStatus()), IsValid(AlertEventText) ? *AlertEventText->GetText().ToString() : TEXT(""),
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
	const TArray<UImage*> AlertStars = {AlertStar01.Get(), AlertStar02.Get(), AlertStar03.Get(), AlertStar04.Get(), AlertStar05.Get(), AlertStar06.Get(), AlertStar07.Get(),
		AlertStar08.Get(), AlertStar09.Get(), AlertStar10.Get()};
	if (!IsValid(HUDViewModel) || !IsValid(AlertTitleText) || AlertStars.Contains(nullptr) || !IsValid(EmptyAlertStarTexture) || !IsValid(HalfAlertStarTexture) ||
		!IsValid(FullAlertStarTexture) || !IsValid(SuspenseMusic) || !IsValid(AlarmMusic))
	{
		return false;
	}

	const bool bAudioModeMatches = HUDViewModel->IsSuspenseMusicActive() != HUDViewModel->IsAlarmMusicActive() ||
								   (!HUDViewModel->IsSuspenseMusicActive() && !HUDViewModel->IsAlarmMusicActive());
	const bool bSuspensePlaybackMatches =
		!HUDViewModel->IsSuspenseMusicActive() || (IsValid(SuspenseMusicComponent) && SuspenseMusicComponent->IsPlaying());
	const bool bAlarmPlaybackMatches = !HUDViewModel->IsAlarmMusicActive() || (IsValid(AlarmMusicComponent) && AlarmMusicComponent->IsPlaying());
	const bool bStarsVisible = AlertStars.ContainsByPredicate([](const UImage* Star) { return !IsValid(Star) || Star->GetVisibility() == ESlateVisibility::Collapsed; }) == false;
	return bStarsVisible && bAudioModeMatches && bSuspensePlaybackMatches && bAlarmPlaybackMatches && bAlertAudioInitialized &&
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
	if (!IsValid(HUDViewModel))
	{
		return false;
	}

	const EHeistCrewStatus CrewStatus = ResolveLocalCrewStatus();
	return bLocalCrewStatusPresentationInitialized && LastPresentedLocalCrewStatus == CrewStatus;
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
		TEXT("[%s] Alert HUD presentation: Level=%s Meter=%.1f/10 Stars=10 Trigger=%s Event='%s' SuspenseRequested=%s SuspensePlaying=%s AlarmRequested=%s "
			 "AlarmPlaying=%s SuspenseAsset=%s AlarmAsset=%s AudioApplied=%s Result=%s"),
		*GetName(), IsValid(HUDViewModel) ? *UEnum::GetValueAsString(HUDViewModel->GetAlertLevel()) : TEXT("None"),
		IsValid(HUDViewModel) ? HUDViewModel->GetAlertMeterValue() : -1.0f,
		IsValid(HUDViewModel) ? *HUDViewModel->GetLastAlertTriggerId().ToString() : TEXT("None"), IsValid(AlertEventText) ? *AlertEventText->GetText().ToString() : TEXT("None"),
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
