#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"

#include "HeistInventoryDragDropOperation.generated.h"

class UImage;

UCLASS()
class PROJECT_MUSEUMHEIST_API UHeistInventoryDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

  public:
	UHeistInventoryDragDropOperation(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Heist|Inventory")
	void SetupDragOperation(int32 InInstanceId, FIntPoint InSourceGridPosition, UImage* InDragVisualImage);

	void SetWorldDropPreview(bool bInWorldDropPreview);

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Inventory")
	int32 InstanceId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Inventory")
	FIntPoint SourceGridPosition = FIntPoint(-1, -1);

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Inventory")
	bool bWorldDropPreview = false;

  private:
	UPROPERTY(Transient)
	TObjectPtr<UImage> DragVisualImage;
};
