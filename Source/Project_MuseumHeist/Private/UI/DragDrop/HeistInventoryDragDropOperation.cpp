#include "UI/DragDrop/HeistInventoryDragDropOperation.h"

UHeistInventoryDragDropOperation::UHeistInventoryDragDropOperation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UHeistInventoryDragDropOperation::SetupDragOperation(
	const int32 InInstanceId,
	const FIntPoint InSourceGridPosition)
{
	InstanceId = InInstanceId;
	SourceGridPosition = InSourceGridPosition;
}
