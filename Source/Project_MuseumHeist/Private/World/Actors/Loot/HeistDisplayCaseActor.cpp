#include "World/Actors/Loot/HeistDisplayCaseActor.h"

#include "Core/HeistLogChannels.h"
#include "Net/UnrealNetwork.h"

AHeistDisplayCaseActor::AHeistDisplayCaseActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

#pragma region StateMachine

EHeistDisplayCaseState AHeistDisplayCaseActor::GetDisplayCaseState() const
{
	return DisplayCaseState;
}

bool AHeistDisplayCaseActor::CanTransitionToDisplayCaseState(const EHeistDisplayCaseState NewState) const
{
	EHeistDisplayCaseState ExpectedNextState = DisplayCaseState;
	return TryGetNextDisplayCaseState(DisplayCaseState, ExpectedNextState)
		&& ExpectedNextState == NewState;
}

bool AHeistDisplayCaseActor::TryTransitionToDisplayCaseState(const EHeistDisplayCaseState NewState)
{
	if (!HasAuthority())
	{
		UE_LOG(
			LogHeistNetwork,
			Warning,
			TEXT("Display case transition rejected: Case=%s Current=%s Requested=%s Reason=NotAuthority"),
			*GetNameSafe(this),
			*UEnum::GetValueAsString(DisplayCaseState),
			*UEnum::GetValueAsString(NewState));
		return false;
	}

	if (!CanTransitionToDisplayCaseState(NewState))
	{
		UE_LOG(
			LogHeistNetwork,
			Warning,
			TEXT("Display case transition rejected: Case=%s Current=%s Requested=%s Reason=IllegalTransition"),
			*GetNameSafe(this),
			*UEnum::GetValueAsString(DisplayCaseState),
			*UEnum::GetValueAsString(NewState));
		return false;
	}

	const EHeistDisplayCaseState PreviousState = DisplayCaseState;
	DisplayCaseState = NewState;
	HandleDisplayCaseStateChanged(PreviousState);
	ForceNetUpdate();
	return true;
}

bool AHeistDisplayCaseActor::TryAdvanceDisplayCaseState()
{
	EHeistDisplayCaseState NextState = DisplayCaseState;
	return TryGetNextDisplayCaseState(DisplayCaseState, NextState)
		&& TryTransitionToDisplayCaseState(NextState);
}

void AHeistDisplayCaseActor::OnRep_DisplayCaseState(const EHeistDisplayCaseState PreviousState)
{
	HandleDisplayCaseStateChanged(PreviousState);
}

bool AHeistDisplayCaseActor::TryGetNextDisplayCaseState(
	const EHeistDisplayCaseState CurrentState,
	EHeistDisplayCaseState& OutNextState)
{
	switch (CurrentState)
	{
	case EHeistDisplayCaseState::Secured:
		OutNextState = EHeistDisplayCaseState::Observed;
		return true;
	case EHeistDisplayCaseState::Observed:
		OutNextState = EHeistDisplayCaseState::ForgeryInProgress;
		return true;
	case EHeistDisplayCaseState::ForgeryInProgress:
		OutNextState = EHeistDisplayCaseState::ReplicaReady;
		return true;
	case EHeistDisplayCaseState::ReplicaReady:
		OutNextState = EHeistDisplayCaseState::ReplicaPlaced;
		return true;
	case EHeistDisplayCaseState::ReplicaPlaced:
		OutNextState = EHeistDisplayCaseState::OriginalAvailable;
		return true;
	case EHeistDisplayCaseState::OriginalAvailable:
		OutNextState = EHeistDisplayCaseState::OriginalRemoved;
		return true;
	default:
		return false;
	}
}

void AHeistDisplayCaseActor::HandleDisplayCaseStateChanged(const EHeistDisplayCaseState PreviousState)
{
	UE_LOG(
		LogHeistNetwork,
		Log,
		TEXT("Display case state changed: Case=%s Previous=%s New=%s Authority=%s"),
		*GetNameSafe(this),
		*UEnum::GetValueAsString(PreviousState),
		*UEnum::GetValueAsString(DisplayCaseState),
		HasAuthority() ? TEXT("true") : TEXT("false"));

	OnDisplayCaseStateChanged.Broadcast(PreviousState, DisplayCaseState);
}

#pragma endregion

#pragma region Replication

void AHeistDisplayCaseActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeistDisplayCaseActor, DisplayCaseState);
}

#pragma endregion
