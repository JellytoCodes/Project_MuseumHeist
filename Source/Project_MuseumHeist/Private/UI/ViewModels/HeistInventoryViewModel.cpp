#include "UI/ViewModels/HeistInventoryViewModel.h"

#include "Character/Components/HeistInventoryComponent.h"
#include "Core/HeistLogChannels.h"

UHeistInventoryViewModel::UHeistInventoryViewModel(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
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
		InventoryComponent->GetInventoryChangedDelegate().AddUObject(this, &UHeistInventoryViewModel::RefreshConfirmedSnapshot);
	}

	RefreshConfirmedSnapshot();
}

void UHeistInventoryViewModel::RefreshConfirmedSnapshot()
{
	TArray<FHeistInventoryItem> ConfirmedItems;
	FHeistOriginalCarryEntry ConfirmedOriginalCarry;
	if (IsValid(InventoryComponent))
	{
		const TArray<FHeistInventoryFastArrayItem>& ReplicatedItems = InventoryComponent->GetReplicatedInventory().Items;
		ConfirmedItems.Reserve(ReplicatedItems.Num());
		for (const FHeistInventoryFastArrayItem& ReplicatedItem : ReplicatedItems)
		{
			ConfirmedItems.Add(ReplicatedItem.InventoryItem);
		}
		ConfirmedOriginalCarry = InventoryComponent->GetOriginalCarryEntry();
	}

	const bool bConfirmedCarryingOriginal = ConfirmedOriginalCarry.IsValid();
	FString OriginalArtifactLabel = ConfirmedOriginalCarry.ArtifactId.ToString();
	OriginalArtifactLabel.ReplaceInline(TEXT("_"), TEXT(" "));
	const FText ConfirmedOriginalCarrySummary = bConfirmedCarryingOriginal
		? FText::Format(NSLOCTEXT("HeistInventory", "OriginalCarrySummaryFormat", "ORIGINAL  {0}  |  VALUE {1}  |  WEIGHT {2}  |  {3}"),
						FText::FromString(OriginalArtifactLabel), FText::AsNumber(ConfirmedOriginalCarry.ArtifactValue), FText::AsNumber(ConfirmedOriginalCarry.Weight),
						ConfirmedOriginalCarry.bRequiredTarget ? NSLOCTEXT("HeistInventory", "OriginalCarryRequired", "REQUIRED")
															 : NSLOCTEXT("HeistInventory", "OriginalCarryOptional", "OPTIONAL"))
		: FText::GetEmpty();

	UE_MVVM_SET_PROPERTY_VALUE(Items, ConfirmedItems);
	UE_MVVM_SET_PROPERTY_VALUE(bInventoryOpen, IsValid(InventoryComponent) && InventoryComponent->IsInventoryOpen());
	UE_MVVM_SET_PROPERTY_VALUE(bCarryingOriginal, bConfirmedCarryingOriginal);
	UE_MVVM_SET_PROPERTY_VALUE(OriginalArtifactId, bConfirmedCarryingOriginal ? ConfirmedOriginalCarry.ArtifactId : NAME_None);
	UE_MVVM_SET_PROPERTY_VALUE(OriginalArtifactValue, bConfirmedCarryingOriginal ? ConfirmedOriginalCarry.ArtifactValue : 0);
	UE_MVVM_SET_PROPERTY_VALUE(OriginalCarryWeight, bConfirmedCarryingOriginal ? ConfirmedOriginalCarry.Weight : 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(bOriginalRequiredTarget, bConfirmedCarryingOriginal && ConfirmedOriginalCarry.bRequiredTarget);
	UE_MVVM_SET_PROPERTY_VALUE(OriginalCarrySummaryText, ConfirmedOriginalCarrySummary);

	GridColumnCount = UHeistInventoryComponent::GridColumnCount;
	GridRowCount = UHeistInventoryComponent::GridRowCount;
	UE_LOG(LogHeistUI, Log, TEXT("Inventory Original carry presentation refreshed: Active=%s Artifact=%s Value=%d Weight=%.1f Required=%s GridItems=%d Result=PASS"),
		   bConfirmedCarryingOriginal ? TEXT("true") : TEXT("false"), *ConfirmedOriginalCarry.ArtifactId.ToString(), ConfirmedOriginalCarry.ArtifactValue,
		   ConfirmedOriginalCarry.Weight, ConfirmedOriginalCarry.bRequiredTarget ? TEXT("true") : TEXT("false"), ConfirmedItems.Num());
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

bool UHeistInventoryViewModel::IsCarryingOriginal() const
{
	return bCarryingOriginal;
}

FName UHeistInventoryViewModel::GetOriginalArtifactId() const
{
	return OriginalArtifactId;
}

int32 UHeistInventoryViewModel::GetOriginalArtifactValue() const
{
	return OriginalArtifactValue;
}

float UHeistInventoryViewModel::GetOriginalCarryWeight() const
{
	return OriginalCarryWeight;
}

bool UHeistInventoryViewModel::IsOriginalRequiredTarget() const
{
	return bOriginalRequiredTarget;
}

const FText& UHeistInventoryViewModel::GetOriginalCarrySummaryText() const
{
	return OriginalCarrySummaryText;
}
