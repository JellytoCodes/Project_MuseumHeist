#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"

#include "HeistGameState.generated.h"

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AHeistGameState();

	int32 GetConnectedPlayerCount() const;
};
