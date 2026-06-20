#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistResultWidget.generated.h"

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistResultWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

public:
	UHeistResultWidget(const FObjectInitializer& ObjectInitializer);
};
