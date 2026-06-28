#include "AI/HeistGuardStateComponent.h"

#include "Debug/HeistDebugFunctionLibrary.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "Inventory/HeistItemDataTypes.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#pragma region Construction

UHeistGuardStateComponent::UHeistGuardStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

#pragma endregion

#pragma region Lifecycle

void UHeistGuardStateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearStateTimer();
	Super::EndPlay(EndPlayReason);
}

#pragma endregion

#pragma region State

bool UHeistGuardStateComponent::EnterPatrol()
{
	const TObjectPtr<AActor> PreviousTarget = ChaseTarget;
	const FVector PreviousFocusLocation = StateFocusLocation;
	ChaseTarget = nullptr;
	StateFocusLocation = FVector::ZeroVector;
	if (CommitState(EHeistGuardState::Patrol))
	{
		return true;
	}

	ChaseTarget = PreviousTarget;
	StateFocusLocation = PreviousFocusLocation;
	return false;
}

bool UHeistGuardStateComponent::EnterChasePlayer(AActor* TargetActor)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor)
		|| !OwnerActor->HasAuthority()
		|| !IsValid(TargetActor)
		|| TargetActor == OwnerActor)
	{
		UHeistDebugFunctionLibrary::DebugGuardStateRequestRejected(
			this,
			OwnerActor,
			EHeistGuardState::ChasePlayer,
			TEXT("InvalidChaseTarget"));
		return false;
	}

	const TObjectPtr<AActor> PreviousTarget = ChaseTarget;
	const FVector PreviousFocusLocation = StateFocusLocation;
	ChaseTarget = TargetActor;
	StateFocusLocation = TargetActor->GetActorLocation();
	if (CommitState(EHeistGuardState::ChasePlayer))
	{
		return true;
	}

	ChaseTarget = PreviousTarget;
	StateFocusLocation = PreviousFocusLocation;
	return false;
}

bool UHeistGuardStateComponent::RefreshChaseTargetLocation()
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor)
		|| !OwnerActor->HasAuthority()
		|| GuardState != EHeistGuardState::ChasePlayer
		|| !IsValid(ChaseTarget))
	{
		return false;
	}

	StateFocusLocation = ChaseTarget->GetActorLocation();
	return true;
}

bool UHeistGuardStateComponent::EnterInvestigateNoise(
	const FVector& InvestigateLocation,
	const float DurationSeconds)
{
	if (InvestigateLocation.ContainsNaN() || DurationSeconds < 0.0f)
	{
		UHeistDebugFunctionLibrary::DebugGuardStateRequestRejected(
			this,
			GetOwner(),
			EHeistGuardState::InvestigateNoise,
			TEXT("InvalidInvestigateRequest"));
		return false;
	}

	const TObjectPtr<AActor> PreviousTarget = ChaseTarget;
	const FVector PreviousFocusLocation = StateFocusLocation;
	ChaseTarget = nullptr;
	StateFocusLocation = InvestigateLocation;
	if (CommitState(EHeistGuardState::InvestigateNoise, DurationSeconds))
	{
		return true;
	}

	ChaseTarget = PreviousTarget;
	StateFocusLocation = PreviousFocusLocation;
	return false;
}

bool UHeistGuardStateComponent::EnterSearchLastKnownLocation(const FVector& SearchLocation)
{
	if (SearchLocation.ContainsNaN())
	{
		UHeistDebugFunctionLibrary::DebugGuardStateRequestRejected(
			this,
			GetOwner(),
			EHeistGuardState::SearchLastKnownLocation,
			TEXT("InvalidSearchLocation"));
		return false;
	}

	const TObjectPtr<AActor> PreviousTarget = ChaseTarget;
	const FVector PreviousFocusLocation = StateFocusLocation;
	ChaseTarget = nullptr;
	StateFocusLocation = SearchLocation;
	if (CommitState(EHeistGuardState::SearchLastKnownLocation))
	{
		return true;
	}

	ChaseTarget = PreviousTarget;
	StateFocusLocation = PreviousFocusLocation;
	return false;
}

bool UHeistGuardStateComponent::EnterReturnToPatrol()
{
	const TObjectPtr<AActor> PreviousTarget = ChaseTarget;
	ChaseTarget = nullptr;
	if (CommitState(EHeistGuardState::ReturnToPatrol))
	{
		return true;
	}

	ChaseTarget = PreviousTarget;
	return false;
}

bool UHeistGuardStateComponent::SetDisabled(const bool bDisabled)
{
	if (!bDisabled && GuardState == EHeistGuardState::Disabled)
	{
		return CommitState(EHeistGuardState::Patrol, 0.0f, true);
	}

	if (bDisabled)
	{
		ChaseTarget = nullptr;
		StateFocusLocation = FVector::ZeroVector;
		return CommitState(EHeistGuardState::Disabled, 0.0f, true);
	}

	return GuardState != EHeistGuardState::Disabled;
}

bool UHeistGuardStateComponent::ApplyStun(const float DurationSeconds)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || DurationSeconds <= 0.0f)
	{
		UHeistDebugFunctionLibrary::DebugGuardStateRequestRejected(
			this,
			OwnerActor,
			EHeistGuardState::Stunned,
			TEXT("InvalidStunRequest"));
		return false;
	}

	if (IsValid(ChaseTarget))
	{
		StateFocusLocation = ChaseTarget->GetActorLocation();
	}
	ChaseTarget = nullptr;

	if (!CommitState(EHeistGuardState::Stunned, DurationSeconds, true))
	{
		return false;
	}

	UHeistDebugFunctionLibrary::DebugGuardStunApplied(this, OwnerActor, DurationSeconds);
	return true;
}

EHeistGuardState UHeistGuardStateComponent::GetGuardState() const
{
	return GuardState;
}

float UHeistGuardStateComponent::GetStateEndServerTime() const
{
	return StateEndServerTime;
}

FVector UHeistGuardStateComponent::GetStateFocusLocation() const
{
	return StateFocusLocation;
}

AActor* UHeistGuardStateComponent::GetChaseTarget() const
{
	return ChaseTarget.Get();
}

FHeistGuardStateChanged& UHeistGuardStateComponent::GetGuardStateChangedDelegate()
{
	return GuardStateChangedDelegate;
}

void UHeistGuardStateComponent::ConfigureGuardProfile(const FHeistGuardDataRow& GuardData)
{
	InvestigateDuration = FMath::Max(0.0f, GuardData.InvestigateDuration);
}

bool UHeistGuardStateComponent::CommitState(
	const EHeistGuardState NewState,
	const float DurationSeconds,
	const bool bBypassPriority)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		UHeistDebugFunctionLibrary::DebugGuardStateRequestRejected(
			this,
			OwnerActor,
			NewState,
			TEXT("NotAuthority"));
		return false;
	}

	if (!bBypassPriority && !CanEnterState(NewState))
	{
		UHeistDebugFunctionLibrary::DebugGuardStateRequestRejected(
			this,
			OwnerActor,
			NewState,
			TEXT("TransitionPriority"));
		return false;
	}

	const EHeistGuardState PreviousState = GuardState;
	ClearStateTimer();
	GuardState = NewState;
	StateEndServerTime = 0.0f;

	const float SafeDuration = FMath::Max(0.0f, DurationSeconds);
	if (SafeDuration > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			StateEndServerTime = World->GetGameState()
				? World->GetGameState()->GetServerWorldTimeSeconds() + SafeDuration
				: World->GetTimeSeconds() + SafeDuration;
			World->GetTimerManager().SetTimer(
				StateTimerHandle,
				this,
				&UHeistGuardStateComponent::HandleTimedStateExpired,
				SafeDuration,
				false);
		}
	}

	OwnerActor->ForceNetUpdate();
	GuardStateChangedDelegate.Broadcast(PreviousState, GuardState);
	UHeistDebugFunctionLibrary::DebugGuardStateChanged(
		this,
		OwnerActor,
		PreviousState,
		GuardState,
		StateEndServerTime);
	return true;
}

bool UHeistGuardStateComponent::CanEnterState(const EHeistGuardState NewState) const
{
	if (NewState == GuardState)
	{
		return true;
	}

	if (GuardState == EHeistGuardState::Disabled)
	{
		return false;
	}

	if (GuardState == EHeistGuardState::Stunned)
	{
		return false;
	}

	if (GuardState == EHeistGuardState::ChasePlayer
		&& NewState == EHeistGuardState::InvestigateNoise)
	{
		return false;
	}

	return true;
}

void UHeistGuardStateComponent::HandleTimedStateExpired()
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return;
	}

	const EHeistGuardState ExpiredState = GuardState;
	if (ExpiredState == EHeistGuardState::Stunned)
	{
		CommitState(
			EHeistGuardState::InvestigateNoise,
			InvestigateDuration,
			true);
		UHeistDebugFunctionLibrary::DebugGuardStunCleared(this, OwnerActor, GuardState);
		return;
	}

	if (ExpiredState == EHeistGuardState::InvestigateNoise)
	{
		EnterPatrol();
	}
}

void UHeistGuardStateComponent::ClearStateTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StateTimerHandle);
	}
}

void UHeistGuardStateComponent::OnRep_GuardState(const EHeistGuardState PreviousState)
{
	GuardStateChangedDelegate.Broadcast(PreviousState, GuardState);
	UHeistDebugFunctionLibrary::DebugGuardStateReplicated(this, GetOwner(), GuardState);
}

#pragma endregion

#pragma region Replication

void UHeistGuardStateComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UHeistGuardStateComponent, GuardState);
	DOREPLIFETIME(UHeistGuardStateComponent, StateEndServerTime);
}

#pragma endregion
