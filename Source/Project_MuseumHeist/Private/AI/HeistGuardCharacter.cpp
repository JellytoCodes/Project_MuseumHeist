#include "AI/HeistGuardCharacter.h"

#include "AI/HeistGuardAIController.h"
#include "AI/HeistGuardNoiseReactionComponent.h"
#include "AI/HeistGuardStateComponent.h"
#include "AI/HeistPatrolPathComponent.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistLogChannels.h"
#include "Components/CapsuleComponent.h"
#include "Core/HeistCollisionChannels.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

#pragma region Construction

AHeistGuardCharacter::AHeistGuardCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	AIControllerClass = AHeistGuardAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	checkf(IsValid(MovementComponent), TEXT("HeistGuardCharacter requires CharacterMovementComponent."));
	MovementComponent->bOrientRotationToMovement = true;
	MovementComponent->bUseControllerDesiredRotation = false;
	MovementComponent->RotationRate = FRotator(0.0f, 360.0f, 0.0f);

	GuardStateComponent = CreateDefaultSubobject<UHeistGuardStateComponent>(TEXT("GuardStateComponent"));
	PatrolPathComponent = CreateDefaultSubobject<UHeistPatrolPathComponent>(TEXT("PatrolPathComponent"));
	NoiseReactionComponent = CreateDefaultSubobject<UHeistGuardNoiseReactionComponent>(TEXT("NoiseReactionComponent"));
}

#pragma endregion

#pragma region Lifecycle

void AHeistGuardCharacter::BeginPlay()
{
	Super::BeginPlay();

	UCapsuleComponent* GuardCapsule = GetCapsuleComponent();
	checkf(IsValid(GuardCapsule), TEXT("HeistGuardCharacter requires CapsuleComponent."));
	GuardCapsule->SetCollisionObjectType(HeistCollisionChannels::Guard);
	GuardCapsule->SetCollisionResponseToChannel(HeistCollisionChannels::Player, ECR_Block);
	GuardCapsule->SetCollisionResponseToChannel(HeistCollisionChannels::Interactable, ECR_Ignore);
	UE_LOG(LogHeist, Log, TEXT("[%s] Guard collision channels configured: Capsule=%s ObjectType=HeistGuard Player=Block Interactable=Ignore"), *GetName(),
		   *GetNameSafe(GuardCapsule));

	checkf(IsValid(GuardStateComponent), TEXT("HeistGuardCharacter requires GuardStateComponent."));
	checkf(IsValid(PatrolPathComponent), TEXT("HeistGuardCharacter requires PatrolPathComponent."));
	checkf(IsValid(NoiseReactionComponent), TEXT("HeistGuardCharacter requires NoiseReactionComponent."));

	GuardStateComponent->GetGuardStateChangedDelegate().AddUObject(this, &AHeistGuardCharacter::HandleGuardStateChanged);

	if (HasAuthority())
	{
		ResolveGuardProfile();
	}

	ApplyDifficultyActivationPresentation();
	HandleGuardStateChanged(GuardStateComponent->GetGuardState(), GuardStateComponent->GetGuardState());
}

void AHeistGuardCharacter::GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	OutLocation = GetPawnViewLocation();
	OutRotation = GetActorRotation();
}

void AHeistGuardCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHeistGuardCharacter, bDifficultyActive);
	DOREPLIFETIME(AHeistGuardCharacter, bDifficultySupplementalGuard);
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
	const AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	if (!IsValid(HeistGameMode) || !HeistGameMode->TryGetGuardDefinition(GuardProfileId, GuardProfile))
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogHeistAI, Warning, TEXT("Guard profile rejected: Guard=%s Profile=%s Reason=MissingGuardDataRow"), *GetNameSafe(this), *GuardProfileId.ToString());
#endif
		return;
	}

	BaseEyeHeight = FMath::Max(0.0f, GuardProfile.EyeHeight);
	bHasResolvedGuardProfile = true;
	GuardStateComponent->ConfigureGuardProfile(GuardProfile);
	NoiseReactionComponent->ConfigureGuardProfile(GuardProfile);
	if (AHeistGuardAIController* GuardAIController = Cast<AHeistGuardAIController>(GetController()))
	{
		GuardAIController->ConfigurePerceptionFromGuardProfile(GuardProfile);
	}
#if !UE_BUILD_SHIPPING
	UE_LOG(LogHeistAI, Log, TEXT("Guard profile resolved: Guard=%s Profile=%s PatrolSpeed=%.1f ChaseSpeed=%.1f"), *GetNameSafe(this), *GuardProfileId.ToString(), GuardProfile.PatrolSpeed,
		   GuardProfile.ChaseSpeed);
#endif
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

void AHeistGuardCharacter::SetAlertPatrolSpeedMultiplier(const float Multiplier)
{
	if (!HasAuthority())
	{
		return;
	}

	AlertPatrolSpeedMultiplier = FMath::Max(0.0f, FMath::IsFinite(Multiplier) ? Multiplier : 1.0f);
	HandleGuardStateChanged(GuardStateComponent->GetGuardState(), GuardStateComponent->GetGuardState());
}

float AHeistGuardCharacter::GetAlertPatrolSpeedMultiplier() const
{
	return AlertPatrolSpeedMultiplier;
}

float AHeistGuardCharacter::GetEffectivePatrolSpeed() const
{
	return bHasResolvedGuardProfile ? FMath::Max(0.0f, GuardProfile.PatrolSpeed * AlertPatrolSpeedMultiplier) : 0.0f;
}

void AHeistGuardCharacter::ConfigureAsDifficultySupplemental(const AHeistGuardCharacter& SourceGuard)
{
	if (!HasAuthority() || HasActorBegunPlay())
	{
		return;
	}

	GuardProfileId = SourceGuard.GuardProfileId;
	if (IsValid(PatrolPathComponent) && IsValid(SourceGuard.PatrolPathComponent))
	{
		PatrolPathComponent->CopyAuthoredConfigurationFrom(*SourceGuard.PatrolPathComponent);
	}
	bDifficultySupplementalGuard = true;
	bDifficultyActive = true;
}

void AHeistGuardCharacter::SetDifficultyActive(const bool bActive)
{
	if (!HasAuthority())
	{
		return;
	}

	bDifficultyActive = bActive;
	if (IsValid(GuardStateComponent))
	{
		GuardStateComponent->SetDisabled(!bDifficultyActive);
	}
	ApplyDifficultyActivationPresentation();
	ForceNetUpdate();
}

bool AHeistGuardCharacter::IsDifficultyActive() const
{
	return bDifficultyActive;
}

bool AHeistGuardCharacter::IsDifficultySupplementalGuard() const
{
	return bDifficultySupplementalGuard;
}

void AHeistGuardCharacter::ApplyDifficultyActivationPresentation()
{
	SetActorHiddenInGame(!bDifficultyActive);
	SetActorEnableCollision(bDifficultyActive);
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement(); IsValid(MovementComponent))
	{
		if (bDifficultyActive)
		{
			if (MovementComponent->MovementMode == MOVE_None)
			{
				MovementComponent->SetMovementMode(MOVE_Walking);
			}
		}
		else
		{
			MovementComponent->DisableMovement();
		}
	}
}

void AHeistGuardCharacter::OnRep_DifficultyActive()
{
	ApplyDifficultyActivationPresentation();
}

void AHeistGuardCharacter::HandleGuardStateChanged(const EHeistGuardState, const EHeistGuardState NewState)
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	checkf(IsValid(MovementComponent), TEXT("HeistGuardCharacter requires CharacterMovementComponent."));

	if (NewState == EHeistGuardState::Disabled || NewState == EHeistGuardState::Stunned)
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

	MovementComponent->MaxWalkSpeed = NewState == EHeistGuardState::ChasePlayer ? FMath::Max(0.0f, GuardProfile.ChaseSpeed) : GetEffectivePatrolSpeed();
}

#pragma endregion
