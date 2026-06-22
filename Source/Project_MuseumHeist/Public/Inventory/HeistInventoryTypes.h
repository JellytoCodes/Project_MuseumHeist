#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "HeistInventoryTypes.generated.h"

class UHeistInventoryComponent;
class AActor;

#pragma region Inventory

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistInventoryItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 InstanceId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	FName ItemId = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	FIntPoint GridPosition = FIntPoint(-1, -1);

	UPROPERTY(BlueprintReadOnly)
	int32 Quantity = 1;

	UPROPERTY(BlueprintReadOnly)
	bool bRotated = false;

	bool operator==(const FHeistInventoryItem& Other) const
	{
		return InstanceId == Other.InstanceId
			&& ItemId == Other.ItemId
			&& GridPosition == Other.GridPosition
			&& Quantity == Other.Quantity
			&& bRotated == Other.bRotated;
	}
};

#pragma endregion

#pragma region LootDrop

USTRUCT()
struct PROJECT_MUSEUMHEIST_API FHeistLootDropRequest
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AActor> DroppedBy;

	UPROPERTY()
	FName ItemId = NAME_None;

	UPROPERTY()
	int32 SourceInstanceId = INDEX_NONE;

	UPROPERTY()
	FVector_NetQuantize DropOrigin = FVector::ZeroVector;
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

	void SetOwnerComponent(UHeistInventoryComponent* InOwnerComponent);
	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);

private:
	TWeakObjectPtr<UHeistInventoryComponent> OwnerComponent;
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

	UPROPERTY(BlueprintReadOnly)
	EHeistQuickSlotType SlotType = EHeistQuickSlotType::None;

	UPROPERTY(BlueprintReadOnly)
	int32 ItemInstanceId = INDEX_NONE;

	bool operator==(const FHeistQuickSlotState& Other) const
	{
		return SlotType == Other.SlotType && ItemInstanceId == Other.ItemInstanceId;
	}
};

#pragma endregion
