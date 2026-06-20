#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "HeistNoiseEmitterComponent.generated.h"

UCLASS(ClassGroup = (Heist), meta = (BlueprintSpawnableComponent))
class PROJECT_MUSEUMHEIST_API UHeistNoiseEmitterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeistNoiseEmitterComponent();
};
