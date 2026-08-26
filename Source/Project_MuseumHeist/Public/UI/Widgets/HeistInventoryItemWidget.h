#pragma once

#include "CoreMinimal.h"
#include "Inventory/HeistInventoryTypes.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistInventoryItemWidget.generated.h"

class UHeistInventoryWidget;
class UImage;
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
	void SetupItem(const FHeistInventoryItem& InConfirmedItem, UTexture2D* InIcon, UHeistInventoryWidget* InInventoryWidget);

	UFUNCTION(BlueprintPure, Category = "Heist|Inventory")
	int32 GetInstanceId() const;

  protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

  private:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|Inventory", meta = (AllowPrivateAccess = "true"))
	FHeistInventoryItem ConfirmedItem;

	UPROPERTY(Transient)
	TObjectPtr<UHeistInventoryWidget> InventoryWidget;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> PlaceholderIcon;

#pragma endregion
};
