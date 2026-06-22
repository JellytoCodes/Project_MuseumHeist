#include "UI/ViewModels/HeistQuickSlotViewModel.h"

#include "Character/Components/HeistInventoryComponent.h"

UHeistQuickSlotViewModel::UHeistQuickSlotViewModel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UHeistQuickSlotViewModel::BeginDestroy()
{
	if (IsValid(InventoryComponent))
	{
		InventoryComponent->GetInventoryChangedDelegate().RemoveAll(this);
	}

	Super::BeginDestroy();
}

void UHeistQuickSlotViewModel::SetupViewModel(UHeistInventoryComponent* InInventoryComponent)
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
			&UHeistQuickSlotViewModel::RefreshConfirmedSnapshot);
	}

	RefreshConfirmedSnapshot();
}

void UHeistQuickSlotViewModel::RefreshConfirmedSnapshot()
{
	const TArray<FHeistQuickSlotState> ConfirmedQuickSlots = IsValid(InventoryComponent)
		? InventoryComponent->GetQuickSlots()
		: TArray<FHeistQuickSlotState>();
	UE_MVVM_SET_PROPERTY_VALUE(QuickSlots, ConfirmedQuickSlots);
	SnapshotChangedDelegate.Broadcast();
}

const TArray<FHeistQuickSlotState>& UHeistQuickSlotViewModel::GetQuickSlots() const
{
	return QuickSlots;
}

FHeistQuickSlotSnapshotChanged& UHeistQuickSlotViewModel::GetSnapshotChangedDelegate()
{
	return SnapshotChangedDelegate;
}
