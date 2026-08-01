#include "Character/Components/HeistInventoryComponent.h"

#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistGameState.h"
#include "Core/HeistLogChannels.h"
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
}

#pragma endregion

#pragma region FastArrayNotifications

void FHeistReplicatedInventory::SetOwnerComponent(UHeistInventoryComponent* InOwnerComponent)
{
	OwnerComponent = InOwnerComponent;
}

void FHeistReplicatedInventory::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
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
	QuickSlots.SetNum(1);
	QuickSlots[0].SlotType = EHeistQuickSlotType::Coin;
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

bool UHeistInventoryComponent::TryGetItemDefinition(const FName ItemId, FHeistItemDataRow& OutItemDefinition) const
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
	const TCHAR* RejectReason = nullptr;
	return TryAddItem(ItemId, OutInstanceId, RejectReason);
}

bool UHeistInventoryComponent::TryAddItem(const FName ItemId, int32& OutInstanceId, const TCHAR*& OutRejectReason)
{
	OutInstanceId = INDEX_NONE;
	OutRejectReason = nullptr;

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		OutRejectReason = TEXT("RequiresAuthority");
		UHeistDebugFunctionLibrary::DebugInventoryAddRejected(OwnerActor, ItemId, TEXT("RequiresAuthority"));
		return false;
	}

	FHeistItemDataRow ItemDefinition;
	if (!TryGetItemDefinition(ItemId, ItemDefinition) || !ItemDefinition.bAvailableInV1)
	{
		OutRejectReason = TEXT("InvalidItemDefinition");
		UHeistDebugFunctionLibrary::DebugInventoryAddRejected(OwnerActor, ItemId, TEXT("InvalidItemDefinition"));
		return false;
	}

	FIntPoint GridPosition = FIntPoint(-1, -1);
	bool bRotated = false;
	if (!TryFindAutoPlacement(ItemDefinition, GridPosition, bRotated))
	{
		OutRejectReason = TEXT("InventoryFull");
		UHeistDebugFunctionLibrary::DebugInventoryAddRejected(OwnerActor, ItemId, TEXT("InventoryFull"), GridColumnCount, GridRowCount);
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
	const FIntPoint PlacedSize = bRotated ? FIntPoint(ItemDefinition.GridSize.Y, ItemDefinition.GridSize.X) : ItemDefinition.GridSize;

	UHeistDebugFunctionLibrary::DebugInventoryItemAdded(OwnerActor, ItemId, OutInstanceId, GridPosition, PlacedSize, bRotated, ReplicatedInventory.Items.Num());

	return true;
}

bool UHeistInventoryComponent::TryGetItem(const int32 InstanceId, FHeistInventoryItem& OutInventoryItem) const
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

	const FIntPoint ItemSize = ItemEntry->InventoryItem.bRotated ? FIntPoint(ItemDefinition.GridSize.Y, ItemDefinition.GridSize.X) : ItemDefinition.GridSize;
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
	if (!TryGetItemDefinition(ItemEntry->InventoryItem.ItemId, ItemDefinition) || !ItemDefinition.bCanRotate)
	{
		return false;
	}

	TArray<bool> OccupiedCells;
	if (!TryBuildOccupiedCellsExcluding(InstanceId, OccupiedCells))
	{
		return false;
	}

	const FIntPoint RotatedSize = ItemEntry->InventoryItem.bRotated ? ItemDefinition.GridSize : FIntPoint(ItemDefinition.GridSize.Y, ItemDefinition.GridSize.X);
	if (!CanPlaceAt(OccupiedCells, ItemEntry->InventoryItem.GridPosition, RotatedSize))
	{
		return false;
	}

	ItemEntry->InventoryItem.bRotated = !ItemEntry->InventoryItem.bRotated;
	ReplicatedInventory.MarkItemDirty(*ItemEntry);
	OwnerActor->ForceNetUpdate();
	NotifyInventoryChanged();
	UHeistDebugFunctionLibrary::DebugInventoryItemRotated(OwnerActor, InstanceId, ItemEntry->InventoryItem.bRotated);
	return true;
}

bool UHeistInventoryComponent::TryRemoveItem(const int32 InstanceId, FHeistInventoryItem& OutRemovedItem)
{
	OutRemovedItem = FHeistInventoryItem();
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return false;
	}

	const int32 ItemIndex = ReplicatedInventory.Items.IndexOfByPredicate([InstanceId](const FHeistInventoryFastArrayItem& Entry) { return Entry.InventoryItem.InstanceId == InstanceId; });
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

	UHeistDebugFunctionLibrary::DebugInventoryItemRemoved(OwnerActor, OutRemovedItem.ItemId, InstanceId, ReplicatedInventory.Items.Num());
	return true;
}

bool UHeistInventoryComponent::TryAssignQuickSlot(const EHeistQuickSlotType SlotType, const int32 InstanceId)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return false;
	}

	FHeistInventoryItem InventoryItem;
	FHeistItemDataRow ItemDefinition;
	FHeistQuickSlotState* QuickSlot = FindQuickSlot(SlotType);
	if (QuickSlot == nullptr || !TryGetItem(InstanceId, InventoryItem) || !TryGetItemDefinition(InventoryItem.ItemId, ItemDefinition) || !ItemDefinition.bCanUseQuickSlot ||
		ResolveQuickSlotType(InventoryItem.ItemId) != SlotType)
	{
		return false;
	}

	QuickSlot->ItemInstanceId = InstanceId;
	OwnerActor->ForceNetUpdate();
	NotifyInventoryChanged();
	UHeistDebugFunctionLibrary::DebugQuickSlotAssigned(OwnerActor, static_cast<int32>(SlotType), InstanceId, InventoryItem.ItemId);
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

bool UHeistInventoryComponent::IsCarryingOriginal() const
{
	return OriginalCarryEntry.IsValid();
}

const FHeistOriginalCarryEntry& UHeistInventoryComponent::GetOriginalCarryEntry() const
{
	return OriginalCarryEntry;
}

bool UHeistInventoryComponent::TryBeginOriginalCarry(AHeistPlayerState* CarryingPlayerState, const FName ArtifactId, const int32 ArtifactValue, const float Weight, const bool bRequiredTarget,
															 AActor* SourceDisplayCase)
{
	AHeistPlayerCharacter* OwnerCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	const TCHAR* RejectReason = nullptr;
	if (!IsValid(OwnerCharacter))
	{
		RejectReason = TEXT("InvalidInventoryOwner");
	}
	else if (!OwnerCharacter->HasAuthority())
	{
		RejectReason = TEXT("NotAuthority");
	}
	else if (!IsValid(CarryingPlayerState) || CarryingPlayerState->GetPawn() != OwnerCharacter)
	{
		RejectReason = TEXT("PlayerStatePawnMismatch");
	}
	else if (ArtifactId.IsNone() || ArtifactValue <= 0 || !FMath::IsFinite(Weight) || Weight < 0.0f || !IsValid(SourceDisplayCase))
	{
		RejectReason = TEXT("InvalidCarryDefinition");
	}
	else if (IsCarryingOriginal())
	{
		RejectReason = TEXT("AlreadyCarryingOriginal");
	}
	else if (!CarryingPlayerState->CanAddLootScoreAndWeight(0, Weight))
	{
		RejectReason = TEXT("CarryWeightRejected");
	}

	if (RejectReason != nullptr)
	{
		UE_LOG(LogHeistInventory, Warning, TEXT("Original carry entry begin rejected: Owner=%s PlayerState=%s Artifact=%s Weight=%.1f SourceCase=%s Reason=%s"), *GetNameSafe(OwnerCharacter),
			   *GetNameSafe(CarryingPlayerState), *ArtifactId.ToString(), Weight, *GetNameSafe(SourceDisplayCase), RejectReason);
		return false;
	}

	if (!CarryingPlayerState->AddLootScoreAndWeight(0, Weight))
	{
		UE_LOG(LogHeistInventory, Warning, TEXT("Original carry entry begin rejected: Owner=%s PlayerState=%s Artifact=%s Weight=%.1f Reason=WeightCommitFailed"), *GetNameSafe(OwnerCharacter),
			   *GetNameSafe(CarryingPlayerState), *ArtifactId.ToString(), Weight);
		return false;
	}

	OriginalCarryEntry.ArtifactId = ArtifactId;
	OriginalCarryEntry.ArtifactValue = ArtifactValue;
	OriginalCarryEntry.Weight = Weight;
	OriginalCarryEntry.bRequiredTarget = bRequiredTarget;
	OriginalCarryEntry.SourceDisplayCase = SourceDisplayCase;
	OwnerCharacter->ForceNetUpdate();
	NotifyInventoryChanged();
	if (AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr)
	{
		HeistGameState->RefreshContractCarriedValue();
	}
	return true;
}

bool UHeistInventoryComponent::TryEndOriginalCarry(AHeistPlayerState* CarryingPlayerState, AActor* ExpectedSourceDisplayCase, FHeistOriginalCarryEntry& OutReleasedEntry)
{
	OutReleasedEntry = FHeistOriginalCarryEntry();
	AHeistPlayerCharacter* OwnerCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	if (!IsValid(OwnerCharacter) || !OwnerCharacter->HasAuthority() || !IsValid(CarryingPlayerState) || CarryingPlayerState->GetPawn() != OwnerCharacter || !OriginalCarryEntry.IsValid() ||
		(IsValid(ExpectedSourceDisplayCase) && OriginalCarryEntry.SourceDisplayCase != ExpectedSourceDisplayCase) || !CarryingPlayerState->RemoveCarriedOriginalWeight(OriginalCarryEntry.Weight))
	{
		return false;
	}

	OutReleasedEntry = OriginalCarryEntry;
	OriginalCarryEntry = FHeistOriginalCarryEntry();
	OwnerCharacter->ForceNetUpdate();
	NotifyInventoryChanged();
	if (AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr)
	{
		HeistGameState->RefreshContractCarriedValue();
	}
	return true;
}

bool UHeistInventoryComponent::TryBuildPlayerDepositPayload(FHeistPlayerDepositPayload& OutPayload, const TCHAR*& OutRejectReason) const
{
	OutPayload = FHeistPlayerDepositPayload();
	OutRejectReason = nullptr;

	const AHeistPlayerCharacter* OwnerCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	const AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	if (!IsValid(OwnerCharacter) || !OwnerCharacter->HasAuthority())
	{
		OutRejectReason = TEXT("RequiresAuthority");
		return false;
	}
	if (!IsValid(HeistGameMode))
	{
		OutRejectReason = TEXT("MissingAuthGameMode");
		return false;
	}

	int64 LooseLootValue = 0;
	double LooseLootWeight = 0.0;
	for (const FHeistInventoryFastArrayItem& Entry : ReplicatedInventory.Items)
	{
		const FHeistInventoryItem& InventoryItem = Entry.InventoryItem;
		FHeistItemDataRow ItemDefinition;
		if (!HeistGameMode->TryGetItemDefinition(InventoryItem.ItemId, ItemDefinition))
		{
			OutRejectReason = TEXT("InvalidItemDefinition");
			return false;
		}
		if (ItemDefinition.ItemType != EHeistItemType::Loot)
		{
			continue;
		}

		FHeistLootDataRow LootDefinition;
		if (!HeistGameMode->TryGetLootDefinition(InventoryItem.ItemId, LootDefinition) || InventoryItem.Quantity <= 0)
		{
			OutRejectReason = TEXT("InvalidLootDefinition");
			return false;
		}

		LooseLootValue += static_cast<int64>(LootDefinition.ScoreValue) * static_cast<int64>(InventoryItem.Quantity);
		LooseLootWeight += static_cast<double>(ItemDefinition.Weight) * static_cast<double>(InventoryItem.Quantity);
		OutPayload.LooseLootItemCount += InventoryItem.Quantity;
		if (LooseLootValue > MAX_int32 || !FMath::IsFinite(LooseLootWeight) || LooseLootWeight > static_cast<double>(TNumericLimits<float>::Max()))
		{
			OutRejectReason = TEXT("DepositValueOverflow");
			return false;
		}
	}

	OutPayload.LooseLootValue = static_cast<int32>(LooseLootValue);
	OutPayload.LooseLootWeight = static_cast<float>(LooseLootWeight);
	OutPayload.OriginalCarry = OriginalCarryEntry;
	const int64 TotalValue = static_cast<int64>(OutPayload.LooseLootValue) + static_cast<int64>(OutPayload.GetOriginalValue());
	if ((OutPayload.HasDeposit() && TotalValue <= 0) || TotalValue > MAX_int32 || !FMath::IsFinite(OutPayload.GetTotalWeight()))
	{
		OutRejectReason = TEXT("InvalidDepositTotals");
		return false;
	}

	return true;
}

bool UHeistInventoryComponent::TryCommitPlayerDeposit(AHeistPlayerState* DepositingPlayerState, const FHeistPlayerDepositPayload& ExpectedPayload,
												  FHeistPlayerDepositPayload& OutCommittedPayload, const TCHAR*& OutRejectReason)
{
	OutCommittedPayload = FHeistPlayerDepositPayload();
	OutRejectReason = nullptr;

	AHeistPlayerCharacter* OwnerCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	if (!IsValid(OwnerCharacter) || !OwnerCharacter->HasAuthority() || !IsValid(DepositingPlayerState) || DepositingPlayerState->GetPawn() != OwnerCharacter)
	{
		OutRejectReason = TEXT("InvalidDepositOwner");
		return false;
	}

	FHeistPlayerDepositPayload CurrentPayload;
	if (!TryBuildPlayerDepositPayload(CurrentPayload, OutRejectReason) || !CurrentPayload.Matches(ExpectedPayload))
	{
		OutRejectReason = OutRejectReason != nullptr ? OutRejectReason : TEXT("DepositPayloadChanged");
		return false;
	}
	if (!DepositingPlayerState->CanRemoveLootScoreAndWeight(CurrentPayload.LooseLootValue, CurrentPayload.LooseLootWeight))
	{
		OutRejectReason = TEXT("LooseLootTotalsMismatch");
		return false;
	}
	if (CurrentPayload.OriginalCarry.IsValid() && CurrentPayload.OriginalCarry.Weight > DepositingPlayerState->GetTotalLootWeight() - CurrentPayload.LooseLootWeight + KINDA_SMALL_NUMBER)
	{
		OutRejectReason = TEXT("OriginalWeightMismatch");
		return false;
	}

	if (CurrentPayload.LooseLootItemCount > 0 && !DepositingPlayerState->RemoveLootScoreAndWeight(CurrentPayload.LooseLootValue, CurrentPayload.LooseLootWeight))
	{
		OutRejectReason = TEXT("LooseLootTotalsCommitFailed");
		return false;
	}
	if (CurrentPayload.OriginalCarry.IsValid() && !DepositingPlayerState->RemoveCarriedOriginalWeight(CurrentPayload.OriginalCarry.Weight))
	{
		OutRejectReason = TEXT("OriginalWeightCommitFailed");
		return false;
	}

	int32 RemovedLooseLootEntryCount = 0;
	for (int32 ItemIndex = ReplicatedInventory.Items.Num() - 1; ItemIndex >= 0; --ItemIndex)
	{
		FHeistItemDataRow ItemDefinition;
		const FHeistInventoryItem& InventoryItem = ReplicatedInventory.Items[ItemIndex].InventoryItem;
		if (TryGetItemDefinition(InventoryItem.ItemId, ItemDefinition) && ItemDefinition.ItemType == EHeistItemType::Loot)
		{
			ClearQuickSlotReferences(InventoryItem.InstanceId);
			ReplicatedInventory.Items.RemoveAt(ItemIndex);
			++RemovedLooseLootEntryCount;
		}
	}
	if (RemovedLooseLootEntryCount > 0)
	{
		ReplicatedInventory.MarkArrayDirty();
	}
	OriginalCarryEntry = FHeistOriginalCarryEntry();

	OwnerCharacter->ForceNetUpdate();
	NotifyInventoryChanged();
	OutCommittedPayload = CurrentPayload;
	return true;
}

bool UHeistInventoryComponent::TryFindAutoPlacement(const FHeistItemDataRow& ItemDefinition, FIntPoint& OutGridPosition, bool& bOutRotated) const
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

bool UHeistInventoryComponent::TryBuildOccupiedCellsExcluding(const int32 ExcludedInstanceId, TArray<bool>& OutOccupiedCells) const
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
			UHeistDebugFunctionLibrary::DebugInventoryOccupancyInvalid(ExistingItem.InstanceId, ExistingItem.ItemId, TEXT("InvalidItemDefinition"));
			return false;
		}

		const FIntPoint ExistingSize = ExistingItem.bRotated ? FIntPoint(ExistingDefinition.GridSize.Y, ExistingDefinition.GridSize.X) : ExistingDefinition.GridSize;

		if (!CanPlaceAt(OutOccupiedCells, ExistingItem.GridPosition, ExistingSize))
		{
			UHeistDebugFunctionLibrary::DebugInventoryOccupancyInvalid(ExistingItem.InstanceId, ExistingItem.ItemId, TEXT("OutOfBoundsOrOverlap"), ExistingItem.GridPosition, ExistingSize);
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
	return ReplicatedInventory.Items.FindByPredicate([InstanceId](const FHeistInventoryFastArrayItem& Entry) { return Entry.InventoryItem.InstanceId == InstanceId; });
}

const FHeistInventoryFastArrayItem* UHeistInventoryComponent::FindItemEntry(const int32 InstanceId) const
{
	return ReplicatedInventory.Items.FindByPredicate([InstanceId](const FHeistInventoryFastArrayItem& Entry) { return Entry.InventoryItem.InstanceId == InstanceId; });
}

FHeistQuickSlotState* UHeistInventoryComponent::FindQuickSlot(const EHeistQuickSlotType SlotType)
{
	return QuickSlots.FindByPredicate([SlotType](const FHeistQuickSlotState& QuickSlot) { return QuickSlot.SlotType == SlotType; });
}

const FHeistQuickSlotState* UHeistInventoryComponent::FindQuickSlot(const EHeistQuickSlotType SlotType) const
{
	return QuickSlots.FindByPredicate([SlotType](const FHeistQuickSlotState& QuickSlot) { return QuickSlot.SlotType == SlotType; });
}

EHeistQuickSlotType UHeistInventoryComponent::ResolveQuickSlotType(const FName ItemId) const
{
	if (ItemId == CoinItemId)
	{
		return EHeistQuickSlotType::Coin;
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

void UHeistInventoryComponent::OnRep_OriginalCarryEntry()
{
	NotifyInventoryChanged();
}

bool UHeistInventoryComponent::CanPlaceAt(const TArray<bool>& OccupiedCells, const FIntPoint& GridPosition, const FIntPoint& ItemSize)
{
	if (GridPosition.X < 0 || GridPosition.Y < 0 || ItemSize.X <= 0 || ItemSize.Y <= 0 || GridPosition.X + ItemSize.X > GridColumnCount || GridPosition.Y + ItemSize.Y > GridRowCount ||
		OccupiedCells.Num() != GridColumnCount * GridRowCount)
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
	checkf(IsValid(OwnerActor) && OwnerActor->HasAuthority(), TEXT("Inventory InstanceId allocation requires an authoritative owner."));
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
	DOREPLIFETIME_CONDITION(UHeistInventoryComponent, OriginalCarryEntry, COND_OwnerOnly);
}

#pragma endregion
