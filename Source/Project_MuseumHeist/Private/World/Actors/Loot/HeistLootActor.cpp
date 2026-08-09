#include "World/Actors/Loot/HeistLootActor.h"

#include "Core/HeistGameMode.h"
#include "Core/HeistLogChannels.h"
#include "Data/HeistGameBalanceDataAsset.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Inventory/HeistItemDataTypes.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"

#pragma region Construction

AHeistLootActor::AHeistLootActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);
}

#pragma endregion

#pragma region Lifecycle

void AHeistLootActor::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		ResolveLootData();
	}
	ResolveLootVisualFromRowId();
	RefreshAvailabilityPresentation();
}

#pragma endregion

#pragma region LootData

FName AHeistLootActor::GetLootRowId() const
{
	return LootRowId;
}

void AHeistLootActor::InitializeLootData(UDataTable* InLootDataTable, const FName InLootRowId)
{
	checkf(HasAuthority(), TEXT("Loot data initialization requires authority."));
	checkf(!HasActorBegunPlay(), TEXT("Loot data must be initialized before BeginPlay."));

	LootDataRow.DataTable = InLootDataTable;
	LootDataRow.RowName = InLootRowId;
}

int32 AHeistLootActor::GetScoreValue() const
{
	return ScoreValue;
}

float AHeistLootActor::GetWeightValue() const
{
	return WeightValue;
}

EHeistLootGrade AHeistLootActor::GetLootGrade() const
{
	return LootGrade;
}

bool AHeistLootActor::IsLootAvailable() const
{
	return bIsAvailable && !PickupReservationOwner.IsValid();
}

#pragma endregion

#pragma region Interaction

bool AHeistLootActor::CanInteract(const AActor* Interactor) const
{
	return IsLootAvailable() && Super::CanInteract(Interactor);
}

#pragma endregion

#pragma region Replication

void AHeistLootActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeistLootActor, LootRowId);
	DOREPLIFETIME(AHeistLootActor, LootGrade);
	DOREPLIFETIME(AHeistLootActor, ScoreValue);
	DOREPLIFETIME(AHeistLootActor, WeightValue);
	DOREPLIFETIME(AHeistLootActor, bIsAvailable);
}

#pragma endregion

#pragma region LootPickup

bool AHeistLootActor::TryReserveForPickup(AActor* Requester)
{
	if (!HasAuthority() || !IsValid(Requester) || !bIsAvailable || PickupReservationOwner.IsValid())
	{
		return false;
	}

	PickupReservationOwner = Requester;
	return true;
}

bool AHeistLootActor::CommitPickupReservation(AActor* Requester)
{
	if (!HasAuthority() || !IsValid(Requester) || PickupReservationOwner.Get() != Requester || !bIsAvailable)
	{
		return false;
	}

	bIsAvailable = false;
	PickupReservationOwner.Reset();
	RefreshAvailabilityPresentation();
	ForceNetUpdate();
	LootPickupCommittedDelegate.Broadcast(this, Requester);
	return true;
}

void AHeistLootActor::ReleasePickupReservation(AActor* Requester)
{
	if (HasAuthority() && IsValid(Requester) && PickupReservationOwner.Get() == Requester)
	{
		PickupReservationOwner.Reset();
	}
}

FHeistLootPickupCommitted& AHeistLootActor::GetLootPickupCommittedDelegate()
{
	return LootPickupCommittedDelegate;
}

#pragma endregion

#pragma region InternalHelpers

void AHeistLootActor::ResolveLootData()
{
	const FHeistLootDataRow* ResolvedRow = LootDataRow.GetRow<FHeistLootDataRow>(TEXT("AHeistLootActor::ResolveLootData"));

	if (ResolvedRow != nullptr)
	{
		const FName ResolvedItemId = ResolvedRow->ItemId.IsNone() ? LootDataRow.RowName : ResolvedRow->ItemId;
		const AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
		FHeistItemDataRow ItemDefinition;
		if (IsValid(HeistGameMode) && HeistGameMode->TryGetItemDefinition(ResolvedItemId, ItemDefinition) && ItemDefinition.ItemType == EHeistItemType::Loot)
		{
			LootRowId = ResolvedItemId;
			LootGrade = ResolvedRow->LootGrade;
			ScoreValue = ResolvedRow->ScoreValue;
			WeightValue = ItemDefinition.Weight;
			bIsAvailable = true;
			RefreshAvailabilityPresentation();
			return;
		}
	}

	ApplyFallbackLootData();
#if !UE_BUILD_SHIPPING
	UE_LOG(LogHeistInventory, Warning, TEXT("LootDataRow '%s' was not found. Fallback values are active."), *LootDataRow.RowName.ToString());
#endif
}

void AHeistLootActor::ResolveLootVisualFromRowId()
{
	if (LootRowId.IsNone() || !IsValid(VisualMeshComponent))
	{
		return;
	}

	const UHeistGameBalanceDataAsset* BalanceData = GetDefault<UHeistGameBalanceDataAsset>();
	UDataTable* LootDataTable = IsValid(BalanceData) ? BalanceData->LootDataTable.LoadSynchronous() : nullptr;
	const FHeistLootDataRow* LootDefinition = IsValid(LootDataTable) && LootDataTable->GetRowStruct() == FHeistLootDataRow::StaticStruct()
		? LootDataTable->FindRow<FHeistLootDataRow>(LootRowId, TEXT("AHeistLootActor::ResolveLootVisualFromRowId"), false)
		: nullptr;
	if (LootDefinition == nullptr || LootDefinition->ItemId != LootRowId)
	{
		VisualMeshComponent->SetStaticMesh(nullptr);
		UE_LOG(LogHeistInventory, Error, TEXT("Loot visual resolution failed: Actor=%s ItemId=%s Reason=MissingVisualRow"), *GetNameSafe(this), *LootRowId.ToString());
		return;
	}

	ApplyLootVisual(*LootDefinition);
}

void AHeistLootActor::ApplyLootVisual(const FHeistLootDataRow& LootDefinition)
{
	UStaticMesh* ResolvedMesh = LootDefinition.WorldMesh.LoadSynchronous();
	if (!IsValid(ResolvedMesh))
	{
		VisualMeshComponent->SetStaticMesh(nullptr);
		UE_LOG(LogHeistInventory, Error, TEXT("Loot visual resolution failed: Actor=%s ItemId=%s Reason=MissingWorldMesh"), *GetNameSafe(this), *LootDefinition.ItemId.ToString());
		return;
	}

	VisualMeshComponent->SetStaticMesh(ResolvedMesh);
	VisualMeshComponent->SetRelativeTransform(LootDefinition.WorldVisualRelativeTransform);
	VisualMeshComponent->EmptyOverrideMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < LootDefinition.WorldMaterials.Num(); ++MaterialIndex)
	{
		if (UMaterialInterface* Material = LootDefinition.WorldMaterials[MaterialIndex].LoadSynchronous(); IsValid(Material))
		{
			VisualMeshComponent->SetMaterial(MaterialIndex, Material);
		}
	}
}

void AHeistLootActor::ApplyFallbackLootData()
{
	LootRowId = LootDataRow.RowName;
	LootGrade = EHeistLootGrade::OneStar;
	ScoreValue = 0;
	WeightValue = 0.0f;
	bIsAvailable = true;
	RefreshAvailabilityPresentation();
}

void AHeistLootActor::RefreshAvailabilityPresentation()
{
	if (IsValid(VisualMeshComponent))
	{
		VisualMeshComponent->SetVisibility(bIsAvailable, true);
	}

	if (IsValid(InteractionCollision))
	{
		InteractionCollision->SetCollisionEnabled(bIsAvailable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		InteractionCollision->SetGenerateOverlapEvents(bIsAvailable);
	}
}

void AHeistLootActor::OnRep_LootRowId()
{
	ResolveLootVisualFromRowId();
	RefreshAvailabilityPresentation();
}

void AHeistLootActor::OnRep_IsAvailable()
{
	RefreshAvailabilityPresentation();
}

#pragma endregion
