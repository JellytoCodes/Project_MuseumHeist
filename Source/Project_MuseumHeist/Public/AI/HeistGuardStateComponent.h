#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/HeistTypes.h"

#include "HeistGuardStateComponent.generated.h"

UCLASS(ClassGroup = (Heist), meta = (BlueprintSpawnableComponent))
class PROJECT_MUSEUMHEIST_API UHeistGuardStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeistGuardStateComponent();

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|AI", meta = (AllowPrivateAccess = "true"))
	EHeistGuardState GuardState = EHeistGuardState::Patrol;
};
