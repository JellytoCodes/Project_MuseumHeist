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

	/** Authoritative unrotated footprint copied from item/artifact data at acquisition time. */
	UPROPERTY(BlueprintReadOnly)
	FIntPoint BaseGridSize = FIntPoint(1, 1);

	UPROPERTY(BlueprintReadOnly)
	float Weight = 0.0f;

	/** Contract value for Original artifacts. Normal inventory items keep this at zero. */
	UPROPERTY(BlueprintReadOnly)
	int32 ContractValue = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bCanRotate = false;

	UPROPERTY(BlueprintReadOnly)
	bool bOriginalArtifact = false;

	UPROPERTY(BlueprintReadOnly)
	bool bRequiredTarget = false;

	/** Display case that owns the world state for this Original. */
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> SourceDisplayCase;

	FIntPoint GetPlacedSize() const
	{
		return bRotated ? FIntPoint(BaseGridSize.Y, BaseGridSize.X) : BaseGridSize;
	}

	bool IsOriginalArtifact() const
	{
		return bOriginalArtifact;
	}

	bool HasValidOriginalData() const
	{
		return bOriginalArtifact && !ItemId.IsNone() && ContractValue > 0 && FMath::IsFinite(Weight) && Weight >= 0.0f && BaseGridSize.X > 0 && BaseGridSize.Y > 0 &&
			   SourceDisplayCase != nullptr;
	}

	bool operator==(const FHeistInventoryItem& Other) const
	{
		return InstanceId == Other.InstanceId && ItemId == Other.ItemId && GridPosition == Other.GridPosition && Quantity == Other.Quantity && bRotated == Other.bRotated &&
			   BaseGridSize == Other.BaseGridSize && FMath::IsNearlyEqual(Weight, Other.Weight) && ContractValue == Other.ContractValue && bCanRotate == Other.bCanRotate &&
			   bOriginalArtifact == Other.bOriginalArtifact && bRequiredTarget == Other.bRequiredTarget && SourceDisplayCase == Other.SourceDisplayCase;
	}
};

#pragma endregion

#pragma region ExtractionDeposit

/**
 * Server-only preview/commit payload for one player's Shared Extraction deposit.
 * Loose Loot and every Original are sourced from the same 5x5 grid inventory and
 * deposited through one authoritative Contract mutation.
 */
struct PROJECT_MUSEUMHEIST_API FHeistPlayerDepositPayload
{
	int32 LooseLootItemCount = 0;
	int32 LooseLootValue = 0;
	float LooseLootWeight = 0.0f;
	TArray<FHeistInventoryItem> OriginalArtifacts;

	int32 GetOriginalItemCount() const
	{
		return OriginalArtifacts.Num();
	}

	int32 GetOriginalValue() const
	{
		int64 Total = 0;
		for (const FHeistInventoryItem& Original : OriginalArtifacts)
		{
			Total += Original.HasValidOriginalData() ? Original.ContractValue : 0;
		}
		return static_cast<int32>(FMath::Min<int64>(MAX_int32, Total));
	}

	float GetOriginalWeight() const
	{
		double Total = 0.0;
		for (const FHeistInventoryItem& Original : OriginalArtifacts)
		{
			Total += Original.HasValidOriginalData() ? Original.Weight : 0.0f;
		}
		return static_cast<float>(Total);
	}

	bool ContainsRequiredTarget() const
	{
		return OriginalArtifacts.ContainsByPredicate([](const FHeistInventoryItem& Original) { return Original.HasValidOriginalData() && Original.bRequiredTarget; });
	}

	int32 GetTotalValue() const
	{
		return LooseLootValue + GetOriginalValue();
	}

	float GetTotalWeight() const
	{
		return LooseLootWeight + GetOriginalWeight();
	}

	bool HasDeposit() const
	{
		return LooseLootItemCount > 0 || OriginalArtifacts.Num() > 0;
	}

	bool Matches(const FHeistPlayerDepositPayload& Other) const
	{
		return LooseLootItemCount == Other.LooseLootItemCount && LooseLootValue == Other.LooseLootValue && FMath::IsNearlyEqual(LooseLootWeight, Other.LooseLootWeight) &&
			   OriginalArtifacts == Other.OriginalArtifacts;
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

template <> struct TStructOpsTypeTraits<FHeistReplicatedInventory> : public TStructOpsTypeTraitsBase2<FHeistReplicatedInventory>
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
