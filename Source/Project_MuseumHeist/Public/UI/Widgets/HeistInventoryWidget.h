#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistInventoryWidget.generated.h"

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistInventoryWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

public:
	UHeistInventoryWidget(const FObjectInitializer& ObjectInitializer);
};
