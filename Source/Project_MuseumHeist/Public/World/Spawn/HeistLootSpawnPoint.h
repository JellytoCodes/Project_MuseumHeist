#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "GameFramework/Actor.h"

#include "HeistLootSpawnPoint.generated.h"

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistLootSpawnPoint : public AActor
{
	GENERATED_BODY()

#pragma region Construction

public:
	AHeistLootSpawnPoint();

#pragma endregion

#pragma region SpawnAvailability

public:
	bool CanSpawnCategory(EHeistSpawnCategory RequestedCategory) const;
	bool IsOccupied() const;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Spawn", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USceneComponent> SceneRoot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Spawn", meta = (AllowPrivateAccess = "true"))
	EHeistSpawnCategory SpawnCategory = EHeistSpawnCategory::RareEvent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Spawn", meta = (AllowPrivateAccess = "true"))
	bool bSpawnEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Spawn", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", Units = "cm"))
	float OccupancyRadius = 100.0f;

#pragma endregion
};
