#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "HeistPatrolPathComponent.generated.h"

UCLASS(ClassGroup = (Heist), meta = (BlueprintSpawnableComponent))
class PROJECT_MUSEUMHEIST_API UHeistPatrolPathComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeistPatrolPathComponent();
};
