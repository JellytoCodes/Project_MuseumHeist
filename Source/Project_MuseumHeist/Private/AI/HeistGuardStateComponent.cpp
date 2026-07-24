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
	const float PreviousPendingDuration = PendingInvestigateDuration;
	ChaseTarget = nullptr;
	StateFocusLocation = FVector::ZeroVector;
	PendingInvestigateDuration = 0.0f;
	if (CommitState(EHeistGuardState::Patrol))
	{
		return true;
	}

	ChaseTarget = PreviousTarget;
	StateFocusLocation = PreviousFocusLocation;
	PendingInvestigateDuration = PreviousPendingDuration;
	return false;
}

bool UHeistGuardStateComponent::EnterChasePlayer(AActor* TargetActor)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || !IsValid(TargetActor) || TargetActor == OwnerActor)
	{
		UHeistDebugFunctionLibrary::DebugGuardStateRequestRejected(this, OwnerActor, EHeistGuardState::ChasePlayer, TEXT("InvalidChaseTarget"));
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
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || GuardState != EHeistGuardState::ChasePlayer || !IsValid(ChaseTarget))
	{
		return false;
	}

	StateFocusLocation = ChaseTarget->GetActorLocation();
	return true;
}

bool UHeistGuardStateComponent::EnterInvestigateNoise(const FVector& InvestigateLocation, const float DurationSeconds)
{
	if (InvestigateLocation.ContainsNaN() || DurationSeconds < 0.0f)
	{
		UHeistDebugFunctionLibrary::DebugGuardStateRequestRejected(this, GetOwner(), EHeistGuardState::InvestigateNoise, TEXT("InvalidInvestigateRequest"));
		return false;
	}

	const TObjectPtr<AActor> PreviousTarget = ChaseTarget;
	const FVector PreviousFocusLocation = StateFocusLocation;
	const float PreviousPendingDuration = PendingInvestigateDuration;
	ChaseTarget = nullptr;
	StateFocusLocation = InvestigateLocation;
	PendingInvestigateDuration = DurationSeconds;
	if (CommitState(EHeistGuardState::InvestigateNoise))
	{
		return true;
	}

	ChaseTarget = PreviousTarget;
	StateFocusLocation = PreviousFocusLocation;
	PendingInvestigateDuration = PreviousPendingDuration;
	return false;
}

bool UHeistGuardStateComponent::StartInvestigateConfirmationTimer()
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || GuardState != EHeistGuardState::InvestigateNoise || PendingInvestigateDuration <= 0.0f)
	{
		UHeistDebugFunctionLibrary::DebugGuardStateRequestRejected(this, OwnerActor, EHeistGuardState::InvestigateNoise, TEXT("InvalidConfirmationTimer"));
		return false;
	}

	return StartStateTimer(PendingInvestigateDuration);
}

bool UHeistGuardStateComponent::EnterInspectExhibit(const FVector& ExhibitLocation)
{
	if (ExhibitLocation.ContainsNaN())
	{
		UHeistDebugFunctionLibrary::DebugGuardStateRequestRejected(this, GetOwner(), EHeistGuardState::InspectExhibit, TEXT("InvalidExhibitLocation"));
		return false;
	}

	const TObjectPtr<AActor> PreviousTarget = ChaseTarget;
	const FVector PreviousFocusLocation = StateFocusLocation;
	ChaseTarget = nullptr;
	StateFocusLocation = ExhibitLocation;
	if (CommitState(EHeistGuardState::InspectExhibit))
	{
		return true;
	}

	ChaseTarget = PreviousTarget;
	StateFocusLocation = PreviousFocusLocation;
	return false;
}

bool UHeistGuardStateComponent::StartInspectExhibitCast(const float DurationSeconds)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || GuardState != EHeistGuardState::InspectExhibit || DurationSeconds <= 0.0f)
	{
		UHeistDebugFunctionLibrary::DebugGuardStateRequestRejected(this, OwnerActor, EHeistGuardState::InspectExhibit, TEXT("InvalidInspectionCast"));
		return false;
	}

	return StartStateTimer(DurationSeconds);
}

bool UHeistGuardStateComponent::EnterSearchLastKnownLocation(const FVector& SearchLocation)
{
	if (SearchLocation.ContainsNaN())
	{
		UHeistDebugFunctionLibrary::DebugGuardStateRequestRejected(this, GetOwner(), EHeistGuardState::SearchLastKnownLocation, TEXT("InvalidSearchLocation"));
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

bool UHeistGuardStateComponent::StartSearchTimer()
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || GuardState != EHeistGuardState::SearchLastKnownLocation || SearchDuration <= 0.0f)
	{
		UHeistDebugFunctionLibrary::DebugGuardStateRequestRejected(this, OwnerActor, EHeistGuardState::SearchLastKnownLocation, TEXT("InvalidSearchTimer"));
		return false;
	}

	return StartStateTimer(SearchDuration);
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
		UHeistDebugFunctionLibrary::DebugGuardStateRequestRejected(this, OwnerActor, EHeistGuardState::Stunned, TEXT("InvalidStunRequest"));
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

float UHeistGuardStateComponent::GetInvestigateConfirmationDuration() const
{
	return PendingInvestigateDuration;
}

float UHeistGuardStateComponent::GetSearchDuration() const
{
	return SearchDuration;
}

void UHeistGuardStateComponent::SetAlertSearchDurationMultiplier(const float Multiplier)
{
	AlertSearchDurationMultiplier = FMath::Max(0.0f, FMath::IsFinite(Multiplier) ? Multiplier : 1.0f);
	SearchDuration = BaseSearchDuration * AlertSearchDurationMultiplier;
	AActor* OwnerActor = GetOwner();
	if (IsValid(OwnerActor) && OwnerActor->HasAuthority() && GuardState == EHeistGuardState::SearchLastKnownLocation && GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(StateTimerHandle))
	{
		StartStateTimer(SearchDuration);
	}
}

float UHeistGuardStateComponent::GetAlertSearchDurationMultiplier() const
{
	return AlertSearchDurationMultiplier;
}

FHeistGuardStateChanged& UHeistGuardStateComponent::GetGuardStateChangedDelegate()
{
	return GuardStateChangedDelegate;
}

UHeistGuardStateComponent::FHeistInspectExhibitCastExpired& UHeistGuardStateComponent::GetInspectExhibitCastExpiredDelegate()
{
	return InspectExhibitCastExpiredDelegate;
}

void UHeistGuardStateComponent::ConfigureGuardProfile(const FHeistGuardDataRow& GuardData)
{
	InvestigateDuration = FMath::Max(0.0f, GuardData.InvestigateDuration);
	BaseSearchDuration = FMath::Max(0.0f, GuardData.SearchDuration);
	SearchDuration = BaseSearchDuration * AlertSearchDurationMultiplier;
}

bool UHeistGuardStateComponent::CommitState(const EHeistGuardState NewState, const float DurationSeconds, const bool bBypassPriority)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		UHeistDebugFunctionLibrary::DebugGuardStateRequestRejected(this, OwnerActor, NewState, TEXT("NotAuthority"));
		return false;
	}

	if (!bBypassPriority && !CanEnterState(NewState))
	{
		UHeistDebugFunctionLibrary::DebugGuardStateRequestRejected(this, OwnerActor, NewState, TEXT("TransitionPriority"));
		return false;
	}

	const EHeistGuardState PreviousState = GuardState;
	ClearStateTimer();
	GuardState = NewState;
	StateEndServerTime = 0.0f;

	const float SafeDuration = FMath::Max(0.0f, DurationSeconds);
	if (SafeDuration > 0.0f)
	{
		StartStateTimer(SafeDuration);
	}

	OwnerActor->ForceNetUpdate();
	GuardStateChangedDelegate.Broadcast(PreviousState, GuardState);
	UHeistDebugFunctionLibrary::DebugGuardStateChanged(this, OwnerActor, PreviousState, GuardState, StateEndServerTime);
	return true;
}

bool UHeistGuardStateComponent::StartStateTimer(const float DurationSeconds)
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	const float SafeDuration = FMath::Max(0.0f, DurationSeconds);
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || !IsValid(World) || SafeDuration <= 0.0f)
	{
		return false;
	}

	ClearStateTimer();
	StateEndServerTime = World->GetGameState() ? World->GetGameState()->GetServerWorldTimeSeconds() + SafeDuration : World->GetTimeSeconds() + SafeDuration;
	World->GetTimerManager().SetTimer(StateTimerHandle, this, &UHeistGuardStateComponent::HandleTimedStateExpired, SafeDuration, false);
	OwnerActor->ForceNetUpdate();
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

	if (GuardState == EHeistGuardState::ChasePlayer && (NewState == EHeistGuardState::InvestigateNoise || NewState == EHeistGuardState::InspectExhibit))
	{
		return false;
	}

	if (GuardState == EHeistGuardState::InspectExhibit && NewState == EHeistGuardState::InvestigateNoise)
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
		PendingInvestigateDuration = InvestigateDuration;
		CommitState(EHeistGuardState::InvestigateNoise, 0.0f, true);
		UHeistDebugFunctionLibrary::DebugGuardStunCleared(this, OwnerActor, GuardState);
		return;
	}

	if (ExpiredState == EHeistGuardState::InvestigateNoise)
	{
		EnterPatrol();
		return;
	}

	if (ExpiredState == EHeistGuardState::InspectExhibit)
	{
		StateEndServerTime = 0.0f;
		InspectExhibitCastExpiredDelegate.Broadcast();
		return;
	}

	if (ExpiredState == EHeistGuardState::SearchLastKnownLocation)
	{
		EnterReturnToPatrol();
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

void UHeistGuardStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UHeistGuardStateComponent, GuardState);
	DOREPLIFETIME(UHeistGuardStateComponent, StateEndServerTime);
	DOREPLIFETIME(UHeistGuardStateComponent, StateFocusLocation);
}

#pragma endregion
