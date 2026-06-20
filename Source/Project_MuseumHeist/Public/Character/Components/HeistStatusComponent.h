#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "HeistStatusComponent.generated.h"

UCLASS(ClassGroup = (Heist), meta = (BlueprintSpawnableComponent))
class PROJECT_MUSEUMHEIST_API UHeistStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeistStatusComponent();
};
