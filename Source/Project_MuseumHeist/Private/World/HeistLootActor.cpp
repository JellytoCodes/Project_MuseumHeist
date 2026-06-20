#include "World/HeistLootActor.h"

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
	return bIsAvailable;
}

#pragma endregion

#pragma region Interaction

bool AHeistLootActor::CanInteract(const AActor* Interactor) const
{
	return bIsAvailable && Super::CanInteract(Interactor);
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

bool AHeistLootActor::TryReserveForPickup(const AActor* Requester)
{
	if (!HasAuthority() || !IsValid(Requester) || !bIsAvailable)
	{
		return false;
	}

	bIsAvailable = false;
	ForceNetUpdate();
	return true;
}

#pragma endregion

#pragma region InternalHelpers

void AHeistLootActor::ResolveLootData()
{
	const FHeistLootDataRow* ResolvedRow =
		LootDataRow.GetRow<FHeistLootDataRow>(TEXT("AHeistLootActor::ResolveLootData"));

	if (ResolvedRow != nullptr)
	{
		LootRowId = ResolvedRow->ItemId.IsNone() ? LootDataRow.RowName : ResolvedRow->ItemId;
		LootGrade = ResolvedRow->LootGrade;
		ScoreValue = ResolvedRow->ScoreValue;
		WeightValue = ResolvedRow->Weight;
		bIsAvailable = true;
	}
	else
	{
		ApplyFallbackLootData();

		UHeistDebugFunctionLibrary::Message(
			this,
			FString::Printf(TEXT("LootDataRow '%s' was not found. Fallback values are active."), *LootDataRow.RowName.ToString()),
			EHeistDebugLevel::Warning);
	}
}

void AHeistLootActor::ApplyFallbackLootData()
{
	LootRowId = LootDataRow.RowName;
	LootGrade = EHeistLootGrade::None;
	ScoreValue = 0;
	WeightValue = 0.0f;
	bIsAvailable = true;
}

#pragma endregion
