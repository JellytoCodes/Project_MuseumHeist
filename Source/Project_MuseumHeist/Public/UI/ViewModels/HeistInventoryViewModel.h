#pragma once

#include "CoreMinimal.h"
#include "Inventory/HeistInventoryTypes.h"
#include "MVVMViewModelBase.h"

#include "HeistInventoryViewModel.generated.h"

class AGameStateBase;
class AHeistGameState;
class UWorld;
struct FHeistContractSnapshot;

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
	void HandleGameStateSet(AGameStateBase* InGameState);
	void HandleContractSnapshotChanged(const FHeistContractSnapshot& ContractSnapshot);

	UPROPERTY(Transient)
	TObjectPtr<UHeistInventoryComponent> InventoryComponent;

	UPROPERTY(Transient)
	TObjectPtr<AHeistGameState> GameState;

	TWeakObjectPtr<UWorld> BoundWorld;
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
	int32 GetRequiredQuota() const;
	int32 GetCarriedValue() const;
	int32 GetSecuredValue() const;

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

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Inventory|Contract", meta = (AllowPrivateAccess = "true"))
	int32 RequiredQuota = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Inventory|Contract", meta = (AllowPrivateAccess = "true"))
	int32 CarriedValue = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Inventory|Contract", meta = (AllowPrivateAccess = "true"))
	int32 SecuredValue = 0;

#pragma endregion
};
