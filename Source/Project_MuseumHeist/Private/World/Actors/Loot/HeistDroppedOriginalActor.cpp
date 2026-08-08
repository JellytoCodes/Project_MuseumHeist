#include "World/Actors/Loot/HeistDroppedOriginalActor.h"

#include "Components/StaticMeshComponent.h"
#include "Core/HeistLogChannels.h"
#include "Data/HeistArtifactDataTypes.h"
#include "Data/HeistGameBalanceDataAsset.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
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

	ResolveDroppedOriginalVisual();
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
	bAvailable = ResolveArtifactPresentationData() && HasValidDropData();
	++DropRevision;
}

FName AHeistDroppedOriginalActor::GetArtifactId() const
{
	return ArtifactId;
}

FText AHeistDroppedOriginalActor::GetArtifactDisplayName() const
{
	return ArtifactDisplayName;
}

EHeistLootGrade AHeistDroppedOriginalActor::GetItemGrade() const
{
	return ItemGrade;
}

EHeistForgeryType AHeistDroppedOriginalActor::GetForgeryType() const
{
	return ForgeryType;
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
	DOREPLIFETIME(AHeistDroppedOriginalActor, ArtifactDisplayName);
	DOREPLIFETIME(AHeistDroppedOriginalActor, ItemGrade);
	DOREPLIFETIME(AHeistDroppedOriginalActor, ForgeryType);
	DOREPLIFETIME(AHeistDroppedOriginalActor, ArtifactValue);
	DOREPLIFETIME(AHeistDroppedOriginalActor, Weight);
	DOREPLIFETIME(AHeistDroppedOriginalActor, bRequiredTarget);
	DOREPLIFETIME(AHeistDroppedOriginalActor, SourceDisplayCase);
	DOREPLIFETIME(AHeistDroppedOriginalActor, bAvailable);
	DOREPLIFETIME(AHeistDroppedOriginalActor, DropRevision);
}

bool AHeistDroppedOriginalActor::HasValidDropData() const
{
	return !ArtifactId.IsNone() && !ArtifactDisplayName.IsEmpty() && ForgeryType != EHeistForgeryType::None && ArtifactValue > 0 && FMath::IsFinite(Weight) && Weight >= 0.0f &&
		IsValid(SourceDisplayCase.Get());
}

bool AHeistDroppedOriginalActor::ResolveArtifactPresentationData()
{
	if (ArtifactId.IsNone())
	{
		return false;
	}

	const UHeistGameBalanceDataAsset* BalanceData = GetDefault<UHeistGameBalanceDataAsset>();
	UDataTable* ArtifactDataTable = IsValid(BalanceData) ? BalanceData->ArtifactDataTable.LoadSynchronous() : nullptr;
	const FHeistArtifactDataRow* ArtifactDefinition = IsValid(ArtifactDataTable) && ArtifactDataTable->GetRowStruct() == FHeistArtifactDataRow::StaticStruct()
		? ArtifactDataTable->FindRow<FHeistArtifactDataRow>(ArtifactId, TEXT("AHeistDroppedOriginalActor::ResolveArtifactPresentationData"), false)
		: nullptr;
	if (ArtifactDefinition == nullptr || ArtifactDefinition->ArtifactId != ArtifactId || ArtifactDefinition->DisplayName.IsEmpty() || ArtifactDefinition->ForgeryType == EHeistForgeryType::None)
	{
		UE_LOG(LogHeistNetwork, Error, TEXT("Dropped Original data resolution failed: Actor=%s Artifact=%s Reason=InvalidArtifactPresentationData"), *GetNameSafe(this),
			   *ArtifactId.ToString());
		return false;
	}

	ArtifactDisplayName = ArtifactDefinition->DisplayName;
	ItemGrade = ArtifactDefinition->ItemGrade;
	ForgeryType = ArtifactDefinition->ForgeryType;
	return true;
}

void AHeistDroppedOriginalActor::ResolveDroppedOriginalVisual()
{
	if (!IsValid(VisualMeshComponent))
	{
		return;
	}

	UStaticMesh* ResolvedMesh = nullptr;
	FTransform ResolvedTransform = FTransform::Identity;
	if (ForgeryType == EHeistForgeryType::Drawing)
	{
		ResolvedMesh = PaintingDropMesh.Get();
		ResolvedTransform = PaintingDropVisualRelativeTransform;
	}
	else if (ForgeryType == EHeistForgeryType::Assembly)
	{
		ResolvedMesh = ObjectDropMesh.Get();
		ResolvedTransform = ObjectDropVisualRelativeTransform;
	}

	VisualMeshComponent->SetStaticMesh(ResolvedMesh);
	VisualMeshComponent->SetRelativeTransform(ResolvedTransform);
	VisualMeshComponent->EmptyOverrideMaterials();
	if (UMaterialInterface* GradeMaterial = ResolveGradeMaterial(); IsValid(GradeMaterial))
	{
		VisualMeshComponent->SetMaterial(GradeMaterialSlot, GradeMaterial);
	}

	if (!IsValid(ResolvedMesh))
	{
		UE_LOG(LogHeistNetwork, Error, TEXT("Dropped Original visual resolution failed: Actor=%s Artifact=%s ForgeryType=%s Reason=CategoryMeshUnassigned"), *GetNameSafe(this),
			   *ArtifactId.ToString(), *UEnum::GetValueAsString(ForgeryType));
	}
}

UMaterialInterface* AHeistDroppedOriginalActor::ResolveGradeMaterial() const
{
	switch (ItemGrade)
	{
	case EHeistLootGrade::OneStar:
		return OneStarGradeMaterial.Get();
	case EHeistLootGrade::TwoStar:
		return TwoStarGradeMaterial.Get();
	case EHeistLootGrade::ThreeStar:
		return ThreeStarGradeMaterial.Get();
	case EHeistLootGrade::FourStar:
		return FourStarGradeMaterial.Get();
	default:
		return nullptr;
	}
}

void AHeistDroppedOriginalActor::OnRep_DropRevision()
{
	ResolveDroppedOriginalVisual();
	BP_DroppedOriginalSnapshotChanged();
}
