#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "HeistInventoryComponent.generated.h"

UCLASS(ClassGroup = (Heist), meta = (BlueprintSpawnableComponent))
class PROJECT_MUSEUMHEIST_API UHeistInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeistInventoryComponent();
};
