#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistInventoryItemWidget.generated.h"

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistInventoryItemWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

public:
	UHeistInventoryItemWidget(const FObjectInitializer& ObjectInitializer);
};
