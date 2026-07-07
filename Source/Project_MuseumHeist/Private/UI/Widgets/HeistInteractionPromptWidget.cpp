#include "UI/Widgets/HeistInteractionPromptWidget.h"

#include "Character/Components/HeistInteractionComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Core/HeistLogChannels.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"
#include "UI/ViewModels/HeistHUDViewModel.h"
#include "World/Actors/Escape/HeistVentActor.h"
#include "World/Actors/Loot/HeistLootActor.h"

#pragma region Construction

UHeistInteractionPromptWidget::UHeistInteractionPromptWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, InteractionKeyLabel(NSLOCTEXT("HeistInteraction", "DefaultInteractionKey", "E"))
{
}

#pragma endregion

#pragma region Lifecycle

void UHeistInteractionPromptWidget::NativeDestruct()
{
	if (IsValid(HUDViewModel))
	{
		HUDViewModel->GetPresentationChangedDelegate().RemoveAll(this);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PresentationRefreshTimerHandle);
	}

	Super::NativeDestruct();
}

#pragma endregion

#pragma region Presentation

void UHeistInteractionPromptWidget::SetupInteractionPresentation(
	UHeistInteractionComponent* InInteractionComponent,
	UHeistHUDViewModel* InHUDViewModel)
{
	if (HUDViewModel != InHUDViewModel && IsValid(HUDViewModel))
	{
		HUDViewModel->GetPresentationChangedDelegate().RemoveAll(this);
	}

	InteractionComponent = InInteractionComponent;
	HUDViewModel = InHUDViewModel;

	UE_LOG(
		LogHeistUI,
		Log,
		TEXT("[%s] Interaction presentation setup: InteractionComponent=%s HUDViewModel=%s PromptContainer=%s ActionProgressContainer=%s SelfPromptFallback=%s SelfActionFallback=%s"),
		*GetName(),
		*GetNameSafe(InteractionComponent.Get()),
		*GetNameSafe(HUDViewModel.Get()),
		IsValid(InteractionPromptContainer) ? TEXT("true") : TEXT("false"),
		IsValid(ActionProgressContainer) ? TEXT("true") : TEXT("false"),
		(!IsValid(InteractionPromptContainer)
			&& (IsValid(TargetText) || IsValid(KeyText) || IsValid(AvailabilityText))
			&& !(IsValid(ActionTypeText) || IsValid(ActionProgressBar) || IsValid(ActionRemainingText) || IsValid(CancelHintText)))
			? TEXT("true") : TEXT("false"),
		(!IsValid(ActionProgressContainer)
			&& (IsValid(ActionTypeText) || IsValid(ActionProgressBar) || IsValid(ActionRemainingText) || IsValid(CancelHintText))
			&& !(IsValid(TargetText) || IsValid(KeyText) || IsValid(AvailabilityText)))
			? TEXT("true") : TEXT("false"));

	if (IsValid(HUDViewModel))
	{
		HUDViewModel->GetPresentationChangedDelegate().RemoveAll(this);
		HUDViewModel->GetPresentationChangedDelegate().AddUObject(
			this,
			&UHeistInteractionPromptWidget::RefreshPresentation);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PresentationRefreshTimerHandle);
		World->GetTimerManager().SetTimer(
			PresentationRefreshTimerHandle,
			this,
			&UHeistInteractionPromptWidget::RefreshPresentation,
			0.1f,
			true);
	}

	RefreshPresentation();
}

void UHeistInteractionPromptWidget::RefreshPresentation()
{
	const bool bActionActive = IsValid(HUDViewModel)
		&& (HUDViewModel->IsEscapeCastActive() || HUDViewModel->IsTrapPlacementCastActive());

	RefreshInteractionPrompt(bActionActive);
	RefreshActionProgress();
}

void UHeistInteractionPromptWidget::RefreshInteractionPrompt(const bool bActionActive)
{
	AActor* TargetActor = nullptr;
	bool bAvailable = false;
	if (IsValid(InteractionComponent))
	{
		InteractionComponent->RefreshInteractionTarget();
		TargetActor = InteractionComponent->GetCurrentInteractionTarget();
		bAvailable = IsValid(TargetActor) && InteractionComponent->HasValidInteractionTarget();
	}

	const bool bVisible = IsValid(TargetActor) && !bActionActive;
	if (IsValid(InteractionPromptContainer))
	{
		InteractionPromptContainer->SetVisibility(
			bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	else if (!IsValid(InteractionPromptContainer)
		&& (IsValid(TargetText) || IsValid(KeyText) || IsValid(AvailabilityText))
		&& !(IsValid(ActionTypeText) || IsValid(ActionProgressBar) || IsValid(ActionRemainingText) || IsValid(CancelHintText)))
	{
		const ESlateVisibility FallbackVisibility = bVisible
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed;
		SetVisibility(FallbackVisibility);
		if (UWidget* RootWidget = GetRootWidget())
		{
			RootWidget->SetVisibility(FallbackVisibility);
		}
	}
	if (IsValid(TargetText))
	{
		TargetText->SetText(ResolveTargetLabel(TargetActor));
	}
	if (IsValid(KeyText))
	{
		KeyText->SetText(InteractionKeyLabel);
	}
	if (IsValid(AvailabilityText))
	{
		AvailabilityText->SetText(
			bAvailable
				? NSLOCTEXT("HeistInteraction", "Available", "AVAILABLE")
				: NSLOCTEXT("HeistInteraction", "Unavailable", "UNAVAILABLE"));
	}
}

void UHeistInteractionPromptWidget::RefreshActionProgress()
{
	const bool bEscapeActive = IsValid(HUDViewModel) && HUDViewModel->IsEscapeCastActive();
	const bool bTrapActive = IsValid(HUDViewModel) && HUDViewModel->IsTrapPlacementCastActive();
	const bool bActionActive = bEscapeActive || bTrapActive;
	const FName ActionType = bEscapeActive
		? FName(TEXT("Escape"))
		: (bTrapActive ? FName(TEXT("TrapPlacement")) : NAME_None);
	const float EndServerTime = bEscapeActive
		? HUDViewModel->GetEscapeCastEndServerTime()
		: (bTrapActive ? HUDViewModel->GetTrapPlacementCastEndServerTime() : 0.0f);
	const float ServerTime = GetServerWorldTimeSeconds();

	if (bActionActive
		&& (TrackedActionType != ActionType
			|| !FMath::IsNearlyEqual(TrackedActionEndServerTime, EndServerTime)))
	{
		TrackedActionType = ActionType;
		TrackedActionEndServerTime = EndServerTime;
		TrackedActionDuration = FMath::Max(EndServerTime - ServerTime, KINDA_SMALL_NUMBER);
	}
	else if (!bActionActive)
	{
		TrackedActionType = NAME_None;
		TrackedActionEndServerTime = 0.0f;
		TrackedActionDuration = 0.0f;
	}

	const float RemainingSeconds = bActionActive
		? FMath::Max(EndServerTime - ServerTime, 0.0f)
		: 0.0f;
	const float CompletionRatio = bActionActive && TrackedActionDuration > KINDA_SMALL_NUMBER
		? 1.0f - FMath::Clamp(RemainingSeconds / TrackedActionDuration, 0.0f, 1.0f)
		: 0.0f;

	if (IsValid(ActionProgressContainer))
	{
		ActionProgressContainer->SetVisibility(
			bActionActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	else if (!IsValid(ActionProgressContainer)
		&& (IsValid(ActionTypeText) || IsValid(ActionProgressBar) || IsValid(ActionRemainingText) || IsValid(CancelHintText))
		&& !(IsValid(TargetText) || IsValid(KeyText) || IsValid(AvailabilityText)))
	{
		const ESlateVisibility FallbackVisibility = bActionActive
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed;
		SetVisibility(FallbackVisibility);
		if (UWidget* RootWidget = GetRootWidget())
		{
			RootWidget->SetVisibility(FallbackVisibility);
		}
	}
	if (IsValid(ActionTypeText))
	{
		ActionTypeText->SetText(
			bEscapeActive
				? NSLOCTEXT("HeistInteraction", "EscapeAction", "ESCAPING")
				: (bTrapActive
					? NSLOCTEXT("HeistInteraction", "TrapAction", "PLACING TRAP")
					: FText::GetEmpty()));
	}
	if (IsValid(ActionProgressBar))
	{
		ActionProgressBar->SetPercent(CompletionRatio);
	}
	if (IsValid(ActionRemainingText))
	{
		FNumberFormattingOptions Formatting;
		Formatting.MinimumFractionalDigits = 1;
		Formatting.MaximumFractionalDigits = 1;
		ActionRemainingText->SetText(FText::Format(
			NSLOCTEXT("HeistInteraction", "RemainingFormat", "{0}s"),
			FText::AsNumber(RemainingSeconds, &Formatting)));
	}
	if (IsValid(CancelHintText))
	{
		CancelHintText->SetText(
			bEscapeActive
				? NSLOCTEXT("HeistInteraction", "EscapeCancelHint", "MOVE OR TAKE DAMAGE TO CANCEL")
				: (bTrapActive
					? NSLOCTEXT("HeistInteraction", "TrapCancelHint", "MOVE TO CANCEL")
					: FText::GetEmpty()));
	}
}

FText UHeistInteractionPromptWidget::ResolveTargetLabel(const AActor* TargetActor) const
{
	if (!IsValid(TargetActor))
	{
		return FText::GetEmpty();
	}

	if (const AHeistLootActor* LootActor = Cast<AHeistLootActor>(TargetActor))
	{
		const FName LootRowId = LootActor->GetLootRowId();
		return LootRowId.IsNone()
			? NSLOCTEXT("HeistInteraction", "LootTarget", "LOOT")
			: FText::FromName(LootRowId);
	}

	if (Cast<AHeistVentActor>(TargetActor) != nullptr)
	{
		return NSLOCTEXT("HeistInteraction", "VentTarget", "ESCAPE VENT");
	}

	return FText::FromString(TargetActor->GetClass()->GetName());
}

float UHeistInteractionPromptWidget::GetServerWorldTimeSeconds() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return IsValid(GameState)
		? GameState->GetServerWorldTimeSeconds()
		: (World ? World->GetTimeSeconds() : 0.0f);
}

#pragma endregion
