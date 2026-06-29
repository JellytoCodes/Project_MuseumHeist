#pragma once

#include "CoreMinimal.h"
#include "World/Actors/Trap/HeistTrapActor.h"

#include "HeistNoiseTrapActor.generated.h"

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistNoiseTrapActor : public AHeistTrapActor
{
	GENERATED_BODY()

#pragma region Construction

public:
	AHeistNoiseTrapActor();

#pragma endregion
};
