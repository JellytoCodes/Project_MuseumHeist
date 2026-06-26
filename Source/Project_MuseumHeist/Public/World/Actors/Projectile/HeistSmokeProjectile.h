#pragma once

#include "CoreMinimal.h"
#include "World/Actors/Projectile/HeistThrowableProjectile.h"

#include "HeistSmokeProjectile.generated.h"

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistSmokeProjectile : public AHeistThrowableProjectile
{
	GENERATED_BODY()

#pragma region Construction

public:
	AHeistSmokeProjectile();

#pragma endregion

#pragma region Projectile

protected:
	virtual void HandleAuthorityImpact(const FHitResult& Hit) override;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Smoke", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class AHeistSmokeCloudActor> SmokeCloudActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Smoke", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float SmokeRadius = 300.0f;

#pragma endregion
};
