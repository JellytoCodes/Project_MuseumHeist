#include "UI/DragDrop/HeistInventoryDragDropOperation.h"

#include "Components/Image.h"

namespace
{
const FLinearColor InventoryMoveDragColor(1.0f, 1.0f, 1.0f, 0.72f);
const FLinearColor WorldDropDragColor(1.0f, 0.30f, 0.22f, 0.82f);
}

UHeistInventoryDragDropOperation::UHeistInventoryDragDropOperation(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UHeistInventoryDragDropOperation::SetupDragOperation(const int32 InInstanceId, const FIntPoint InSourceGridPosition, UImage* InDragVisualImage)
{
	InstanceId = InInstanceId;
	SourceGridPosition = InSourceGridPosition;
	DragVisualImage = InDragVisualImage;
	DefaultDragVisual = DragVisualImage;
	SetWorldDropPreview(false);
}

void UHeistInventoryDragDropOperation::SetWorldDropPreview(const bool bInWorldDropPreview)
{
	bWorldDropPreview = bInWorldDropPreview;
	if (IsValid(DragVisualImage))
	{
		DragVisualImage->SetColorAndOpacity(bWorldDropPreview ? WorldDropDragColor : InventoryMoveDragColor);
	}
}
