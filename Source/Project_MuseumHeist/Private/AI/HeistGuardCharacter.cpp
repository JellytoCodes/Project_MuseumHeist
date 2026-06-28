#include "AI/HeistGuardCharacter.h"

#include "AI/HeistGuardAIController.h"
#include "AI/HeistGuardNoiseReactionComponent.h"
#include "AI/HeistGuardStateComponent.h"
#include "AI/HeistPatrolPathComponent.h"
#include "Core/HeistGameMode.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"

#pragma region Construction

AHeistGuardCharacter::AHeistGuardCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	AIControllerClass = AHeistGuardAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GuardStateComponent = CreateDefaultSubobject<UHeistGuardStateComponent>(TEXT("GuardStateComponent"));
	PatrolPathComponent = CreateDefaultSubobject<UHeistPatrolPathComponent>(TEXT("PatrolPathComponent"));
	NoiseReactionComponent = CreateDefaultSubobject<UHeistGuardNoiseReactionComponent>(TEXT("NoiseReactionComponent"));
}

#pragma endregion

#pragma region Lifecycle

void AHeistGuardCharacter::BeginPlay()
{
	Super::BeginPlay();

	checkf(IsValid(GuardStateComponent), TEXT("HeistGuardCharacter requires GuardStateComponent."));
	checkf(IsValid(PatrolPathComponent), TEXT("HeistGuardCharacter requires PatrolPathComponent."));
	checkf(IsValid(NoiseReactionComponent), TEXT("HeistGuardCharacter requires NoiseReactionComponent."));

	GuardStateComponent->GetGuardStateChangedDelegate().AddUObject(
		this,
		&AHeistGuardCharacter::HandleGuardStateChanged);

	if (HasAuthority())
	{
		ResolveGuardProfile();
	}

	HandleGuardStateChanged(
		GuardStateComponent->GetGuardState(),
		GuardStateComponent->GetGuardState());
}

#pragma endregion

#pragma region GameplayComponents

UHeistGuardStateComponent* AHeistGuardCharacter::GetGuardStateComponent() const
{
	return GuardStateComponent.Get();
}

UHeistPatrolPathComponent* AHeistGuardCharacter::GetPatrolPathComponent() const
{
	return PatrolPathComponent.Get();
}

UHeistGuardNoiseReactionComponent* AHeistGuardCharacter::GetNoiseReactionComponent() const
{
	return NoiseReactionComponent.Get();
}

#pragma endregion

#pragma region GuardProfile

void AHeistGuardCharacter::ResolveGuardProfile()
{
	const AHeistGameMode* HeistGameMode =
		GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	if (!IsValid(HeistGameMode)
		|| !HeistGameMode->TryGetGuardDefinition(GuardProfileId, GuardProfile))
	{
		UHeistDebugFunctionLibrary::DebugGuardProfileRejected(
			this,
			this,
			GuardProfileId,
			TEXT("MissingGuardDataRow"));
		return;
	}

	bHasResolvedGuardProfile = true;
	GuardStateComponent->ConfigureGuardProfile(GuardProfile);
	NoiseReactionComponent->ConfigureGuardProfile(GuardProfile);
	if (AHeistGuardAIController* GuardAIController =
		Cast<AHeistGuardAIController>(GetController()))
	{
		GuardAIController->ConfigurePerceptionFromGuardProfile(GuardProfile);
	}
	UHeistDebugFunctionLibrary::DebugGuardProfileResolved(
		this,
		this,
		GuardProfileId,
		GuardProfile.PatrolSpeed,
		GuardProfile.ChaseSpeed);
}

FName AHeistGuardCharacter::GetGuardProfileId() const
{
	return GuardProfileId;
}

bool AHeistGuardCharacter::HasResolvedGuardProfile() const
{
	return bHasResolvedGuardProfile;
}

const FHeistGuardDataRow& AHeistGuardCharacter::GetGuardProfile() const
{
	return GuardProfile;
}

void AHeistGuardCharacter::HandleGuardStateChanged(
	const EHeistGuardState,
	const EHeistGuardState NewState)
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	checkf(IsValid(MovementComponent), TEXT("HeistGuardCharacter requires CharacterMovementComponent."));

	if (NewState == EHeistGuardState::Disabled
		|| NewState == EHeistGuardState::Stunned)
	{
		MovementComponent->MaxWalkSpeed = 0.0f;
		if (HasAuthority() && IsValid(GetController()))
		{
			GetController()->StopMovement();
		}
		return;
	}

	if (!bHasResolvedGuardProfile)
	{
		return;
	}

	MovementComponent->MaxWalkSpeed =
		NewState == EHeistGuardState::ChasePlayer
			? FMath::Max(0.0f, GuardProfile.ChaseSpeed)
			: FMath::Max(0.0f, GuardProfile.PatrolSpeed);
}

#pragma endregion
