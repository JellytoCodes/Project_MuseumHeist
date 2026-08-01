#include "World/Actors/Loot/HeistDroppedOriginalActor.h"

#include "Core/HeistLogChannels.h"
#include "Net/UnrealNetwork.h"

AHeistDroppedOriginalActor::AHeistDroppedOriginalActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
}

void AHeistDroppedOriginalActor::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		if (!HasValidDropData())
		{
			UE_LOG(LogHeistNetwork, Error, TEXT("Dropped Original invalid at BeginPlay: Actor=%s Artifact=%s Value=%d Weight=%.1f Source=%s"),
				   *GetNameSafe(this), *ArtifactId.ToString(), ArtifactValue, Weight, *GetNameSafe(SourceDisplayCase.Get()));
			bAvailable = false;
		}
		ForceNetUpdate();
	}

	BP_DroppedOriginalSnapshotChanged();
}

void AHeistDroppedOriginalActor::InitializeDroppedOriginal(const FName InArtifactId, const int32 InArtifactValue, const float InWeight, const bool bInRequiredTarget,
														 AActor* InSourceDisplayCase)
{
	checkf(HasAuthority(), TEXT("Dropped Original initialization requires authority."));
	checkf(!HasActorBegunPlay(), TEXT("Dropped Original must be initialized before BeginPlay."));

	ArtifactId = InArtifactId;
	ArtifactValue = InArtifactValue;
	Weight = InWeight;
	bRequiredTarget = bInRequiredTarget;
	SourceDisplayCase = InSourceDisplayCase;
	bAvailable = HasValidDropData();
	++DropRevision;
}

FName AHeistDroppedOriginalActor::GetArtifactId() const
{
	return ArtifactId;
}

int32 AHeistDroppedOriginalActor::GetArtifactValue() const
{
	return ArtifactValue;
}

float AHeistDroppedOriginalActor::GetWeight() const
{
	return Weight;
}

bool AHeistDroppedOriginalActor::IsRequiredTarget() const
{
	return bRequiredTarget;
}

AActor* AHeistDroppedOriginalActor::GetSourceDisplayCase() const
{
	return SourceDisplayCase.Get();
}

bool AHeistDroppedOriginalActor::IsDropAvailable() const
{
	return bAvailable && !PickupReservationOwner.IsValid() && HasValidDropData();
}

bool AHeistDroppedOriginalActor::TryReserveForPickup(AActor* Requester)
{
	if (!HasAuthority() || !IsValid(Requester) || !IsDropAvailable())
	{
		return false;
	}

	PickupReservationOwner = Requester;
	return true;
}

bool AHeistDroppedOriginalActor::IsReservedBy(const AActor* Requester) const
{
	return IsValid(Requester) && PickupReservationOwner.Get() == Requester;
}

bool AHeistDroppedOriginalActor::CommitPickupReservation(AActor* Requester)
{
	if (!HasAuthority() || !IsReservedBy(Requester) || !bAvailable)
	{
		return false;
	}

	bAvailable = false;
	PickupReservationOwner.Reset();
	++DropRevision;
	ForceNetUpdate();
	BP_DroppedOriginalSnapshotChanged();
	return true;
}

void AHeistDroppedOriginalActor::ReleasePickupReservation(AActor* Requester)
{
	if (HasAuthority() && IsReservedBy(Requester))
	{
		PickupReservationOwner.Reset();
	}
}

bool AHeistDroppedOriginalActor::CanInteract(const AActor* Interactor) const
{
	return IsDropAvailable() && Super::CanInteract(Interactor);
}

void AHeistDroppedOriginalActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeistDroppedOriginalActor, ArtifactId);
	DOREPLIFETIME(AHeistDroppedOriginalActor, ArtifactValue);
	DOREPLIFETIME(AHeistDroppedOriginalActor, Weight);
	DOREPLIFETIME(AHeistDroppedOriginalActor, bRequiredTarget);
	DOREPLIFETIME(AHeistDroppedOriginalActor, SourceDisplayCase);
	DOREPLIFETIME(AHeistDroppedOriginalActor, bAvailable);
	DOREPLIFETIME(AHeistDroppedOriginalActor, DropRevision);
}

bool AHeistDroppedOriginalActor::HasValidDropData() const
{
	return !ArtifactId.IsNone() && ArtifactValue > 0 && FMath::IsFinite(Weight) && Weight >= 0.0f && IsValid(SourceDisplayCase.Get());
}

void AHeistDroppedOriginalActor::OnRep_DropRevision()
{
	BP_DroppedOriginalSnapshotChanged();
}
