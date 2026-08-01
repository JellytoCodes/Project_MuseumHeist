#include "Character/Components/HeistInteractionComponent.h"

#include "Character/HeistPlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Core/HeistCollisionChannels.h"
#include "Core/HeistLogChannels.h"
#include "World/Interaction/HeistInteractable.h"

#pragma region Construction

UHeistInteractionComponent::UHeistInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

#pragma endregion

#pragma region Lifecycle

void UHeistInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	checkf(IsValid(OwnerCharacter), TEXT("HeistInteractionComponent requires AHeistPlayerCharacter owner."));

	UCapsuleComponent* OwnerCapsule = OwnerCharacter->GetCapsuleComponent();
	checkf(IsValid(OwnerCapsule), TEXT("HeistInteractionComponent requires an owner CapsuleComponent."));
	OwnerCapsule->OnComponentBeginOverlap.AddDynamic(this, &UHeistInteractionComponent::HandleInteractionOverlapBegin);
	OwnerCapsule->OnComponentEndOverlap.AddDynamic(this, &UHeistInteractionComponent::HandleInteractionOverlapEnd);

	TArray<AActor*> InitiallyOverlappingActors;
	OwnerCapsule->GetOverlappingActors(InitiallyOverlappingActors);
	for (AActor* OverlappingActor : InitiallyOverlappingActors)
	{
		if (IsActorOverlappingInteractionArea(OverlappingActor))
		{
			OverlappingInteractionActors.Add(OverlappingActor);
		}
	}
	RefreshInteractionTarget();
}

#pragma endregion

#pragma region Interaction

bool UHeistInteractionComponent::RefreshInteractionTarget()
{
	AActor* PreviousTarget = CurrentInteractionTarget.Get();
	const bool bPreviousTargetAvailable = bCurrentTargetAvailable;
	if (!IsValid(OwnerCharacter) || !CanOwnerInteract())
	{
		ClearInteractionTarget(TEXT("OwnerCannotInteract"));
		return false;
	}

	AActor* BestTarget = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	TArray<TWeakObjectPtr<AActor>> StaleCandidates;
	for (const TWeakObjectPtr<AActor>& CandidatePtr : OverlappingInteractionActors)
	{
		AActor* Candidate = CandidatePtr.Get();
		if (!IsActorOverlappingInteractionArea(Candidate))
		{
			StaleCandidates.Add(CandidatePtr);
			continue;
		}

		IHeistInteractable* Interactable = Cast<IHeistInteractable>(Candidate);
		if (Interactable == nullptr || !Interactable->CanInteract(OwnerCharacter))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(OwnerCharacter->GetActorLocation(), Candidate->GetActorLocation());
		if (!IsValid(BestTarget) || DistanceSquared < BestDistanceSquared ||
			(FMath::IsNearlyEqual(DistanceSquared, BestDistanceSquared) && Candidate->GetName().Compare(BestTarget->GetName()) < 0))
		{
			BestTarget = Candidate;
			BestDistanceSquared = DistanceSquared;
		}
	}

	for (const TWeakObjectPtr<AActor>& StaleCandidate : StaleCandidates)
	{
		OverlappingInteractionActors.Remove(StaleCandidate);
	}

	CurrentInteractionTarget = BestTarget;
	bCurrentTargetAvailable = IsValid(BestTarget);
	if (PreviousTarget != BestTarget || bPreviousTargetAvailable != bCurrentTargetAvailable)
	{
		UE_LOG(LogHeistUI, Verbose, TEXT("[%s] Overlap interaction target changed: Previous=%s Target=%s Available=%s Candidates=%d Key=E"), *GetNameSafe(OwnerCharacter),
			   *GetNameSafe(PreviousTarget), *GetNameSafe(BestTarget), bCurrentTargetAvailable ? TEXT("true") : TEXT("false"), OverlappingInteractionActors.Num());
		InteractionTargetChangedDelegate.Broadcast(BestTarget, bCurrentTargetAvailable);
	}

	return bCurrentTargetAvailable;
}

AActor* UHeistInteractionComponent::GetCurrentInteractionTarget() const
{
	return HasValidInteractionTarget() ? CurrentInteractionTarget.Get() : nullptr;
}

bool UHeistInteractionComponent::HasValidInteractionTarget() const
{
	if (!bCurrentTargetAvailable || !CanOwnerInteract())
	{
		return false;
	}

	AActor* TargetActor = CurrentInteractionTarget.Get();
	IHeistInteractable* Interactable = Cast<IHeistInteractable>(TargetActor);
	return Interactable != nullptr && IsActorOverlappingInteractionArea(TargetActor) && Interactable->CanInteract(OwnerCharacter);
}

bool UHeistInteractionComponent::IsActorOverlappingInteractionArea(const AActor* TargetActor) const
{
	if (!IsValid(OwnerCharacter) || !IsValid(TargetActor))
	{
		return false;
	}

	const UCapsuleComponent* OwnerCapsule = OwnerCharacter->GetCapsuleComponent();
	if (!IsValid(OwnerCapsule))
	{
		return false;
	}

	TArray<UPrimitiveComponent*> OverlappingComponents;
	OwnerCapsule->GetOverlappingComponents(OverlappingComponents);
	return OverlappingComponents.ContainsByPredicate(
		[this, TargetActor](const UPrimitiveComponent* Component) { return IsInteractionCollisionComponent(Component, TargetActor); });
}

FHeistInteractionTargetChanged& UHeistInteractionComponent::GetInteractionTargetChangedDelegate()
{
	return InteractionTargetChangedDelegate;
}

bool UHeistInteractionComponent::IsInteractionCollisionComponent(const UPrimitiveComponent* Component, const AActor* ExpectedOwner) const
{
	return IsValid(Component) && IsValid(ExpectedOwner) && Component->GetOwner() == ExpectedOwner &&
		Component->GetCollisionObjectType() == HeistCollisionChannels::Interactable;
}

void UHeistInteractionComponent::ClearInteractionTarget(const TCHAR* Reason)
{
	AActor* PreviousTarget = CurrentInteractionTarget.Get();
	CurrentInteractionTarget.Reset();
	bCurrentTargetAvailable = false;

	if (IsValid(PreviousTarget))
	{
		UE_LOG(LogHeistUI, Verbose, TEXT("[%s] Overlap interaction target cleared: PreviousTarget=%s Reason=%s"), *GetNameSafe(GetOwner()), *GetNameSafe(PreviousTarget), Reason);
	}

	if (IsValid(PreviousTarget))
	{
		InteractionTargetChangedDelegate.Broadcast(nullptr, false);
	}
}

void UHeistInteractionComponent::HandleInteractionOverlapBegin(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32, bool, const FHitResult&)
{
	if (OtherActor == OwnerCharacter.Get() || !IsInteractionCollisionComponent(OtherComponent, OtherActor) || Cast<IHeistInteractable>(OtherActor) == nullptr)
	{
		return;
	}

	OverlappingInteractionActors.Add(OtherActor);
	RefreshInteractionTarget();
}

void UHeistInteractionComponent::HandleInteractionOverlapEnd(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32)
{
	if (IsValid(OtherActor) && IsActorOverlappingInteractionArea(OtherActor))
	{
		return;
	}

	if (IsValid(OtherActor))
	{
		OverlappingInteractionActors.Remove(OtherActor);
	}
	RefreshInteractionTarget();
}

bool UHeistInteractionComponent::CanOwnerInteract() const
{
	return IsValid(OwnerCharacter) && OwnerCharacter->CanPerformGameplayActions();
}

#pragma endregion
