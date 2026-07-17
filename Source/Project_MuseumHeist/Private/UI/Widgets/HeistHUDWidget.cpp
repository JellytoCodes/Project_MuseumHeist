#include "UI/Widgets/HeistHUDWidget.h"

#include "Character/Components/HeistInteractionComponent.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Core/HeistGameState.h"
#include "Core/HeistLogChannels.h"
#include "Core/HeistPlayerController.h"
#include "GameFramework/PlayerController.h"
#include "UI/Pool/HeistPopupWidgetPool.h"
#include "UI/Pool/HeistSoundPingWidgetPool.h"
#include "UI/ViewModels/HeistHUDViewModel.h"
#include "UI/ViewModels/HeistInventoryViewModel.h"
#include "UI/ViewModels/HeistQuickSlotViewModel.h"
#include "UI/Widgets/HeistInteractionPromptWidget.h"
#include "UI/Widgets/HeistSoundPingMarkerWidget.h"

#pragma region Construction

UHeistHUDWidget::UHeistHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#pragma endregion

#pragma region Lifecycle

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
	if (IsValid(SoundPingWidgetPool))
	{
		SoundPingWidgetPool->ShutdownPool();
	}
	if (IsValid(HUDViewModel))
	{
		HUDViewModel->GetPresentationChangedDelegate().RemoveAll(this);
	}
	if (IsValid(QuickSlotViewModel))
	{
		QuickSlotViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	}
	Super::NativeDestruct();
}

#pragma endregion

#pragma region ViewModels

void UHeistHUDWidget::SetupHUDWidget(
	UHeistHUDViewModel* InHUDViewModel,
	UHeistInventoryViewModel* InInventoryViewModel,
	UHeistQuickSlotViewModel* InQuickSlotViewModel,
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
		QuickSlotViewModel->GetSnapshotChangedDelegate().AddUObject(
			this,
			&UHeistHUDWidget::RefreshToolPresentation);
	}
	if (InteractionComponent != InInteractionComponent && IsValid(InteractionComponent))
	{
		InteractionComponent->GetInteractionTargetChangedDelegate().RemoveAll(this);
	}
	InteractionComponent = InInteractionComponent;
	if (IsValid(InteractionComponent))
	{
		InteractionComponent->GetInteractionTargetChangedDelegate().RemoveAll(this);
		InteractionComponent->GetInteractionTargetChangedDelegate().AddUObject(
			this,
			&UHeistHUDWidget::RefreshCrosshairPresentation);
	}

	HUDViewModel->GetPresentationChangedDelegate().RemoveAll(this);
	HUDViewModel->GetPresentationChangedDelegate().AddUObject(
		this,
		&UHeistHUDWidget::RefreshHUDPresentation);
	ResolveInteractionChildWidgets();
	ResolveCrosshairWidgets();
	UE_LOG(
		LogHeistUI,
		Verbose,
		TEXT("[%s] HUD widget setup: Class=%s HUDViewModel=%s InteractionComponent=%s InteractionPromptWidget=%s InteractionPromptClass=%s ActionProgressWidget=%s ActionProgressClass=%s"),
		*GetName(),
		*GetClass()->GetName(),
		*GetNameSafe(HUDViewModel.Get()),
		*GetNameSafe(InteractionComponent.Get()),
		*GetNameSafe(InteractionPromptWidget.Get()),
		IsValid(InteractionPromptWidget) ? *InteractionPromptWidget->GetClass()->GetName() : TEXT("None"),
		*GetNameSafe(ActionProgressWidget.Get()),
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
	RefreshCrosshairPresentation(
		IsValid(InteractionComponent) ? InteractionComponent->GetCurrentInteractionTarget() : nullptr,
		IsValid(InteractionComponent) && InteractionComponent->HasValidInteractionTarget());
	SetupPopupFeedbackPresentation();
	SetupSoundPingPresentation();
	RefreshToolPresentation();
	RefreshHUDPresentation();
}

void UHeistHUDWidget::SetupPopupFeedbackPresentation()
{
	AHeistPlayerController* OwningPlayerController = Cast<AHeistPlayerController>(GetOwningPlayer());
	if (!IsValid(OwningPlayerController))
	{
		UE_LOG(
			LogHeistUI,
			Warning,
			TEXT("[%s] Popup feedback presentation setup rejected: Reason=MissingController"),
			*GetName());
		return;
	}
	if (!IsValid(PopupFeedbackLayer) && !PopupFeedbackWidgetClass)
	{
		UE_LOG(
			LogHeistUI,
			Verbose,
			TEXT("[%s] Popup feedback presentation disabled: Layer=None Class=None"),
			*GetName());
		return;
	}
	if (!IsValid(PopupFeedbackLayer) || !PopupFeedbackWidgetClass)
	{
		UE_LOG(
			LogHeistUI,
			Warning,
			TEXT("[%s] Popup feedback presentation setup rejected: Layer=%s Class=%s"),
			*GetName(),
			*GetNameSafe(PopupFeedbackLayer),
			*GetNameSafe(PopupFeedbackWidgetClass.Get()));
		return;
	}

	if (!IsValid(PopupWidgetPool))
	{
		PopupWidgetPool = NewObject<UHeistPopupWidgetPool>(this);
	}
	PopupWidgetPool->SetupPool(
		OwningPlayerController,
		PopupFeedbackLayer,
		PopupFeedbackWidgetClass,
		PopupFeedbackCapacity);
}

void UHeistHUDWidget::SetupSoundPingPresentation()
{
	APlayerController* OwningPlayerController = GetOwningPlayer();
	AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(OwningPlayerController))
	{
		UE_LOG(
			LogHeistUI,
			Warning,
			TEXT("[%s] Sound Ping presentation setup rejected: Reason=MissingController"),
			*GetName());
		return;
	}
	if (!IsValid(HeistGameState))
	{
		UE_LOG(
			LogHeistUI,
			Verbose,
			TEXT("[%s] Sound Ping presentation setup deferred: Reason=GameStateNotReady"),
			*GetName());
		return;
	}
	if (!IsValid(SoundPingMarkerLayer) || !SoundPingMarkerWidgetClass)
	{
		UE_LOG(
			LogHeistUI,
			Warning,
			TEXT("[%s] Sound Ping presentation setup rejected: MarkerLayer=%s MarkerClass=%s"),
			*GetName(),
			*GetNameSafe(SoundPingMarkerLayer),
			*GetNameSafe(SoundPingMarkerWidgetClass.Get()));
		return;
	}

	if (!IsValid(SoundPingWidgetPool))
	{
		SoundPingWidgetPool = NewObject<UHeistSoundPingWidgetPool>(this);
	}

	SoundPingWidgetPool->SetupPool(
		OwningPlayerController,
		HeistGameState,
		SoundPingMarkerLayer,
		SoundPingMarkerWidgetClass,
		SoundPingMarkerScreenMarginPixels);
}

void UHeistHUDWidget::ResolveInteractionChildWidgets()
{
	InteractionPromptWidget = ResolveInteractionChildWidget(
		TEXT("InteractionPromptWidget"),
		InteractionPromptWidget);
	ActionProgressWidget = ResolveInteractionChildWidget(
		TEXT("ActionProgressWidget"),
		ActionProgressWidget);
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

	const bool bContractValid = IsValid(CrosshairContainer)
		&& IsValid(CrosshairIdleIndicator)
		&& IsValid(CrosshairFocusIndicator);
	if (bContractValid)
	{
		UE_LOG(
			LogHeistUI,
			Verbose,
			TEXT("[%s] Crosshair widget contract: Container=%s Idle=%s Focus=%s Valid=true"),
			*GetName(),
			*GetNameSafe(CrosshairContainer),
			*GetNameSafe(CrosshairIdleIndicator),
			*GetNameSafe(CrosshairFocusIndicator));
	}
	else
	{
		UE_LOG(
			LogHeistUI,
			Warning,
			TEXT("[%s] Crosshair widget contract: Container=%s Idle=%s Focus=%s Valid=false"),
			*GetName(),
			*GetNameSafe(CrosshairContainer),
			*GetNameSafe(CrosshairIdleIndicator),
			*GetNameSafe(CrosshairFocusIndicator));
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
		CrosshairIdleIndicator->SetVisibility(
			bFocused ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (IsValid(CrosshairFocusIndicator))
	{
		CrosshairFocusIndicator->SetVisibility(
			bFocused ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	UE_LOG(
		LogHeistUI,
		Verbose,
		TEXT("[%s] Crosshair presentation refreshed: Target=%s Available=%s State=%s"),
		*GetName(),
		*GetNameSafe(TargetActor),
		bAvailable ? TEXT("true") : TEXT("false"),
		bFocused ? TEXT("Focus") : TEXT("Idle"));
}

UHeistInteractionPromptWidget* UHeistHUDWidget::ResolveInteractionChildWidget(
	const FName WidgetName,
	UHeistInteractionPromptWidget* ExistingWidget) const
{
	if (IsValid(ExistingWidget))
	{
		return ExistingWidget;
	}

	UWidget* FoundWidget = GetWidgetFromName(WidgetName);
	if (!IsValid(FoundWidget))
	{
		UE_LOG(
			LogHeistUI,
			Warning,
			TEXT("[%s] HUD child widget missing: Name=%s"),
			*GetName(),
			*WidgetName.ToString());
		return nullptr;
	}

	UHeistInteractionPromptWidget* ResolvedWidget = Cast<UHeistInteractionPromptWidget>(FoundWidget);
	if (!IsValid(ResolvedWidget))
	{
		UE_LOG(
			LogHeistUI,
			Warning,
			TEXT("[%s] HUD child widget type mismatch: Name=%s Found=%s FoundClass=%s Expected=HeistInteractionPromptWidget"),
			*GetName(),
			*WidgetName.ToString(),
			*GetNameSafe(FoundWidget),
			*FoundWidget->GetClass()->GetName());
		return nullptr;
	}

	UE_LOG(
		LogHeistUI,
		Verbose,
		TEXT("[%s] HUD child widget resolved by name: Name=%s Widget=%s Class=%s"),
		*GetName(),
		*WidgetName.ToString(),
		*GetNameSafe(ResolvedWidget),
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
		CoinPresentation = QuickSlotViewModel->GetQuickSlotPresentations().FindByPredicate(
			[](const FHeistQuickSlotPresentation& Presentation)
			{
				return Presentation.SlotType == EHeistQuickSlotType::Coin;
			});
	}

	if (CoinPresentation == nullptr)
	{
		ToolText->SetText(NSLOCTEXT("HeistHUD", "ToolUnavailable", "TOOL  --"));
	}
	else if (!CoinPresentation->bAssigned)
	{
		ToolText->SetText(FText::Format(
			NSLOCTEXT("HeistHUD", "ToolEmptyFormat", "TOOL  {0}  EMPTY"),
			CoinPresentation->KeyLabel));
	}
	else
	{
		ToolText->SetText(FText::Format(
			NSLOCTEXT("HeistHUD", "CoinToolFormat", "TOOL  {0}  COIN  x{1}"),
			CoinPresentation->KeyLabel,
			FText::AsNumber(CoinPresentation->Quantity)));
	}

	ToolText->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UHeistHUDWidget::RefreshHUDPresentation()
{
	if (!IsValid(HUDViewModel))
	{
		return;
	}

	const int32 LocalLootScore = HUDViewModel->GetLocalLootScore();
	const float LocalLootWeight = HUDViewModel->GetLocalLootWeight();
	const int32 LocalPlayerId = HUDViewModel->GetLocalPlayerId();
	const int32 ConnectedPlayerCount = HUDViewModel->GetConnectedPlayerCount();
	const bool bLocalPlayerEscaped = HUDViewModel->IsLocalPlayerEscaped();
	const bool bEscapePhaseOpen = HUDViewModel->IsEscapePhaseOpen();
	const bool bEscapeCastActive = HUDViewModel->IsEscapeCastActive();
	const float EscapeCastEndServerTime = HUDViewModel->GetEscapeCastEndServerTime();
	const bool bTrapPlacementCastActive = HUDViewModel->IsTrapPlacementCastActive();
	const float TrapPlacementCastEndServerTime = HUDViewModel->GetTrapPlacementCastEndServerTime();
	const bool bObservationCastActive = HUDViewModel->IsObservationCastActive();

	if (IsValid(ScoreText))
	{
		ScoreText->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (IsValid(WeightText))
	{
		FNumberFormattingOptions WeightFormatting;
		WeightFormatting.MinimumFractionalDigits = 1;
		WeightFormatting.MaximumFractionalDigits = 1;
		WeightText->SetText(FText::Format(
			NSLOCTEXT("HeistHUD", "WeightFormat", "WEIGHT  {0}"),
			FText::AsNumber(LocalLootWeight, &WeightFormatting)));
	}

	if (IsValid(ActionText))
	{
		const FText ActionLabel = bObservationCastActive
			? NSLOCTEXT("HeistHUD", "ObservationCastAction", "ACTION  OBSERVING")
			: (bEscapeCastActive
				? NSLOCTEXT("HeistHUD", "EscapeCastAction", "ACTION  ESCAPING")
				: bTrapPlacementCastActive
					? NSLOCTEXT("HeistHUD", "TrapCastAction", "ACTION  PLACING TRAP")
					: NSLOCTEXT("HeistHUD", "ReadyAction", "ACTION  READY"));
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
			? NSLOCTEXT("HeistHUD", "EscapedStatus", "STATUS  ESCAPED")
			: NSLOCTEXT("HeistHUD", "NormalStatus", "STATUS  NORMAL");
		StatusText->SetText(StatusLabel);
	}

	if (IsValid(AlertText))
	{
		const FText PlayerIdText = LocalPlayerId > 0
			? FText::AsNumber(LocalPlayerId)
			: NSLOCTEXT("HeistHUD", "UnknownPlayerId", "?");
		AlertText->SetText(FText::Format(
			bEscapePhaseOpen
				? NSLOCTEXT("HeistHUD", "EscapeOpenAlertFormat", "ALERT  ESCAPE OPEN  |  PLAYER {0}  |  PLAYERS {1}/4")
				: NSLOCTEXT("HeistHUD", "PlayerIdentityCountAlertFormat", "PLAYER {0}  |  PLAYERS {1}/4"),
			PlayerIdText,
			FText::AsNumber(ConnectedPlayerCount)));
	}

	BP_RefreshHUDPresentation(
		LocalLootScore,
		LocalLootWeight,
		ConnectedPlayerCount,
		bLocalPlayerEscaped,
		bEscapePhaseOpen,
		bEscapeCastActive,
		EscapeCastEndServerTime,
		bTrapPlacementCastActive,
		TrapPlacementCastEndServerTime);
	RefreshToolPresentation();
}

#pragma endregion

#pragma region Debug

void UHeistHUDWidget::DebugDumpFirstPersonHUDState() const
{
	const bool bCrosshairReady = IsValid(CrosshairContainer)
		&& IsValid(CrosshairIdleIndicator)
		&& IsValid(CrosshairFocusIndicator);
	const bool bCenterPromptReady = IsValid(InteractionPromptWidget)
		&& IsValid(ActionProgressWidget);
	const bool bToolReady = IsValid(ToolText);
	const bool bStatusReady = IsValid(StatusText) && IsValid(WeightText);
	const bool bCompetitiveScoreHidden = !IsValid(ScoreText)
		|| ScoreText->GetVisibility() == ESlateVisibility::Collapsed
		|| ScoreText->GetVisibility() == ESlateVisibility::Hidden;
	const bool bLegacyCompetitiveWidgetsAbsent = !IsValid(GetWidgetFromName(TEXT("GapTracker")))
		&& !IsValid(GetWidgetFromName(TEXT("GapTrackerWidget")))
		&& !IsValid(GetWidgetFromName(TEXT("RankText")))
		&& !IsValid(GetWidgetFromName(TEXT("WinnerText")));
	const bool bContractPass = bCrosshairReady
		&& bCenterPromptReady
		&& bToolReady
		&& bStatusReady
		&& bCompetitiveScoreHidden
		&& bLegacyCompetitiveWidgetsAbsent;

	const FString ContractMessage = FString::Printf(
		TEXT("[%s] First-person HUD contract: Crosshair=%s CenterPrompt=%s Tool=%s Status=%s CompetitiveScoreHidden=%s LegacyGapRankAbsent=%s Result=%s"),
		*GetName(),
		bCrosshairReady ? TEXT("true") : TEXT("false"),
		bCenterPromptReady ? TEXT("true") : TEXT("false"),
		bToolReady ? TEXT("true") : TEXT("false"),
		bStatusReady ? TEXT("true") : TEXT("false"),
		bCompetitiveScoreHidden ? TEXT("true") : TEXT("false"),
		bLegacyCompetitiveWidgetsAbsent ? TEXT("true") : TEXT("false"),
		bContractPass ? TEXT("PASS") : TEXT("FAIL"));
	if (bContractPass)
	{
		UE_LOG(LogHeistUI, Log, TEXT("%s"), *ContractMessage);
	}
	else
	{
		UE_LOG(LogHeistUI, Error, TEXT("%s"), *ContractMessage);
	}

	UE_LOG(
		LogHeistUI,
		Log,
		TEXT("[%s] First-person HUD state: ToolText='%s' StatusText='%s' WeightText='%s'"),
		*GetName(),
		IsValid(ToolText) ? *ToolText->GetText().ToString() : TEXT("None"),
		IsValid(StatusText) ? *StatusText->GetText().ToString() : TEXT("None"),
		IsValid(WeightText) ? *WeightText->GetText().ToString() : TEXT("None"));

	if (IsValid(HUDViewModel))
	{
		UE_LOG(
			LogHeistUI,
			Log,
			TEXT("[%s] Observation presentation: Active=%s ReferenceVisible=%s ReferenceArtifact=%s EndServerTime=%.2f ObjectiveArtifact=%s ObjectiveCase=%s ObjectiveState=%d ObjectiveText='%s' OwnerOnly=true ObjectiveWidget=%s"),
			*GetName(),
			HUDViewModel->IsObservationCastActive() ? TEXT("true") : TEXT("false"),
			HUDViewModel->IsObservationReferenceVisible() ? TEXT("true") : TEXT("false"),
			*HUDViewModel->GetObservationReferenceArtifactId().ToString(),
			HUDViewModel->GetObservationCastEndServerTime(),
			*HUDViewModel->GetObjectiveArtifactId().ToString(),
			*HUDViewModel->GetObjectiveCaseId().ToString(),
			static_cast<int32>(HUDViewModel->GetObjectiveState()),
			*HUDViewModel->GetObjectiveStateText().ToString(),
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

void UHeistHUDWidget::DebugDumpSoundPingMarkers() const
{
	if (IsValid(SoundPingWidgetPool))
	{
		SoundPingWidgetPool->DebugDumpState();
	}
	else
	{
		UE_LOG(LogHeistUI, Warning, TEXT("[%s] Sound Ping pool dump failed: Reason=MissingPool"), *GetName());
	}
}

void UHeistHUDWidget::DebugRunSoundPingPoolTest()
{
	if (IsValid(SoundPingWidgetPool))
	{
		SoundPingWidgetPool->DebugRunPresentationTest();
	}
	else
	{
		UE_LOG(LogHeistUI, Warning, TEXT("[%s] Sound Ping pool test failed: Reason=MissingPool"), *GetName());
	}
}

#pragma endregion
