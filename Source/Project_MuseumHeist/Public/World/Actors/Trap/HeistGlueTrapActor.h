#pragma once

#include "CoreMinimal.h"
#include "World/Actors/Trap/HeistTrapActor.h"

#include "HeistGlueTrapActor.generated.h"

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistGlueTrapActor : public AHeistTrapActor
{
	GENERATED_BODY()

#pragma region Construction

public:
	AHeistGlueTrapActor();

#pragma endregion

#pragma region Trap

protected:
	virtual bool HandleAuthorityTrigger(AActor* TriggeringActor) override;

#pragma endregion
};
