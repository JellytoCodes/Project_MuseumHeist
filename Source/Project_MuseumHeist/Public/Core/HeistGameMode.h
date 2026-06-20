#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "HeistGameMode.generated.h"

class APlayerController;
class AController;

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHeistGameMode();

protected:
	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void RestartPlayer(AController* NewPlayer) override;

private:
	int32 NextHeistPlayerId = 1;
};
