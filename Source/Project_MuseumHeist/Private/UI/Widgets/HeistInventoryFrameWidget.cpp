#include "UI/Widgets/HeistInventoryFrameWidget.h"

UHeistInventoryFrameWidget::UHeistInventoryFrameWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

UUniformGridPanel* UHeistInventoryFrameWidget::GetInventoryGrid() const
{
	return InventoryGrid;
}

UCanvasPanel* UHeistInventoryFrameWidget::GetItemOverlay() const
{
	return ItemOverlay;
}
