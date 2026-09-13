#include "UI/Widgets/HeistInventoryItemWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Input/Reply.h"
#include "UI/DragDrop/HeistInventoryDragDropOperation.h"
#include "UI/Widgets/HeistInventoryWidget.h"

void UHeistInventoryItemWidget::SetupItem(const FHeistInventoryItem& InConfirmedItem, UTexture2D* InIcon, UHeistInventoryWidget* InInventoryWidget)
{
	ConfirmedItem = InConfirmedItem;
	InventoryWidget = InInventoryWidget;

	if (IsValid(PlaceholderIcon))
	{
		if (IsValid(InIcon))
		{
			PlaceholderIcon->SetBrushFromTexture(InIcon, true);
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
	USizeBox* DragVisualBounds = nullptr;
	if (IsValid(PlaceholderIcon))
	{
		DragVisualImage = NewObject<UImage>(InventoryOperation);
		FSlateBrush DragVisualBrush = PlaceholderIcon->GetBrush();
		const FVector2D DragVisualSize = InGeometry.GetLocalSize();
		DragVisualImage->SetBrush(DragVisualBrush);
		DragVisualImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		UScaleBox* ArtworkFit = NewObject<UScaleBox>(InventoryOperation);
		ArtworkFit->SetStretch(EStretch::ScaleToFit);
		ArtworkFit->SetContent(DragVisualImage);
		DragVisualBounds = NewObject<USizeBox>(InventoryOperation);
		DragVisualBounds->SetWidthOverride(DragVisualSize.X);
		DragVisualBounds->SetHeightOverride(DragVisualSize.Y);
		DragVisualBounds->SetContent(ArtworkFit);
		DragVisualBounds->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	InventoryOperation->SetupDragOperation(ConfirmedItem.InstanceId, ConfirmedItem.GridPosition, DragVisualImage);
	if (IsValid(DragVisualBounds))
	{
		InventoryOperation->DefaultDragVisual = DragVisualBounds;
	}
	InventoryOperation->Pivot = EDragPivot::CenterCenter;
	OutOperation = InventoryOperation;
}
