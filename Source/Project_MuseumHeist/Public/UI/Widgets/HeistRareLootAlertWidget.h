#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistRareLootAlertWidget.generated.h"

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistRareLootAlertWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

public:
	UHeistRareLootAlertWidget(const FObjectInitializer& ObjectInitializer);
};
