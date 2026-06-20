#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "HeistInventoryTypes.generated.h"

#pragma region Inventory

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistInventoryItem
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid InstanceId;

	UPROPERTY()
	FName ItemId = NAME_None;

	UPROPERTY()
	FIntPoint GridPosition = FIntPoint(-1, -1);

	UPROPERTY()
	int32 Quantity = 1;

	UPROPERTY()
	bool bRotated = false;
};

#pragma endregion

#pragma region Replication

USTRUCT()
struct PROJECT_MUSEUMHEIST_API FHeistInventoryFastArrayItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	FHeistInventoryItem InventoryItem;
};

USTRUCT()
struct PROJECT_MUSEUMHEIST_API FHeistReplicatedInventory : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FHeistInventoryFastArrayItem> Items;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FastArrayDeltaSerialize<FHeistInventoryFastArrayItem, FHeistReplicatedInventory>(Items, DeltaParams, *this);
	}
};

template<>
struct TStructOpsTypeTraits<FHeistReplicatedInventory> : public TStructOpsTypeTraitsBase2<FHeistReplicatedInventory>
{
	enum
	{
		WithNetDeltaSerializer = true
	};
};

#pragma endregion

#pragma region QuickSlots

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistQuickSlotState
{
	GENERATED_BODY()

	UPROPERTY()
	EHeistQuickSlotType SlotType = EHeistQuickSlotType::None;

	UPROPERTY()
	FGuid ItemInstanceId;

	UPROPERTY()
	FName ItemId = NAME_None;
};

#pragma endregion
