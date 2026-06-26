#include "World/Actors/Trap/HeistTrapActor.h"

#include "Character/HeistPlayerCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

#pragma region Construction

AHeistTrapActor::AHeistTrapActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	TriggerComponent = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerComponent"));
	SetRootComponent(TriggerComponent);
	TriggerComponent->InitSphereRadius(75.0f);
	TriggerComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerComponent->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	VisualMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMeshComponent"));
	VisualMeshComponent->SetupAttachment(TriggerComponent);
	VisualMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMeshComponent->SetGenerateOverlapEvents(false);
}

#pragma endregion

#pragma region Lifecycle

void AHeistTrapActor::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && IsValid(TriggerComponent))
	{
		TriggerComponent->OnComponentBeginOverlap.AddDynamic(this, &AHeistTrapActor::HandleTrapOverlap);
	}
}

#pragma endregion

#pragma region Trap

void AHeistTrapActor::InitializeTrap(
	AHeistPlayerCharacter* InOwningCharacter,
	const FName InSourceItemId,
	const float InEffectDurationSeconds)
{
	checkf(HasAuthority(), TEXT("Trap initialization requires authority."));

	OwningCharacter = InOwningCharacter;
	SourceItemId = InSourceItemId;
	EffectDurationSeconds = FMath::Max(0.0f, InEffectDurationSeconds);
	SetOwner(InOwningCharacter);
	SetInstigator(InOwningCharacter);
	ForceNetUpdate();
}

bool AHeistTrapActor::HandleAuthorityTrigger(AActor*)
{
	return false;
}

AHeistPlayerCharacter* AHeistTrapActor::GetOwningCharacter() const
{
	return OwningCharacter;
}

FName AHeistTrapActor::GetSourceItemId() const
{
	return SourceItemId;
}

float AHeistTrapActor::GetEffectDurationSeconds() const
{
	return EffectDurationSeconds;
}

void AHeistTrapActor::HandleTrapOverlap(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent*,
	int32,
	bool,
	const FHitResult&)
{
	if (!HasAuthority() || bTriggered || !IsValid(OtherActor) || OtherActor == this || OtherActor == OwningCharacter)
	{
		return;
	}

	if (HandleAuthorityTrigger(OtherActor))
	{
		bTriggered = true;
		ForceNetUpdate();
		Destroy();
	}
}

#pragma endregion

#pragma region Replication

void AHeistTrapActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeistTrapActor, OwningCharacter);
	DOREPLIFETIME(AHeistTrapActor, SourceItemId);
	DOREPLIFETIME(AHeistTrapActor, EffectDurationSeconds);
	DOREPLIFETIME(AHeistTrapActor, bTriggered);
}

#pragma endregion
