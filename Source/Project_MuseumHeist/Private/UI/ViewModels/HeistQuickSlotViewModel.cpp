#include "UI/ViewModels/HeistQuickSlotViewModel.h"

#include "Character/Components/HeistInventoryComponent.h"

UHeistQuickSlotViewModel::UHeistQuickSlotViewModel(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
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
		InventoryComponent->GetInventoryChangedDelegate().AddUObject(this, &UHeistQuickSlotViewModel::RefreshConfirmedSnapshot);
	}

	RefreshConfirmedSnapshot();
}

void UHeistQuickSlotViewModel::RefreshConfirmedSnapshot()
{
	const TArray<FHeistQuickSlotState> ConfirmedQuickSlots = IsValid(InventoryComponent) ? InventoryComponent->GetQuickSlots() : TArray<FHeistQuickSlotState>();

	TArray<FHeistQuickSlotPresentation> ConfirmedPresentations;
	ConfirmedPresentations.Reserve(ConfirmedQuickSlots.Num());
	for (const FHeistQuickSlotState& QuickSlot : ConfirmedQuickSlots)
	{
		FHeistQuickSlotPresentation& Presentation = ConfirmedPresentations.Emplace_GetRef();
		Presentation.SlotType = QuickSlot.SlotType;
		Presentation.KeyLabel = GetKeyLabel(QuickSlot.SlotType);
		Presentation.ItemInstanceId = QuickSlot.ItemInstanceId;

		FHeistInventoryItem InventoryItem;
		if (QuickSlot.ItemInstanceId != INDEX_NONE && IsValid(InventoryComponent) && InventoryComponent->TryGetItem(QuickSlot.ItemInstanceId, InventoryItem))
		{
			Presentation.bAssigned = true;
			Presentation.ItemId = InventoryItem.ItemId;
			Presentation.Quantity = InventoryItem.Quantity;
		}
	}

	UE_MVVM_SET_PROPERTY_VALUE(QuickSlots, ConfirmedQuickSlots);
	UE_MVVM_SET_PROPERTY_VALUE(QuickSlotPresentations, ConfirmedPresentations);
	SnapshotChangedDelegate.Broadcast();
}

const TArray<FHeistQuickSlotState>& UHeistQuickSlotViewModel::GetQuickSlots() const
{
	return QuickSlots;
}

const TArray<FHeistQuickSlotPresentation>& UHeistQuickSlotViewModel::GetQuickSlotPresentations() const
{
	return QuickSlotPresentations;
}

FHeistQuickSlotSnapshotChanged& UHeistQuickSlotViewModel::GetSnapshotChangedDelegate()
{
	return SnapshotChangedDelegate;
}

FText UHeistQuickSlotViewModel::GetKeyLabel(const EHeistQuickSlotType SlotType)
{
	switch (SlotType)
	{
	case EHeistQuickSlotType::Coin:
		return NSLOCTEXT("HeistQuickSlot", "CoinKey", "Q");
	default:
		return FText::GetEmpty();
	}
}
