#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistInventorySlotWidget.generated.h"

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistInventorySlotWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

public:
	UHeistInventorySlotWidget(const FObjectInitializer& ObjectInitializer);
};
