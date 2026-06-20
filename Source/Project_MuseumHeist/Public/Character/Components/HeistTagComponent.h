#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "HeistTagComponent.generated.h"

UCLASS(ClassGroup = (Heist), meta = (BlueprintSpawnableComponent))
class PROJECT_MUSEUMHEIST_API UHeistTagComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeistTagComponent();
};
