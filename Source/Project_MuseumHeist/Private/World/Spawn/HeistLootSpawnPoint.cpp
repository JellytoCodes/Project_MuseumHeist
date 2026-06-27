#include "World/Spawn/HeistLootSpawnPoint.h"

#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "World/Actors/Loot/HeistLootActor.h"

#pragma region Construction

AHeistLootSpawnPoint::AHeistLootSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

#pragma endregion

#pragma region SpawnAvailability

bool AHeistLootSpawnPoint::CanSpawnCategory(const EHeistSpawnCategory RequestedCategory) const
{
	return bSpawnEnabled
		&& SpawnCategory == RequestedCategory
		&& !IsOccupied();
}

bool AHeistLootSpawnPoint::IsOccupied() const
{
	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return true;
	}

	const float OccupancyRadiusSquared = FMath::Square(FMath::Max(1.0f, OccupancyRadius));
	for (TActorIterator<AHeistLootActor> It(World); It; ++It)
	{
		const AHeistLootActor* LootActor = *It;
		if (IsValid(LootActor)
			&& LootActor->IsLootAvailable()
			&& FVector::DistSquared(GetActorLocation(), LootActor->GetActorLocation()) <= OccupancyRadiusSquared)
		{
			return true;
		}
	}

	return false;
}

#pragma endregion
