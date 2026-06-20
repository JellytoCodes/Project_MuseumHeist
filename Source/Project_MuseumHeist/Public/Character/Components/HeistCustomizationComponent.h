#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "HeistCustomizationComponent.generated.h"

UCLASS(ClassGroup = (Heist), meta = (BlueprintSpawnableComponent))
class PROJECT_MUSEUMHEIST_API UHeistCustomizationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeistCustomizationComponent();
};
