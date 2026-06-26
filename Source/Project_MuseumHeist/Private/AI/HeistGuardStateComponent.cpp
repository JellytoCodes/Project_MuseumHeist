#include "AI/HeistGuardStateComponent.h"

#include "Debug/HeistDebugFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UHeistGuardStateComponent::UHeistGuardStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

bool UHeistGuardStateComponent::ApplyStun(const float DurationSeconds)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || DurationSeconds <= 0.0f)
	{
		return false;
	}

	GuardState = EHeistGuardState::Stunned;
	OwnerActor->ForceNetUpdate();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StunTimerHandle);
		World->GetTimerManager().SetTimer(
			StunTimerHandle,
			this,
			&UHeistGuardStateComponent::ClearStun,
			DurationSeconds,
			false);
	}

	UHeistDebugFunctionLibrary::DebugGuardStunApplied(this, OwnerActor, DurationSeconds);
	return true;
}

EHeistGuardState UHeistGuardStateComponent::GetGuardState() const
{
	return GuardState;
}

void UHeistGuardStateComponent::ClearStun()
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || GuardState != EHeistGuardState::Stunned)
	{
		return;
	}

	GuardState = EHeistGuardState::Investigate;
	OwnerActor->ForceNetUpdate();
	UHeistDebugFunctionLibrary::DebugGuardStunCleared(this, OwnerActor, GuardState);
}

void UHeistGuardStateComponent::OnRep_GuardState()
{
	UHeistDebugFunctionLibrary::DebugGuardStateReplicated(this, GetOwner(), GuardState);
}

void UHeistGuardStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UHeistGuardStateComponent, GuardState);
}
