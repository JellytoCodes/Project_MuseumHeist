#include "UI/Widgets/HeistInventoryFrameWidget.h"

UUniformGridPanel* UHeistInventoryFrameWidget::GetInventoryGrid() const
{
	return InventoryGrid;
}

UCanvasPanel* UHeistInventoryFrameWidget::GetItemOverlay() const
{
	return ItemOverlay;
}
