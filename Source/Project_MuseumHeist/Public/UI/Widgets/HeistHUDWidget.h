#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistHUDWidget.generated.h"

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistHUDWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

public:
	UHeistHUDWidget(const FObjectInitializer& ObjectInitializer);
};
