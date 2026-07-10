#include "UI/Widgets/HeistHUDWidget.h"

#include "Character/Components/HeistInteractionComponent.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Core/HeistLogChannels.h"
#include "UI/ViewModels/HeistGapTrackerViewModel.h"
#include "UI/ViewModels/HeistHUDViewModel.h"
#include "UI/ViewModels/HeistInventoryViewModel.h"
#include "UI/ViewModels/HeistQuickSlotViewModel.h"
#include "UI/Widgets/HeistGapTrackerWidget.h"
#include "UI/Widgets/HeistInteractionPromptWidget.h"
#include "UI/Widgets/HeistRareLootAlertWidget.h"

#pragma region Construction

UHeistHUDWidget::UHeistHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#pragma endregion

#pragma region Lifecycle

void UHeistHUDWidget::NativeDestruct()
{
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
	RefreshHUDPresentation();
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
