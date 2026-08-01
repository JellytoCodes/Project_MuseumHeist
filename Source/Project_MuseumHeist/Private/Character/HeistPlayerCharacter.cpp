#include "Character/HeistPlayerCharacter.h"

#include "Character/Components/HeistActionComponent.h"
#include "Character/Components/HeistCustomizationComponent.h"
#include "Character/Components/HeistForgeryComponent.h"
#include "Character/Components/HeistInteractionComponent.h"
#include "Character/Components/HeistInventoryComponent.h"
#include "Character/Components/HeistNoiseEmitterComponent.h"
#include "Character/Components/HeistObjectAssemblyComponent.h"
#include "Character/Components/HeistStatusComponent.h"
#include "Character/Components/HeistTagComponent.h"
#include "Character/Components/HeistVisionComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Core/HeistCollisionChannels.h"
#include "Core/HeistGameState.h"
#include "Core/HeistLogChannels.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistPlayerState.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

#pragma region Construction

AHeistPlayerCharacter::AHeistPlayerCharacter()
{
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetMesh(), TEXT("FirstPersonCameraSocket"));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetFieldOfView(90.0f);

	TagComponent = CreateDefaultSubobject<UHeistTagComponent>(TEXT("TagComponent"));
	StatusComponent = CreateDefaultSubobject<UHeistStatusComponent>(TEXT("StatusComponent"));
	InventoryComponent = CreateDefaultSubobject<UHeistInventoryComponent>(TEXT("InventoryComponent"));
	InteractionComponent = CreateDefaultSubobject<UHeistInteractionComponent>(TEXT("InteractionComponent"));
	ActionComponent = CreateDefaultSubobject<UHeistActionComponent>(TEXT("ActionComponent"));
	ForgeryComponent = CreateDefaultSubobject<UHeistForgeryComponent>(TEXT("ForgeryComponent"));
	ObjectAssemblyComponent = CreateDefaultSubobject<UHeistObjectAssemblyComponent>(TEXT("ObjectAssemblyComponent"));
	VisionComponent = CreateDefaultSubobject<UHeistVisionComponent>(TEXT("VisionComponent"));
	CustomizationComponent = CreateDefaultSubobject<UHeistCustomizationComponent>(TEXT("CustomizationComponent"));
	NoiseEmitterComponent = CreateDefaultSubobject<UHeistNoiseEmitterComponent>(TEXT("NoiseEmitterComponent"));
	RescueInteractionTarget = CreateDefaultSubobject<USphereComponent>(TEXT("RescueInteractionTarget"));
	RescueInteractionTarget->SetupAttachment(GetCapsuleComponent());
	RescueInteractionTarget->SetSphereRadius(60.0f);
	RescueInteractionTarget->SetRelativeLocation(FVector(0.0f, 0.0f, 45.0f));
	RescueInteractionTarget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RescueInteractionTarget->SetCollisionObjectType(HeistCollisionChannels::Interactable);
	RescueInteractionTarget->SetCollisionResponseToAllChannels(ECR_Ignore);
	RescueInteractionTarget->SetCollisionResponseToChannel(HeistCollisionChannels::Player, ECR_Overlap);
	RescueInteractionTarget->SetGenerateOverlapEvents(true);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = BaseMoveSpeed;
}

#pragma endregion

#pragma region Lifecycle

void AHeistPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	checkf(IsValid(FirstPersonCamera), TEXT("HeistPlayerCharacter requires FirstPersonCamera"));
	checkf(IsValid(TagComponent), TEXT("HeistPlayerCharacter requires HeistTagComponent"));
	checkf(IsValid(StatusComponent), TEXT("HeistPlayerCharacter requires HeistStatusComponent"));
	checkf(IsValid(InventoryComponent), TEXT("HeistPlayerCharacter requires HeistInventoryComponent"));
	checkf(IsValid(InteractionComponent), TEXT("HeistPlayerCharacter requires HeistInteractionComponent"));
	checkf(IsValid(ActionComponent), TEXT("HeistPlayerCharacter requires HeistActionComponent"));
	checkf(IsValid(ForgeryComponent), TEXT("HeistPlayerCharacter requires HeistForgeryComponent"));
	checkf(IsValid(ObjectAssemblyComponent), TEXT("HeistPlayerCharacter requires HeistObjectAssemblyComponent"));
	checkf(IsValid(VisionComponent), TEXT("HeistPlayerCharacter requires HeistVisionComponent"));
	checkf(IsValid(CustomizationComponent), TEXT("HeistPlayerCharacter requires HeistCustomizationComponent"));
	checkf(IsValid(NoiseEmitterComponent), TEXT("HeistPlayerCharacter requires HeistNoiseEmitterComponent"));
	checkf(IsValid(RescueInteractionTarget), TEXT("HeistPlayerCharacter requires RescueInteractionTarget"));
	checkf(IsValid(GetMesh()), TEXT("HeistPlayerCharacter requires a Full Body SkeletalMeshComponent"));
	GetCapsuleComponent()->SetCollisionObjectType(HeistCollisionChannels::Player);
	GetCapsuleComponent()->SetCollisionResponseToChannel(HeistCollisionChannels::Guard, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(HeistCollisionChannels::Interactable, ECR_Overlap);
	GetCapsuleComponent()->SetGenerateOverlapEvents(true);

	const bool bCameraSocketResolved = GetMesh()->DoesSocketExist(FirstPersonCameraSocketName);
	if (bCameraSocketResolved)
	{
		FirstPersonCamera->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FirstPersonCameraSocketName);
		FirstPersonCamera->SetRelativeLocation(FirstPersonCameraSocketOffset);
	}
	else
	{
		FirstPersonCamera->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f));

		UE_LOG(LogHeist, Warning, TEXT("[%s] First-person camera socket setup failed: RequestedSocket=%s Reason=MissingSocket Fallback=CapsuleEyeHeight"), *GetName(),
			   *FirstPersonCameraSocketName.ToString());
	}

	GetMesh()->SetCastShadow(true);

	UE_LOG(LogHeist, Log,
		TEXT(
			"[%s] First-person camera contract: Camera=%s RequestedSocket=%s SocketResolved=%s Parent=%s AttachedSocket=%s RelativeLocation=%s FOV=%.1f UsePawnControlRotation=%s UseControllerYaw=%s OrientRotationToMovement=%s FullBodyVisible=%s HeadHidden=false CastShadow=%s"),
		*GetName(), *GetNameSafe(FirstPersonCamera), *FirstPersonCameraSocketName.ToString(), bCameraSocketResolved ? TEXT("true") : TEXT("false"), *GetNameSafe(FirstPersonCamera->GetAttachParent()),
		*FirstPersonCamera->GetAttachSocketName().ToString(), *FirstPersonCamera->GetRelativeLocation().ToCompactString(), FirstPersonCamera->FieldOfView,
		FirstPersonCamera->bUsePawnControlRotation ? TEXT("true") : TEXT("false"), bUseControllerRotationYaw ? TEXT("true") : TEXT("false"),
		GetCharacterMovement()->bOrientRotationToMovement ? TEXT("true") : TEXT("false"), GetMesh()->IsVisible() ? TEXT("true") : TEXT("false"), GetMesh()->CastShadow ? TEXT("true") : TEXT("false"));
}

#pragma endregion

#pragma region Camera

void AHeistPlayerCharacter::SetFirstPersonFieldOfView(const float NewFieldOfView)
{
	if (IsValid(FirstPersonCamera) && FMath::IsFinite(NewFieldOfView))
	{
		FirstPersonCamera->SetFieldOfView(NewFieldOfView);
	}
}

float AHeistPlayerCharacter::GetFirstPersonFieldOfView() const
{
	return IsValid(FirstPersonCamera) ? FirstPersonCamera->FieldOfView : 0.0f;
}

#pragma endregion

#pragma region Movement

void AHeistPlayerCharacter::MoveOnGameplayPlane(const FVector2D& MovementInput)
{
	if (!CanPerformGameplayActions() || MovementInput.IsNearlyZero())
	{
		return;
	}

	const FRotator ControlRotation = Controller != nullptr ? Controller->GetControlRotation() : GetActorRotation();
	const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, MovementInput.Y);
	AddMovementInput(RightDirection, MovementInput.X);
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
		UHeistDebugFunctionLibrary::DebugWeightMovementSkipped(this, TEXT("MissingPlayerState"));
		return;
	}

	const float TotalLootWeight = HeistPlayerState->GetTotalLootWeight();
	CurrentMoveSpeed = CalculateMoveSpeedFromWeight(TotalLootWeight);
	ApplyCurrentMoveSpeed();
	ForceNetUpdate();

	UHeistDebugFunctionLibrary::DebugWeightMovementSpeedApplied(this, TotalLootWeight, BaseMoveSpeed, CurrentMoveSpeed);
}

void AHeistPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	RefreshMovementSpeedFromWeight();
	ApplyPlayerStateGameplayRestrictions();
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
		UHeistDebugFunctionLibrary::DebugWeightMovementSkipped(this, TEXT("MissingCharacterMovement"));
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
	const bool bArrested = IsValid(HeistPlayerState) && HeistPlayerState->IsArrested();
	const bool bInventoryOpen = IsValid(InventoryComponent) && InventoryComponent->IsInventoryOpen();
	const bool bForgeryActive = IsValid(ForgeryComponent) && ForgeryComponent->IsSessionActive();
	const bool bObjectAssemblyActive = IsValid(ObjectAssemblyComponent) && ObjectAssemblyComponent->IsSessionActive();
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const bool bWorldRestricted = IsValid(HeistGameState) && HeistGameState->AreWorldInteractionsRestricted();
	return !bEscaped && !bArrested && !bInventoryOpen && !bForgeryActive && !bObjectAssemblyActive && !bWorldRestricted;
}

void AHeistPlayerCharacter::HandleInventoryOpenStateChanged(const bool bInventoryOpen)
{
	if (AHeistPlayerController* HeistPlayerController = Cast<AHeistPlayerController>(GetController()))
	{
		HeistPlayerController->HandleInventoryOpenStateChanged(bInventoryOpen);
	}

	if (!bInventoryOpen)
	{
		return;
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}
}

void AHeistPlayerCharacter::ApplyPlayerStateGameplayRestrictions()
{
	const AHeistPlayerState* HeistPlayerState = GetPlayerState<AHeistPlayerState>();
	if (!IsValid(HeistPlayerState))
	{
		return;
	}
	const bool bEscaped = HeistPlayerState->IsEscaped();
	const bool bArrested = HeistPlayerState->IsArrested();

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		if (bEscaped || bArrested)
		{
			MovementComponent->StopMovementImmediately();
			MovementComponent->DisableMovement();
		}
		else
		{
			MovementComponent->SetMovementMode(MOVE_Walking);
			ApplyCurrentMoveSpeed();
		}
	}

	SetActorEnableCollision(!bEscaped);
	SetActorHiddenInGame(bEscaped);
	if (IsValid(RescueInteractionTarget))
	{
		RescueInteractionTarget->SetCollisionEnabled(bArrested && !bEscaped ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}

	if (bEscaped)
	{
		UHeistDebugFunctionLibrary::DebugEscapedPlayerRestrictionsApplied(this);
	}
	else
	{
		UHeistDebugFunctionLibrary::Message(this, FString::Printf(TEXT("Player arrest restrictions applied: Character=%s Arrested=%s MovementDisabled=%s Visible=true Collision=true"), *GetName(),
																  bArrested ? TEXT("true") : TEXT("false"), bArrested ? TEXT("true") : TEXT("false")));
	}

	if (AHeistPlayerController* HeistPlayerController = Cast<AHeistPlayerController>(GetController()))
	{
		HeistPlayerController->HandlePlayerTerminalStateChanged(bEscaped, bArrested);
	}
}

void AHeistPlayerCharacter::ApplyEscapedGameplayRestrictions()
{
	ApplyPlayerStateGameplayRestrictions();
}

void AHeistPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	ApplyPlayerStateGameplayRestrictions();
}

#pragma endregion

#pragma region RescueInteraction

bool AHeistPlayerCharacter::CanInteract(const AActor* Interactor) const
{
	const AHeistPlayerCharacter* RescuingCharacter = Cast<AHeistPlayerCharacter>(Interactor);
	const AHeistPlayerState* TargetPlayerState = GetPlayerState<AHeistPlayerState>();
	const AHeistPlayerState* RescuingPlayerState = IsValid(RescuingCharacter) ? RescuingCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	return IsValid(RescuingCharacter) && RescuingCharacter != this && IsValid(TargetPlayerState) && IsValid(RescuingPlayerState) &&
		TargetPlayerState->IsArrested() && !TargetPlayerState->IsEscaped() && !RescuingPlayerState->IsArrested() && !RescuingPlayerState->IsEscaped() &&
		IsValid(HeistGameState) && HeistGameState->GetMatchPhase() == EHeistMatchPhase::InGame && !HeistGameState->AreWorldInteractionsRestricted();
}

void AHeistPlayerCharacter::Interact(AActor* Interactor)
{
	if (!HasAuthority() || !CanInteract(Interactor))
	{
		return;
	}

	if (AHeistPlayerState* TargetPlayerState = GetPlayerState<AHeistPlayerState>())
	{
		TargetPlayerState->ClearArrested();
	}
}

bool AHeistPlayerCharacter::IsRescueInteractionAvailable() const
{
	const AHeistPlayerState* TargetPlayerState = GetPlayerState<AHeistPlayerState>();
	return IsValid(TargetPlayerState) && TargetPlayerState->IsArrested() && !TargetPlayerState->IsEscaped() && IsValid(RescueInteractionTarget) &&
		RescueInteractionTarget->GetCollisionEnabled() == ECollisionEnabled::QueryOnly;
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

UHeistForgeryComponent* AHeistPlayerCharacter::GetForgeryComponent() const
{
	return ForgeryComponent.Get();
}

UHeistObjectAssemblyComponent* AHeistPlayerCharacter::GetObjectAssemblyComponent() const
{
	return ObjectAssemblyComponent.Get();
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
