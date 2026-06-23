#include "Character/Components/HeistInteractionComponent.h"

#include "Character/HeistPlayerCharacter.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "World/HeistInteractable.h"

#pragma region Construction

UHeistInteractionComponent::UHeistInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

#pragma endregion

#pragma region Interaction

bool UHeistInteractionComponent::RefreshInteractionTarget()
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!IsValid(OwnerActor) || World == nullptr || !CanOwnerInteract())
	{
		CurrentInteractionTarget.Reset();
		return false;
	}

	AActor* ClosestTarget = nullptr;
	float ClosestDistanceSquared = FMath::Square(InteractionRange);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HeistInteractionTarget), false, OwnerActor);
	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByObjectType(
		OverlapResults,
		OwnerActor->GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(InteractionRange),
		QueryParams);

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* Candidate = OverlapResult.GetActor();
		if (!IsValid(Candidate) || Candidate == OwnerActor)
		{
			continue;
		}

		IHeistInteractable* Interactable = Cast<IHeistInteractable>(Candidate);
		if (Interactable == nullptr || !Interactable->CanInteract(OwnerActor))
		{
			continue;
		}

		const float CandidateDistanceSquared =
			FVector::DistSquared(OwnerActor->GetActorLocation(), Candidate->GetActorLocation());
		if (CandidateDistanceSquared > ClosestDistanceSquared)
		{
			continue;
		}

		const bool bIsCloser = CandidateDistanceSquared < ClosestDistanceSquared;
		const bool bWinsDistanceTie =
			ClosestTarget != nullptr
			&& FMath::IsNearlyEqual(CandidateDistanceSquared, ClosestDistanceSquared)
			&& Candidate->GetPathName().Compare(ClosestTarget->GetPathName()) < 0;

		if (ClosestTarget == nullptr || bIsCloser || bWinsDistanceTie)
		{
			ClosestTarget = Candidate;
			ClosestDistanceSquared = CandidateDistanceSquared;
		}
	}

	CurrentInteractionTarget = ClosestTarget;

	return ClosestTarget != nullptr;
}

AActor* UHeistInteractionComponent::GetCurrentInteractionTarget() const
{
	return HasValidInteractionTarget() ? CurrentInteractionTarget.Get() : nullptr;
}

bool UHeistInteractionComponent::HasValidInteractionTarget() const
{
	if (!CanOwnerInteract())
	{
		return false;
	}

	AActor* TargetActor = CurrentInteractionTarget.Get();
	if (!IsActorWithinInteractionRange(TargetActor))
	{
		return false;
	}

	const IHeistInteractable* Interactable = Cast<IHeistInteractable>(TargetActor);
	return Interactable != nullptr && Interactable->CanInteract(GetOwner());
}

float UHeistInteractionComponent::GetInteractionRange() const
{
	return InteractionRange;
}

bool UHeistInteractionComponent::IsActorWithinInteractionRange(const AActor* TargetActor) const
{
	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !IsValid(TargetActor) || !CanOwnerInteract())
	{
		return false;
	}

	return FVector::DistSquared(OwnerActor->GetActorLocation(), TargetActor->GetActorLocation())
		<= FMath::Square(InteractionRange);
}

bool UHeistInteractionComponent::CanOwnerInteract() const
{
	const AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	return IsValid(HeistCharacter) && HeistCharacter->CanPerformGameplayActions();
}

#pragma endregion
