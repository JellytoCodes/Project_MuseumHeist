#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "HeistUserWidgetBase.generated.h"

UCLASS(Abstract, Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistUserWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UHeistUserWidgetBase(const FObjectInitializer& ObjectInitializer);

	virtual void SetupWidget();
};
