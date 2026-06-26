#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "HeistGuardCharacter.generated.h"

class UHeistGuardNoiseReactionComponent;
class UHeistGuardStateComponent;
class UHeistPatrolPathComponent;

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistGuardCharacter : public ACharacter
{
	GENERATED_BODY()

#pragma region Construction

public:
	AHeistGuardCharacter();

#pragma endregion

#pragma region GameplayComponents

public:
	UHeistGuardStateComponent* GetGuardStateComponent() const;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|AI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistGuardStateComponent> GuardStateComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|AI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistPatrolPathComponent> PatrolPathComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|AI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistGuardNoiseReactionComponent> NoiseReactionComponent;

#pragma endregion
};
