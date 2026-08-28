#pragma once

#include "CoreMinimal.h"
#include "Inventory/HeistInventoryTypes.h"
#include "MVVMViewModelBase.h"

#include "HeistInventoryViewModel.generated.h"

DECLARE_MULTICAST_DELEGATE(FHeistInventorySnapshotChanged);

UCLASS(BlueprintType)
class PROJECT_MUSEUMHEIST_API UHeistInventoryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

#pragma region Lifecycle

  protected:
	virtual void BeginDestroy() override;

#pragma endregion

#pragma region Setup

  public:
	void SetupViewModel(class UHeistInventoryComponent* InInventoryComponent);
	void RefreshConfirmedSnapshot();
	FHeistInventorySnapshotChanged& GetSnapshotChangedDelegate();

  private:
	UPROPERTY(Transient)
	TObjectPtr<UHeistInventoryComponent> InventoryComponent;

	FHeistInventorySnapshotChanged SnapshotChangedDelegate;

#pragma endregion

#pragma region Snapshot

  public:
	const TArray<FHeistInventoryItem>& GetItems() const;
	bool IsInventoryOpen() const;
	int32 GetGridColumnCount() const;
	int32 GetGridRowCount() const;
	int32 GetItemCount() const;
	float GetTotalWeight() const;

  private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Inventory", meta = (AllowPrivateAccess = "true"))
	TArray<FHeistInventoryItem> Items;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Inventory", meta = (AllowPrivateAccess = "true"))
	bool bInventoryOpen = false;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Inventory", meta = (AllowPrivateAccess = "true"))
	int32 GridColumnCount = 5;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Inventory", meta = (AllowPrivateAccess = "true"))
	int32 GridRowCount = 5;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Inventory", meta = (AllowPrivateAccess = "true"))
	int32 ItemCount = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Inventory", meta = (AllowPrivateAccess = "true"))
	float TotalWeight = 0.0f;

#pragma endregion
};
