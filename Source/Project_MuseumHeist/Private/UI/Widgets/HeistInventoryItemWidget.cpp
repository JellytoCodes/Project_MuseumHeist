#include "UI/Widgets/HeistInventoryItemWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Input/Reply.h"
#include "UI/DragDrop/HeistInventoryDragDropOperation.h"
#include "UI/Widgets/HeistInventoryWidget.h"

UHeistInventoryItemWidget::UHeistInventoryItemWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UHeistInventoryItemWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(DropButton))
	{
		DropButton->OnClicked.RemoveDynamic(this, &UHeistInventoryItemWidget::HandleDropButtonClicked);
		DropButton->OnClicked.AddDynamic(this, &UHeistInventoryItemWidget::HandleDropButtonClicked);
	}
}

void UHeistInventoryItemWidget::NativeDestruct()
{
	if (IsValid(DropButton))
	{
		DropButton->OnClicked.RemoveDynamic(this, &UHeistInventoryItemWidget::HandleDropButtonClicked);
	}

	Super::NativeDestruct();
}

void UHeistInventoryItemWidget::SetupItem(
	const FHeistInventoryItem& InConfirmedItem,
	const FIntPoint& InPlacedSize,
	UTexture2D* InIcon,
	UHeistInventoryWidget* InInventoryWidget)
{
	ConfirmedItem = InConfirmedItem;
	PlacedSize = InPlacedSize;
	InventoryWidget = InInventoryWidget;

	if (IsValid(PlaceholderIcon) && IsValid(InIcon))
	{
		PlaceholderIcon->SetBrushFromTexture(InIcon);
	}

	RefreshPresentation();
}

int32 UHeistInventoryItemWidget::GetInstanceId() const
{
	return ConfirmedItem.InstanceId;
}

FReply UHeistInventoryItemWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton
		&& IsValid(InventoryWidget)
		&& ConfirmedItem.InstanceId != INDEX_NONE)
	{
		InventoryWidget->RequestRotateItem(ConfirmedItem.InstanceId);
		return FReply::Handled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return UWidgetBlueprintLibrary::DetectDragIfPressed(
			InMouseEvent,
			this,
			EKeys::LeftMouseButton).NativeReply;
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UHeistInventoryItemWidget::NativeOnDragDetected(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent,
	UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	if (ConfirmedItem.InstanceId == INDEX_NONE)
	{
		return;
	}

	UHeistInventoryDragDropOperation* InventoryOperation =
		NewObject<UHeistInventoryDragDropOperation>(this);
	InventoryOperation->SetupDragOperation(
		ConfirmedItem.InstanceId,
		ConfirmedItem.GridPosition);
	InventoryOperation->Pivot = EDragPivot::CenterCenter;
	OutOperation = InventoryOperation;
}

void UHeistInventoryItemWidget::RefreshPresentation()
{
	if (IsValid(ItemIdText))
	{
		ItemIdText->SetText(FText::FromName(ConfirmedItem.ItemId));
	}

	if (IsValid(InstanceIdText))
	{
		InstanceIdText->SetText(FText::Format(
			NSLOCTEXT("HeistInventory", "ItemInstanceFormat", "#{0}"),
			FText::AsNumber(ConfirmedItem.InstanceId)));
	}

	if (IsValid(ItemDetailsText))
	{
		ItemDetailsText->SetText(FText::Format(
			NSLOCTEXT("HeistInventory", "ItemDetailsFormat", "{0}x{1}  Grid {2},{3}  {4}"),
			FText::AsNumber(PlacedSize.X),
			FText::AsNumber(PlacedSize.Y),
			FText::AsNumber(ConfirmedItem.GridPosition.X),
			FText::AsNumber(ConfirmedItem.GridPosition.Y),
			ConfirmedItem.bRotated
				? NSLOCTEXT("HeistInventory", "ItemRotated", "ROT")
				: NSLOCTEXT("HeistInventory", "ItemNotRotated", "BASE")));
	}

	if (IsValid(ItemBackground))
	{
		ItemBackground->SetBrushColor(FLinearColor(0.10f, 0.30f, 0.42f, 0.96f));
	}
}

void UHeistInventoryItemWidget::HandleDropButtonClicked()
{
	if (IsValid(InventoryWidget) && ConfirmedItem.InstanceId != INDEX_NONE)
	{
		InventoryWidget->RequestDropItem(ConfirmedItem.InstanceId);
	}
}
