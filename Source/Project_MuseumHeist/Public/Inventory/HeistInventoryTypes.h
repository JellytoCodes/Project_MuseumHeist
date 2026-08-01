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
		return InstanceId == Other.InstanceId && ItemId == Other.ItemId && GridPosition == Other.GridPosition && Quantity == Other.Quantity && bRotated == Other.bRotated;
	}
};

#pragma endregion

#pragma region OriginalCarry

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistOriginalCarryEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName ArtifactId = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	int32 ArtifactValue = 0;

	UPROPERTY(BlueprintReadOnly)
	float Weight = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	bool bRequiredTarget = false;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> SourceDisplayCase;

	bool IsValid() const
	{
		return !ArtifactId.IsNone() && ArtifactValue > 0 && FMath::IsFinite(Weight) && Weight >= 0.0f && SourceDisplayCase != nullptr;
	}
};

#pragma endregion

#pragma region ExtractionDeposit

/**
 * Server-only preview/commit payload for one player's Shared Extraction deposit.
 * Loose Loot and the dedicated Original carry entry remain separate inventory
 * concepts, but are deposited through one authoritative Contract mutation.
 */
struct PROJECT_MUSEUMHEIST_API FHeistPlayerDepositPayload
{
	int32 LooseLootItemCount = 0;
	int32 LooseLootValue = 0;
	float LooseLootWeight = 0.0f;
	FHeistOriginalCarryEntry OriginalCarry;

	int32 GetOriginalValue() const
	{
		return OriginalCarry.IsValid() ? OriginalCarry.ArtifactValue : 0;
	}

	float GetOriginalWeight() const
	{
		return OriginalCarry.IsValid() ? OriginalCarry.Weight : 0.0f;
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
		return LooseLootItemCount > 0 || OriginalCarry.IsValid();
	}

	bool Matches(const FHeistPlayerDepositPayload& Other) const
	{
		return LooseLootItemCount == Other.LooseLootItemCount && LooseLootValue == Other.LooseLootValue && FMath::IsNearlyEqual(LooseLootWeight, Other.LooseLootWeight) &&
			   OriginalCarry.ArtifactId == Other.OriginalCarry.ArtifactId && OriginalCarry.ArtifactValue == Other.OriginalCarry.ArtifactValue &&
			   FMath::IsNearlyEqual(OriginalCarry.Weight, Other.OriginalCarry.Weight) && OriginalCarry.bRequiredTarget == Other.OriginalCarry.bRequiredTarget &&
			   OriginalCarry.SourceDisplayCase == Other.OriginalCarry.SourceDisplayCase;
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
