#include "UI/ViewModels/HeistInventoryViewModel.h"

#include "Character/Components/HeistInventoryComponent.h"
#include "Core/HeistGameState.h"
#include "Core/HeistLogChannels.h"
#include "Engine/World.h"

void UHeistInventoryViewModel::BeginDestroy()
{
	if (IsValid(InventoryComponent))
	{
		InventoryComponent->GetInventoryChangedDelegate().RemoveAll(this);
	}
	if (IsValid(GameState))
	{
		GameState->GetContractSnapshotChangedDelegate().RemoveAll(this);
	}
	if (BoundWorld.IsValid())
	{
		BoundWorld->GameStateSetEvent.RemoveAll(this);
	}

	Super::BeginDestroy();
}

void UHeistInventoryViewModel::SetupViewModel(UHeistInventoryComponent* InInventoryComponent)
{
	if (InventoryComponent != InInventoryComponent && IsValid(InventoryComponent))
	{
		InventoryComponent->GetInventoryChangedDelegate().RemoveAll(this);
	}
	if (BoundWorld.IsValid())
	{
		BoundWorld->GameStateSetEvent.RemoveAll(this);
	}

	InventoryComponent = InInventoryComponent;
	if (IsValid(InventoryComponent))
	{
		InventoryComponent->GetInventoryChangedDelegate().RemoveAll(this);
		InventoryComponent->GetInventoryChangedDelegate().AddUObject(this, &UHeistInventoryViewModel::RefreshConfirmedSnapshot);
	}

	BoundWorld = IsValid(InventoryComponent) ? InventoryComponent->GetWorld() : nullptr;
	if (BoundWorld.IsValid())
	{
		BoundWorld->GameStateSetEvent.AddUObject(this, &UHeistInventoryViewModel::HandleGameStateSet);
	}
	HandleGameStateSet(BoundWorld.IsValid() ? BoundWorld->GetGameState() : nullptr);
}

void UHeistInventoryViewModel::HandleGameStateSet(AGameStateBase* InGameState)
{
	if (IsValid(GameState))
	{
		GameState->GetContractSnapshotChangedDelegate().RemoveAll(this);
	}
	GameState = Cast<AHeistGameState>(InGameState);
	if (IsValid(GameState))
	{
		GameState->GetContractSnapshotChangedDelegate().AddUObject(this, &UHeistInventoryViewModel::HandleContractSnapshotChanged);
	}
	RefreshConfirmedSnapshot();
}

void UHeistInventoryViewModel::HandleContractSnapshotChanged(const FHeistContractSnapshot& ContractSnapshot)
{
	const bool bContractInitialized = ContractSnapshot.IsInitialized();
	UE_MVVM_SET_PROPERTY_VALUE(RequiredQuota, bContractInitialized ? ContractSnapshot.LootValueQuota : 0);
	UE_MVVM_SET_PROPERTY_VALUE(CarriedValue, bContractInitialized ? ContractSnapshot.CarriedValue : 0);
	UE_MVVM_SET_PROPERTY_VALUE(SecuredValue, bContractInitialized ? ContractSnapshot.SecuredValue : 0);
	SnapshotChangedDelegate.Broadcast();
}

void UHeistInventoryViewModel::RefreshConfirmedSnapshot()
{
	TArray<FHeistInventoryItem> ConfirmedItems;
	if (IsValid(InventoryComponent))
	{
		const TArray<FHeistInventoryFastArrayItem>& ReplicatedItems = InventoryComponent->GetReplicatedInventory().Items;
		ConfirmedItems.Reserve(ReplicatedItems.Num());
		for (const FHeistInventoryFastArrayItem& ReplicatedItem : ReplicatedItems)
		{
			ConfirmedItems.Add(ReplicatedItem.InventoryItem);
		}
	}

	UE_MVVM_SET_PROPERTY_VALUE(Items, ConfirmedItems);
	UE_MVVM_SET_PROPERTY_VALUE(bInventoryOpen, IsValid(InventoryComponent) && InventoryComponent->IsInventoryOpen());

	GridColumnCount = UHeistInventoryComponent::GridColumnCount;
	GridRowCount = UHeistInventoryComponent::GridRowCount;
	int32 OriginalItemCount = 0;
	int32 ConfirmedItemCount = 0;
	float ConfirmedTotalWeight = 0.0f;
	for (const FHeistInventoryItem& Item : ConfirmedItems)
	{
		const int32 SafeQuantity = FMath::Max(1, Item.Quantity);
		ConfirmedItemCount += SafeQuantity;
		ConfirmedTotalWeight += FMath::Max(0.0f, Item.Weight) * SafeQuantity;
		if (Item.IsOriginalArtifact())
		{
			++OriginalItemCount;
		}
	}
	UE_MVVM_SET_PROPERTY_VALUE(ItemCount, ConfirmedItemCount);
	UE_MVVM_SET_PROPERTY_VALUE(TotalWeight, ConfirmedTotalWeight);
	UE_LOG(LogHeistUI, Log, TEXT("Inventory grid presentation refreshed: GridItems=%d ItemCount=%d TotalWeight=%.1f OriginalItems=%d Result=PASS"), ConfirmedItems.Num(), ItemCount, TotalWeight,
		   OriginalItemCount);
	HandleContractSnapshotChanged(IsValid(GameState) ? GameState->GetContractSnapshot() : FHeistContractSnapshot());
}

FHeistInventorySnapshotChanged& UHeistInventoryViewModel::GetSnapshotChangedDelegate()
{
	return SnapshotChangedDelegate;
}

const TArray<FHeistInventoryItem>& UHeistInventoryViewModel::GetItems() const
{
	return Items;
}

bool UHeistInventoryViewModel::IsInventoryOpen() const
{
	return bInventoryOpen;
}

int32 UHeistInventoryViewModel::GetGridColumnCount() const
{
	return GridColumnCount;
}

int32 UHeistInventoryViewModel::GetGridRowCount() const
{
	return GridRowCount;
}

int32 UHeistInventoryViewModel::GetItemCount() const
{
	return ItemCount;
}

float UHeistInventoryViewModel::GetTotalWeight() const
{
	return TotalWeight;
}

int32 UHeistInventoryViewModel::GetRequiredQuota() const
{
	return RequiredQuota;
}

int32 UHeistInventoryViewModel::GetCarriedValue() const
{
	return CarriedValue;
}

int32 UHeistInventoryViewModel::GetSecuredValue() const
{
	return SecuredValue;
}
