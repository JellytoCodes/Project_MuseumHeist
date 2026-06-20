#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "HeistGameMode.generated.h"

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistGameMode : public AGameModeBase
{
	GENERATED_BODY()

#pragma region Construction

public:
	AHeistGameMode();

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void RestartPlayer(AController* NewPlayer) override;

#pragma endregion

#pragma region RuntimeState

private:
	int32 NextHeistPlayerId = 1;

#pragma endregion
};
