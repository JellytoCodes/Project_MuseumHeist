#pragma once

#include "CoreMinimal.h"
#include "Inventory/HeistInventoryTypes.h"
#include "MVVMViewModelBase.h"

#include "HeistQuickSlotViewModel.generated.h"

DECLARE_MULTICAST_DELEGATE(FHeistQuickSlotSnapshotChanged);

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
	FHeistQuickSlotSnapshotChanged& GetSnapshotChangedDelegate();

private:
	UPROPERTY(Transient)
	TObjectPtr<UHeistInventoryComponent> InventoryComponent;

	FHeistQuickSlotSnapshotChanged SnapshotChangedDelegate;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|QuickSlot", meta = (AllowPrivateAccess = "true"))
	TArray<FHeistQuickSlotState> QuickSlots;

#pragma endregion
};
