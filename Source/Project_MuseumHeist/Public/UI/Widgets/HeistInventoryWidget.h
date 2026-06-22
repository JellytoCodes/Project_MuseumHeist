#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "Inventory/HeistInventoryTypes.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistInventoryWidget.generated.h"

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistInventoryWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

#pragma region Construction

public:
	UHeistInventoryWidget(const FObjectInitializer& ObjectInitializer);

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void NativeDestruct() override;

#pragma endregion

#pragma region ViewModels

public:
	void SetupInventoryWidget(
		class UHeistInventoryViewModel* InInventoryViewModel,
		class UHeistQuickSlotViewModel* InQuickSlotViewModel,
		class AHeistPlayerController* InPlayerController);

private:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistInventoryViewModel> InventoryViewModel;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistQuickSlotViewModel> QuickSlotViewModel;

	UPROPERTY(Transient)
	TObjectPtr<AHeistPlayerController> PlayerController;

	void RefreshVisibilityFromConfirmedSnapshot();
	void RefreshQuickSlotPresentation();

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|Inventory", meta = (DisplayName = "Refresh Confirmed Inventory"))
	void BP_RefreshConfirmedInventory(
		const TArray<FHeistInventoryItem>& ConfirmedItems,
		int32 GridColumns,
		int32 GridRows);

	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|Inventory", meta = (DisplayName = "Refresh Confirmed QuickSlots"))
	void BP_RefreshConfirmedQuickSlots(const TArray<FHeistQuickSlotState>& ConfirmedQuickSlots);

#pragma endregion

#pragma region Requests

public:
	UFUNCTION(BlueprintCallable, Category = "Heist|Inventory")
	void RequestCloseInventory();

	UFUNCTION(BlueprintCallable, Category = "Heist|Inventory")
	void RequestMoveItem(int32 InstanceId, FIntPoint TargetGridPosition);

	UFUNCTION(BlueprintCallable, Category = "Heist|Inventory")
	void RequestRotateItem(int32 InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Inventory")
	void RequestDropItem(int32 InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Inventory")
	void RequestAssignQuickSlot(EHeistQuickSlotType SlotType, int32 InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Inventory")
	void RequestClearQuickSlot(EHeistQuickSlotType SlotType);

#pragma endregion
};
