#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "HeistPopupWidgetPool.generated.h"

UCLASS()
class PROJECT_MUSEUMHEIST_API UHeistPopupWidgetPool : public UObject
{
	GENERATED_BODY()

public:
	UHeistPopupWidgetPool(const FObjectInitializer& ObjectInitializer);

	void SetupPool();
};
