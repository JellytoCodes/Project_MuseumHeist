#include "Character/HeistPlayerCharacter.h"

#include "Character/Components/HeistActionComponent.h"
#include "Character/Components/HeistForgeryComponent.h"
#include "Character/Components/HeistInteractionComponent.h"
#include "Character/Components/HeistInventoryComponent.h"
#include "Character/Components/HeistNoiseEmitterComponent.h"
#include "Character/Components/HeistObjectAssemblyComponent.h"
#include "Character/Components/HeistStatusComponent.h"
#include "Character/Components/HeistVisionComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Core/HeistCollisionChannels.h"
#include "Core/HeistGameState.h"
#include "Core/HeistGameplayTags.h"
#include "Core/HeistLogChannels.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistPlayerState.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundMix.h"
#include "UI/Widgets/HeistNameplateWidget.h"

#pragma region Construction

AHeistPlayerCharacter::AHeistPlayerCharacter()
{
	NameplateWidgetClass = UHeistNameplateWidget::StaticClass();
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetMesh(), TEXT("FirstPersonCameraSocket"));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetFieldOfView(90.0f);

	StatusComponent = CreateDefaultSubobject<UHeistStatusComponent>(TEXT("StatusComponent"));
	InventoryComponent = CreateDefaultSubobject<UHeistInventoryComponent>(TEXT("InventoryComponent"));
	InteractionComponent = CreateDefaultSubobject<UHeistInteractionComponent>(TEXT("InteractionComponent"));
	ActionComponent = CreateDefaultSubobject<UHeistActionComponent>(TEXT("ActionComponent"));
	ForgeryComponent = CreateDefaultSubobject<UHeistForgeryComponent>(TEXT("ForgeryComponent"));
	ObjectAssemblyComponent = CreateDefaultSubobject<UHeistObjectAssemblyComponent>(TEXT("ObjectAssemblyComponent"));
	VisionComponent = CreateDefaultSubobject<UHeistVisionComponent>(TEXT("VisionComponent"));
	NoiseEmitterComponent = CreateDefaultSubobject<UHeistNoiseEmitterComponent>(TEXT("NoiseEmitterComponent"));
	CrewStatusVFXComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("CrewStatusVFXComponent"));
	CrewStatusVFXComponent->SetupAttachment(GetMesh());
	CrewStatusVFXComponent->SetAutoActivate(false);
	CrewStatusTransitionAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("CrewStatusTransitionAudioComponent"));
	CrewStatusTransitionAudioComponent->SetupAttachment(GetRootComponent());
	CrewStatusTransitionAudioComponent->bAutoActivate = false;
	CrewStatusTransitionAudioComponent->bAllowSpatialization = true;
	CrewStatusTransitionAudioComponent->bOverrideAttenuation = true;
	CrewStatusFootstepAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("CrewStatusFootstepAudioComponent"));
	CrewStatusFootstepAudioComponent->SetupAttachment(GetRootComponent());
	CrewStatusFootstepAudioComponent->bAutoActivate = false;
	CrewStatusFootstepAudioComponent->bAllowSpatialization = true;
	CrewStatusFootstepAudioComponent->bOverrideAttenuation = true;
	FSoundAttenuationSettings CrewStatusAttenuation;
	CrewStatusAttenuation.bAttenuate = true;
	CrewStatusAttenuation.bSpatialize = true;
	CrewStatusAttenuation.AttenuationShape = EAttenuationShape::Sphere;
	CrewStatusAttenuation.AttenuationShapeExtents = FVector(150.0f, 0.0f, 0.0f);
	CrewStatusAttenuation.FalloffDistance = 850.0f;
	CrewStatusFootstepAudioComponent->SetAttenuationOverrides(CrewStatusAttenuation);
	CrewStatusTransitionAudioComponent->SetAttenuationOverrides(CrewStatusAttenuation);
	RescueInteractionTarget = CreateDefaultSubobject<USphereComponent>(TEXT("RescueInteractionTarget"));
	RescueInteractionTarget->SetupAttachment(GetCapsuleComponent());
	RescueInteractionTarget->SetSphereRadius(60.0f);
	RescueInteractionTarget->SetRelativeLocation(FVector(0.0f, 0.0f, 45.0f));
	RescueInteractionTarget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RescueInteractionTarget->SetCollisionObjectType(HeistCollisionChannels::Interactable);
	RescueInteractionTarget->SetCollisionResponseToAllChannels(ECR_Ignore);
	RescueInteractionTarget->SetCollisionResponseToChannel(HeistCollisionChannels::Player, ECR_Overlap);
	RescueInteractionTarget->SetGenerateOverlapEvents(true);
	NameplateWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("NameplateWidgetComponent"));
	NameplateWidgetComponent->SetupAttachment(GetCapsuleComponent());
	NameplateWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	NameplateWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	NameplateWidgetComponent->SetDrawSize(FVector2D(260.0f, 56.0f));
	NameplateWidgetComponent->SetWidgetClass(NameplateWidgetClass);
	NameplateWidgetComponent->SetOwnerNoSee(true);
	NameplateWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = WalkMoveSpeed;
}

#pragma endregion

#pragma region Lifecycle

void AHeistPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	checkf(IsValid(FirstPersonCamera), TEXT("HeistPlayerCharacter requires FirstPersonCamera"));
	checkf(IsValid(StatusComponent), TEXT("HeistPlayerCharacter requires HeistStatusComponent"));
	checkf(IsValid(InventoryComponent), TEXT("HeistPlayerCharacter requires HeistInventoryComponent"));
	checkf(IsValid(InteractionComponent), TEXT("HeistPlayerCharacter requires HeistInteractionComponent"));
	checkf(IsValid(ActionComponent), TEXT("HeistPlayerCharacter requires HeistActionComponent"));
	checkf(IsValid(ForgeryComponent), TEXT("HeistPlayerCharacter requires HeistForgeryComponent"));
	checkf(IsValid(ObjectAssemblyComponent), TEXT("HeistPlayerCharacter requires HeistObjectAssemblyComponent"));
	checkf(IsValid(VisionComponent), TEXT("HeistPlayerCharacter requires HeistVisionComponent"));
	checkf(IsValid(NoiseEmitterComponent), TEXT("HeistPlayerCharacter requires HeistNoiseEmitterComponent"));
	checkf(IsValid(CrewStatusVFXComponent), TEXT("HeistPlayerCharacter requires CrewStatusVFXComponent"));
	checkf(IsValid(CrewStatusTransitionAudioComponent), TEXT("HeistPlayerCharacter requires CrewStatusTransitionAudioComponent"));
	checkf(IsValid(CrewStatusFootstepAudioComponent), TEXT("HeistPlayerCharacter requires CrewStatusFootstepAudioComponent"));
	checkf(IsValid(RescueInteractionTarget), TEXT("HeistPlayerCharacter requires RescueInteractionTarget"));
	checkf(IsValid(NameplateWidgetComponent), TEXT("HeistPlayerCharacter requires NameplateWidgetComponent"));
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

		const USkeletalMesh* SkeletalMeshAsset = GetMesh()->GetSkeletalMeshAsset();
		const bool bUsesEnginePlaceholderMesh = IsValid(SkeletalMeshAsset) && SkeletalMeshAsset->GetPathName().StartsWith(TEXT("/Engine/EngineMeshes/SkeletalCube"));
		if (bUsesEnginePlaceholderMesh)
		{
			UE_LOG(LogHeist, Log, TEXT("[%s] First-person camera placeholder fallback: Mesh=%s RequestedSocket=%s Fallback=CapsuleEyeHeight"), *GetName(),
				   *GetNameSafe(SkeletalMeshAsset), *FirstPersonCameraSocketName.ToString());
		}
		else
		{
			UE_LOG(LogHeist, Warning, TEXT("[%s] First-person camera socket setup failed: Mesh=%s RequestedSocket=%s Reason=MissingSocket Fallback=CapsuleEyeHeight"), *GetName(),
				   *GetNameSafe(SkeletalMeshAsset), *FirstPersonCameraSocketName.ToString());
		}
	}

	GetMesh()->SetCastShadow(true);
	StatusComponent->GetStatusTagsChangedDelegate().AddUObject(this, &AHeistPlayerCharacter::HandleStatusStateForCrewStatus);
	InventoryComponent->GetInventoryChangedDelegate().AddUObject(this, &AHeistPlayerCharacter::HandleInventoryStateForCrewStatus);
	ForgeryComponent->GetSessionStateChangedDelegate().AddUObject(this, &AHeistPlayerCharacter::HandleForgeryStateForCrewStatus);
	ObjectAssemblyComponent->GetSessionStateChangedDelegate().AddUObject(this, &AHeistPlayerCharacter::HandleAssemblyStateForCrewStatus);
	BindPresentationGameState();
	RefreshAuthoritativeCrewStatus();
	RefreshNameplatePresentation();

	UE_LOG(LogHeist, Log,
		TEXT(
			"[%s] First-person camera contract: Camera=%s RequestedSocket=%s SocketResolved=%s Parent=%s AttachedSocket=%s RelativeLocation=%s FOV=%.1f UsePawnControlRotation=%s UseControllerYaw=%s OrientRotationToMovement=%s FullBodyVisible=%s HeadHidden=false CastShadow=%s"),
		*GetName(), *GetNameSafe(FirstPersonCamera), *FirstPersonCameraSocketName.ToString(), bCameraSocketResolved ? TEXT("true") : TEXT("false"), *GetNameSafe(FirstPersonCamera->GetAttachParent()),
		*FirstPersonCamera->GetAttachSocketName().ToString(), *FirstPersonCamera->GetRelativeLocation().ToCompactString(), FirstPersonCamera->FieldOfView,
		FirstPersonCamera->bUsePawnControlRotation ? TEXT("true") : TEXT("false"), bUseControllerRotationYaw ? TEXT("true") : TEXT("false"),
		GetCharacterMovement()->bOrientRotationToMovement ? TEXT("true") : TEXT("false"), GetMesh()->IsVisible() ? TEXT("true") : TEXT("false"), GetMesh()->CastShadow ? TEXT("true") : TEXT("false"));
}

void AHeistPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BoundPresentationGameState.IsValid())
	{
		BoundPresentationGameState->GetMatchPhaseChangedDelegate().RemoveAll(this);
	}
	if (BoundPresentationPlayerState.IsValid())
	{
		BoundPresentationPlayerState->GetCrewStatusChangedDelegate().RemoveAll(this);
	}
	BoundPresentationPlayerState.Reset();
	ResetCrewStatusPresentation();
	Super::EndPlay(EndPlayReason);
}

void AHeistPlayerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	BindPresentationGameState();
	RefreshNameplatePresentation();
	const AHeistPlayerState* HeistPlayerState = GetPlayerState<AHeistPlayerState>();
	ApplyCrewStatusPresentation(IsValid(HeistPlayerState) ? HeistPlayerState->GetCrewStatus() : EHeistCrewStatus::Active);
}

void AHeistPlayerCharacter::UnPossessed()
{
	if (BoundPresentationPlayerState.IsValid())
	{
		BoundPresentationPlayerState->GetCrewStatusChangedDelegate().RemoveAll(this);
		BoundPresentationPlayerState.Reset();
	}
	ResetCrewStatusPresentation();
	Super::UnPossessed();
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
	bSprinting = bSprintRequested && CanPerformGameplayActions();
	CurrentMoveSpeed = CalculateMoveSpeedFromWeight(TotalLootWeight, bSprinting);
	ApplyCurrentMoveSpeed();
	ForceNetUpdate();

	UHeistDebugFunctionLibrary::DebugWeightMovementSpeedApplied(this, TotalLootWeight, bSprinting ? SprintMoveSpeed : WalkMoveSpeed, CurrentMoveSpeed);
}

void AHeistPlayerCharacter::SetSprintRequested(const bool bRequested)
{
	if (!HasAuthority())
	{
		return;
	}
	bSprintRequested = bRequested;
	RefreshMovementSpeedFromWeight();
}

bool AHeistPlayerCharacter::IsSprinting() const
{
	return bSprinting;
}

float AHeistPlayerCharacter::CalculateMovementSpeedForPace(const float TotalWeight, const bool bSprintPace) const
{
	return CalculateMoveSpeedFromWeight(TotalWeight, bSprintPace);
}

void AHeistPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	RefreshMovementSpeedFromWeight();
	ApplyPlayerStateGameplayRestrictions();
	RefreshNameplatePresentation();
}

float AHeistPlayerCharacter::CalculateMoveSpeedFromWeight(float InTotalWeight, const bool bUseSprintPace) const
{
	const float SafeBaseMoveSpeed = FMath::Max(0.0f, bUseSprintPace ? SprintMoveSpeed : WalkMoveSpeed);
	const float SafeMinimumMoveSpeed = FMath::Clamp(bUseSprintPace ? MinimumSprintMoveSpeed : MinimumWalkMoveSpeed, 0.0f, SafeBaseMoveSpeed);
	const float SafeWeightSpeedPenalty = FMath::Max(0.0f, bUseSprintPace ? SprintWeightSpeedPenalty : WalkWeightSpeedPenalty);
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

void AHeistPlayerCharacter::OnRep_Sprinting()
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
	const bool bStunned = IsValid(StatusComponent) && StatusComponent->HasStatusTag(FHeistGameplayTags::Get().Event_Player_Stunned);
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const bool bWorldRestricted = IsValid(HeistGameState) && HeistGameState->AreWorldInteractionsRestricted();
	return !bEscaped && !bArrested && !bStunned && !bInventoryOpen && !bForgeryActive && !bObjectAssemblyActive && !bWorldRestricted;
}

void AHeistPlayerCharacter::HandleInventoryOpenStateChanged(const bool bInventoryOpen)
{
	if (AHeistPlayerController* HeistPlayerController = Cast<AHeistPlayerController>(GetController()))
	{
		HeistPlayerController->HandleInventoryOpenStateChanged(bInventoryOpen);
	}

	if (!bInventoryOpen)
	{
		RefreshAuthoritativeCrewStatus();
		return;
	}
	if (HasAuthority())
	{
		SetSprintRequested(false);
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
	const bool bStunned = IsValid(StatusComponent) && StatusComponent->HasStatusTag(FHeistGameplayTags::Get().Event_Player_Stunned);

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		if (bEscaped || bArrested || bStunned)
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
		HeistPlayerController->HandlePlayerStunStateChanged(bStunned);
	}
	RefreshAuthoritativeCrewStatus();
}

void AHeistPlayerCharacter::ApplyEscapedGameplayRestrictions()
{
	ApplyPlayerStateGameplayRestrictions();
}

void AHeistPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	ApplyPlayerStateGameplayRestrictions();
	RefreshNameplatePresentation();
}

void AHeistPlayerCharacter::OnPlayerStateChanged(APlayerState* NewPlayerState, APlayerState* OldPlayerState)
{
	Super::OnPlayerStateChanged(NewPlayerState, OldPlayerState);
	if (AHeistPlayerState* OldHeistPlayerState = Cast<AHeistPlayerState>(OldPlayerState))
	{
		OldHeistPlayerState->GetCrewStatusChangedDelegate().RemoveAll(this);
	}
	BoundPresentationPlayerState.Reset();
	RefreshNameplatePresentation();
}

void AHeistPlayerCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	RefreshNameplatePresentation();
	if (!IsLocallyControlled())
	{
		SetLocalStunPostProcessEnabled(false);
		return;
	}

	BindPresentationGameState();
	const AHeistPlayerState* HeistPlayerState = GetPlayerState<AHeistPlayerState>();
	ApplyCrewStatusPresentation(IsValid(HeistPlayerState) ? HeistPlayerState->GetCrewStatus() : EHeistCrewStatus::Active);
}

void AHeistPlayerCharacter::RefreshNameplatePresentation()
{
	if (!IsValid(NameplateWidgetComponent))
	{
		return;
	}
	if (NameplateWidgetClass && NameplateWidgetComponent->GetWidgetClass() != NameplateWidgetClass)
	{
		NameplateWidgetComponent->SetWidgetClass(NameplateWidgetClass);
		NameplateWidgetComponent->InitWidget();
	}
	NameplateWidgetComponent->SetVisibility(UHeistNameplateWidget::ShouldDisplayForLocalControl(IsLocallyControlled()));
	if (UHeistNameplateWidget* NameplateWidget = Cast<UHeistNameplateWidget>(NameplateWidgetComponent->GetUserWidgetObject()))
	{
		NameplateWidget->SetupPlayerState(GetPlayerState<AHeistPlayerState>());
	}
	AHeistPlayerState* HeistPlayerState = GetPlayerState<AHeistPlayerState>();
	if (BoundPresentationPlayerState.Get() != HeistPlayerState)
	{
		if (BoundPresentationPlayerState.IsValid())
		{
			BoundPresentationPlayerState->GetCrewStatusChangedDelegate().RemoveAll(this);
		}
		BoundPresentationPlayerState = HeistPlayerState;
		if (IsValid(HeistPlayerState))
		{
			HeistPlayerState->GetCrewStatusChangedDelegate().AddUObject(this, &AHeistPlayerCharacter::HandleReplicatedCrewStatus);
		}
	}
	if (IsValid(HeistPlayerState))
	{
		HandleReplicatedCrewStatus(HeistPlayerState->GetCrewStatus());
	}
}

void AHeistPlayerCharacter::HandleReplicatedCrewStatus(const EHeistCrewStatus CrewStatus)
{
	ApplyCrewStatusPresentation(CrewStatus);
}

void AHeistPlayerCharacter::BindPresentationGameState()
{
	AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (BoundPresentationGameState.Get() == HeistGameState)
	{
		return;
	}
	if (BoundPresentationGameState.IsValid())
	{
		BoundPresentationGameState->GetMatchPhaseChangedDelegate().RemoveAll(this);
	}
	BoundPresentationGameState = HeistGameState;
	if (IsValid(HeistGameState))
	{
		HeistGameState->GetMatchPhaseChangedDelegate().AddUObject(this, &AHeistPlayerCharacter::HandlePresentationMatchPhaseChanged);
	}
}

void AHeistPlayerCharacter::HandlePresentationMatchPhaseChanged(const EHeistMatchPhase, const EHeistMatchPhase NewMatchPhase)
{
	if (NewMatchPhase != EHeistMatchPhase::InGame)
	{
		ResetCrewStatusPresentation();
		return;
	}
	const AHeistPlayerState* HeistPlayerState = GetPlayerState<AHeistPlayerState>();
	ApplyCrewStatusPresentation(IsValid(HeistPlayerState) ? HeistPlayerState->GetCrewStatus() : EHeistCrewStatus::Active);
}

void AHeistPlayerCharacter::ApplyCrewStatusPresentation(const EHeistCrewStatus CrewStatus)
{
	BindPresentationGameState();
	const AHeistGameState* HeistGameState = BoundPresentationGameState.Get();
	const EHeistCrewStatus PresentationStatus = IsValid(HeistGameState) && HeistGameState->GetMatchPhase() == EHeistMatchPhase::InGame ? CrewStatus : EHeistCrewStatus::Active;
	const EHeistCrewStatus PreviousPresentationStatus = AppliedCrewStatusPresentation;
	const bool bStatusChanged = bCrewStatusPresentationInitialized && PreviousPresentationStatus != PresentationStatus;
	AppliedCrewStatusPresentation = PresentationStatus;

	SetLocalStunPostProcessEnabled(PresentationStatus == EHeistCrewStatus::Stunned);
	if (PresentationStatus != EHeistCrewStatus::CarryingOriginal && PresentationStatus != EHeistCrewStatus::Heavy &&
		(PreviousPresentationStatus == EHeistCrewStatus::CarryingOriginal || PreviousPresentationStatus == EHeistCrewStatus::Heavy) && IsValid(CrewStatusFootstepAudioComponent))
	{
		CrewStatusFootstepAudioComponent->Stop();
	}
	ApplyCrewStatusVFX(PresentationStatus, bStatusChanged);
	if (bStatusChanged)
	{
		PlayCrewStatusTransitionSound(PresentationStatus);
	}
	BP_ApplyCrewStatusPresentation(PresentationStatus);
	bCrewStatusPresentationInitialized = true;
}

void AHeistPlayerCharacter::ResetCrewStatusPresentation()
{
	SetLocalStunPostProcessEnabled(false);
	if (IsValid(CrewStatusVFXComponent))
	{
		CrewStatusVFXComponent->DeactivateImmediate();
		CrewStatusVFXComponent->SetAsset(nullptr);
	}
	if (IsValid(CrewStatusTransitionAudioComponent))
	{
		CrewStatusTransitionAudioComponent->Stop();
		CrewStatusTransitionAudioComponent->SetSound(nullptr);
	}
	if (IsValid(CrewStatusFootstepAudioComponent))
	{
		CrewStatusFootstepAudioComponent->Stop();
	}
	AppliedCrewStatusPresentation = EHeistCrewStatus::Active;
	bCrewStatusPresentationInitialized = false;
	BP_ApplyCrewStatusPresentation(EHeistCrewStatus::Active);
}

UNiagaraSystem* AHeistPlayerCharacter::ResolveCrewStatusVFX(const EHeistCrewStatus CrewStatus) const
{
	switch (CrewStatus)
	{
	case EHeistCrewStatus::Forging:
		return ForgingStatusVFX;
	case EHeistCrewStatus::Assembling:
		return AssemblingStatusVFX;
	case EHeistCrewStatus::CarryingOriginal:
		return CarryingOriginalStatusVFX;
	case EHeistCrewStatus::Heavy:
		return HeavyStatusVFX;
	case EHeistCrewStatus::Stunned:
		return StunnedStatusVFX;
	case EHeistCrewStatus::Arrested:
		return ArrestedStatusVFX;
	case EHeistCrewStatus::Escaped:
		return EscapedStatusBurstVFX;
	case EHeistCrewStatus::Active:
	default:
		return nullptr;
	}
}

USoundBase* AHeistPlayerCharacter::ResolveCrewStatusTransitionSound(const EHeistCrewStatus CrewStatus) const
{
	switch (CrewStatus)
	{
	case EHeistCrewStatus::Forging:
		return ForgingStatusSound;
	case EHeistCrewStatus::Assembling:
		return AssemblingStatusSound;
	case EHeistCrewStatus::CarryingOriginal:
		return CarryingOriginalStatusSound;
	case EHeistCrewStatus::Heavy:
		return HeavyStatusSound;
	case EHeistCrewStatus::Stunned:
		return StunnedStatusSound;
	case EHeistCrewStatus::Arrested:
		return ArrestedStatusSound;
	case EHeistCrewStatus::Escaped:
		return EscapedStatusSound;
	case EHeistCrewStatus::Active:
	default:
		return nullptr;
	}
}

void AHeistPlayerCharacter::ApplyCrewStatusVFX(const EHeistCrewStatus CrewStatus, const bool bForceRestart)
{
	if (!IsValid(CrewStatusVFXComponent))
	{
		return;
	}

	UNiagaraSystem* DesiredSystem = ResolveCrewStatusVFX(CrewStatus);
	if (CrewStatus == EHeistCrewStatus::Escaped)
	{
		CrewStatusVFXComponent->DeactivateImmediate();
		CrewStatusVFXComponent->SetAsset(nullptr);
		if (bForceRestart && IsValid(DesiredSystem))
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, DesiredSystem, GetActorLocation(), GetActorRotation(), FVector(1.0f), true, true, ENCPoolMethod::AutoRelease);
		}
		return;
	}
	if (!IsValid(DesiredSystem))
	{
		CrewStatusVFXComponent->DeactivateImmediate();
		CrewStatusVFXComponent->SetAsset(nullptr);
		return;
	}

	const bool bAssetChanged = CrewStatusVFXComponent->GetAsset() != DesiredSystem;
	if (bAssetChanged)
	{
		CrewStatusVFXComponent->DeactivateImmediate();
		CrewStatusVFXComponent->SetAsset(DesiredSystem);
	}
	if (bForceRestart || bAssetChanged)
	{
		CrewStatusVFXComponent->Activate(true);
	}
}

void AHeistPlayerCharacter::PlayCrewStatusTransitionSound(const EHeistCrewStatus CrewStatus)
{
	if (!IsValid(CrewStatusTransitionAudioComponent))
	{
		return;
	}

	CrewStatusTransitionAudioComponent->Stop();
	USoundBase* DesiredSound = ResolveCrewStatusTransitionSound(CrewStatus);
	CrewStatusTransitionAudioComponent->SetSound(DesiredSound);
	if (IsValid(DesiredSound))
	{
		CrewStatusTransitionAudioComponent->Play();
	}
}

void AHeistPlayerCharacter::SetLocalStunPostProcessEnabled(const bool bEnabled)
{
	if (bEnabled && (!IsLocallyControlled() || !IsValid(FirstPersonCamera)))
	{
		return;
	}
	if (bEnabled == bLocalStunPostProcessEnabled && (bEnabled || (!bStunPostProcessSnapshotValid && !bStunSoundMixPushed)))
	{
		return;
	}

	if (bEnabled)
	{
		FPostProcessSettings& Settings = FirstPersonCamera->PostProcessSettings;
		bSavedOverrideColorSaturation = Settings.bOverride_ColorSaturation;
		bSavedOverrideVignetteIntensity = Settings.bOverride_VignetteIntensity;
		SavedColorSaturation = Settings.ColorSaturation;
		SavedVignetteIntensity = Settings.VignetteIntensity;
		SavedPostProcessBlendWeight = FirstPersonCamera->PostProcessBlendWeight;
		bStunPostProcessSnapshotValid = true;

		Settings.bOverride_ColorSaturation = true;
		Settings.ColorSaturation = FVector4(0.45f, 0.45f, 0.45f, 1.0f);
		Settings.bOverride_VignetteIntensity = true;
		Settings.VignetteIntensity = 0.60f;
		FirstPersonCamera->PostProcessBlendWeight = 1.0f;
		if (IsValid(StunSoundMix) && !bStunSoundMixPushed)
		{
			UGameplayStatics::PushSoundMixModifier(this, StunSoundMix);
			bStunSoundMixPushed = true;
		}
	}
	else
	{
		if (bStunPostProcessSnapshotValid && IsValid(FirstPersonCamera))
		{
			FPostProcessSettings& Settings = FirstPersonCamera->PostProcessSettings;
			Settings.bOverride_ColorSaturation = bSavedOverrideColorSaturation;
			Settings.ColorSaturation = SavedColorSaturation;
			Settings.bOverride_VignetteIntensity = bSavedOverrideVignetteIntensity;
			Settings.VignetteIntensity = SavedVignetteIntensity;
			FirstPersonCamera->PostProcessBlendWeight = SavedPostProcessBlendWeight;
			bStunPostProcessSnapshotValid = false;
		}
		if (bStunSoundMixPushed && IsValid(StunSoundMix))
		{
			UGameplayStatics::PopSoundMixModifier(this, StunSoundMix);
		}
		bStunSoundMixPushed = false;
	}
	bLocalStunPostProcessEnabled = bEnabled;
}

void AHeistPlayerCharacter::NotifyAuthoritativeCrewStatusFootstep(const bool bSprintingPace)
{
	if (!HasAuthority())
	{
		return;
	}
	const AHeistPlayerState* HeistPlayerState = GetPlayerState<AHeistPlayerState>();
	if (!IsValid(HeistPlayerState))
	{
		return;
	}
	const EHeistCrewStatus CrewStatus = HeistPlayerState->GetCrewStatus();
	if (CrewStatus == EHeistCrewStatus::CarryingOriginal || CrewStatus == EHeistCrewStatus::Heavy)
	{
		Multicast_PlayCrewStatusFootstep(CrewStatus, bSprintingPace);
	}
}

void AHeistPlayerCharacter::Multicast_PlayCrewStatusFootstep_Implementation(const EHeistCrewStatus CrewStatus, const bool bSprintingPace)
{
	if (!IsValid(CrewStatusFootstepAudioComponent))
	{
		return;
	}
	USoundBase* CrewStatusSound = CrewStatus == EHeistCrewStatus::CarryingOriginal ? CarryingOriginalFootstepSound.Get() :
		(CrewStatus == EHeistCrewStatus::Heavy ? HeavyFootstepSound.Get() : nullptr);
	if (!IsValid(CrewStatusSound))
	{
		return;
	}
	CrewStatusFootstepAudioComponent->Stop();
	CrewStatusFootstepAudioComponent->SetSound(CrewStatusSound);
	CrewStatusFootstepAudioComponent->SetPitchMultiplier(bSprintingPace ? 1.08f : 1.0f);
	CrewStatusFootstepAudioComponent->Play();
	++CrewStatusFootstepPlayCount;
}

bool AHeistPlayerCharacter::AreCrewStatusAudioAssetsAssignedForDebug() const
{
	return IsValid(StunSoundMix) && IsValid(CarryingOriginalFootstepSound) && IsValid(HeavyFootstepSound);
}

bool AHeistPlayerCharacter::IsCrewStatusAudioPlayingForDebug() const
{
	return IsValid(CrewStatusFootstepAudioComponent) && CrewStatusFootstepAudioComponent->IsPlaying();
}

bool AHeistPlayerCharacter::AreCrewStatusEffectComponentsReadyForDebug() const
{
	return IsValid(CrewStatusVFXComponent) && IsValid(CrewStatusTransitionAudioComponent);
}

bool AHeistPlayerCharacter::IsCrewStatusEffectPresentationCleanForDebug() const
{
	const bool bVFXClean = !IsValid(CrewStatusVFXComponent) || (!CrewStatusVFXComponent->IsActive() && CrewStatusVFXComponent->GetAsset() == nullptr);
	const bool bTransitionAudioClean = !IsValid(CrewStatusTransitionAudioComponent) ||
		(!CrewStatusTransitionAudioComponent->IsPlaying() && CrewStatusTransitionAudioComponent->GetSound() == nullptr);
	return bVFXClean && bTransitionAudioClean;
}

void AHeistPlayerCharacter::HandleInventoryStateForCrewStatus()
{
	RefreshAuthoritativeCrewStatus();
}

void AHeistPlayerCharacter::HandleForgeryStateForCrewStatus()
{
	if (HasAuthority() && ForgeryComponent->IsSessionActive())
	{
		SetSprintRequested(false);
	}
	RefreshAuthoritativeCrewStatus();
}

void AHeistPlayerCharacter::HandleAssemblyStateForCrewStatus()
{
	if (HasAuthority() && ObjectAssemblyComponent->IsSessionActive())
	{
		SetSprintRequested(false);
	}
	RefreshAuthoritativeCrewStatus();
}

void AHeistPlayerCharacter::HandleStatusStateForCrewStatus(const TArray<FHeistTimedTagState>&)
{
	if (HasAuthority() && StatusComponent->HasStatusTag(FHeistGameplayTags::Get().Event_Player_Stunned))
	{
		SetSprintRequested(false);
		ActionComponent->CancelGameplayActions(TEXT("PlayerStunned"));
		if (InventoryComponent->IsInventoryOpen())
		{
			InventoryComponent->TrySetInventoryOpen(false);
		}
	}
	ApplyPlayerStateGameplayRestrictions();
	RefreshAuthoritativeCrewStatus();
}

void AHeistPlayerCharacter::RefreshAuthoritativeCrewStatus()
{
	AHeistPlayerState* HeistPlayerState = GetPlayerState<AHeistPlayerState>();
	if (HasAuthority() && IsValid(HeistPlayerState))
	{
		HeistPlayerState->RefreshCrewStatus();
	}
	if (IsValid(HeistPlayerState))
	{
		ApplyCrewStatusPresentation(HeistPlayerState->GetCrewStatus());
	}
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
	DOREPLIFETIME(AHeistPlayerCharacter, bSprinting);
}

#pragma endregion

#pragma region GameplayComponents

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

UHeistNoiseEmitterComponent* AHeistPlayerCharacter::GetNoiseEmitterComponent() const
{
	return NoiseEmitterComponent.Get();
}

#pragma endregion
