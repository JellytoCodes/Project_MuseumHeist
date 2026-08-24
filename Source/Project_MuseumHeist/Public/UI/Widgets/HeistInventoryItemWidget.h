#pragma once

#include "CoreMinimal.h"
#include "Inventory/HeistInventoryTypes.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistInventoryItemWidget.generated.h"

class UBorder;
class UHeistInventoryWidget;
class UImage;
class UTextBlock;
class UTexture2D;

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistInventoryItemWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

#pragma region Construction

  public:
	UHeistInventoryItemWidget(const FObjectInitializer& ObjectInitializer);

#pragma endregion

#pragma region Presentation

  public:
	void SetupItem(const FHeistInventoryItem& InConfirmedItem, const FIntPoint& InPlacedSize, UTexture2D* InIcon, UHeistInventoryWidget* InInventoryWidget);

	UFUNCTION(BlueprintPure, Category = "Heist|Inventory")
	int32 GetInstanceId() const;

  protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

  private:
	void RefreshPresentation();

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|Inventory", meta = (AllowPrivateAccess = "true"))
	FHeistInventoryItem ConfirmedItem;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|Inventory", meta = (AllowPrivateAccess = "true"))
	FIntPoint PlacedSize = FIntPoint(1, 1);

	UPROPERTY(Transient)
	TObjectPtr<UHeistInventoryWidget> InventoryWidget;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UBorder> ItemBackground;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> PlaceholderIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ItemIdText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ItemDetailsText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> InstanceIdText;

#pragma endregion
};
