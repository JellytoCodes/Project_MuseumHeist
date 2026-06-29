#include "Character/Components/HeistInventoryComponent.h"

#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistPlayerState.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "Inventory/HeistItemDataTypes.h"
#include "Net/UnrealNetwork.h"
#include "World/Actors/Loot/HeistLootActor.h"

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
		UHeistDebugFunctionLibrary::DebugInventoryItemDefinitionLookupRejected(ItemId, TEXT("RequiresAuthority"));
		return false;
	}

	const UWorld* World = GetWorld();
	const AHeistGameMode* HeistGameMode = World ? World->GetAuthGameMode<AHeistGameMode>() : nullptr;
	if (!IsValid(HeistGameMode))
	{
		UHeistDebugFunctionLibrary::DebugInventoryItemDefinitionLookupRejected(ItemId, TEXT("MissingAuthGameMode"));
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
		UHeistDebugFunctionLibrary::DebugInventoryAddRejected(OwnerActor, ItemId, TEXT("RequiresAuthority"));
		return false;
	}

	FHeistItemDataRow ItemDefinition;
	if (!TryGetItemDefinition(ItemId, ItemDefinition) || !ItemDefinition.bAvailableInV1)
	{
		UHeistDebugFunctionLibrary::DebugInventoryAddRejected(OwnerActor, ItemId, TEXT("InvalidItemDefinition"));
		return false;
	}

	FIntPoint GridPosition = FIntPoint(-1, -1);
	bool bRotated = false;
	if (!TryFindAutoPlacement(ItemDefinition, GridPosition, bRotated))
	{
		UHeistDebugFunctionLibrary::DebugInventoryAddRejected(
			OwnerActor,
			ItemId,
			TEXT("InventoryFull"),
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

	UHeistDebugFunctionLibrary::DebugInventoryItemAdded(
		OwnerActor,
		ItemId,
		OutInstanceId,
		GridPosition,
		PlacedSize,
		bRotated,
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
	UHeistDebugFunctionLibrary::DebugInventoryItemMoved(OwnerActor, InstanceId, TargetGridPosition);
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
	UHeistDebugFunctionLibrary::DebugInventoryItemRotated(
		OwnerActor,
		InstanceId,
		ItemEntry->InventoryItem.bRotated);
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

	UHeistDebugFunctionLibrary::DebugInventoryItemRemoved(
		OwnerActor,
		OutRemovedItem.ItemId,
		InstanceId,
		ReplicatedInventory.Items.Num());
	return true;
}

bool UHeistInventoryComponent::TryDropRandomLootOnStun(AActor* DropInstigator)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return false;
	}

	AHeistPlayerCharacter* OwnerCharacter = Cast<AHeistPlayerCharacter>(OwnerActor);
	AHeistPlayerState* HeistPlayerState = nullptr;
	if (IsValid(OwnerCharacter))
	{
		HeistPlayerState = OwnerCharacter->GetPlayerState<AHeistPlayerState>();
	}
	if (!IsValid(HeistPlayerState))
	{
		UHeistDebugFunctionLibrary::DebugPinataDropSkipped(OwnerActor, TEXT("MissingPlayerState"));
		return false;
	}

	FHeistInventoryItem DropCandidate;
	if (!TrySelectRandomStunDropCandidate(DropCandidate))
	{
		UHeistDebugFunctionLibrary::DebugPinataDropSkipped(OwnerActor, TEXT("NoEligibleLoot"));
		return false;
	}

	AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	FHeistItemDataRow ItemDefinition;
	FHeistLootDataRow LootDefinition;
	if (!IsValid(HeistGameMode)
		|| !HeistGameMode->TryGetItemDefinition(DropCandidate.ItemId, ItemDefinition)
		|| ItemDefinition.ItemType != EHeistItemType::Loot
		|| !HeistGameMode->TryGetLootDefinition(DropCandidate.ItemId, LootDefinition)
		|| !HeistPlayerState->CanRemoveLootScoreAndWeight(LootDefinition.ScoreValue, ItemDefinition.Weight))
	{
		UHeistDebugFunctionLibrary::DebugPinataDropSkipped(OwnerActor, TEXT("InvalidLootState"));
		return false;
	}

	const FVector OwnerLocation = OwnerActor->GetActorLocation();
	const FVector RandomDirection = FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f).Vector();
	const float DropDistance = FMath::FRandRange(50.0f, 150.0f);

	FHeistLootDropRequest DropRequest;
	DropRequest.DroppedBy = IsValid(DropInstigator) ? DropInstigator : OwnerActor;
	DropRequest.ItemId = DropCandidate.ItemId;
	DropRequest.SourceInstanceId = DropCandidate.InstanceId;
	DropRequest.DropOrigin = OwnerLocation + RandomDirection * DropDistance;

	AHeistLootActor* DroppedLootActor = nullptr;
	if (!HeistGameMode->TrySpawnDroppedLoot(DropRequest, DroppedLootActor))
	{
		UHeistDebugFunctionLibrary::DebugPinataDropSkipped(OwnerActor, TEXT("WorldSpawnFailed"));
		return false;
	}

	FHeistInventoryItem RemovedItem;
	if (!TryRemoveItem(DropCandidate.InstanceId, RemovedItem))
	{
		DroppedLootActor->Destroy();
		UHeistDebugFunctionLibrary::DebugPinataDropSkipped(OwnerActor, TEXT("InventoryRemovalFailed"));
		return false;
	}

	checkf(RemovedItem.ItemId == DropRequest.ItemId, TEXT("Validated stun drop item changed during commit."));
	checkf(
		HeistPlayerState->RemoveLootScoreAndWeight(LootDefinition.ScoreValue, ItemDefinition.Weight),
		TEXT("Validated loot score and weight removal must succeed after stun drop inventory commit."));

	UHeistDebugFunctionLibrary::DebugPinataDropAccepted(
		OwnerActor,
		OwnerActor,
		DropRequest.DroppedBy,
		DropRequest.ItemId,
		DropCandidate.InstanceId,
		DroppedLootActor,
		FVector(DropRequest.DropOrigin));
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
	UHeistDebugFunctionLibrary::DebugQuickSlotAssigned(
		OwnerActor,
		static_cast<int32>(SlotType),
		InstanceId,
		InventoryItem.ItemId);
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
			UHeistDebugFunctionLibrary::DebugInventoryOccupancyInvalid(
				ExistingItem.InstanceId,
				ExistingItem.ItemId,
				TEXT("InvalidItemDefinition"));
			return false;
		}

		const FIntPoint ExistingSize = ExistingItem.bRotated
			? FIntPoint(ExistingDefinition.GridSize.Y, ExistingDefinition.GridSize.X)
			: ExistingDefinition.GridSize;

		if (!CanPlaceAt(OutOccupiedCells, ExistingItem.GridPosition, ExistingSize))
		{
			UHeistDebugFunctionLibrary::DebugInventoryOccupancyInvalid(
				ExistingItem.InstanceId,
				ExistingItem.ItemId,
				TEXT("OutOfBoundsOrOverlap"),
				ExistingItem.GridPosition,
				ExistingSize);
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

bool UHeistInventoryComponent::TrySelectRandomStunDropCandidate(FHeistInventoryItem& OutInventoryItem) const
{
	OutInventoryItem = FHeistInventoryItem();

	TArray<FHeistInventoryItem> Candidates;
	for (const FHeistInventoryFastArrayItem& Entry : ReplicatedInventory.Items)
	{
		FHeistItemDataRow ItemDefinition;
		FHeistLootDataRow LootDefinition;
		const AActor* OwnerActor = GetOwner();
		const UWorld* World = GetWorld();
		const AHeistGameMode* HeistGameMode = World ? World->GetAuthGameMode<AHeistGameMode>() : nullptr;
		if (!IsValid(OwnerActor)
			|| !OwnerActor->HasAuthority()
			|| !IsValid(HeistGameMode)
			|| !HeistGameMode->TryGetItemDefinition(Entry.InventoryItem.ItemId, ItemDefinition)
			|| ItemDefinition.ItemType != EHeistItemType::Loot
			|| !HeistGameMode->TryGetLootDefinition(Entry.InventoryItem.ItemId, LootDefinition)
			|| !LootDefinition.bCanDropOnStun)
		{
			continue;
		}

		Candidates.Add(Entry.InventoryItem);
	}

	if (Candidates.IsEmpty())
	{
		return false;
	}

	OutInventoryItem = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	return true;
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
