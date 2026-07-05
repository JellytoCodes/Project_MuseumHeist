#include "UI/Widgets/HeistHUDWidget.h"

#include "Components/TextBlock.h"
#include "UI/ViewModels/HeistGapTrackerViewModel.h"
#include "UI/ViewModels/HeistHUDViewModel.h"
#include "UI/ViewModels/HeistInventoryViewModel.h"
#include "UI/ViewModels/HeistQuickSlotViewModel.h"

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
	UHeistGapTrackerViewModel* InGapTrackerViewModel)
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

	HUDViewModel->GetPresentationChangedDelegate().RemoveAll(this);
	HUDViewModel->GetPresentationChangedDelegate().AddUObject(
		this,
		&UHeistHUDWidget::RefreshHUDPresentation);

	BP_OnHUDSourcesReady();
	RefreshHUDPresentation();
}

void UHeistHUDWidget::RefreshHUDPresentation()
{
	if (!IsValid(HUDViewModel))
	{
		return;
	}

	const int32 LocalLootScore = HUDViewModel->GetLocalLootScore();
	const float LocalLootWeight = HUDViewModel->GetLocalLootWeight();
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
		AlertText->SetText(FText::Format(
			bEscapePhaseOpen
				? NSLOCTEXT("HeistHUD", "EscapeOpenAlertFormat", "ALERT  ESCAPE OPEN  |  PLAYERS {0}/4")
				: NSLOCTEXT("HeistHUD", "PlayerCountAlertFormat", "PLAYERS  {0}/4"),
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
