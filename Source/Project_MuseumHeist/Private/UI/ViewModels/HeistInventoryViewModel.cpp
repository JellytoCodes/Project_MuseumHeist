#include "UI/ViewModels/HeistInventoryViewModel.h"

#include "Character/Components/HeistInventoryComponent.h"

UHeistInventoryViewModel::UHeistInventoryViewModel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UHeistInventoryViewModel::BeginDestroy()
{
	if (IsValid(InventoryComponent))
	{
		InventoryComponent->GetInventoryChangedDelegate().RemoveAll(this);
	}

	Super::BeginDestroy();
}

void UHeistInventoryViewModel::SetupViewModel(UHeistInventoryComponent* InInventoryComponent)
{
	if (InventoryComponent != InInventoryComponent && IsValid(InventoryComponent))
	{
		InventoryComponent->GetInventoryChangedDelegate().RemoveAll(this);
	}

	InventoryComponent = InInventoryComponent;
	if (IsValid(InventoryComponent))
	{
		InventoryComponent->GetInventoryChangedDelegate().RemoveAll(this);
		InventoryComponent->GetInventoryChangedDelegate().AddUObject(
			this,
			&UHeistInventoryViewModel::RefreshConfirmedSnapshot);
	}

	RefreshConfirmedSnapshot();
}

void UHeistInventoryViewModel::RefreshConfirmedSnapshot()
{
	TArray<FHeistInventoryItem> ConfirmedItems;
	if (IsValid(InventoryComponent))
	{
		const TArray<FHeistInventoryFastArrayItem>& ReplicatedItems =
			InventoryComponent->GetReplicatedInventory().Items;
		ConfirmedItems.Reserve(ReplicatedItems.Num());
		for (const FHeistInventoryFastArrayItem& ReplicatedItem : ReplicatedItems)
		{
			ConfirmedItems.Add(ReplicatedItem.InventoryItem);
		}
	}

	UE_MVVM_SET_PROPERTY_VALUE(Items, ConfirmedItems);
	UE_MVVM_SET_PROPERTY_VALUE(
		bInventoryOpen,
		IsValid(InventoryComponent) && InventoryComponent->IsInventoryOpen());

	GridColumnCount = UHeistInventoryComponent::GridColumnCount;
	GridRowCount = UHeistInventoryComponent::GridRowCount;
	SnapshotChangedDelegate.Broadcast();
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
