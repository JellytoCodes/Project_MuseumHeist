#include "Character/Components/HeistStatusComponent.h"

#include "Debug/HeistDebugFunctionLibrary.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

#pragma region Construction

UHeistStatusComponent::UHeistStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

#pragma endregion

#pragma region StatusQueries

bool UHeistStatusComponent::HasStatusTag(const FGameplayTag StateTag) const
{
	return FindStatusTag(StateTag) != nullptr;
}

const TArray<FHeistTimedTagState>& UHeistStatusComponent::GetStatusTags() const
{
	return StatusTags;
}

FHeistStatusTagsChanged& UHeistStatusComponent::GetStatusTagsChangedDelegate()
{
	return StatusTagsChangedDelegate;
}

#pragma endregion

#pragma region StatusMutation

bool UHeistStatusComponent::ApplyTimedStatusTag(const FGameplayTag StateTag, const float DurationSeconds)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !StateTag.IsValid() || DurationSeconds <= 0.0f)
	{
		return false;
	}

	FHeistTimedTagState* ExistingState = FindMutableStatusTag(StateTag);
	if (ExistingState == nullptr)
	{
		ExistingState = &StatusTags.AddDefaulted_GetRef();
		ExistingState->StateTag = StateTag;
	}

	ExistingState->EndServerTime = GetStatusEndServerTime(DurationSeconds);
	RefreshStatusTagTimer(*ExistingState);
	GetOwner()->ForceNetUpdate();
	StatusTagsChangedDelegate.Broadcast(StatusTags);

	UHeistDebugFunctionLibrary::DebugStatusTagApplied(this, StateTag, ExistingState->EndServerTime);
	return true;
}

bool UHeistStatusComponent::ClearStatusTag(const FGameplayTag StateTag)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !StateTag.IsValid())
	{
		return false;
	}

	const int32 RemovedCount = StatusTags.RemoveAll(
		[StateTag](const FHeistTimedTagState& StatusTagState)
		{
			return StatusTagState.StateTag == StateTag;
		});

	if (RemovedCount <= 0)
	{
		return false;
	}

	ClearStatusTagTimer(StateTag);
	GetOwner()->ForceNetUpdate();
	StatusTagsChangedDelegate.Broadcast(StatusTags);

	UHeistDebugFunctionLibrary::DebugStatusTagCleared(this, StateTag);
	return true;
}

FHeistTimedTagState* UHeistStatusComponent::FindMutableStatusTag(const FGameplayTag StateTag)
{
	return StatusTags.FindByPredicate(
		[StateTag](const FHeistTimedTagState& StatusTagState)
		{
			return StatusTagState.StateTag == StateTag;
		});
}

const FHeistTimedTagState* UHeistStatusComponent::FindStatusTag(const FGameplayTag StateTag) const
{
	return StatusTags.FindByPredicate(
		[StateTag](const FHeistTimedTagState& StatusTagState)
		{
			return StatusTagState.StateTag == StateTag;
		});
}

void UHeistStatusComponent::RefreshStatusTagTimer(const FHeistTimedTagState& StatusTagState)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	ClearStatusTagTimer(StatusTagState.StateTag);

	const float RemainingSeconds = StatusTagState.EndServerTime - World->GetTimeSeconds();
	if (RemainingSeconds <= 0.0f)
	{
		ExpireStatusTag(StatusTagState.StateTag);
		return;
	}

	FTimerHandle& TimerHandle = StatusTagTimers.FindOrAdd(StatusTagState.StateTag);
	FTimerDelegate TimerDelegate = FTimerDelegate::CreateUObject(
		this,
		&UHeistStatusComponent::ExpireStatusTag,
		StatusTagState.StateTag);
	World->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, RemainingSeconds, false);
}

void UHeistStatusComponent::ClearStatusTagTimer(const FGameplayTag StateTag)
{
	FTimerHandle RemovedHandle;
	if (!StatusTagTimers.RemoveAndCopyValue(StateTag, RemovedHandle))
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RemovedHandle);
	}
}

void UHeistStatusComponent::ExpireStatusTag(const FGameplayTag StateTag)
{
	ClearStatusTag(StateTag);
}

float UHeistStatusComponent::GetStatusEndServerTime(const float DurationSeconds) const
{
	const UWorld* World = GetWorld();
	const float CurrentServerTime = IsValid(World) ? World->GetTimeSeconds() : 0.0f;
	return CurrentServerTime + FMath::Max(0.0f, DurationSeconds);
}

#pragma endregion

#pragma region Replication

void UHeistStatusComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UHeistStatusComponent, StatusTags);
}

void UHeistStatusComponent::OnRep_StatusTags()
{
	StatusTagsChangedDelegate.Broadcast(StatusTags);
	UHeistDebugFunctionLibrary::DebugStatusTagsReplicated(this, StatusTags);
}

#pragma endregion
