#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "HeistSoundPingWidgetPool.generated.h"

UCLASS()
class PROJECT_MUSEUMHEIST_API UHeistSoundPingWidgetPool : public UObject
{
	GENERATED_BODY()

public:
	UHeistSoundPingWidgetPool(const FObjectInitializer& ObjectInitializer);

	void SetupPool();
};
