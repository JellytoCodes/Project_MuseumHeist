#pragma once

#include "CoreMinimal.h"
#include "World/Actors/Projectile/HeistThrowableProjectile.h"

#include "HeistCoinProjectile.generated.h"

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistCoinProjectile : public AHeistThrowableProjectile
{
	GENERATED_BODY()

public:
	AHeistCoinProjectile();

#pragma region Projectile

protected:
	virtual void HandleAuthorityImpact(const FHitResult& Hit) override;

#pragma endregion
};
