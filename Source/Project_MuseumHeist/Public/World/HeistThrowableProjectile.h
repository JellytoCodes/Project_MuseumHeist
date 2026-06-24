#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "HeistThrowableProjectile.generated.h"

class AHeistPlayerCharacter;
class UProjectileMovementComponent;
class USphereComponent;

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistThrowableProjectile : public AActor
{
	GENERATED_BODY()

#pragma region Construction

public:
	AHeistThrowableProjectile();

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void BeginPlay() override;

#pragma endregion

#pragma region Projectile

public:
	void InitializeThrowable(AHeistPlayerCharacter* InThrowerCharacter, FName InSourceItemId, const FVector& InLaunchDirection, float InProjectileSpeed, float InEffectDurationSeconds);

	AHeistPlayerCharacter* GetThrowerCharacter() const;
	FName GetSourceItemId() const;
	float GetEffectDurationSeconds() const;

protected:
	virtual void HandleAuthorityImpact(const FHitResult& Hit);

private:
	UFUNCTION()
	void HandleProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Projectile", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Projectile", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UProjectileMovementComponent> ProjectileMovementComponent;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Projectile", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AHeistPlayerCharacter> ThrowerCharacter;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Projectile", meta = (AllowPrivateAccess = "true"))
	FName SourceItemId = NAME_None;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Projectile", meta = (AllowPrivateAccess = "true"))
	float EffectDurationSeconds = 0.0f;

#pragma endregion

#pragma region Replication

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion
};
