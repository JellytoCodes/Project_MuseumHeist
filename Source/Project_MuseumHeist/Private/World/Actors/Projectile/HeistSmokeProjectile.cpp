#include "World/Actors/Projectile/HeistSmokeProjectile.h"

#include "Character/HeistPlayerCharacter.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "World/Actors/Area/HeistSmokeCloudActor.h"

#pragma region Construction

AHeistSmokeProjectile::AHeistSmokeProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	SmokeCloudActorClass = AHeistSmokeCloudActor::StaticClass();
}

#pragma endregion

#pragma region Projectile

void AHeistSmokeProjectile::HandleAuthorityImpact(const FHitResult& Hit)
{
	UHeistDebugFunctionLibrary::DebugThrowableProjectileImpact(
		this,
		this,
		Hit.GetActor(),
		GetSourceItemId(),
		Hit.ImpactPoint);

	if (!HasAuthority())
	{
		return;
	}

	UClass* ResolvedSmokeCloudClass = SmokeCloudActorClass.Get();
	if (!IsValid(ResolvedSmokeCloudClass) || !ResolvedSmokeCloudClass->IsChildOf(AHeistSmokeCloudActor::StaticClass()))
	{
		ResolvedSmokeCloudClass = AHeistSmokeCloudActor::StaticClass();
	}

	const FVector SmokeLocation = !Hit.ImpactPoint.IsNearlyZero()
		? FVector(Hit.ImpactPoint)
		: GetActorLocation();
	const FTransform SpawnTransform(FRotator::ZeroRotator, SmokeLocation);
	AHeistSmokeCloudActor* SmokeCloud = GetWorld()->SpawnActorDeferred<AHeistSmokeCloudActor>(
		ResolvedSmokeCloudClass,
		SpawnTransform,
		GetThrowerCharacter(),
		GetThrowerCharacter(),
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (IsValid(SmokeCloud))
	{
		SmokeCloud->InitializeSmokeCloud(
			GetThrowerCharacter(),
			GetSourceItemId(),
			GetEffectDurationSeconds(),
			SmokeRadius);
		SmokeCloud = Cast<AHeistSmokeCloudActor>(UGameplayStatics::FinishSpawningActor(SmokeCloud, SpawnTransform));
	}

	if (IsValid(SmokeCloud))
	{
		UHeistDebugFunctionLibrary::DebugSmokeCloudSpawned(
			this,
			this,
			SmokeCloud,
			GetSourceItemId(),
			SmokeLocation,
			SmokeRadius,
			GetEffectDurationSeconds());
	}
	else
	{
		UHeistDebugFunctionLibrary::Message(this, TEXT("Smoke cloud spawn failed."), EHeistDebugLevel::Warning);
	}

	Destroy();
}

#pragma endregion
