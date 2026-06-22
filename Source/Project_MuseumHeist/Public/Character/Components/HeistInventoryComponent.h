#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/HeistInventoryTypes.h"

#include "HeistInventoryComponent.generated.h"

DECLARE_MULTICAST_DELEGATE(FHeistInventoryChanged);

struct FHeistItemDataRow;

UCLASS(ClassGroup = (Heist), meta = (BlueprintSpawnableComponent))
class PROJECT_MUSEUMHEIST_API UHeistInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

#pragma region Construction

public:
	UHeistInventoryComponent();

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void BeginPlay() override;

#pragma endregion

#pragma region InventoryContract

public:
	static constexpr int32 GridColumnCount = 4;
	static constexpr int32 GridRowCount = 5;

	UFUNCTION(BlueprintPure, Category = "Heist|Inventory")
	int32 GetGridColumnCount() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Inventory")
	int32 GetGridRowCount() const;

	const FHeistReplicatedInventory& GetReplicatedInventory() const;
	const TArray<FHeistQuickSlotState>& GetQuickSlots() const;
	FHeistInventoryChanged& GetInventoryChangedDelegate();
	bool TryGetItemDefinition(FName ItemId, FHeistItemDataRow& OutItemDefinition) const;
	bool TryAddItem(FName ItemId, int32& OutInstanceId);
	bool TryGetItem(int32 InstanceId, FHeistInventoryItem& OutInventoryItem) const;
	bool TryMoveItem(int32 InstanceId, const FIntPoint& TargetGridPosition);
	bool TryRotateItem(int32 InstanceId);
	bool TryRemoveItem(int32 InstanceId, FHeistInventoryItem& OutRemovedItem);
	bool TryAssignQuickSlot(EHeistQuickSlotType SlotType, int32 InstanceId);
	bool TryClearQuickSlot(EHeistQuickSlotType SlotType);
	bool IsInventoryOpen() const;
	bool TrySetInventoryOpen(bool bInInventoryOpen);

private:
	bool TryFindAutoPlacement(
		const FHeistItemDataRow& ItemDefinition,
		FIntPoint& OutGridPosition,
		bool& bOutRotated) const;
	bool TryBuildOccupiedCells(TArray<bool>& OutOccupiedCells) const;
	bool TryBuildOccupiedCellsExcluding(int32 ExcludedInstanceId, TArray<bool>& OutOccupiedCells) const;
	FHeistInventoryFastArrayItem* FindItemEntry(int32 InstanceId);
	const FHeistInventoryFastArrayItem* FindItemEntry(int32 InstanceId) const;
	FHeistQuickSlotState* FindQuickSlot(EHeistQuickSlotType SlotType);
	const FHeistQuickSlotState* FindQuickSlot(EHeistQuickSlotType SlotType) const;
	EHeistQuickSlotType ResolveQuickSlotType(FName ItemId) const;
	void ClearQuickSlotReferences(int32 InstanceId);
	static bool CanPlaceAt(
		const TArray<bool>& OccupiedCells,
		const FIntPoint& GridPosition,
		const FIntPoint& ItemSize);
	int32 AllocateNextInstanceId();
	void NotifyInventoryChanged();

	UPROPERTY(Transient)
	int32 NextInstanceId = 1;

	UPROPERTY(ReplicatedUsing = OnRep_InventoryOpen)
	bool bInventoryOpen = false;

	UFUNCTION()
	void OnRep_InventoryOpen();

	UPROPERTY(ReplicatedUsing = OnRep_QuickSlots)
	TArray<FHeistQuickSlotState> QuickSlots;

	UFUNCTION()
	void OnRep_QuickSlots();

	FHeistInventoryChanged InventoryChangedDelegate;

#pragma endregion

#pragma region Replication

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	friend struct FHeistReplicatedInventory;

	UPROPERTY(Replicated)
	FHeistReplicatedInventory ReplicatedInventory;

#pragma endregion
};
