#pragma once

#include "CoreMinimal.h"
#include "Inventory/HeistInventoryTypes.h"
#include "MVVMViewModelBase.h"

#include "HeistQuickSlotViewModel.generated.h"

DECLARE_MULTICAST_DELEGATE(FHeistQuickSlotSnapshotChanged);

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistQuickSlotPresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	EHeistQuickSlotType SlotType = EHeistQuickSlotType::None;

	UPROPERTY(BlueprintReadOnly)
	FText KeyLabel;

	UPROPERTY(BlueprintReadOnly)
	bool bAssigned = false;

	UPROPERTY(BlueprintReadOnly)
	int32 ItemInstanceId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	FName ItemId = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	int32 Quantity = 0;

	bool operator==(const FHeistQuickSlotPresentation& Other) const
	{
		return SlotType == Other.SlotType
			&& KeyLabel.EqualTo(Other.KeyLabel)
			&& bAssigned == Other.bAssigned
			&& ItemInstanceId == Other.ItemInstanceId
			&& ItemId == Other.ItemId
			&& Quantity == Other.Quantity;
	}
};

UCLASS(BlueprintType)
class PROJECT_MUSEUMHEIST_API UHeistQuickSlotViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

#pragma region Construction

public:
	UHeistQuickSlotViewModel(const FObjectInitializer& ObjectInitializer);

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void BeginDestroy() override;

#pragma endregion

#pragma region Setup

public:
	void SetupViewModel(class UHeistInventoryComponent* InInventoryComponent);
	void RefreshConfirmedSnapshot();
	const TArray<FHeistQuickSlotState>& GetQuickSlots() const;
	const TArray<FHeistQuickSlotPresentation>& GetQuickSlotPresentations() const;
	FHeistQuickSlotSnapshotChanged& GetSnapshotChangedDelegate();

private:
	static FText GetKeyLabel(EHeistQuickSlotType SlotType);

	UPROPERTY(Transient)
	TObjectPtr<UHeistInventoryComponent> InventoryComponent;

	FHeistQuickSlotSnapshotChanged SnapshotChangedDelegate;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|QuickSlot", meta = (AllowPrivateAccess = "true"))
	TArray<FHeistQuickSlotState> QuickSlots;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|QuickSlot", meta = (AllowPrivateAccess = "true"))
	TArray<FHeistQuickSlotPresentation> QuickSlotPresentations;

#pragma endregion
};
