#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistInteractionPromptWidget.generated.h"

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistInteractionPromptWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

public:
	UHeistInteractionPromptWidget(const FObjectInitializer& ObjectInitializer);
};
