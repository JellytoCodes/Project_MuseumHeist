#include "Character/Components/HeistInteractionComponent.h"

#include "Character/HeistPlayerCharacter.h"
#include "Components/PrimitiveComponent.h"
#include "Core/HeistCollisionChannels.h"
#include "Core/HeistLogChannels.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
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
}

#pragma endregion

#pragma region Interaction

bool UHeistInteractionComponent::RefreshInteractionTarget(const bool bForceRefresh)
{
	AActor* PreviousTarget = CurrentInteractionTarget.Get();
	AActor* PreviousTraceHitActor = CurrentTraceHitActor.Get();
	UWorld* World = GetWorld();
	if (!IsValid(OwnerCharacter) || World == nullptr || !CanOwnerInteract())
	{
		ClearInteractionTarget(TEXT("OwnerCannotInteract"));
		return false;
	}

	const float CurrentWorldTime = World->GetTimeSeconds();
	if (!bForceRefresh
		&& CurrentWorldTime - LastInteractionTraceTime < InteractionScanInterval)
	{
		return HasValidInteractionTarget();
	}
	LastInteractionTraceTime = CurrentWorldTime;

	FVector TraceStart = FVector::ZeroVector;
	FVector TraceEnd = FVector::ZeroVector;
	if (!ResolveCenterScreenTrace(TraceStart, TraceEnd))
	{
		ClearInteractionTarget(TEXT("MissingViewpoint"));
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HeistCenterInteractionTrace), false, OwnerCharacter);
	FHitResult HitResult;
	const bool bHit = World->LineTraceSingleByChannel(
		HitResult,
		TraceStart,
		TraceEnd,
		HeistCollisionChannels::InteractionTrace,
		QueryParams);
	AActor* HitActor = bHit ? HitResult.GetActor() : nullptr;
	CurrentTraceHitActor = HitActor;

	const UPrimitiveComponent* HitComponent = bHit ? HitResult.GetComponent() : nullptr;
	const bool bHitInteractableChannel = IsValid(HitComponent)
		&& HitComponent->GetCollisionObjectType() == HeistCollisionChannels::Interactable;
	IHeistInteractable* Interactable = bHitInteractableChannel
		? Cast<IHeistInteractable>(HitActor)
		: nullptr;
	const bool bWithinRange = IsActorWithinInteractionRange(HitActor);
	const bool bAvailable = Interactable != nullptr
		&& bWithinRange
		&& Interactable->CanInteract(OwnerCharacter);
	AActor* CenterTarget = bAvailable ? HitActor : nullptr;

	CurrentInteractionTarget = CenterTarget;
	bCurrentTargetAvailable = bAvailable;
	if (PreviousTraceHitActor != HitActor || PreviousTarget != CenterTarget)
	{
		UE_LOG(
			LogHeistUI,
			Verbose,
			TEXT("[%s] Center interaction trace: Channel=HeistInteractionTrace Start=%s End=%s Hit=%s InteractableChannel=%s WithinRange=%s Target=%s Available=%s Distance=%.1f Key=E"),
			*GetNameSafe(OwnerCharacter),
			*TraceStart.ToCompactString(),
			*TraceEnd.ToCompactString(),
			*GetNameSafe(HitActor),
			bHitInteractableChannel ? TEXT("true") : TEXT("false"),
			bWithinRange ? TEXT("true") : TEXT("false"),
			*GetNameSafe(CenterTarget),
			bAvailable ? TEXT("true") : TEXT("false"),
			bHit ? HitResult.Distance : InteractionRange);
	}

	if (PreviousTarget != CenterTarget)
	{
		InteractionTargetChangedDelegate.Broadcast(CenterTarget, bAvailable);
	}

	return bAvailable;
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
	return IsActorWithinInteractionRange(TargetActor);
}

float UHeistInteractionComponent::GetInteractionRange() const
{
	return InteractionRange;
}

bool UHeistInteractionComponent::IsActorWithinInteractionRange(const AActor* TargetActor) const
{
	if (!IsValid(OwnerCharacter) || !IsValid(TargetActor) || !CanOwnerInteract())
	{
		return false;
	}

	return FVector::DistSquared(OwnerCharacter->GetActorLocation(), TargetActor->GetActorLocation())
		<= FMath::Square(InteractionRange);
}

FHeistInteractionTargetChanged& UHeistInteractionComponent::GetInteractionTargetChangedDelegate()
{
	return InteractionTargetChangedDelegate;
}

bool UHeistInteractionComponent::ResolveCenterScreenTrace(FVector& OutTraceStart, FVector& OutTraceEnd) const
{
	if (!IsValid(OwnerCharacter))
	{
		return false;
	}

	FRotator ViewRotation = OwnerCharacter->GetActorRotation();
	if (const AController* Controller = OwnerCharacter->GetController())
	{
		Controller->GetPlayerViewPoint(OutTraceStart, ViewRotation);
	}
	else
	{
		OwnerCharacter->GetActorEyesViewPoint(OutTraceStart, ViewRotation);
	}

	OutTraceEnd = OutTraceStart + (ViewRotation.Vector() * InteractionRange);
	return true;
}

void UHeistInteractionComponent::ClearInteractionTarget(const TCHAR* Reason)
{
	AActor* PreviousTarget = CurrentInteractionTarget.Get();
	AActor* PreviousTraceHitActor = CurrentTraceHitActor.Get();
	CurrentInteractionTarget.Reset();
	CurrentTraceHitActor.Reset();
	bCurrentTargetAvailable = false;

	if (IsValid(PreviousTarget) || IsValid(PreviousTraceHitActor))
	{
		UE_LOG(
			LogHeistUI,
			Verbose,
			TEXT("[%s] Center interaction target cleared: PreviousTarget=%s PreviousHit=%s Reason=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(PreviousTarget),
			*GetNameSafe(PreviousTraceHitActor),
			Reason);
	}

	if (IsValid(PreviousTarget))
	{
		InteractionTargetChangedDelegate.Broadcast(nullptr, false);
	}
}

bool UHeistInteractionComponent::CanOwnerInteract() const
{
	return IsValid(OwnerCharacter) && OwnerCharacter->CanPerformGameplayActions();
}

#pragma endregion
