#include "UI/Widgets/HeistInventoryItemWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Input/Reply.h"
#include "UI/DragDrop/HeistInventoryDragDropOperation.h"
#include "UI/Widgets/HeistInventoryWidget.h"

UHeistInventoryItemWidget::UHeistInventoryItemWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UHeistInventoryItemWidget::SetupItem(const FHeistInventoryItem& InConfirmedItem, UTexture2D* InIcon, UHeistInventoryWidget* InInventoryWidget)
{
	ConfirmedItem = InConfirmedItem;
	InventoryWidget = InInventoryWidget;

	if (IsValid(PlaceholderIcon))
	{
		if (IsValid(InIcon))
		{
			PlaceholderIcon->SetBrushFromTexture(InIcon, false);
		}
		PlaceholderIcon->SetColorAndOpacity(FLinearColor::White);
	}
}

int32 UHeistInventoryItemWidget::GetInstanceId() const
{
	return ConfirmedItem.InstanceId;
}

FReply UHeistInventoryItemWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && IsValid(InventoryWidget) && ConfirmedItem.InstanceId != INDEX_NONE)
	{
		InventoryWidget->RequestRotateItem(ConfirmedItem.InstanceId);
		return FReply::Handled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UHeistInventoryItemWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	if (ConfirmedItem.InstanceId == INDEX_NONE)
	{
		return;
	}

	UHeistInventoryDragDropOperation* InventoryOperation = NewObject<UHeistInventoryDragDropOperation>(this);
	UImage* DragVisualImage = nullptr;
	if (IsValid(PlaceholderIcon))
	{
		DragVisualImage = NewObject<UImage>(InventoryOperation);
		FSlateBrush DragVisualBrush = PlaceholderIcon->GetBrush();
		const FVector2D DragVisualSize = InGeometry.GetLocalSize();
		DragVisualBrush.ImageSize = DragVisualSize;
		DragVisualImage->SetBrush(DragVisualBrush);
		DragVisualImage->SetDesiredSizeOverride(DragVisualSize);
		DragVisualImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	InventoryOperation->SetupDragOperation(ConfirmedItem.InstanceId, ConfirmedItem.GridPosition, DragVisualImage);
	InventoryOperation->Pivot = EDragPivot::CenterCenter;
	OutOperation = InventoryOperation;
}
