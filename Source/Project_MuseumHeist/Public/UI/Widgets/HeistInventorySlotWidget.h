#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistInventorySlotWidget.generated.h"

class UBorder;
class UDragDropOperation;
class UHeistInventoryWidget;
class UTextBlock;

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistInventorySlotWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

#pragma region Construction

public:
	UHeistInventorySlotWidget(const FObjectInitializer& ObjectInitializer);

#pragma endregion

#pragma region Presentation

public:
	void SetupSlot(
		const FIntPoint& InGridCoordinate,
		bool bInOccupied,
		UHeistInventoryWidget* InInventoryWidget);
	void SetOccupied(bool bInOccupied);
	void SetDropPreview(bool bVisible, bool bValid);

	UFUNCTION(BlueprintPure, Category = "Heist|Inventory")
	FIntPoint GetGridCoordinate() const;

protected:
	virtual void NativeOnDragEnter(
		const FGeometry& InGeometry,
		const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(
		const FGeometry& InGeometry,
		const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;

private:
	void RefreshPresentation();

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|Inventory", meta = (AllowPrivateAccess = "true"))
	FIntPoint GridCoordinate = FIntPoint(-1, -1);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|Inventory", meta = (AllowPrivateAccess = "true"))
	bool bOccupied = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|Inventory", meta = (AllowPrivateAccess = "true"))
	bool bDropPreviewVisible = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|Inventory", meta = (AllowPrivateAccess = "true"))
	bool bDropPreviewValid = false;

	UPROPERTY(Transient)
	TObjectPtr<UHeistInventoryWidget> InventoryWidget;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UBorder> SlotBackground;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> CoordinateText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> OccupancyText;

#pragma endregion
};
