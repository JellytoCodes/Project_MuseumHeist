#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "Inventory/HeistInventoryTypes.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistInventoryWidget.generated.h"

class UButton;
class UCanvasPanel;
class UDataTable;
class UHeistInventoryFrameWidget;
class UHeistInventoryItemWidget;
class UHeistInventorySlotWidget;
class UTextBlock;
class UTexture2D;
class UUniformGridPanel;

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
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

#pragma endregion

#pragma region ViewModels

  public:
	void SetupInventoryWidget(class UHeistInventoryViewModel* InInventoryViewModel, class AHeistPlayerController* InPlayerController);

  private:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistInventoryViewModel> InventoryViewModel;

	UPROPERTY(Transient)
	TObjectPtr<AHeistPlayerController> PlayerController;

	void RefreshVisibilityFromConfirmedSnapshot();

  protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|Inventory", meta = (DisplayName = "Refresh Confirmed Inventory"))
	void BP_RefreshConfirmedInventory(const TArray<FHeistInventoryItem>& ConfirmedItems, int32 GridColumns, int32 GridRows);

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

#pragma endregion

#pragma region DragDropPresentation

  public:
	bool CanPreviewItemDrop(int32 InstanceId, const FIntPoint& TargetGridPosition) const;

	private:
	void RebuildConfirmedInventory(const TArray<FHeistInventoryItem>& ConfirmedItems, int32 GridColumns, int32 GridRows);
	void RefreshItemOverlayLayout();
	FVector2D ResolveInventoryCellSize() const;
	bool TryResolveItemPresentation(const FHeistInventoryItem& InventoryItem, FIntPoint& OutPlacedSize, UTexture2D*& OutIcon) const;
	bool TryGetDropTargetGridPosition(const FDragDropEvent& DragDropEvent, FIntPoint& OutGridPosition) const;
	void UpdateDropPreview(int32 InstanceId, const FIntPoint& TargetGridPosition);
	void ClearDropPreview();
	bool IsGridCoordinateOccupied(const FIntPoint& GridCoordinate, int32 ExcludedInstanceId = INDEX_NONE) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Inventory|Presentation", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UHeistInventorySlotWidget> InventorySlotWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Inventory|Presentation", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UHeistInventoryItemWidget> InventoryItemWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Inventory|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDataTable> ItemDataTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Inventory|Presentation", meta = (AllowPrivateAccess = "true"))
	FVector2D InventoryCellSize = FVector2D(100.0, 100.0);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHeistInventorySlotWidget>> InventorySlotWidgets;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHeistInventoryItemWidget>> InventoryItemWidgets;

	UPROPERTY(Transient)
	TArray<FHeistInventoryItem> ConfirmedInventoryItems;

	UPROPERTY(Transient)
	int32 ConfirmedGridColumns = 0;

	UPROPERTY(Transient)
	int32 ConfirmedGridRows = 0;

	bool bItemOverlayLayoutDirty = true;
	FVector2D LastGridTopLeftInOverlay = FVector2D::ZeroVector;
	FVector2D LastGridSizeInOverlay = FVector2D::ZeroVector;

#pragma endregion

#pragma region Presentation

  private:
	UFUNCTION()
	void HandleCloseButtonClicked();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> InventorySummaryText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UHeistInventoryFrameWidget> InventoryFrameWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> InventoryGrid;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> ItemOverlay;

#pragma endregion
};
