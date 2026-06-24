#include "World/HeistThrowableProjectile.h"

#include "Character/HeistPlayerCharacter.h"
#include "Components/SphereComponent.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"

#pragma region Construction

AHeistThrowableProjectile::AHeistThrowableProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	InitialLifeSpan = 5.0f;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	SetRootComponent(CollisionComponent);
	CollisionComponent->InitSphereRadius(12.0f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionComponent->SetNotifyRigidBodyCollision(true);

	ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComponent"));
	ProjectileMovementComponent->InitialSpeed = 1500.0f;
	ProjectileMovementComponent->MaxSpeed = 1500.0f;
	ProjectileMovementComponent->bRotationFollowsVelocity = true;
	ProjectileMovementComponent->bShouldBounce = false;
	ProjectileMovementComponent->ProjectileGravityScale = 0.0f;
}

#pragma endregion

#pragma region Lifecycle

void AHeistThrowableProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && IsValid(CollisionComponent))
	{
		CollisionComponent->OnComponentHit.AddDynamic(this, &AHeistThrowableProjectile::HandleProjectileHit);
	}
}

#pragma endregion

#pragma region Projectile

void AHeistThrowableProjectile::InitializeThrowable(AHeistPlayerCharacter* InThrowerCharacter, const FName InSourceItemId, const FVector& InLaunchDirection, const float InProjectileSpeed, const float InEffectDurationSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	ThrowerCharacter = InThrowerCharacter;
	SourceItemId = InSourceItemId;
	EffectDurationSeconds = FMath::Max(0.0f, InEffectDurationSeconds);
	SetOwner(InThrowerCharacter);
	SetInstigator(InThrowerCharacter);

	if (IsValid(CollisionComponent) && IsValid(InThrowerCharacter))
	{
		CollisionComponent->IgnoreActorWhenMoving(InThrowerCharacter, true);
	}

	const FVector LaunchDirection = InLaunchDirection.GetSafeNormal();
	const float ProjectileSpeed = FMath::Max(1.0f, InProjectileSpeed);
	if (IsValid(ProjectileMovementComponent))
	{
		ProjectileMovementComponent->InitialSpeed = ProjectileSpeed;
		ProjectileMovementComponent->MaxSpeed = ProjectileSpeed;
		ProjectileMovementComponent->Velocity = LaunchDirection * ProjectileSpeed;
	}

	SetActorRotation(LaunchDirection.Rotation());
	ForceNetUpdate();
}

AHeistPlayerCharacter* AHeistThrowableProjectile::GetThrowerCharacter() const
{
	return ThrowerCharacter;
}

FName AHeistThrowableProjectile::GetSourceItemId() const
{
	return SourceItemId;
}

float AHeistThrowableProjectile::GetEffectDurationSeconds() const
{
	return EffectDurationSeconds;
}

void AHeistThrowableProjectile::HandleAuthorityImpact(const FHitResult& Hit)
{
	UHeistDebugFunctionLibrary::DebugThrowableProjectileImpact(
		this,
		this,
		Hit.GetActor(),
		SourceItemId,
		Hit.ImpactPoint);

	Destroy();
}

void AHeistThrowableProjectile::HandleProjectileHit(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, FVector, const FHitResult& Hit)
{
	if (!HasAuthority() || OtherActor == this || OtherActor == ThrowerCharacter)
	{
		return;
	}

	HandleAuthorityImpact(Hit);
}

#pragma endregion

#pragma region Replication

void AHeistThrowableProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeistThrowableProjectile, ThrowerCharacter);
	DOREPLIFETIME(AHeistThrowableProjectile, SourceItemId);
	DOREPLIFETIME(AHeistThrowableProjectile, EffectDurationSeconds);
}

#pragma endregion
