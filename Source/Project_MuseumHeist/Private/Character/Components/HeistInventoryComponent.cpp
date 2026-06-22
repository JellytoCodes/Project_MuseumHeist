#include "Character/Components/HeistInventoryComponent.h"

#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistLogChannels.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Inventory/HeistItemDataTypes.h"
#include "Net/UnrealNetwork.h"

#pragma region InternalConstants

namespace
{
	const FName CoinItemId(TEXT("Throwable_Coin"));
	const FName SmokeItemId(TEXT("Throwable_Smoke"));
	const FName GlueTrapItemId(TEXT("Trap_Glue"));
}

#pragma endregion

#pragma region FastArrayNotifications

void FHeistReplicatedInventory::SetOwnerComponent(UHeistInventoryComponent* InOwnerComponent)
{
	OwnerComponent = InOwnerComponent;
}

void FHeistReplicatedInventory::PostReplicatedReceive(
	const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	(void)Parameters;

	if (UHeistInventoryComponent* InventoryComponent = OwnerComponent.Get())
	{
		InventoryComponent->NotifyInventoryChanged();
	}
}

#pragma endregion

#pragma region Construction

UHeistInventoryComponent::UHeistInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	ReplicatedInventory.SetOwnerComponent(this);
	QuickSlots.SetNum(3);
	QuickSlots[0].SlotType = EHeistQuickSlotType::Coin;
	QuickSlots[1].SlotType = EHeistQuickSlotType::SmokeGrenade;
	QuickSlots[2].SlotType = EHeistQuickSlotType::GlueTrap;
}

#pragma endregion

#pragma region Lifecycle

void UHeistInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	ReplicatedInventory.SetOwnerComponent(this);
}

#pragma endregion

#pragma region InventoryContract

int32 UHeistInventoryComponent::GetGridColumnCount() const
{
	return GridColumnCount;
}

int32 UHeistInventoryComponent::GetGridRowCount() const
{
	return GridRowCount;
}

const FHeistReplicatedInventory& UHeistInventoryComponent::GetReplicatedInventory() const
{
	return ReplicatedInventory;
}

const TArray<FHeistQuickSlotState>& UHeistInventoryComponent::GetQuickSlots() const
{
	return QuickSlots;
}

FHeistInventoryChanged& UHeistInventoryComponent::GetInventoryChangedDelegate()
{
	return InventoryChangedDelegate;
}

bool UHeistInventoryComponent::TryGetItemDefinition(
	const FName ItemId,
	FHeistItemDataRow& OutItemDefinition) const
{
	OutItemDefinition = FHeistItemDataRow();

	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		UE_LOG(
			LogHeistInventory,
			Warning,
			TEXT("Item definition lookup rejected: ItemId=%s Reason=RequiresAuthority"),
			*ItemId.ToString());
		return false;
	}

	const UWorld* World = GetWorld();
	const AHeistGameMode* HeistGameMode = World ? World->GetAuthGameMode<AHeistGameMode>() : nullptr;
	if (!IsValid(HeistGameMode))
	{
		UE_LOG(
			LogHeistInventory,
			Warning,
			TEXT("Item definition lookup rejected: ItemId=%s Reason=MissingAuthGameMode"),
			*ItemId.ToString());
		return false;
	}

	return HeistGameMode->TryGetItemDefinition(ItemId, OutItemDefinition);
}

bool UHeistInventoryComponent::TryAddItem(const FName ItemId, int32& OutInstanceId)
{
	OutInstanceId = INDEX_NONE;

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		UE_LOG(
			LogHeistInventory,
			Warning,
			TEXT("Inventory add rejected: Owner=%s ItemId=%s Reason=RequiresAuthority"),
			*GetNameSafe(OwnerActor),
			*ItemId.ToString());
		return false;
	}

	FHeistItemDataRow ItemDefinition;
	if (!TryGetItemDefinition(ItemId, ItemDefinition) || !ItemDefinition.bAvailableInV1)
	{
		UE_LOG(
			LogHeistInventory,
			Warning,
			TEXT("Inventory add rejected: Owner=%s ItemId=%s Reason=InvalidItemDefinition"),
			*GetNameSafe(OwnerActor),
			*ItemId.ToString());
		return false;
	}

	FIntPoint GridPosition = FIntPoint(-1, -1);
	bool bRotated = false;
	if (!TryFindAutoPlacement(ItemDefinition, GridPosition, bRotated))
	{
		UE_LOG(
			LogHeistInventory,
			Warning,
			TEXT("Inventory add rejected: Owner=%s ItemId=%s Reason=InventoryFull Grid=%dx%d"),
			*GetNameSafe(OwnerActor),
			*ItemId.ToString(),
			GridColumnCount,
			GridRowCount);
		return false;
	}

	FHeistInventoryFastArrayItem& AddedEntry = ReplicatedInventory.Items.Emplace_GetRef();
	AddedEntry.InventoryItem.InstanceId = AllocateNextInstanceId();
	AddedEntry.InventoryItem.ItemId = ItemId;
	AddedEntry.InventoryItem.GridPosition = GridPosition;
	AddedEntry.InventoryItem.Quantity = 1;
	AddedEntry.InventoryItem.bRotated = bRotated;
	ReplicatedInventory.MarkItemDirty(AddedEntry);
	OwnerActor->ForceNetUpdate();
	NotifyInventoryChanged();

	OutInstanceId = AddedEntry.InventoryItem.InstanceId;
	const FIntPoint PlacedSize = bRotated
		? FIntPoint(ItemDefinition.GridSize.Y, ItemDefinition.GridSize.X)
		: ItemDefinition.GridSize;

	UE_LOG(
		LogHeistInventory,
		Log,
		TEXT("Inventory item added: Owner=%s ItemId=%s InstanceId=%d Grid=(%d,%d) Size=%dx%d Rotated=%s ItemCount=%d"),
		*GetNameSafe(OwnerActor),
		*ItemId.ToString(),
		OutInstanceId,
		GridPosition.X,
		GridPosition.Y,
		PlacedSize.X,
		PlacedSize.Y,
		bRotated ? TEXT("true") : TEXT("false"),
		ReplicatedInventory.Items.Num());

	return true;
}

bool UHeistInventoryComponent::TryGetItem(
	const int32 InstanceId,
	FHeistInventoryItem& OutInventoryItem) const
{
	OutInventoryItem = FHeistInventoryItem();
	const FHeistInventoryFastArrayItem* ItemEntry = FindItemEntry(InstanceId);
	if (ItemEntry == nullptr)
	{
		return false;
	}

	OutInventoryItem = ItemEntry->InventoryItem;
	return true;
}

bool UHeistInventoryComponent::TryMoveItem(const int32 InstanceId, const FIntPoint& TargetGridPosition)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return false;
	}

	FHeistInventoryFastArrayItem* ItemEntry = FindItemEntry(InstanceId);
	if (ItemEntry == nullptr)
	{
		return false;
	}

	FHeistItemDataRow ItemDefinition;
	if (!TryGetItemDefinition(ItemEntry->InventoryItem.ItemId, ItemDefinition))
	{
		return false;
	}

	TArray<bool> OccupiedCells;
	if (!TryBuildOccupiedCellsExcluding(InstanceId, OccupiedCells))
	{
		return false;
	}

	const FIntPoint ItemSize = ItemEntry->InventoryItem.bRotated
		? FIntPoint(ItemDefinition.GridSize.Y, ItemDefinition.GridSize.X)
		: ItemDefinition.GridSize;
	if (!CanPlaceAt(OccupiedCells, TargetGridPosition, ItemSize))
	{
		return false;
	}

	ItemEntry->InventoryItem.GridPosition = TargetGridPosition;
	ReplicatedInventory.MarkItemDirty(*ItemEntry);
	OwnerActor->ForceNetUpdate();
	NotifyInventoryChanged();
	UE_LOG(
		LogHeistInventory,
		Log,
		TEXT("Inventory item moved: Owner=%s InstanceId=%d Grid=(%d,%d)"),
		*GetNameSafe(OwnerActor),
		InstanceId,
		TargetGridPosition.X,
		TargetGridPosition.Y);
	return true;
}

bool UHeistInventoryComponent::TryRotateItem(const int32 InstanceId)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return false;
	}

	FHeistInventoryFastArrayItem* ItemEntry = FindItemEntry(InstanceId);
	if (ItemEntry == nullptr)
	{
		return false;
	}

	FHeistItemDataRow ItemDefinition;
	if (!TryGetItemDefinition(ItemEntry->InventoryItem.ItemId, ItemDefinition)
		|| !ItemDefinition.bCanRotate)
	{
		return false;
	}

	TArray<bool> OccupiedCells;
	if (!TryBuildOccupiedCellsExcluding(InstanceId, OccupiedCells))
	{
		return false;
	}

	const FIntPoint RotatedSize = ItemEntry->InventoryItem.bRotated
		? ItemDefinition.GridSize
		: FIntPoint(ItemDefinition.GridSize.Y, ItemDefinition.GridSize.X);
	if (!CanPlaceAt(OccupiedCells, ItemEntry->InventoryItem.GridPosition, RotatedSize))
	{
		return false;
	}

	ItemEntry->InventoryItem.bRotated = !ItemEntry->InventoryItem.bRotated;
	ReplicatedInventory.MarkItemDirty(*ItemEntry);
	OwnerActor->ForceNetUpdate();
	NotifyInventoryChanged();
	UE_LOG(
		LogHeistInventory,
		Log,
		TEXT("Inventory item rotated: Owner=%s InstanceId=%d Rotated=%s"),
		*GetNameSafe(OwnerActor),
		InstanceId,
		ItemEntry->InventoryItem.bRotated ? TEXT("true") : TEXT("false"));
	return true;
}

bool UHeistInventoryComponent::TryRemoveItem(
	const int32 InstanceId,
	FHeistInventoryItem& OutRemovedItem)
{
	OutRemovedItem = FHeistInventoryItem();
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return false;
	}

	const int32 ItemIndex = ReplicatedInventory.Items.IndexOfByPredicate(
		[InstanceId](const FHeistInventoryFastArrayItem& Entry)
		{
			return Entry.InventoryItem.InstanceId == InstanceId;
		});
	if (!ReplicatedInventory.Items.IsValidIndex(ItemIndex))
	{
		return false;
	}

	OutRemovedItem = ReplicatedInventory.Items[ItemIndex].InventoryItem;
	ClearQuickSlotReferences(InstanceId);
	ReplicatedInventory.Items.RemoveAt(ItemIndex);
	ReplicatedInventory.MarkArrayDirty();
	OwnerActor->ForceNetUpdate();
	NotifyInventoryChanged();

	UE_LOG(
		LogHeistInventory,
		Log,
		TEXT("Inventory item removed: Owner=%s ItemId=%s InstanceId=%d ItemCount=%d"),
		*GetNameSafe(OwnerActor),
		*OutRemovedItem.ItemId.ToString(),
		InstanceId,
		ReplicatedInventory.Items.Num());
	return true;
}

bool UHeistInventoryComponent::TryAssignQuickSlot(
	const EHeistQuickSlotType SlotType,
	const int32 InstanceId)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return false;
	}

	FHeistInventoryItem InventoryItem;
	FHeistItemDataRow ItemDefinition;
	FHeistQuickSlotState* QuickSlot = FindQuickSlot(SlotType);
	if (QuickSlot == nullptr
		|| !TryGetItem(InstanceId, InventoryItem)
		|| !TryGetItemDefinition(InventoryItem.ItemId, ItemDefinition)
		|| !ItemDefinition.bCanUseQuickSlot
		|| ResolveQuickSlotType(InventoryItem.ItemId) != SlotType)
	{
		return false;
	}

	QuickSlot->ItemInstanceId = InstanceId;
	OwnerActor->ForceNetUpdate();
	NotifyInventoryChanged();
	UE_LOG(
		LogHeistInventory,
		Log,
		TEXT("QuickSlot assigned: Owner=%s Slot=%d InstanceId=%d ItemId=%s"),
		*GetNameSafe(OwnerActor),
		static_cast<int32>(SlotType),
		InstanceId,
		*InventoryItem.ItemId.ToString());
	return true;
}

bool UHeistInventoryComponent::TryClearQuickSlot(const EHeistQuickSlotType SlotType)
{
	AActor* OwnerActor = GetOwner();
	FHeistQuickSlotState* QuickSlot = FindQuickSlot(SlotType);
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || QuickSlot == nullptr)
	{
		return false;
	}

	QuickSlot->ItemInstanceId = INDEX_NONE;
	OwnerActor->ForceNetUpdate();
	NotifyInventoryChanged();
	return true;
}

bool UHeistInventoryComponent::IsInventoryOpen() const
{
	return bInventoryOpen;
}

bool UHeistInventoryComponent::TrySetInventoryOpen(const bool bInInventoryOpen)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return false;
	}

	bInventoryOpen = bInInventoryOpen;
	if (AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(OwnerActor))
	{
		HeistCharacter->HandleInventoryOpenStateChanged(bInventoryOpen);
	}
	OwnerActor->ForceNetUpdate();
	NotifyInventoryChanged();
	return true;
}

bool UHeistInventoryComponent::TryFindAutoPlacement(
	const FHeistItemDataRow& ItemDefinition,
	FIntPoint& OutGridPosition,
	bool& bOutRotated) const
{
	OutGridPosition = FIntPoint(-1, -1);
	bOutRotated = false;

	TArray<bool> OccupiedCells;
	if (!TryBuildOccupiedCells(OccupiedCells))
	{
		return false;
	}

	const auto TryFindForSize = [&OccupiedCells, &OutGridPosition](const FIntPoint& ItemSize)
	{
		for (int32 Row = 0; Row <= GridRowCount - ItemSize.Y; ++Row)
		{
			for (int32 Column = 0; Column <= GridColumnCount - ItemSize.X; ++Column)
			{
				const FIntPoint CandidatePosition(Column, Row);
				if (CanPlaceAt(OccupiedCells, CandidatePosition, ItemSize))
				{
					OutGridPosition = CandidatePosition;
					return true;
				}
			}
		}

		return false;
	};

	if (TryFindForSize(ItemDefinition.GridSize))
	{
		return true;
	}

	if (!ItemDefinition.bCanRotate || ItemDefinition.GridSize.X == ItemDefinition.GridSize.Y)
	{
		return false;
	}

	const FIntPoint RotatedSize(ItemDefinition.GridSize.Y, ItemDefinition.GridSize.X);
	bOutRotated = TryFindForSize(RotatedSize);
	return bOutRotated;
}

bool UHeistInventoryComponent::TryBuildOccupiedCells(TArray<bool>& OutOccupiedCells) const
{
	return TryBuildOccupiedCellsExcluding(INDEX_NONE, OutOccupiedCells);
}

bool UHeistInventoryComponent::TryBuildOccupiedCellsExcluding(
	const int32 ExcludedInstanceId,
	TArray<bool>& OutOccupiedCells) const
{
	OutOccupiedCells.Init(false, GridColumnCount * GridRowCount);

	for (const FHeistInventoryFastArrayItem& ExistingEntry : ReplicatedInventory.Items)
	{
		const FHeistInventoryItem& ExistingItem = ExistingEntry.InventoryItem;
		if (ExistingItem.InstanceId == ExcludedInstanceId)
		{
			continue;
		}
		FHeistItemDataRow ExistingDefinition;
		if (!TryGetItemDefinition(ExistingItem.ItemId, ExistingDefinition))
		{
			UE_LOG(
				LogHeistInventory,
				Error,
				TEXT("Inventory occupancy invalid: InstanceId=%d ItemId=%s Reason=InvalidItemDefinition"),
				ExistingItem.InstanceId,
				*ExistingItem.ItemId.ToString());
			return false;
		}

		const FIntPoint ExistingSize = ExistingItem.bRotated
			? FIntPoint(ExistingDefinition.GridSize.Y, ExistingDefinition.GridSize.X)
			: ExistingDefinition.GridSize;

		if (!CanPlaceAt(OutOccupiedCells, ExistingItem.GridPosition, ExistingSize))
		{
			UE_LOG(
				LogHeistInventory,
				Error,
				TEXT("Inventory occupancy invalid: InstanceId=%d ItemId=%s Grid=(%d,%d) Size=%dx%d Reason=OutOfBoundsOrOverlap"),
				ExistingItem.InstanceId,
				*ExistingItem.ItemId.ToString(),
				ExistingItem.GridPosition.X,
				ExistingItem.GridPosition.Y,
				ExistingSize.X,
				ExistingSize.Y);
			return false;
		}

		for (int32 Row = ExistingItem.GridPosition.Y; Row < ExistingItem.GridPosition.Y + ExistingSize.Y; ++Row)
		{
			for (int32 Column = ExistingItem.GridPosition.X; Column < ExistingItem.GridPosition.X + ExistingSize.X; ++Column)
			{
				OutOccupiedCells[Row * GridColumnCount + Column] = true;
			}
		}
	}

	return true;
}

FHeistInventoryFastArrayItem* UHeistInventoryComponent::FindItemEntry(const int32 InstanceId)
{
	return ReplicatedInventory.Items.FindByPredicate(
		[InstanceId](const FHeistInventoryFastArrayItem& Entry)
		{
			return Entry.InventoryItem.InstanceId == InstanceId;
		});
}

const FHeistInventoryFastArrayItem* UHeistInventoryComponent::FindItemEntry(const int32 InstanceId) const
{
	return ReplicatedInventory.Items.FindByPredicate(
		[InstanceId](const FHeistInventoryFastArrayItem& Entry)
		{
			return Entry.InventoryItem.InstanceId == InstanceId;
		});
}

FHeistQuickSlotState* UHeistInventoryComponent::FindQuickSlot(const EHeistQuickSlotType SlotType)
{
	return QuickSlots.FindByPredicate(
		[SlotType](const FHeistQuickSlotState& QuickSlot)
		{
			return QuickSlot.SlotType == SlotType;
		});
}

const FHeistQuickSlotState* UHeistInventoryComponent::FindQuickSlot(const EHeistQuickSlotType SlotType) const
{
	return QuickSlots.FindByPredicate(
		[SlotType](const FHeistQuickSlotState& QuickSlot)
		{
			return QuickSlot.SlotType == SlotType;
		});
}

EHeistQuickSlotType UHeistInventoryComponent::ResolveQuickSlotType(const FName ItemId) const
{
	if (ItemId == CoinItemId)
	{
		return EHeistQuickSlotType::Coin;
	}

	if (ItemId == SmokeItemId)
	{
		return EHeistQuickSlotType::SmokeGrenade;
	}

	if (ItemId == GlueTrapItemId)
	{
		return EHeistQuickSlotType::GlueTrap;
	}

	return EHeistQuickSlotType::None;
}

void UHeistInventoryComponent::ClearQuickSlotReferences(const int32 InstanceId)
{
	for (FHeistQuickSlotState& QuickSlot : QuickSlots)
	{
		if (QuickSlot.ItemInstanceId == InstanceId)
		{
			QuickSlot.ItemInstanceId = INDEX_NONE;
		}
	}
}

void UHeistInventoryComponent::OnRep_QuickSlots()
{
	NotifyInventoryChanged();
}

void UHeistInventoryComponent::OnRep_InventoryOpen()
{
	if (AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner()))
	{
		HeistCharacter->HandleInventoryOpenStateChanged(bInventoryOpen);
	}
	NotifyInventoryChanged();
}

bool UHeistInventoryComponent::CanPlaceAt(
	const TArray<bool>& OccupiedCells,
	const FIntPoint& GridPosition,
	const FIntPoint& ItemSize)
{
	if (GridPosition.X < 0
		|| GridPosition.Y < 0
		|| ItemSize.X <= 0
		|| ItemSize.Y <= 0
		|| GridPosition.X + ItemSize.X > GridColumnCount
		|| GridPosition.Y + ItemSize.Y > GridRowCount
		|| OccupiedCells.Num() != GridColumnCount * GridRowCount)
	{
		return false;
	}

	for (int32 Row = GridPosition.Y; Row < GridPosition.Y + ItemSize.Y; ++Row)
	{
		for (int32 Column = GridPosition.X; Column < GridPosition.X + ItemSize.X; ++Column)
		{
			if (OccupiedCells[Row * GridColumnCount + Column])
			{
				return false;
			}
		}
	}

	return true;
}

int32 UHeistInventoryComponent::AllocateNextInstanceId()
{
	const AActor* OwnerActor = GetOwner();
	checkf(
		IsValid(OwnerActor) && OwnerActor->HasAuthority(),
		TEXT("Inventory InstanceId allocation requires an authoritative owner."));
	checkf(NextInstanceId > 0 && NextInstanceId < MAX_int32, TEXT("Inventory InstanceId counter exhausted."));

	return NextInstanceId++;
}

void UHeistInventoryComponent::NotifyInventoryChanged()
{
	InventoryChangedDelegate.Broadcast();
}

#pragma endregion

#pragma region Replication

void UHeistInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UHeistInventoryComponent, ReplicatedInventory, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistInventoryComponent, bInventoryOpen, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistInventoryComponent, QuickSlots, COND_OwnerOnly);
}

#pragma endregion
