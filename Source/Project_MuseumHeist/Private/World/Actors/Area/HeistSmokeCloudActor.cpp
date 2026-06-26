#include "World/Actors/Area/HeistSmokeCloudActor.h"

#include "Character/Components/HeistStatusComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HeistGameplayTags.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

#pragma region Construction

AHeistSmokeCloudActor::AHeistSmokeCloudActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	InitialLifeSpan = 5.0f;

	SmokeCollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SmokeCollisionComponent"));
	SetRootComponent(SmokeCollisionComponent);
	SmokeCollisionComponent->InitSphereRadius(SmokeRadius);
	SmokeCollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SmokeCollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	SmokeCollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	SmokeCollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SmokeCollisionComponent->SetGenerateOverlapEvents(true);

	VisualMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMeshComponent"));
	VisualMeshComponent->SetupAttachment(SmokeCollisionComponent);
	VisualMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMeshComponent->SetGenerateOverlapEvents(false);
}

#pragma endregion

#pragma region Lifecycle

void AHeistSmokeCloudActor::BeginPlay()
{
	Super::BeginPlay();

	UpdateSmokeRadius();

	if (HasAuthority() && IsValid(SmokeCollisionComponent))
	{
		SmokeCollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &AHeistSmokeCloudActor::HandleSmokeOverlapBegin);
		SmokeCollisionComponent->OnComponentEndOverlap.AddDynamic(this, &AHeistSmokeCloudActor::HandleSmokeOverlapEnd);
		ApplySmokeToCurrentOverlaps();
	}
}

void AHeistSmokeCloudActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && IsValid(SmokeCollisionComponent))
	{
		TArray<AActor*> OverlappingActors;
		SmokeCollisionComponent->GetOverlappingActors(OverlappingActors, AHeistPlayerCharacter::StaticClass());
		for (AActor* OverlappingActor : OverlappingActors)
		{
			ClearSmokeFromActorIfUncovered(OverlappingActor);
		}
	}

	Super::EndPlay(EndPlayReason);
}

#pragma endregion

#pragma region Smoke

void AHeistSmokeCloudActor::InitializeSmokeCloud(
	AHeistPlayerCharacter* InOwningCharacter,
	const FName InSourceItemId,
	const float InDurationSeconds,
	const float InSmokeRadius)
{
	checkf(HasAuthority(), TEXT("Smoke cloud initialization requires authority."));

	OwningCharacter = InOwningCharacter;
	SourceItemId = InSourceItemId;
	SmokeRadius = FMath::Max(0.0f, InSmokeRadius);

	const float DurationSeconds = FMath::Max(0.1f, InDurationSeconds);
	EndServerTime = GetWorld() ? GetWorld()->GetTimeSeconds() + DurationSeconds : DurationSeconds;
	InitialLifeSpan = DurationSeconds;
	SetLifeSpan(DurationSeconds);
	SetOwner(InOwningCharacter);
	SetInstigator(InOwningCharacter);
	UpdateSmokeRadius();
	ForceNetUpdate();
}

bool AHeistSmokeCloudActor::IsActorInsideSmoke(const AActor* Actor) const
{
	return IsValid(Actor) && IsLocationInsideSmoke(Actor->GetActorLocation());
}

bool AHeistSmokeCloudActor::IsLocationInsideSmoke(const FVector& WorldLocation) const
{
	return FVector::DistSquared(GetActorLocation(), WorldLocation) <= FMath::Square(SmokeRadius);
}

bool AHeistSmokeCloudActor::BlocksAISight() const
{
	return bBlocksAISight && !IsActorBeingDestroyed() && GetRemainingLifetimeSeconds() > 0.0f;
}

float AHeistSmokeCloudActor::GetRemainingLifetimeSeconds() const
{
	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, EndServerTime - World->GetTimeSeconds());
}

bool AHeistSmokeCloudActor::IsAISightBlockedBySmoke(
	const UObject* WorldContextObject,
	const FVector& FromLocation,
	const FVector& ToLocation,
	AHeistSmokeCloudActor*& OutBlockingSmokeCloud)
{
	OutBlockingSmokeCloud = nullptr;

	UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	if (!IsValid(World))
	{
		return false;
	}

	for (TActorIterator<AHeistSmokeCloudActor> It(World); It; ++It)
	{
		AHeistSmokeCloudActor* SmokeCloud = *It;
		if (!IsValid(SmokeCloud) || !SmokeCloud->BlocksAISight())
		{
			continue;
		}

		const float DistanceToSightSegment = FMath::PointDistToSegment(
			SmokeCloud->GetActorLocation(),
			FromLocation,
			ToLocation);
		if (DistanceToSightSegment <= SmokeCloud->SmokeRadius)
		{
			OutBlockingSmokeCloud = SmokeCloud;
			UHeistDebugFunctionLibrary::DebugSmokeSightQuery(WorldContextObject, SmokeCloud, FromLocation, ToLocation, true);
			return true;
		}
	}

	UHeistDebugFunctionLibrary::DebugSmokeSightQuery(WorldContextObject, nullptr, FromLocation, ToLocation, false);
	return false;
}

void AHeistSmokeCloudActor::HandleSmokeOverlapBegin(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent*,
	int32,
	bool,
	const FHitResult&)
{
	ApplySmokeToActor(OtherActor);
}

void AHeistSmokeCloudActor::HandleSmokeOverlapEnd(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent*,
	int32)
{
	ClearSmokeFromActorIfUncovered(OtherActor);
}

void AHeistSmokeCloudActor::ApplySmokeToActor(AActor* Actor)
{
	if (!HasAuthority())
	{
		return;
	}

	AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(Actor);
	UHeistStatusComponent* StatusComponent = IsValid(HeistCharacter)
		? HeistCharacter->GetStatusComponent()
		: nullptr;
	if (!IsValid(StatusComponent))
	{
		return;
	}

	const float RemainingSeconds = GetRemainingLifetimeSeconds();
	if (RemainingSeconds <= 0.0f)
	{
		return;
	}

	if (StatusComponent->ApplyTimedStatusTag(FHeistGameplayTags::Get().State_InSmoke, RemainingSeconds))
	{
		UHeistDebugFunctionLibrary::DebugSmokeCloudOverlapChanged(this, this, HeistCharacter, true, RemainingSeconds);
	}
}

void AHeistSmokeCloudActor::ClearSmokeFromActorIfUncovered(AActor* Actor)
{
	if (!HasAuthority())
	{
		return;
	}

	AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(Actor);
	UHeistStatusComponent* StatusComponent = IsValid(HeistCharacter)
		? HeistCharacter->GetStatusComponent()
		: nullptr;
	if (!IsValid(StatusComponent) || HasOtherSmokeCloudCoveringActor(HeistCharacter))
	{
		return;
	}

	if (StatusComponent->ClearStatusTag(FHeistGameplayTags::Get().State_InSmoke))
	{
		UHeistDebugFunctionLibrary::DebugSmokeCloudOverlapChanged(this, this, HeistCharacter, false, 0.0f);
	}
}

void AHeistSmokeCloudActor::ApplySmokeToCurrentOverlaps()
{
	if (!HasAuthority() || !IsValid(SmokeCollisionComponent))
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	SmokeCollisionComponent->GetOverlappingActors(OverlappingActors, AHeistPlayerCharacter::StaticClass());
	for (AActor* OverlappingActor : OverlappingActors)
	{
		ApplySmokeToActor(OverlappingActor);
	}
}

bool AHeistSmokeCloudActor::HasOtherSmokeCloudCoveringActor(const AActor* Actor) const
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || !IsValid(Actor))
	{
		return false;
	}

	for (TActorIterator<AHeistSmokeCloudActor> It(World); It; ++It)
	{
		const AHeistSmokeCloudActor* SmokeCloud = *It;
		if (!IsValid(SmokeCloud)
			|| SmokeCloud == this
			|| SmokeCloud->IsActorBeingDestroyed()
			|| SmokeCloud->GetRemainingLifetimeSeconds() <= 0.0f)
		{
			continue;
		}

		if (SmokeCloud->IsActorInsideSmoke(Actor))
		{
			return true;
		}
	}

	return false;
}

void AHeistSmokeCloudActor::UpdateSmokeRadius()
{
	if (IsValid(SmokeCollisionComponent))
	{
		SmokeCollisionComponent->SetSphereRadius(FMath::Max(0.0f, SmokeRadius), true);
	}
}

void AHeistSmokeCloudActor::OnRep_SmokeCloudState()
{
	UpdateSmokeRadius();
	UHeistDebugFunctionLibrary::DebugSmokeCloudStateReplicated(this, this, SmokeRadius, EndServerTime, bBlocksAISight);
}

#pragma endregion

#pragma region Replication

void AHeistSmokeCloudActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeistSmokeCloudActor, OwningCharacter);
	DOREPLIFETIME(AHeistSmokeCloudActor, SourceItemId);
	DOREPLIFETIME(AHeistSmokeCloudActor, SmokeRadius);
	DOREPLIFETIME(AHeistSmokeCloudActor, EndServerTime);
	DOREPLIFETIME(AHeistSmokeCloudActor, bBlocksAISight);
}

#pragma endregion
