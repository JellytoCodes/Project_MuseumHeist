#include "Character/HeistPlayerCharacter.h"

#include "Character/Components/HeistActionComponent.h"
#include "Character/Components/HeistCustomizationComponent.h"
#include "Character/Components/HeistInteractionComponent.h"
#include "Character/Components/HeistInventoryComponent.h"
#include "Character/Components/HeistNoiseEmitterComponent.h"
#include "Character/Components/HeistStatusComponent.h"
#include "Character/Components/HeistTagComponent.h"
#include "Character/Components/HeistVisionComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/HeistPlayerState.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"

#pragma region Construction

AHeistPlayerCharacter::AHeistPlayerCharacter()
{
	CameraSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraSpringArm"));
	CameraSpringArm->SetupAttachment(GetCapsuleComponent());
	CameraSpringArm->SetUsingAbsoluteRotation(true);
	CameraSpringArm->TargetArmLength = 1200.0f;
	CameraSpringArm->SetRelativeRotation(FRotator(-75.0f, 0.0f, 0.0f));
	CameraSpringArm->bUsePawnControlRotation = false;
	CameraSpringArm->bInheritPitch = false;
	CameraSpringArm->bInheritYaw = false;
	CameraSpringArm->bInheritRoll = false;
	CameraSpringArm->bDoCollisionTest = false;
	CameraSpringArm->bEnableCameraLag = false;

	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCamera->SetupAttachment(CameraSpringArm, USpringArmComponent::SocketName);
	TopDownCamera->bUsePawnControlRotation = false;

	TagComponent = CreateDefaultSubobject<UHeistTagComponent>(TEXT("TagComponent"));
	StatusComponent = CreateDefaultSubobject<UHeistStatusComponent>(TEXT("StatusComponent"));
	InventoryComponent = CreateDefaultSubobject<UHeistInventoryComponent>(TEXT("InventoryComponent"));
	InteractionComponent = CreateDefaultSubobject<UHeistInteractionComponent>(TEXT("InteractionComponent"));
	ActionComponent = CreateDefaultSubobject<UHeistActionComponent>(TEXT("ActionComponent"));
	VisionComponent = CreateDefaultSubobject<UHeistVisionComponent>(TEXT("VisionComponent"));
	CustomizationComponent = CreateDefaultSubobject<UHeistCustomizationComponent>(TEXT("CustomizationComponent"));
	NoiseEmitterComponent = CreateDefaultSubobject<UHeistNoiseEmitterComponent>(TEXT("NoiseEmitterComponent"));

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->MaxWalkSpeed = BaseMoveSpeed;
}

#pragma endregion

#pragma region Lifecycle

void AHeistPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	checkf(IsValid(CameraSpringArm), TEXT("HeistPlayerCharacter requires CameraSpringArm"));
	checkf(IsValid(TopDownCamera), TEXT("HeistPlayerCharacter requires TopDownCamera"));
	checkf(IsValid(TagComponent), TEXT("HeistPlayerCharacter requires HeistTagComponent"));
	checkf(IsValid(StatusComponent), TEXT("HeistPlayerCharacter requires HeistStatusComponent"));
	checkf(IsValid(InventoryComponent), TEXT("HeistPlayerCharacter requires HeistInventoryComponent"));
	checkf(IsValid(InteractionComponent), TEXT("HeistPlayerCharacter requires HeistInteractionComponent"));
	checkf(IsValid(ActionComponent), TEXT("HeistPlayerCharacter requires HeistActionComponent"));
	checkf(IsValid(VisionComponent), TEXT("HeistPlayerCharacter requires HeistVisionComponent"));
	checkf(IsValid(CustomizationComponent), TEXT("HeistPlayerCharacter requires HeistCustomizationComponent"));
	checkf(IsValid(NoiseEmitterComponent), TEXT("HeistPlayerCharacter requires HeistNoiseEmitterComponent"));
}

#pragma endregion

#pragma region Movement

void AHeistPlayerCharacter::MoveOnGameplayPlane(const FVector2D& MovementInput)
{
	if (!CanPerformGameplayActions() || MovementInput.IsNearlyZero())
	{
		return;
	}

	AddMovementInput(FVector::ForwardVector, MovementInput.Y);
	AddMovementInput(FVector::RightVector, MovementInput.X);
}

void AHeistPlayerCharacter::RefreshMovementSpeedFromWeight()
{
	if (!HasAuthority())
	{
		return;
	}

	const AHeistPlayerState* HeistPlayerState = GetPlayerState<AHeistPlayerState>();
	if (!IsValid(HeistPlayerState))
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			FString::Printf(
				TEXT("Weight movement speed skipped: Character=%s Reason=MissingPlayerState"),
				*GetNameSafe(this)),
			EHeistDebugLevel::Warning);
		return;
	}

	const float TotalLootWeight = HeistPlayerState->GetTotalLootWeight();
	CurrentMoveSpeed = CalculateMoveSpeedFromWeight(TotalLootWeight);
	ApplyCurrentMoveSpeed();
	ForceNetUpdate();

	UHeistDebugFunctionLibrary::Message(
		this,
		FString::Printf(
			TEXT("Weight movement speed applied: Character=%s TotalWeight=%.2f BaseSpeed=%.2f FinalSpeed=%.2f"),
			*GetNameSafe(this),
			TotalLootWeight,
			BaseMoveSpeed,
			CurrentMoveSpeed));
}

void AHeistPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	RefreshMovementSpeedFromWeight();
	ApplyEscapedGameplayRestrictions();
}

float AHeistPlayerCharacter::CalculateMoveSpeedFromWeight(float InTotalWeight) const
{
	const float SafeBaseMoveSpeed = FMath::Max(0.0f, BaseMoveSpeed);
	const float SafeMinimumMoveSpeed = FMath::Clamp(MinimumMoveSpeed, 0.0f, SafeBaseMoveSpeed);
	const float SafeWeightSpeedPenalty = FMath::Max(0.0f, WeightSpeedPenalty);
	const float SafeTotalWeight = FMath::IsFinite(InTotalWeight) ? FMath::Max(0.0f, InTotalWeight) : 0.0f;
	const float UnclampedMoveSpeed = SafeBaseMoveSpeed - (SafeTotalWeight * SafeWeightSpeedPenalty);

	return FMath::Clamp(UnclampedMoveSpeed, SafeMinimumMoveSpeed, SafeBaseMoveSpeed);
}

void AHeistPlayerCharacter::ApplyCurrentMoveSpeed()
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (!IsValid(MovementComponent))
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			FString::Printf(
				TEXT("Weight movement speed skipped: Character=%s Reason=MissingCharacterMovement"),
				*GetNameSafe(this)),
			EHeistDebugLevel::Warning);
		return;
	}

	MovementComponent->MaxWalkSpeed = CurrentMoveSpeed;
}

void AHeistPlayerCharacter::OnRep_CurrentMoveSpeed()
{
	ApplyCurrentMoveSpeed();
}

#pragma endregion

#pragma region EscapeState

bool AHeistPlayerCharacter::CanPerformGameplayActions() const
{
	const AHeistPlayerState* HeistPlayerState = GetPlayerState<AHeistPlayerState>();
	const bool bEscaped = IsValid(HeistPlayerState) && HeistPlayerState->IsEscaped();
	const bool bInventoryOpen = IsValid(InventoryComponent) && InventoryComponent->IsInventoryOpen();
	const bool bStunned = IsValid(StatusComponent) && StatusComponent->IsStunned();
	return !bEscaped && !bInventoryOpen && !bStunned;
}

void AHeistPlayerCharacter::HandleInventoryOpenStateChanged(const bool bInventoryOpen)
{
	if (!bInventoryOpen)
	{
		return;
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}
}

void AHeistPlayerCharacter::ApplyEscapedGameplayRestrictions()
{
	if (CanPerformGameplayActions())
	{
		return;
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
	}

	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);

	UHeistDebugFunctionLibrary::Message(
		this,
		FString::Printf(
			TEXT("Escaped player restrictions applied: Character=%s MovementDisabled=true InteractionDisabled=true CollisionDisabled=true Hidden=true"),
			*GetNameSafe(this)));
}

void AHeistPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	ApplyEscapedGameplayRestrictions();
}

#pragma endregion

#pragma region Replication

void AHeistPlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeistPlayerCharacter, CurrentMoveSpeed);
}

#pragma endregion

#pragma region GameplayComponents

UHeistTagComponent* AHeistPlayerCharacter::GetTagComponent() const
{
	return TagComponent.Get();
}

UHeistStatusComponent* AHeistPlayerCharacter::GetStatusComponent() const
{
	return StatusComponent.Get();
}

UHeistInventoryComponent* AHeistPlayerCharacter::GetInventoryComponent() const
{
	return InventoryComponent.Get();
}

UHeistInteractionComponent* AHeistPlayerCharacter::GetInteractionComponent() const
{
	return InteractionComponent.Get();
}

UHeistActionComponent* AHeistPlayerCharacter::GetActionComponent() const
{
	return ActionComponent.Get();
}

UHeistVisionComponent* AHeistPlayerCharacter::GetVisionComponent() const
{
	return VisionComponent.Get();
}

UHeistCustomizationComponent* AHeistPlayerCharacter::GetCustomizationComponent() const
{
	return CustomizationComponent.Get();
}

UHeistNoiseEmitterComponent* AHeistPlayerCharacter::GetNoiseEmitterComponent() const
{
	return NoiseEmitterComponent.Get();
}

#pragma endregion
