#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "HeistGuardNoiseReactionComponent.generated.h"

UCLASS(ClassGroup = (Heist), meta = (BlueprintSpawnableComponent))
class PROJECT_MUSEUMHEIST_API UHeistGuardNoiseReactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeistGuardNoiseReactionComponent();
};
