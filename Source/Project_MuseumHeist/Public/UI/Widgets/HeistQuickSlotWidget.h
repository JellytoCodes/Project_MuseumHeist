#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"
#include "UI/ViewModels/HeistQuickSlotViewModel.h"

#include "HeistQuickSlotWidget.generated.h"

class UBorder;
class UButton;
class UHeistInventoryWidget;
class UImage;
class UTextBlock;
class UTexture2D;

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistQuickSlotWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

#pragma region Construction

public:
	UHeistQuickSlotWidget(const FObjectInitializer& ObjectInitializer);

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual bool NativeOnDragOver(
		const FGeometry& InGeometry,
		const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(
		const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(
		const FGeometry& InGeometry,
		const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;

#pragma endregion

#pragma region Presentation

public:
	void SetupQuickSlot(
		const FHeistQuickSlotPresentation& InConfirmedPresentation,
		UTexture2D* InIcon,
		UHeistInventoryWidget* InInventoryWidget);

private:
	void RefreshPresentation();
	void SetDropPreview(bool bInDropPreview);

	UFUNCTION()
	void HandleClearButtonClicked();

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|QuickSlot", meta = (AllowPrivateAccess = "true"))
	FHeistQuickSlotPresentation ConfirmedPresentation;

	UPROPERTY(Transient)
	TObjectPtr<UHeistInventoryWidget> InventoryWidget;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UBorder> SlotBackground;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> KeyLabelText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> PlaceholderIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ItemIdText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> CountText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> AssignmentStateText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> ClearButton;

#pragma endregion
};
