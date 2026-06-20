#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistQuickSlotWidget.generated.h"

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistQuickSlotWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

public:
	UHeistQuickSlotWidget(const FObjectInitializer& ObjectInitializer);
};
