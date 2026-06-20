#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"

#include "HeistCheatManager.generated.h"

UCLASS()
class PROJECT_MUSEUMHEIST_API UHeistCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	UHeistCheatManager(const FObjectInitializer& ObjectInitializer);
};
