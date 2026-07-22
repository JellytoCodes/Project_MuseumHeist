#include "UI/Widgets/HeistInventorySlotWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "UI/DragDrop/HeistInventoryDragDropOperation.h"
#include "UI/Widgets/HeistInventoryWidget.h"

UHeistInventorySlotWidget::UHeistInventorySlotWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UHeistInventorySlotWidget::SetupSlot(const FIntPoint& InGridCoordinate, const bool bInOccupied, UHeistInventoryWidget* InInventoryWidget)
{
	GridCoordinate = InGridCoordinate;
	bOccupied = bInOccupied;
	InventoryWidget = InInventoryWidget;
	bDropPreviewVisible = false;
	bDropPreviewValid = false;
	RefreshPresentation();
}

void UHeistInventorySlotWidget::SetOccupied(const bool bInOccupied)
{
	bOccupied = bInOccupied;
	RefreshPresentation();
}

void UHeistInventorySlotWidget::SetDropPreview(const bool bVisible, const bool bValid)
{
	bDropPreviewVisible = bVisible;
	bDropPreviewValid = bValid;
	RefreshPresentation();
}

FIntPoint UHeistInventorySlotWidget::GetGridCoordinate() const
{
	return GridCoordinate;
}

void UHeistInventorySlotWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);

	const UHeistInventoryDragDropOperation* InventoryOperation = Cast<UHeistInventoryDragDropOperation>(InOperation);
	if (IsValid(InventoryOperation) && IsValid(InventoryWidget))
	{
		SetDropPreview(true, InventoryWidget->CanPreviewItemDrop(InventoryOperation->InstanceId, GridCoordinate));
	}
}

void UHeistInventorySlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	SetDropPreview(false, false);
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}

bool UHeistInventorySlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	const UHeistInventoryDragDropOperation* InventoryOperation = Cast<UHeistInventoryDragDropOperation>(InOperation);
	const bool bHandled = IsValid(InventoryOperation) && IsValid(InventoryWidget);
	if (bHandled)
	{
		InventoryWidget->RequestMoveItem(InventoryOperation->InstanceId, GridCoordinate);
	}

	SetDropPreview(false, false);
	return bHandled || Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

void UHeistInventorySlotWidget::RefreshPresentation()
{
	if (IsValid(CoordinateText))
	{
		CoordinateText->SetText(FText::Format(NSLOCTEXT("HeistInventory", "SlotCoordinateFormat", "{0},{1}"), FText::AsNumber(GridCoordinate.X), FText::AsNumber(GridCoordinate.Y)));
	}

	if (IsValid(OccupancyText))
	{
		OccupancyText->SetText(bOccupied ? NSLOCTEXT("HeistInventory", "SlotOccupied", "OCC") : FText::GetEmpty());
	}

	if (IsValid(SlotBackground))
	{
		FLinearColor SlotColor = bOccupied ? FLinearColor(0.10f, 0.16f, 0.22f, 0.90f) : FLinearColor(0.04f, 0.07f, 0.10f, 0.85f);
		if (bDropPreviewVisible)
		{
			SlotColor = bDropPreviewValid ? FLinearColor(0.06f, 0.55f, 0.20f, 0.95f) : FLinearColor(0.70f, 0.06f, 0.08f, 0.95f);
		}
		SlotBackground->SetBrushColor(SlotColor);
	}
}
