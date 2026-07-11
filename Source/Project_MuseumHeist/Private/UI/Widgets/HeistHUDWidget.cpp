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
#include "UI/ViewModels/HeistGapTrackerViewModel.h"
#include "UI/ViewModels/HeistHUDViewModel.h"
#include "UI/ViewModels/HeistInventoryViewModel.h"
#include "UI/ViewModels/HeistQuickSlotViewModel.h"
#include "UI/Widgets/HeistGapTrackerWidget.h"
#include "UI/Widgets/HeistInteractionPromptWidget.h"
#include "UI/Widgets/HeistRareLootAlertWidget.h"
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
	Super::NativeDestruct();
}

#pragma endregion

#pragma region ViewModels

void UHeistHUDWidget::SetupHUDWidget(
	UHeistHUDViewModel* InHUDViewModel,
	UHeistInventoryViewModel* InInventoryViewModel,
	UHeistQuickSlotViewModel* InQuickSlotViewModel,
	UHeistGapTrackerViewModel* InGapTrackerViewModel,
	UHeistInteractionComponent* InInteractionComponent)
{
	checkf(IsValid(InHUDViewModel), TEXT("HeistHUDWidget requires a valid HUD ViewModel."));

	if (HUDViewModel != InHUDViewModel && IsValid(HUDViewModel))
	{
		HUDViewModel->GetPresentationChangedDelegate().RemoveAll(this);
	}
	HUDViewModel = InHUDViewModel;
	InventoryViewModel = InInventoryViewModel;
	QuickSlotViewModel = InQuickSlotViewModel;
	GapTrackerViewModel = InGapTrackerViewModel;
	InteractionComponent = InInteractionComponent;

	HUDViewModel->GetPresentationChangedDelegate().RemoveAll(this);
	HUDViewModel->GetPresentationChangedDelegate().AddUObject(
		this,
		&UHeistHUDWidget::RefreshHUDPresentation);
	ResolveInteractionChildWidgets();
	ResolveRareLootChildWidgets();
	ResolveGapTrackerChildWidget();
	ResolveStatusFeedbackChildWidgets();
	UE_LOG(
		LogHeistUI,
		Log,
		TEXT("[%s] HUD widget setup: Class=%s HUDViewModel=%s InteractionComponent=%s InteractionPromptWidget=%s InteractionPromptClass=%s ActionProgressWidget=%s ActionProgressClass=%s RareLootWarningWidget=%s RareLootWarningClass=%s RareLootMarkerWidget=%s RareLootMarkerClass=%s"),
		*GetName(),
		*GetClass()->GetName(),
		*GetNameSafe(HUDViewModel.Get()),
		*GetNameSafe(InteractionComponent.Get()),
		*GetNameSafe(InteractionPromptWidget.Get()),
		IsValid(InteractionPromptWidget) ? *InteractionPromptWidget->GetClass()->GetName() : TEXT("None"),
		*GetNameSafe(ActionProgressWidget.Get()),
		IsValid(ActionProgressWidget) ? *ActionProgressWidget->GetClass()->GetName() : TEXT("None"),
		*GetNameSafe(RareLootWarningWidget.Get()),
		IsValid(RareLootWarningWidget) ? *RareLootWarningWidget->GetClass()->GetName() : TEXT("None"),
		*GetNameSafe(RareLootMarkerWidget.Get()),
		IsValid(RareLootMarkerWidget) ? *RareLootMarkerWidget->GetClass()->GetName() : TEXT("None"));

	BP_OnHUDSourcesReady();
	ResolveInteractionChildWidgets();
	ResolveRareLootChildWidgets();
	ResolveGapTrackerChildWidget();
	ResolveStatusFeedbackChildWidgets();
	if (IsValid(InteractionPromptWidget))
	{
		InteractionPromptWidget->SetupInteractionPresentation(InteractionComponent, HUDViewModel);
	}
	if (IsValid(ActionProgressWidget))
	{
		ActionProgressWidget->SetupInteractionPresentation(InteractionComponent, HUDViewModel);
	}
	if (IsValid(RareLootWarningWidget))
	{
		RareLootWarningWidget->SetupRareLootAlertWidget(HUDViewModel);
	}
	if (IsValid(RareLootMarkerWidget))
	{
		RareLootMarkerWidget->SetupRareLootAlertWidget(HUDViewModel);
	}
	if (IsValid(GapTrackerWidget) && IsValid(GapTrackerViewModel))
	{
		GapTrackerWidget->SetupGapTrackerWidget(GapTrackerViewModel);
	}
	SetupPopupFeedbackPresentation();
	SetupSoundPingPresentation();
	RefreshHUDPresentation();
}

void UHeistHUDWidget::SetupPopupFeedbackPresentation()
{
	AHeistPlayerController* OwningPlayerController = Cast<AHeistPlayerController>(GetOwningPlayer());
	if (!IsValid(OwningPlayerController) || !IsValid(PopupFeedbackLayer) || !PopupFeedbackWidgetClass)
	{
		UE_LOG(
			LogHeistUI,
			Warning,
			TEXT("[%s] Popup feedback presentation setup skipped: Controller=%s Layer=%s Class=%s"),
			*GetName(),
			*GetNameSafe(OwningPlayerController),
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
	if (!IsValid(OwningPlayerController)
		|| !IsValid(HeistGameState)
		|| !IsValid(SoundPingMarkerLayer)
		|| !SoundPingMarkerWidgetClass)
	{
		UE_LOG(
			LogHeistUI,
			Warning,
			TEXT("[%s] Sound Ping presentation setup skipped: Controller=%s GameState=%s MarkerLayer=%s MarkerClass=%s"),
			*GetName(),
			*GetNameSafe(OwningPlayerController),
			*GetNameSafe(HeistGameState),
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
		Log,
		TEXT("[%s] HUD child widget resolved by name: Name=%s Widget=%s Class=%s"),
		*GetName(),
		*WidgetName.ToString(),
		*GetNameSafe(ResolvedWidget),
		*ResolvedWidget->GetClass()->GetName());
	return ResolvedWidget;
}

void UHeistHUDWidget::ResolveRareLootChildWidgets()
{
	RareLootWarningWidget = ResolveRareLootChildWidget(
		TEXT("RareLootWarningWidget"),
		RareLootWarningWidget);
	RareLootMarkerWidget = ResolveRareLootChildWidget(
		TEXT("RareLootMarkerWidget"),
		RareLootMarkerWidget);
}

UHeistRareLootAlertWidget* UHeistHUDWidget::ResolveRareLootChildWidget(
	const FName WidgetName,
	UHeistRareLootAlertWidget* ExistingWidget) const
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
			TEXT("[%s] HUD rare loot child widget missing: Name=%s"),
			*GetName(),
			*WidgetName.ToString());
		return nullptr;
	}

	UHeistRareLootAlertWidget* ResolvedWidget = Cast<UHeistRareLootAlertWidget>(FoundWidget);
	if (!IsValid(ResolvedWidget))
	{
		UE_LOG(
			LogHeistUI,
			Warning,
			TEXT("[%s] HUD rare loot child widget type mismatch: Name=%s Found=%s FoundClass=%s Expected=HeistRareLootAlertWidget"),
			*GetName(),
			*WidgetName.ToString(),
			*GetNameSafe(FoundWidget),
			*FoundWidget->GetClass()->GetName());
		return nullptr;
	}

	UE_LOG(
		LogHeistUI,
		Log,
		TEXT("[%s] HUD rare loot child widget resolved by name: Name=%s Widget=%s Class=%s"),
		*GetName(),
		*WidgetName.ToString(),
		*GetNameSafe(ResolvedWidget),
		*ResolvedWidget->GetClass()->GetName());
	return ResolvedWidget;
}

void UHeistHUDWidget::ResolveGapTrackerChildWidget()
{
	if (IsValid(GapTrackerWidget))
	{
		return;
	}

	UWidget* FoundWidget = GetWidgetFromName(TEXT("GapTrackerWidget"));
	if (!IsValid(FoundWidget))
	{
		UE_LOG(
			LogHeistUI,
			Warning,
			TEXT("[%s] HUD Gap Tracker child widget missing: Name=GapTrackerWidget"),
			*GetName());
		return;
	}

	GapTrackerWidget = Cast<UHeistGapTrackerWidget>(FoundWidget);
	if (!IsValid(GapTrackerWidget))
	{
		UE_LOG(
			LogHeistUI,
			Warning,
			TEXT("[%s] HUD Gap Tracker child widget type mismatch: Name=GapTrackerWidget Found=%s FoundClass=%s Expected=HeistGapTrackerWidget"),
			*GetName(),
			*GetNameSafe(FoundWidget),
			*FoundWidget->GetClass()->GetName());
		return;
	}

	UE_LOG(
		LogHeistUI,
		Log,
		TEXT("[%s] HUD Gap Tracker child widget resolved: Widget=%s Class=%s"),
		*GetName(),
		*GetNameSafe(GapTrackerWidget.Get()),
		*GapTrackerWidget->GetClass()->GetName());
}

void UHeistHUDWidget::ResolveStatusFeedbackChildWidgets()
{
	if (!IsValid(StatusFeedbackWidget))
	{
		StatusFeedbackWidget = Cast<UHeistUserWidgetBase>(GetWidgetFromName(TEXT("StatusFeedbackWidget")));
	}

	if (!IsValid(StatusFeedbackWidget))
	{
		UE_LOG(LogHeistUI, Warning, TEXT("[%s] HUD status feedback child widget missing: Name=StatusFeedbackWidget"), *GetName());
		return;
	}

	StatusFeedbackContainer = StatusFeedbackWidget->GetWidgetFromName(TEXT("StatusFeedbackContainer"));
	StatusFeedbackText = Cast<UTextBlock>(StatusFeedbackWidget->GetWidgetFromName(TEXT("StatusFeedbackText")));
	StatusStunnedVignette = StatusFeedbackWidget->GetWidgetFromName(TEXT("StatusStunnedVignette"));
	StatusImmuneVignette = StatusFeedbackWidget->GetWidgetFromName(TEXT("StatusImmuneVignette"));
	StatusSmokeVignette = StatusFeedbackWidget->GetWidgetFromName(TEXT("StatusSmokeVignette"));

	const bool bContractValid = IsValid(StatusFeedbackContainer)
		&& IsValid(StatusFeedbackText)
		&& IsValid(StatusStunnedVignette)
		&& IsValid(StatusImmuneVignette)
		&& IsValid(StatusSmokeVignette);
	if (bContractValid)
	{
		UE_LOG(
			LogHeistUI,
			Log,
			TEXT("[%s] Status feedback widget contract resolved: Widget=%s Container=%s Text=%s Stunned=%s Immune=%s Smoke=%s Valid=true"),
			*GetName(),
			*GetNameSafe(StatusFeedbackWidget),
			*GetNameSafe(StatusFeedbackContainer),
			*GetNameSafe(StatusFeedbackText),
			*GetNameSafe(StatusStunnedVignette),
			*GetNameSafe(StatusImmuneVignette),
			*GetNameSafe(StatusSmokeVignette));
	}
	else
	{
		UE_LOG(
			LogHeistUI,
			Warning,
			TEXT("[%s] Status feedback widget contract incomplete: Widget=%s Container=%s Text=%s Stunned=%s Immune=%s Smoke=%s"),
			*GetName(),
			*GetNameSafe(StatusFeedbackWidget),
			*GetNameSafe(StatusFeedbackContainer),
			*GetNameSafe(StatusFeedbackText),
			*GetNameSafe(StatusStunnedVignette),
			*GetNameSafe(StatusImmuneVignette),
			*GetNameSafe(StatusSmokeVignette));
	}

	StatusFeedbackWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void UHeistHUDWidget::RefreshStatusFeedbackPresentation(
	const bool bStunned,
	const bool bStunImmune,
	const bool bInSmoke)
{
	if (!IsValid(StatusFeedbackWidget))
	{
		return;
	}

	const bool bAnyStatus = bStunned || bStunImmune || bInSmoke;
	StatusFeedbackWidget->SetVisibility(bAnyStatus ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (IsValid(StatusFeedbackContainer))
	{
		StatusFeedbackContainer->SetVisibility(bAnyStatus ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (IsValid(StatusStunnedVignette))
	{
		StatusStunnedVignette->SetVisibility(bStunned ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (IsValid(StatusImmuneVignette))
	{
		StatusImmuneVignette->SetVisibility(bStunImmune ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (IsValid(StatusSmokeVignette))
	{
		StatusSmokeVignette->SetVisibility(bInSmoke ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (IsValid(StatusFeedbackText))
	{
		StatusFeedbackText->SetText(
			bStunned
				? NSLOCTEXT("HeistFeedback", "StunnedStatusFeedback", "STUNNED")
				: bStunImmune
					? NSLOCTEXT("HeistFeedback", "ImmuneStatusFeedback", "STUN IMMUNE")
					: bInSmoke
						? NSLOCTEXT("HeistFeedback", "SmokeStatusFeedback", "IN SMOKE")
						: FText::GetEmpty());
	}

	if (!bStatusFeedbackInitialized
		|| bCachedStatusStunned != bStunned
		|| bCachedStatusStunImmune != bStunImmune
		|| bCachedStatusInSmoke != bInSmoke)
	{
		UE_LOG(
			LogHeistUI,
			Log,
			TEXT("[%s] Status feedback refreshed: Stunned=%s StunImmune=%s InSmoke=%s Visible=%s"),
			*GetName(),
			bStunned ? TEXT("true") : TEXT("false"),
			bStunImmune ? TEXT("true") : TEXT("false"),
			bInSmoke ? TEXT("true") : TEXT("false"),
			bAnyStatus ? TEXT("true") : TEXT("false"));
		bStatusFeedbackInitialized = true;
		bCachedStatusStunned = bStunned;
		bCachedStatusStunImmune = bStunImmune;
		bCachedStatusInSmoke = bInSmoke;
	}
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
	const bool bStunned = HUDViewModel->IsStunned();
	const bool bStunImmune = HUDViewModel->IsStunImmune();
	const bool bInSmoke = HUDViewModel->IsInSmoke();
	const bool bEscapeCastActive = HUDViewModel->IsEscapeCastActive();
	const float EscapeCastEndServerTime = HUDViewModel->GetEscapeCastEndServerTime();
	const bool bTrapPlacementCastActive = HUDViewModel->IsTrapPlacementCastActive();
	const float TrapPlacementCastEndServerTime = HUDViewModel->GetTrapPlacementCastEndServerTime();

	if (IsValid(ScoreText))
	{
		ScoreText->SetText(FText::Format(
			NSLOCTEXT("HeistHUD", "ScoreFormat", "SCORE  {0}"),
			FText::AsNumber(LocalLootScore)));
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
		const FText ActionLabel = bEscapeCastActive
			? NSLOCTEXT("HeistHUD", "EscapeCastAction", "ACTION  ESCAPING")
			: bTrapPlacementCastActive
				? NSLOCTEXT("HeistHUD", "TrapCastAction", "ACTION  PLACING TRAP")
				: NSLOCTEXT("HeistHUD", "ReadyAction", "ACTION  READY");
		ActionText->SetText(ActionLabel);
	}

	if (IsValid(StatusText))
	{
		const FText StatusLabel = bLocalPlayerEscaped
			? NSLOCTEXT("HeistHUD", "EscapedStatus", "STATUS  ESCAPED")
			: bStunned
				? NSLOCTEXT("HeistHUD", "StunnedStatus", "STATUS  STUNNED")
				: bStunImmune
					? NSLOCTEXT("HeistHUD", "StunImmuneStatus", "STATUS  STUN IMMUNE")
					: bInSmoke
						? NSLOCTEXT("HeistHUD", "SmokeStatus", "STATUS  IN SMOKE")
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

	RefreshStatusFeedbackPresentation(bStunned, bStunImmune, bInSmoke);

	BP_RefreshHUDPresentation(
		LocalLootScore,
		LocalLootWeight,
		ConnectedPlayerCount,
		bLocalPlayerEscaped,
		bEscapePhaseOpen,
		bStunned,
		bStunImmune,
		bInSmoke,
		bEscapeCastActive,
		EscapeCastEndServerTime,
		bTrapPlacementCastActive,
		TrapPlacementCastEndServerTime);
}

#pragma endregion

#pragma region Debug

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

	UE_LOG(
		LogHeistUI,
		Log,
		TEXT("[%s] Status feedback dump: Initialized=%s Stunned=%s StunImmune=%s InSmoke=%s Widget=%s"),
		*GetName(),
		bStatusFeedbackInitialized ? TEXT("true") : TEXT("false"),
		bCachedStatusStunned ? TEXT("true") : TEXT("false"),
		bCachedStatusStunImmune ? TEXT("true") : TEXT("false"),
		bCachedStatusInSmoke ? TEXT("true") : TEXT("false"),
		*GetNameSafe(StatusFeedbackWidget));
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
