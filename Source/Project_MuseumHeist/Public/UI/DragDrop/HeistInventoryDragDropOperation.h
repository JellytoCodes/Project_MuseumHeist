#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"

#include "HeistInventoryDragDropOperation.generated.h"

UCLASS()
class PROJECT_MUSEUMHEIST_API UHeistInventoryDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	UHeistInventoryDragDropOperation(const FObjectInitializer& ObjectInitializer);

	void SetupDragOperation();
};
