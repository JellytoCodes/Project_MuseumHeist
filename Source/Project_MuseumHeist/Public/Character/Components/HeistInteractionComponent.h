#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "HeistInteractionComponent.generated.h"

UCLASS(ClassGroup = (Heist), meta = (BlueprintSpawnableComponent))
class PROJECT_MUSEUMHEIST_API UHeistInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeistInteractionComponent();
};
