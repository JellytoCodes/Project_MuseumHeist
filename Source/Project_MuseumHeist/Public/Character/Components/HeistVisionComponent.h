#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "HeistVisionComponent.generated.h"

UCLASS(ClassGroup = (Heist), meta = (BlueprintSpawnableComponent))
class PROJECT_MUSEUMHEIST_API UHeistVisionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeistVisionComponent();
};
