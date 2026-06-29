#include "World/Actors/Loot/HeistLootActor.h"

#include "Core/HeistGameMode.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Inventory/HeistItemDataTypes.h"
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
	const FHeistLootDataRow* ResolvedRow =
		LootDataRow.GetRow<FHeistLootDataRow>(TEXT("AHeistLootActor::ResolveLootData"));

	if (ResolvedRow != nullptr)
	{
		const FName ResolvedItemId = ResolvedRow->ItemId.IsNone()
			? LootDataRow.RowName
			: ResolvedRow->ItemId;
		const AHeistGameMode* HeistGameMode =
			GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
		FHeistItemDataRow ItemDefinition;
		if (IsValid(HeistGameMode)
			&& HeistGameMode->TryGetItemDefinition(ResolvedItemId, ItemDefinition)
			&& ItemDefinition.ItemType == EHeistItemType::Loot)
		{
			LootRowId = ResolvedItemId;
			LootGrade = ResolvedRow->LootGrade;
			ScoreValue = ResolvedRow->ScoreValue;
			WeightValue = ItemDefinition.Weight;
			bIsAvailable = true;
			return;
		}
	}

	ApplyFallbackLootData();
	UHeistDebugFunctionLibrary::DebugLootDataFallbackApplied(this, LootDataRow.RowName);
}

void AHeistLootActor::ApplyFallbackLootData()
{
	LootRowId = LootDataRow.RowName;
	LootGrade = EHeistLootGrade::OneStar;
	ScoreValue = 0;
	WeightValue = 0.0f;
	bIsAvailable = true;
}

#pragma endregion
