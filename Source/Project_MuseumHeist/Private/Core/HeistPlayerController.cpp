#include "Core/HeistPlayerController.h"

#include "AI/HeistGuardAIController.h"
#include "AI/HeistGuardCharacter.h"
#include "AI/HeistGuardStateComponent.h"
#include "Character/Components/HeistActionComponent.h"
#include "Character/Components/HeistForgeryComponent.h"
#include "Character/Components/HeistInteractionComponent.h"
#include "Character/Components/HeistInventoryComponent.h"
#include "Character/Components/HeistNoiseEmitterComponent.h"
#include "Character/Components/HeistObjectAssemblyComponent.h"
#include "Character/Components/HeistVisionComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Components/InputComponent.h"
#include "Core/HeistGameInstance.h"
#include "Core/HeistGameUserSettings.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistGameState.h"
#include "Core/HeistHUD.h"
#include "Core/HeistLogChannels.h"
#include "Core/HeistPlayerState.h"
#if !UE_BUILD_SHIPPING
#include "Debug/HeistCheatManager.h"
#endif
#include "Debug/HeistDebugFunctionLibrary.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Interfaces/VoiceInterface.h"
#include "Inventory/HeistInventoryTypes.h"
#include "Inventory/HeistItemDataTypes.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystemUtils.h"
#include "World/Actors/Escape/HeistVentActor.h"
#include "World/Actors/Loot/HeistPaintingDisplayCaseActor.h"
#include "World/Actors/Loot/HeistObjectDisplayCaseActor.h"
#include "World/Actors/Loot/HeistDroppedOriginalActor.h"
#include "World/Actors/Loot/HeistLootActor.h"
#include "World/Actors/Projectile/HeistCoinProjectile.h"
#include "World/Actors/Projectile/HeistThrowableProjectile.h"
#include "World/Actors/Security/HeistSecurityHoldButtonActor.h"
#include "World/Interaction/HeistInteractable.h"

namespace
{
constexpr double ForgeryPointQuantizationMaximum = 65535.0;

bool TryPackForgeryPoint(const FVector2D& NormalizedPoint, uint32& OutPackedPoint)
{
	if (!FMath::IsFinite(NormalizedPoint.X) || !FMath::IsFinite(NormalizedPoint.Y) || !FMath::IsWithinInclusive(NormalizedPoint.X, 0.0, 1.0) ||
		!FMath::IsWithinInclusive(NormalizedPoint.Y, 0.0, 1.0))
	{
		return false;
	}

	const uint32 QuantizedX = static_cast<uint32>(FMath::Clamp(FMath::RoundToInt64(NormalizedPoint.X * ForgeryPointQuantizationMaximum), static_cast<int64>(0), static_cast<int64>(MAX_uint16)));
	const uint32 QuantizedY = static_cast<uint32>(FMath::Clamp(FMath::RoundToInt64(NormalizedPoint.Y * ForgeryPointQuantizationMaximum), static_cast<int64>(0), static_cast<int64>(MAX_uint16)));
	OutPackedPoint = QuantizedX | (QuantizedY << 16);
	return true;
}

FVector2D UnpackForgeryPoint(const uint32 PackedPoint)
{
	const double NormalizedX = static_cast<double>(PackedPoint & MAX_uint16) / ForgeryPointQuantizationMaximum;
	const double NormalizedY = static_cast<double>((PackedPoint >> 16) & MAX_uint16) / ForgeryPointQuantizationMaximum;
	return FVector2D(NormalizedX, NormalizedY);
}

bool CanUseHeistInteraction(AActor* TargetActor, const AActor* Interactor)
{
	IHeistInteractable* InteractableTarget = Cast<IHeistInteractable>(TargetActor);
	return InteractableTarget != nullptr && InteractableTarget->CanInteract(Interactor);
}

IOnlineSubsystem* ResolveVoiceOnlineSubsystem(UWorld* World)
{
	IOnlineSubsystem* OnlineSubsystem = nullptr;
#if WITH_EDITOR
	if (GIsEditor && IsValid(World))
	{
		OnlineSubsystem = Online::GetSubsystem(World, NULL_SUBSYSTEM);
	}
#endif
	if (OnlineSubsystem == nullptr && IsValid(World))
	{
		OnlineSubsystem = Online::GetSubsystem(World);
	}
	return OnlineSubsystem != nullptr ? OnlineSubsystem : IOnlineSubsystem::Get();
}
}

#pragma region Construction

AHeistPlayerController::AHeistPlayerController()
{
#if !UE_BUILD_SHIPPING
	CheatClass = UHeistCheatManager::StaticClass();
#endif
}

#pragma endregion

#pragma region Lifecycle

void AHeistPlayerController::BeginPlay()
{
	Super::BeginPlay();

#if !UE_BUILD_SHIPPING
	if (IsLocalController())
	{
		EnableCheats();
	}
#endif

	if (IsLocalController())
	{
		if (UHeistGameInstance* HeistGameInstance = Cast<UHeistGameInstance>(GetGameInstance()))
		{
			HeistGameInstance->NotifySessionWorldReady();
		}
	}

	RefreshLocalInputModeFromPawn();
	RefreshLocalPlayerTerminalState();
	RefreshLocalHUDPresentation();
	ApplyLocalUserSettings();
	UpdateFlashlightAimDirection();
}

void AHeistPlayerController::PawnLeavingGame()
{
	if (HasAuthority())
	{
		if (AHeistGameMode* HeistGameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr)
		{
			HeistGameMode->HandlePlayerPawnLeavingGame(this);
		}
	}

	Super::PawnLeavingGame();
}

void AHeistPlayerController::ResetLocalHeldInteractionInputState()
{
	bLocalObservationInputHeld = false;
	LocalSecurityHoldButton.Reset();
}

void AHeistPlayerController::UnbindMatchPhasePresentationState()
{
	if (BoundMatchPhaseGameState.IsValid())
	{
		BoundMatchPhaseGameState->GetMatchPhaseChangedDelegate().RemoveAll(this);
		BoundMatchPhaseGameState->GetPlayerResultsChangedDelegate().RemoveAll(this);
		BoundMatchPhaseGameState->GetAlertStateChangedDelegate().RemoveAll(this);
		BoundMatchPhaseGameState->GetEscapePhaseStateChangedDelegate().RemoveAll(this);
	}

	BoundMatchPhaseGameState.Reset();
}

void AHeistPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetLocalHeldInteractionInputState();
	ResetLocalVoicePushToTalk(false);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LocalTutorialStepTimerHandle);
	}
	UnbindLocalForgeryInputState();
	UnbindLocalObjectAssemblyInputState();
	UnbindMatchPhasePresentationState();
	if (IsLocalController())
	{
		CloseFloorPlanMapForStateTransition();
		ResetIgnoreMoveInput();
		ResetIgnoreLookInput();
		if (APawn* ControlledPawn = GetPawn())
		{
			SetViewTarget(ControlledPawn);
		}
	}
	bLocalAwaitingCrew = false;
	LocalSpectateTarget.Reset();
	Super::EndPlay(EndPlayReason);
}

void AHeistPlayerController::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();

	ResetLocalHeldInteractionInputState();
	ResetLocalVoicePushToTalk(false);
	UnbindLocalForgeryInputState();
	UnbindLocalObjectAssemblyInputState();
	UnbindMatchPhasePresentationState();
	bLocalAwaitingCrew = false;
	LocalSpectateTarget.Reset();

	if (!IsLocalController())
	{
		return;
	}

	CloseFloorPlanMapForStateTransition();
	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
	if (UHeistGameInstance* HeistGameInstance = Cast<UHeistGameInstance>(GetGameInstance()))
	{
		HeistGameInstance->NotifySessionWorldReady();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LocalTutorialStepTimerHandle);
		World->GetTimerManager().SetTimerForNextTick(this, &AHeistPlayerController::RefreshLocalPresentationAfterSeamlessTravel);
	}
}

void AHeistPlayerController::RefreshLocalPresentationAfterSeamlessTravel()
{
	if (!IsLocalController())
	{
		return;
	}

	RefreshLocalInputModeFromPawn();
	RefreshLocalPlayerTerminalState();
	RefreshLocalHUDPresentation();
	ApplyLocalUserSettings();
	UpdateFlashlightAimDirection();
}

void AHeistPlayerController::OnPossess(APawn* InPawn)
{
	ResetLocalHeldInteractionInputState();
	ResetLocalVoicePushToTalk(true);
	UnbindLocalForgeryInputState();
	UnbindLocalObjectAssemblyInputState();
	Super::OnPossess(InPawn);
	RefreshLocalInputModeFromPawn();
	RefreshLocalPlayerTerminalState();
	RefreshLocalHUDPresentation();
	ApplyLocalUserSettings();
	UpdateFlashlightAimDirection();
}

void AHeistPlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();
	ResetLocalHeldInteractionInputState();
	ResetLocalVoicePushToTalk(true);
	RefreshLocalInputModeFromPawn();
	RefreshLocalPlayerTerminalState();
	RefreshLocalHUDPresentation();
	ApplyLocalUserSettings();
	UpdateFlashlightAimDirection();
}

void AHeistPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	RefreshLocalPlayerTerminalState();
	RefreshLocalHUDPresentation();
}

void AHeistPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!ensureMsgf(EnhancedInputComponent != nullptr, TEXT("HeistPlayerController requires EnhancedInputComponent")))
	{
		return;
	}

	if (MoveInputAction != nullptr)
	{
		EnhancedInputComponent->BindAction(MoveInputAction, ETriggerEvent::Triggered, this, &AHeistPlayerController::HandleMoveInput);
	}
	else
	{
		UHeistDebugFunctionLibrary::DebugMissingInputAsset(this, TEXT("MoveInputAction"));
	}

	if (LookInputAction != nullptr)
	{
		EnhancedInputComponent->BindAction(LookInputAction, ETriggerEvent::Triggered, this, &AHeistPlayerController::HandleLookInput);
	}
	else
	{
		UHeistDebugFunctionLibrary::DebugMissingInputAsset(this, TEXT("LookInputAction"));
	}

	if (InteractInputAction != nullptr)
	{
		EnhancedInputComponent->BindAction(InteractInputAction, ETriggerEvent::Started, this, &AHeistPlayerController::HandleInteractPressed);
		EnhancedInputComponent->BindAction(InteractInputAction, ETriggerEvent::Completed, this, &AHeistPlayerController::HandleInteractReleased);
		EnhancedInputComponent->BindAction(InteractInputAction, ETriggerEvent::Canceled, this, &AHeistPlayerController::HandleInteractReleased);
	}
	else
	{
		UHeistDebugFunctionLibrary::DebugMissingInputAsset(this, TEXT("InteractInputAction"));
	}

	if (InventoryInputAction != nullptr)
	{
		EnhancedInputComponent->BindAction(InventoryInputAction, ETriggerEvent::Started, this, &AHeistPlayerController::HandleInventoryToggle);
	}
	else
	{
		UHeistDebugFunctionLibrary::DebugMissingInputAsset(this, TEXT("InventoryInputAction"));
	}

	if (ForgeryCancelInputAction != nullptr)
	{
		EnhancedInputComponent->BindAction(ForgeryCancelInputAction, ETriggerEvent::Started, this, &AHeistPlayerController::HandleForgeryCancel);
	}
	else
	{
		UHeistDebugFunctionLibrary::DebugMissingInputAsset(this, TEXT("ForgeryCancelInputAction"));
	}

	if (SprintInputAction != nullptr)
	{
		EnhancedInputComponent->BindAction(SprintInputAction, ETriggerEvent::Started, this, &AHeistPlayerController::HandleSprintStarted);
		EnhancedInputComponent->BindAction(SprintInputAction, ETriggerEvent::Completed, this, &AHeistPlayerController::HandleSprintStopped);
		EnhancedInputComponent->BindAction(SprintInputAction, ETriggerEvent::Canceled, this, &AHeistPlayerController::HandleSprintStopped);
	}
	else
	{
		UHeistDebugFunctionLibrary::DebugMissingInputAsset(this, TEXT("SprintInputAction"));
	}

	if (MapInputAction != nullptr)
	{
		EnhancedInputComponent->BindAction(MapInputAction, ETriggerEvent::Started, this, &AHeistPlayerController::HandleMapToggle);
	}
	else
	{
		UHeistDebugFunctionLibrary::DebugMissingInputAsset(this, TEXT("MapInputAction"));
	}
	// Replica review is a world-state choice, not a full-screen forgery action.
	// Keep R available only while the controller is back in Gameplay mode.
	FInputKeyBinding& ReplicaRedrawBinding = InputComponent->BindKey(EKeys::R, IE_Pressed, this, &AHeistPlayerController::HandleReplicaRedraw);
	ReplicaRedrawBinding.bConsumeInput = false;
	FInputKeyBinding& VoicePressedBinding = InputComponent->BindKey(EKeys::V, IE_Pressed, this, &AHeistPlayerController::HandleVoicePushToTalkPressed);
	VoicePressedBinding.bConsumeInput = false;
	FInputKeyBinding& VoiceReleasedBinding = InputComponent->BindKey(EKeys::V, IE_Released, this, &AHeistPlayerController::HandleVoicePushToTalkReleased);
	VoiceReleasedBinding.bConsumeInput = false;

	RefreshLocalInputModeFromPawn();
}

void AHeistPlayerController::RefreshLocalHUDPresentation()
{
	if (!IsLocalController())
	{
		return;
	}

	RefreshMatchPhasePresentationBinding();
	if (AHeistHUD* HeistHUD = GetHUD<AHeistHUD>())
	{
		HeistHUD->RefreshPresentationSources();
		const UHeistGameInstance* HeistGameInstance = Cast<UHeistGameInstance>(GetGameInstance());
		const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
		if (IsValid(HeistGameInstance) && HeistGameInstance->IsCurrentWorldTitleMenu())
		{
			HeistHUD->HideResultScreen();
			HeistHUD->HideMainHUD();
			HeistHUD->HideLobbyScreen();
			HeistHUD->ShowTitleMenuScreen();
		}
		else if (IsValid(HeistGameState) && HeistGameState->GetMatchPhase() == EHeistMatchPhase::Lobby)
		{
			HeistHUD->HideResultScreen();
			HeistHUD->HideMainHUD();
			HeistHUD->HideTitleMenuScreen();
			HeistHUD->ShowLobbyScreen();
		}
		else if (IsValid(HeistGameState) && HeistGameState->GetMatchPhase() == EHeistMatchPhase::End)
		{
			HeistHUD->HideMainHUD();
			HeistHUD->HideTitleMenuScreen();
			HeistHUD->HideLobbyScreen();
			HeistHUD->ShowResultScreen();
			ApplyLocalInputMode(EHeistInputMode::Inventory);
		}
		else
		{
			HeistHUD->HideResultScreen();
			HeistHUD->HideTitleMenuScreen();
			HeistHUD->HideLobbyScreen();
			HeistHUD->ShowMainHUD();
		}
	}
	RefreshLocalTutorialFromMatchPhase();
}

void AHeistPlayerController::RefreshMatchPhasePresentationBinding()
{
	AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (BoundMatchPhaseGameState.Get() == HeistGameState)
	{
		return;
	}

	UnbindMatchPhasePresentationState();
	BoundMatchPhaseGameState = HeistGameState;
	if (IsValid(HeistGameState))
	{
		HeistGameState->GetMatchPhaseChangedDelegate().AddUObject(this, &AHeistPlayerController::HandleMatchPhasePresentationChanged);
		HeistGameState->GetPlayerResultsChangedDelegate().AddUObject(this, &AHeistPlayerController::HandlePlayerResultsPresentationChanged);
		HeistGameState->GetAlertStateChangedDelegate().AddUObject(this, &AHeistPlayerController::HandleAlertStatePresentationChanged);
		HeistGameState->GetEscapePhaseStateChangedDelegate().AddUObject(this, &AHeistPlayerController::HandleEscapePhasePresentationChanged);
	}
}

void AHeistPlayerController::HandleMatchPhasePresentationChanged(const EHeistMatchPhase, const EHeistMatchPhase NewMatchPhase)
{
	if (NewMatchPhase != EHeistMatchPhase::InGame)
	{
		LocalSecurityHoldButton.Reset();
		CloseFloorPlanMapForStateTransition();
	}
	RefreshLocalPlayerTerminalState();
	RefreshLocalHUDPresentation();
}

void AHeistPlayerController::HandlePlayerResultsPresentationChanged()
{
	if (bLocalAwaitingCrew)
	{
		RefreshLocalSpectateTarget();
	}
	RefreshLocalHUDPresentation();
}

void AHeistPlayerController::HandleAlertStatePresentationChanged(const EHeistAlertLevel, const EHeistAlertLevel NewAlertLevel, const int32, const FName)
{
	if (NewAlertLevel != EHeistAlertLevel::Quiet)
	{
		NotifyLocalTutorialMilestone(TEXT("Alert"), TEXT("AlertRaised"));
	}
}

void AHeistPlayerController::HandleEscapePhasePresentationChanged(const bool bEscapePhaseOpen)
{
	if (AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>())
	{
		HeistCharacter->GetInteractionComponent()->RefreshInteractionTarget();
	}

	if (bEscapePhaseOpen)
	{
		NotifyLocalTutorialMilestone(TEXT("Extraction"), TEXT("EscapePhaseOpened"));
	}
}

#pragma endregion

#pragma region Input

void AHeistPlayerController::HandleLookInput(const FInputActionValue& InputValue)
{
	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	if (!ensureMsgf(HeistCharacter != nullptr, TEXT("Look input requires a possessed HeistPlayerCharacter")) || !HeistCharacter->CanPerformGameplayActions())
	{
		return;
	}

	const FVector2D LookInput = InputValue.Get<FVector2D>() * LocalMouseSensitivity;
	AddYawInput(LookInput.X);
	AddPitchInput(LookInput.Y);
	UpdateFlashlightAimDirection();
}

void AHeistPlayerController::HandleMoveInput(const FInputActionValue& InputValue)
{
	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	if (!ensureMsgf(HeistCharacter != nullptr, TEXT("Move input requires a possessed HeistPlayerCharacter")))
	{
		return;
	}

	const FVector2D MovementInput = InputValue.Get<FVector2D>();
	HeistCharacter->MoveOnGameplayPlane(MovementInput);
}

void AHeistPlayerController::HandleSprintStarted()
{
	if (LocalInputMode == EHeistInputMode::Gameplay)
	{
		RequestSetSprintRequested(true);
	}
}

void AHeistPlayerController::HandleSprintStopped()
{
	RequestSetSprintRequested(false);
}

void AHeistPlayerController::RequestSetSprintRequested(const bool bRequested)
{
	if (IsLocalController())
	{
		Server_SetSprintRequested(bRequested);
	}
}

void AHeistPlayerController::HandleMapToggle()
{
	ToggleFloorPlanMap();
}

void AHeistPlayerController::CloseFloorPlanMapForStateTransition()
{
	if (!IsLocalController())
	{
		return;
	}
	if (AHeistHUD* HeistHUD = GetHUD<AHeistHUD>())
	{
		HeistHUD->HideFloorPlanMap();
	}
	if (LocalInputMode == EHeistInputMode::Map)
	{
		ApplyLocalInputMode(EHeistInputMode::Gameplay);
	}
}

bool AHeistPlayerController::ToggleFloorPlanMap()
{
	if (!IsLocalController())
	{
		return false;
	}
	AHeistHUD* HeistHUD = GetHUD<AHeistHUD>();
	if (!IsValid(HeistHUD))
	{
		return false;
	}
	if (LocalInputMode == EHeistInputMode::Map)
	{
		HeistHUD->HideFloorPlanMap();
		ApplyLocalInputMode(EHeistInputMode::Gameplay);
		return !HeistHUD->IsFloorPlanMapVisible() && LocalInputMode == EHeistInputMode::Gameplay && IsLocalInputModeContractSatisfied();
	}
	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	if (LocalInputMode != EHeistInputMode::Gameplay || !IsValid(HeistCharacter) || !HeistCharacter->CanPerformGameplayActions())
	{
		return false;
	}
	if (HeistHUD->ShowFloorPlanMap())
	{
		RequestSetSprintRequested(false);
		ApplyLocalInputMode(EHeistInputMode::Map);
		return LocalInputMode == EHeistInputMode::Map && IsLocalInputModeContractSatisfied();
	}
	return false;
}

void AHeistPlayerController::HandleInventoryToggle()
{
	if (LocalInputMode == EHeistInputMode::Forgery || LocalInputMode == EHeistInputMode::Map)
	{
		return;
	}

	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	if (!IsValid(HeistCharacter))
	{
		return;
	}

	UHeistInventoryComponent* InventoryComponent = HeistCharacter->GetInventoryComponent();
	checkf(IsValid(InventoryComponent), TEXT("HeistPlayerCharacter requires HeistInventoryComponent"));

	const bool bRequestOpen = !InventoryComponent->IsInventoryOpen();
	RequestSetInventoryOpen(bRequestOpen);
}

void AHeistPlayerController::HandleForgeryCancel()
{
	if (LocalInputMode == EHeistInputMode::Forgery)
	{
		const AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
		const UHeistObjectAssemblyComponent* ObjectAssemblyComponent =
			IsValid(HeistCharacter) ? HeistCharacter->GetObjectAssemblyComponent() : nullptr;
		if (IsValid(ObjectAssemblyComponent) && ObjectAssemblyComponent->IsSessionActive())
		{
			RequestCancelObjectAssembly();
		}
		else
		{
			RequestCancelForgery();
		}
	}
}

void AHeistPlayerController::HandleReplicaRedraw()
{
	if (LocalInputMode != EHeistInputMode::Gameplay)
	{
		return;
	}

	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	if (!IsValid(HeistCharacter) || !HeistCharacter->CanPerformGameplayActions())
	{
		return;
	}

	UHeistInteractionComponent* InteractionComponent = HeistCharacter->GetInteractionComponent();
	if (!IsValid(InteractionComponent) || !InteractionComponent->RefreshInteractionTarget())
	{
		return;
	}

	AHeistPaintingDisplayCaseActor* TargetDisplayCase = Cast<AHeistPaintingDisplayCaseActor>(InteractionComponent->GetCurrentInteractionTarget());
	if (IsValid(TargetDisplayCase) && TargetDisplayCase->IsReplicaReviewReadyFor(HeistCharacter))
	{
		RequestRestartForgeryFromPreview();
		return;
	}

	AHeistObjectDisplayCaseActor* TargetObjectDisplayCase = Cast<AHeistObjectDisplayCaseActor>(InteractionComponent->GetCurrentInteractionTarget());
	if (!IsValid(TargetObjectDisplayCase) || !TargetObjectDisplayCase->IsReplicaReviewReadyFor(HeistCharacter))
	{
		return;
	}

	RequestRestartObjectAssemblyFromPreview();
}

void AHeistPlayerController::RefreshLocalForgeryInputBinding()
{
	if (!IsLocalController())
	{
		return;
	}

	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	UHeistForgeryComponent* ForgeryComponent = IsValid(HeistCharacter) ? HeistCharacter->GetForgeryComponent() : nullptr;
	if (BoundForgeryComponent.Get() == ForgeryComponent)
	{
		return;
	}

	UnbindLocalForgeryInputState();
	if (IsValid(ForgeryComponent))
	{
		BoundForgeryComponent = ForgeryComponent;
		bLocalForgerySessionActive = ForgeryComponent->IsSessionActive();
		ForgeryComponent->GetSessionStateChangedDelegate().AddUObject(this, &AHeistPlayerController::HandleForgerySessionStateChanged);
	}
}

void AHeistPlayerController::UnbindLocalForgeryInputState()
{
	if (UHeistForgeryComponent* ForgeryComponent = BoundForgeryComponent.Get())
	{
		ForgeryComponent->GetSessionStateChangedDelegate().RemoveAll(this);
	}
	BoundForgeryComponent.Reset();
	bLocalForgerySessionActive = false;
}

void AHeistPlayerController::HandleForgerySessionStateChanged()
{
	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	const UHeistForgeryComponent* ForgeryComponent = IsValid(HeistCharacter) ? HeistCharacter->GetForgeryComponent() : nullptr;
	const bool bForgeryActive = IsValid(ForgeryComponent) && ForgeryComponent->IsSessionActive();
	if (bForgeryActive == bLocalForgerySessionActive)
	{
		return;
	}
	bLocalForgerySessionActive = bForgeryActive;

	if (bForgeryActive && IsValid(HeistCharacter->GetCharacterMovement()))
	{
		HeistCharacter->GetCharacterMovement()->StopMovementImmediately();
	}

	RefreshLocalInputModeFromPawn();
	if (bForgeryActive && IsCurrentLocalTutorialStep(TEXT("Forgery")))
	{
		bLocalTutorialObservedForgerySession = true;
	}
	if (!bForgeryActive)
	{
		UpdateFlashlightAimDirection();
		if (IsValid(HeistCharacter))
		{
			HeistCharacter->GetInteractionComponent()->RefreshInteractionTarget();
		}
		if (bLocalTutorialObservedForgerySession)
		{
			bLocalTutorialObservedForgerySession = false;
			NotifyLocalTutorialMilestone(TEXT("Forgery"), TEXT("ForgerySessionClosed"));
		}
	}
}

void AHeistPlayerController::RefreshLocalObjectAssemblyInputBinding()
{
	if (!IsLocalController())
	{
		return;
	}

	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	UHeistObjectAssemblyComponent* ObjectAssemblyComponent =
		IsValid(HeistCharacter) ? HeistCharacter->GetObjectAssemblyComponent() : nullptr;
	if (BoundObjectAssemblyComponent.Get() == ObjectAssemblyComponent)
	{
		return;
	}

	UnbindLocalObjectAssemblyInputState();
	if (IsValid(ObjectAssemblyComponent))
	{
		BoundObjectAssemblyComponent = ObjectAssemblyComponent;
		bLocalObjectAssemblySessionActive = ObjectAssemblyComponent->IsSessionActive();
		ObjectAssemblyComponent->GetSessionStateChangedDelegate().AddUObject(this, &AHeistPlayerController::HandleObjectAssemblySessionStateChanged);
	}
}

void AHeistPlayerController::UnbindLocalObjectAssemblyInputState()
{
	if (UHeistObjectAssemblyComponent* ObjectAssemblyComponent = BoundObjectAssemblyComponent.Get())
	{
		ObjectAssemblyComponent->GetSessionStateChangedDelegate().RemoveAll(this);
	}
	BoundObjectAssemblyComponent.Reset();
	bLocalObjectAssemblySessionActive = false;
}

void AHeistPlayerController::HandleObjectAssemblySessionStateChanged()
{
	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	const UHeistObjectAssemblyComponent* ObjectAssemblyComponent =
		IsValid(HeistCharacter) ? HeistCharacter->GetObjectAssemblyComponent() : nullptr;
	const bool bObjectAssemblyActive = IsValid(ObjectAssemblyComponent) && ObjectAssemblyComponent->IsSessionActive();
	if (bObjectAssemblyActive == bLocalObjectAssemblySessionActive)
	{
		return;
	}
	bLocalObjectAssemblySessionActive = bObjectAssemblyActive;

	if (bObjectAssemblyActive && IsValid(HeistCharacter->GetCharacterMovement()))
	{
		HeistCharacter->GetCharacterMovement()->StopMovementImmediately();
	}

	RefreshLocalInputModeFromPawn();
	if (bObjectAssemblyActive && IsCurrentLocalTutorialStep(TEXT("Forgery")))
	{
		bLocalTutorialObservedForgerySession = true;
	}
	if (!bObjectAssemblyActive && LocalInputMode == EHeistInputMode::Gameplay)
	{
		UpdateFlashlightAimDirection();
		if (IsValid(HeistCharacter))
		{
			HeistCharacter->GetInteractionComponent()->RefreshInteractionTarget();
		}
		if (bLocalTutorialObservedForgerySession)
		{
			bLocalTutorialObservedForgerySession = false;
			NotifyLocalTutorialMilestone(TEXT("Forgery"), TEXT("ObjectAssemblySessionClosed"));
		}
	}
}

void AHeistPlayerController::RefreshLocalInputModeFromPawn()
{
	if (!IsLocalController())
	{
		return;
	}

	RefreshLocalForgeryInputBinding();
	RefreshLocalObjectAssemblyInputBinding();
	const AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	const UHeistForgeryComponent* ForgeryComponent = IsValid(HeistCharacter) ? HeistCharacter->GetForgeryComponent() : nullptr;
	const UHeistObjectAssemblyComponent* ObjectAssemblyComponent =
		IsValid(HeistCharacter) ? HeistCharacter->GetObjectAssemblyComponent() : nullptr;
	const UHeistInventoryComponent* InventoryComponent = IsValid(HeistCharacter) ? HeistCharacter->GetInventoryComponent() : nullptr;
	if (AHeistHUD* HeistHUD = GetHUD<AHeistHUD>(); IsValid(HeistHUD) && HeistHUD->IsFloorPlanMapVisible())
	{
		ApplyLocalInputMode(EHeistInputMode::Map);
		return;
	}
	ApplyLocalInputMode((IsValid(ForgeryComponent) && ForgeryComponent->IsSessionActive()) ||
								(IsValid(ObjectAssemblyComponent) && ObjectAssemblyComponent->IsSessionActive())
							? EHeistInputMode::Forgery
						: IsValid(InventoryComponent) && InventoryComponent->IsInventoryOpen() ? EHeistInputMode::Inventory
																							   : EHeistInputMode::Gameplay);
}

void AHeistPlayerController::ApplyLocalInputMode(const EHeistInputMode NewInputMode)
{
	if (!IsLocalController())
	{
		return;
	}

	if (LocalInputMode == NewInputMode && IsLocalInputModeContractSatisfied())
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (IsValid(InputSubsystem))
	{
		if (IsValid(GameplayInputMappingContext.Get()))
		{
			InputSubsystem->RemoveMappingContext(GameplayInputMappingContext);
		}
		if (IsValid(InventoryInputMappingContext.Get()))
		{
			InputSubsystem->RemoveMappingContext(InventoryInputMappingContext);
		}
		if (IsValid(ForgeryInputMappingContext.Get()))
		{
			InputSubsystem->RemoveMappingContext(ForgeryInputMappingContext);
		}
		if (IsValid(MapInputMappingContext.Get()))
		{
			InputSubsystem->RemoveMappingContext(MapInputMappingContext);
		}
	}

	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
	FlushPressedKeys();
	LocalInputMode = NewInputMode;

	if (NewInputMode == EHeistInputMode::Gameplay)
	{
		if (IsValid(InputSubsystem) && IsValid(GameplayInputMappingContext.Get()))
		{
			InputSubsystem->AddMappingContext(GameplayInputMappingContext, 0);
		}
		else if (!IsValid(GameplayInputMappingContext.Get()))
		{
			UHeistDebugFunctionLibrary::DebugMissingInputAsset(this, TEXT("GameplayInputMappingContext"));
		}

		bShowMouseCursor = false;
		bEnableClickEvents = false;
		bEnableMouseOverEvents = false;

		FInputModeGameOnly InputMode;
		InputMode.SetConsumeCaptureMouseDown(true);
		SetInputMode(InputMode);
	}
	else
	{
		if (IsValid(InputSubsystem))
		{
			UInputMappingContext* RequestedMappingContext = NewInputMode == EHeistInputMode::Inventory ? InventoryInputMappingContext.Get()
				: NewInputMode == EHeistInputMode::Map ? MapInputMappingContext.Get()
				: ForgeryInputMappingContext.Get();
			if (IsValid(RequestedMappingContext))
			{
				InputSubsystem->AddMappingContext(RequestedMappingContext, NewInputMode == EHeistInputMode::Inventory ? 10 : 20);
			}
		}
		if (NewInputMode == EHeistInputMode::Inventory && !IsValid(InventoryInputMappingContext.Get()))
		{
			UHeistDebugFunctionLibrary::DebugMissingInputAsset(this, TEXT("InventoryInputMappingContext"));
		}
		else if (NewInputMode == EHeistInputMode::Forgery && !IsValid(ForgeryInputMappingContext.Get()))
		{
			UHeistDebugFunctionLibrary::DebugMissingInputAsset(this, TEXT("ForgeryInputMappingContext"));
		}
		else if (NewInputMode == EHeistInputMode::Map && !IsValid(MapInputMappingContext.Get()))
		{
			UHeistDebugFunctionLibrary::DebugMissingInputAsset(this, TEXT("MapInputMappingContext"));
		}

		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
		bShowMouseCursor = true;
		bEnableClickEvents = true;
		bEnableMouseOverEvents = true;

		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}

	if (IsLocalPlayerTerminalInputBlocked())
	{
		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
	}

	UE_LOG(LogHeist, Verbose,
		   TEXT("[%s] Local input mode applied: Mode=%s GameplayContext=%s InventoryContext=%s ForgeryContext=%s MapContext=%s ActiveContexts=%d Cursor=%s IgnoreMove=%s IgnoreLook=%s Contract=%s"), *GetName(),
		   NewInputMode == EHeistInputMode::Gameplay	? TEXT("Gameplay")
		   : NewInputMode == EHeistInputMode::Inventory ? TEXT("Inventory")
		   : NewInputMode == EHeistInputMode::Map       ? TEXT("Map")
													: TEXT("Forgery"),
		   *GetNameSafe(GameplayInputMappingContext.Get()), *GetNameSafe(InventoryInputMappingContext.Get()), *GetNameSafe(ForgeryInputMappingContext.Get()),
		   *GetNameSafe(MapInputMappingContext.Get()), GetActiveHeistInputMappingContextCount(),
		   bShowMouseCursor ? TEXT("true") : TEXT("false"), IsMoveInputIgnored() ? TEXT("true") : TEXT("false"), IsLookInputIgnored() ? TEXT("true") : TEXT("false"),
		   IsLocalInputModeContractSatisfied() ? TEXT("PASS") : TEXT("FAIL"));
}

void AHeistPlayerController::HandleInventoryOpenStateChanged(const bool bInventoryOpen)
{
	if (!IsLocalController())
	{
		return;
	}

	RefreshLocalInputModeFromPawn();
	if (!bInventoryOpen && LocalInputMode == EHeistInputMode::Gameplay)
	{
		UpdateFlashlightAimDirection();
		if (AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>())
		{
			HeistCharacter->GetInteractionComponent()->RefreshInteractionTarget();
		}
	}
}

void AHeistPlayerController::ApplyLocalUserSettings()
{
	if (!IsLocalController())
	{
		return;
	}

	const UHeistGameUserSettings* Settings = UHeistGameUserSettings::GetHeistGameUserSettings();
	if (!IsValid(Settings))
	{
		LocalMouseSensitivity = UHeistGameUserSettings::DefaultMouseSensitivity;
		return;
	}

	LocalMouseSensitivity = Settings->GetMouseSensitivity();
	if (AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>())
	{
		HeistCharacter->SetFirstPersonFieldOfView(Settings->GetFieldOfView());
	}
}

float AHeistPlayerController::GetLocalMouseSensitivity() const
{
	return LocalMouseSensitivity;
}

void AHeistPlayerController::HandleArrestStateChanged(const bool bArrested)
{
	const AHeistPlayerState* HeistPlayerState = GetPlayerState<AHeistPlayerState>();
	HandlePlayerTerminalStateChanged(IsValid(HeistPlayerState) && HeistPlayerState->IsEscaped(), bArrested);
}

void AHeistPlayerController::HandlePlayerTerminalStateChanged(const bool bEscaped, const bool bArrested)
{
	if (!IsLocalController())
	{
		return;
	}

	if (bEscaped || bArrested)
	{
		CloseFloorPlanMapForStateTransition();
	}
	RefreshLocalPlayerTerminalState();
	UHeistDebugFunctionLibrary::Message(
		this, FString::Printf(TEXT("Local player terminal state applied: Escaped=%s Arrested=%s AwaitingCrew=%s SpectateTarget=%s InputMode=%s Cursor=%s IgnoreMove=%s IgnoreLook=%s"),
							  bEscaped ? TEXT("true") : TEXT("false"), bArrested ? TEXT("true") : TEXT("false"), bLocalAwaitingCrew ? TEXT("true") : TEXT("false"),
							  *GetNameSafe(LocalSpectateTarget.Get()),
							  LocalInputMode == EHeistInputMode::Gameplay	 ? TEXT("Gameplay")
							  : LocalInputMode == EHeistInputMode::Inventory ? TEXT("Inventory")
															 : TEXT("Forgery"),
							  bShowMouseCursor ? TEXT("true") : TEXT("false"), IsMoveInputIgnored() ? TEXT("true") : TEXT("false"), IsLookInputIgnored() ? TEXT("true") : TEXT("false")));
}

void AHeistPlayerController::HandlePlayerStunStateChanged(const bool bStunned)
{
	if (!IsLocalController())
	{
		return;
	}
	if (bStunned)
	{
		CloseFloorPlanMapForStateTransition();
		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
	}
	else
	{
		RefreshLocalInputModeFromPawn();
	}
}

void AHeistPlayerController::RefreshLocalPlayerTerminalState()
{
	if (!IsLocalController())
	{
		return;
	}

	const AHeistPlayerState* HeistPlayerState = GetPlayerState<AHeistPlayerState>();
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const bool bEscaped = IsValid(HeistPlayerState) && HeistPlayerState->IsEscaped();
	const bool bArrested = IsValid(HeistPlayerState) && HeistPlayerState->IsArrested();
	const bool bMatchInGame = !IsValid(HeistGameState) || HeistGameState->GetMatchPhase() == EHeistMatchPhase::InGame;
	const bool bShouldAwaitCrew = bEscaped && bMatchInGame;

	if (bLocalAwaitingCrew != bShouldAwaitCrew)
	{
		bLocalAwaitingCrew = bShouldAwaitCrew;
	}

	RefreshLocalInputModeFromPawn();
	if (bLocalAwaitingCrew)
	{
		RefreshLocalSpectateTarget();
	}
	else
	{
		LocalSpectateTarget.Reset();
		if (APawn* ControlledPawn = GetPawn(); IsValid(ControlledPawn) && GetViewTarget() != ControlledPawn)
		{
			SetViewTargetWithBlend(ControlledPawn, 0.15f);
		}
	}

	UE_LOG(LogHeist, Log,
		   TEXT("[%s] Local terminal lifecycle: Escaped=%s Arrested=%s MatchPhase=%s AwaitingCrew=%s SpectateTarget=%s InputBlocked=%s"), *GetName(),
		   bEscaped ? TEXT("true") : TEXT("false"), bArrested ? TEXT("true") : TEXT("false"),
		   IsValid(HeistGameState) ? *UEnum::GetValueAsString(HeistGameState->GetMatchPhase()) : TEXT("MissingGameState"), bLocalAwaitingCrew ? TEXT("true") : TEXT("false"),
		   *GetNameSafe(LocalSpectateTarget.Get()), IsLocalPlayerTerminalInputBlocked() ? TEXT("true") : TEXT("false"));
}

void AHeistPlayerController::RefreshLocalSpectateTarget()
{
	if (!IsLocalController() || !bLocalAwaitingCrew)
	{
		return;
	}

	AActor* NewSpectateTarget = FindLocalSpectateTarget();
	if (LocalSpectateTarget.Get() == NewSpectateTarget)
	{
		return;
	}

	LocalSpectateTarget = NewSpectateTarget;
	AActor* ResolvedViewTarget = IsValid(NewSpectateTarget) ? NewSpectateTarget : GetPawn();
	if (IsValid(ResolvedViewTarget) && GetViewTarget() != ResolvedViewTarget)
	{
		SetViewTargetWithBlend(ResolvedViewTarget, 0.25f);
	}

	UE_LOG(LogHeist, Log, TEXT("[%s] Escaped player spectate target refreshed: AwaitingCrew=true Target=%s Authority=%s"), *GetName(), *GetNameSafe(NewSpectateTarget),
		   HasAuthority() ? TEXT("true") : TEXT("false"));
}

AActor* AHeistPlayerController::FindLocalSpectateTarget() const
{
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const AHeistPlayerState* LocalPlayerState = GetPlayerState<AHeistPlayerState>();
	AHeistPlayerState* BestPlayerState = nullptr;
	if (!IsValid(HeistGameState))
	{
		return nullptr;
	}

	for (APlayerState* CandidateBaseState : HeistGameState->PlayerArray)
	{
		AHeistPlayerState* CandidateState = Cast<AHeistPlayerState>(CandidateBaseState);
		if (!IsValid(CandidateState) || CandidateState == LocalPlayerState || CandidateState->IsEscaped() || CandidateState->IsArrested() || !IsValid(CandidateState->GetPawn()))
		{
			continue;
		}
		if (!IsValid(BestPlayerState) || CandidateState->HeistPlayerId < BestPlayerState->HeistPlayerId)
		{
			BestPlayerState = CandidateState;
		}
	}

	return IsValid(BestPlayerState) ? BestPlayerState->GetPawn() : nullptr;
}

bool AHeistPlayerController::IsLocalPlayerTerminalInputBlocked() const
{
	const AHeistPlayerState* HeistPlayerState = GetPlayerState<AHeistPlayerState>();
	if (!IsValid(HeistPlayerState) || (!HeistPlayerState->IsEscaped() && !HeistPlayerState->IsArrested()))
	{
		return false;
	}

	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	return !IsValid(HeistGameState) || HeistGameState->GetMatchPhase() == EHeistMatchPhase::InGame;
}

EHeistInputMode AHeistPlayerController::GetLocalInputMode() const
{
	return LocalInputMode;
}

bool AHeistPlayerController::IsLocalInputMappingContextActive(const EHeistInputMode InputMode) const
{
	const UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!IsValid(InputSubsystem))
	{
		return false;
	}

	const UInputMappingContext* MappingContext = nullptr;
	switch (InputMode)
	{
	case EHeistInputMode::Gameplay:
		MappingContext = GameplayInputMappingContext;
		break;
	case EHeistInputMode::Inventory:
		MappingContext = InventoryInputMappingContext;
		break;
	case EHeistInputMode::Forgery:
		MappingContext = ForgeryInputMappingContext;
		break;
	case EHeistInputMode::Map:
		MappingContext = MapInputMappingContext;
		break;
	default:
		break;
	}

	return IsValid(MappingContext) && InputSubsystem->HasMappingContext(MappingContext);
}

int32 AHeistPlayerController::GetActiveHeistInputMappingContextCount() const
{
	int32 ActiveContextCount = 0;
	ActiveContextCount += IsLocalInputMappingContextActive(EHeistInputMode::Gameplay) ? 1 : 0;
	ActiveContextCount += IsLocalInputMappingContextActive(EHeistInputMode::Inventory) ? 1 : 0;
	ActiveContextCount += IsLocalInputMappingContextActive(EHeistInputMode::Forgery) ? 1 : 0;
	ActiveContextCount += IsLocalInputMappingContextActive(EHeistInputMode::Map) ? 1 : 0;
	return ActiveContextCount;
}

bool AHeistPlayerController::IsLocalInputModeContractSatisfied() const
{
	if (!IsLocalController() || GetActiveHeistInputMappingContextCount() != 1 || !IsLocalInputMappingContextActive(LocalInputMode))
	{
		return false;
	}

	const bool bGameplayMode = LocalInputMode == EHeistInputMode::Gameplay;
	const bool bCursorContract = bShowMouseCursor == !bGameplayMode;
	const bool bGameplayInputBlocked = !bGameplayMode || IsLocalPlayerTerminalInputBlocked();
	return bCursorContract && IsMoveInputIgnored() == bGameplayInputBlocked && IsLookInputIgnored() == bGameplayInputBlocked;
}

bool AHeistPlayerController::IsLocalAwaitingCrew() const
{
	return bLocalAwaitingCrew;
}

AActor* AHeistPlayerController::GetLocalSpectateTarget() const
{
	return LocalSpectateTarget.Get();
}

bool AHeistPlayerController::AreW7InputAssetsConfigured() const
{
	return IsValid(SprintInputAction) && IsValid(MapInputAction) && IsValid(GameplayInputMappingContext) && IsValid(MapInputMappingContext);
}

void AHeistPlayerController::UpdateFlashlightAimDirection()
{
	if (!IsLocalController())
	{
		return;
	}

	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	if (!IsValid(HeistCharacter))
	{
		return;
	}

	UHeistInventoryComponent* InventoryComponent = HeistCharacter->GetInventoryComponent();
	checkf(IsValid(InventoryComponent), TEXT("HeistPlayerCharacter requires HeistInventoryComponent"));
	if (InventoryComponent->IsInventoryOpen() || !HeistCharacter->CanPerformGameplayActions())
	{
		return;
	}

	FVector ViewLocation;
	FVector CameraForward;
	FVector TargetWorldLocation;
	if (!TryBuildCameraForwardAim(5000.0f, ViewLocation, CameraForward, TargetWorldLocation))
	{
		return;
	}

	UHeistVisionComponent* VisionComponent = HeistCharacter->GetVisionComponent();
	checkf(IsValid(VisionComponent), TEXT("HeistPlayerCharacter requires HeistVisionComponent"));
	VisionComponent->UpdateFlashlightAimDirection(CameraForward);
	if (!HasAuthority())
	{
		Server_UpdateFlashlightAimDirection(CameraForward);
	}
}

#pragma endregion

#pragma region Voice

void AHeistPlayerController::HandleVoicePushToTalkPressed()
{
	if (!IsLocalController() || bLocalVoicePushToTalkHeld)
	{
		return;
	}

	bLocalVoicePushToTalkHeld = true;
	LocalVoiceActivityLevel = 0.0f;
	LocalVoiceActivityDuration = 0.0f;
	bLocalVoiceSpeaking = false;
	ToggleSpeaking(true);
	Server_SetVoicePushToTalk(true);

	if (UWorld* World = GetWorld())
	{
		const float PollInterval = FMath::Max(0.05f, VoiceActivityPollInterval);
		World->GetTimerManager().SetTimer(LocalVoiceActivityTimerHandle, this, &AHeistPlayerController::PollLocalVoiceActivity, PollInterval, true, 0.0f);
	}
}

void AHeistPlayerController::HandleVoicePushToTalkReleased()
{
	ResetLocalVoicePushToTalk(true);
}

void AHeistPlayerController::ResetLocalVoicePushToTalk(const bool bNotifyServer)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LocalVoiceActivityTimerHandle);
	}

	if (IsLocalController())
	{
		ToggleSpeaking(false);
		if (bNotifyServer)
		{
			Server_SetVoicePushToTalk(false);
		}
	}
	if (HasAuthority() && (!IsLocalController() || !bNotifyServer))
	{
		bServerVoicePushToTalkHeld = false;
	}

	bLocalVoicePushToTalkHeld = false;
	bLocalVoiceSpeaking = false;
	LocalVoiceActivityLevel = 0.0f;
	LocalVoiceActivityDuration = 0.0f;
	LastLocalVoiceActivityReportTime = -1.0;
}

void AHeistPlayerController::PollLocalVoiceActivity()
{
	if (!IsLocalController() || !bLocalVoicePushToTalkHeld)
	{
		ResetLocalVoicePushToTalk(true);
		return;
	}

	bool bVoiceInterfaceReportsTalking = false;
	IOnlineSubsystem* OnlineSubsystem = ResolveVoiceOnlineSubsystem(GetWorld());
	const IOnlineVoicePtr VoiceInterface = OnlineSubsystem != nullptr ? OnlineSubsystem->GetVoiceInterface() : nullptr;
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (VoiceInterface.IsValid() && IsValid(LocalPlayer))
	{
		bVoiceInterfaceReportsTalking = VoiceInterface->IsLocalPlayerTalking(LocalPlayer->GetControllerId());
	}

	bLocalVoiceSpeaking = bVoiceInterfaceReportsTalking;
	LocalVoiceActivityLevel = bLocalVoiceSpeaking ? 1.0f : 0.0f;
	const float PollInterval = FMath::Max(0.05f, VoiceActivityPollInterval);
	LocalVoiceActivityDuration = bLocalVoiceSpeaking ? LocalVoiceActivityDuration + PollInterval : 0.0f;
	if (!bLocalVoiceSpeaking || LocalVoiceActivityDuration + KINDA_SMALL_NUMBER < FMath::Max(0.0f, MinimumVoiceActivityDuration))
	{
		return;
	}

	const UWorld* World = GetWorld();
	const double CurrentTime = IsValid(World) ? static_cast<double>(World->GetRealTimeSeconds()) : 0.0;
	const double ReportInterval = static_cast<double>(FMath::Max(0.0f, VoiceActivityReportInterval));
	if (LastLocalVoiceActivityReportTime >= 0.0 && CurrentTime - LastLocalVoiceActivityReportTime < ReportInterval)
	{
		return;
	}

	Server_ReportVoiceActivity();
	LastLocalVoiceActivityReportTime = CurrentTime;
}

void AHeistPlayerController::SetPlayerVoiceMuted(AHeistPlayerState* TargetPlayerState, const bool bMuted)
{
	if (!IsLocalController() || !IsValid(TargetPlayerState) || !TargetPlayerState->GetUniqueId().IsValid())
	{
		return;
	}

	if (bMuted)
	{
		GameplayMutePlayer(TargetPlayerState->GetUniqueId());
	}
	else
	{
		GameplayUnmutePlayer(TargetPlayerState->GetUniqueId());
	}
}

bool AHeistPlayerController::IsPlayerVoiceMuted(AHeistPlayerState* TargetPlayerState)
{
	if (!IsLocalController() || !IsValid(TargetPlayerState))
	{
		return false;
	}

	const FUniqueNetIdPtr UniqueNetId = TargetPlayerState->GetUniqueId().GetUniqueNetId();
	return UniqueNetId.IsValid() && IsPlayerMuted(*UniqueNetId);
}

#pragma endregion

#pragma region Interaction

void AHeistPlayerController::HandleInteractPressed()
{
	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	if (!ensureMsgf(HeistCharacter != nullptr, TEXT("Interact input requires a possessed HeistPlayerCharacter")))
	{
		return;
	}

	if (!HeistCharacter->CanPerformGameplayActions())
	{
		return;
	}

	UHeistInteractionComponent* InteractionComponent = HeistCharacter->GetInteractionComponent();
	if (!InteractionComponent->RefreshInteractionTarget())
	{
		return;
	}

	NotifyLocalTutorialMilestone(TEXT("ProximityInteraction"), TEXT("ValidInteractionInput"));

	AHeistSecurityHoldButtonActor* TargetSecurityButton = Cast<AHeistSecurityHoldButtonActor>(InteractionComponent->GetCurrentInteractionTarget());
	if (TargetSecurityButton != nullptr)
	{
		LocalSecurityHoldButton = TargetSecurityButton;
		Server_RequestBeginSecurityHold(TargetSecurityButton);
		return;
	}

	AHeistPlayerCharacter* TargetPlayerCharacter = Cast<AHeistPlayerCharacter>(InteractionComponent->GetCurrentInteractionTarget());
	if (TargetPlayerCharacter != nullptr)
	{
		RequestRescuePlayer(TargetPlayerCharacter);
		return;
	}

	AHeistDroppedOriginalActor* TargetDroppedOriginal = Cast<AHeistDroppedOriginalActor>(InteractionComponent->GetCurrentInteractionTarget());
	if (TargetDroppedOriginal != nullptr)
	{
		Server_RequestDroppedOriginalPickup(TargetDroppedOriginal);
		return;
	}

	AHeistLootActor* TargetLootActor = Cast<AHeistLootActor>(InteractionComponent->GetCurrentInteractionTarget());
	if (TargetLootActor != nullptr)
	{
		Server_RequestLootPickup(TargetLootActor);
		return;
	}

	AHeistPaintingDisplayCaseActor* TargetDisplayCase = Cast<AHeistPaintingDisplayCaseActor>(InteractionComponent->GetCurrentInteractionTarget());
	if (TargetDisplayCase != nullptr)
	{
		if (TargetDisplayCase->IsReplicaReviewReadyFor(HeistCharacter))
		{
			RequestConfirmForgeryReplicaSwap();
		}
		else if (TargetDisplayCase->GetDisplayCaseState() == EHeistDisplayCaseState::OriginalAvailable)
		{
			Server_RequestTakeOriginal(TargetDisplayCase);
		}
		else
		{
			bLocalObservationInputHeld = true;
			Server_RequestObservation(TargetDisplayCase);
		}
		return;
	}

	AHeistObjectDisplayCaseActor* TargetObjectDisplayCase = Cast<AHeistObjectDisplayCaseActor>(InteractionComponent->GetCurrentInteractionTarget());
	if (TargetObjectDisplayCase != nullptr)
	{
		if (TargetObjectDisplayCase->IsReplicaReviewReadyFor(HeistCharacter))
		{
			RequestConfirmObjectAssemblyReplicaSwap();
		}
		else if (TargetObjectDisplayCase->GetAssemblyState() == EHeistObjectAssemblyState::OriginalAvailable)
		{
			Server_RequestTakeObjectOriginal(TargetObjectDisplayCase);
		}
		else
		{
			bLocalObservationInputHeld = true;
			Server_RequestObjectObservation(TargetObjectDisplayCase);
		}
		return;
	}

	AHeistVentActor* TargetVentActor = Cast<AHeistVentActor>(InteractionComponent->GetCurrentInteractionTarget());
	if (TargetVentActor != nullptr)
	{
		Server_RequestEscape(TargetVentActor);
	}
}

void AHeistPlayerController::HandleInteractReleased()
{
	if (AHeistSecurityHoldButtonActor* HeldSecurityButton = LocalSecurityHoldButton.Get())
	{
		Server_RequestEndSecurityHold(HeldSecurityButton);
		LocalSecurityHoldButton.Reset();
		return;
	}

	if (!bLocalObservationInputHeld)
	{
		return;
	}
	bLocalObservationInputHeld = false;

	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	if (!IsValid(HeistCharacter))
	{
		return;
	}

	Server_CancelObservation();
}

#pragma endregion

#pragma region Tutorial

FHeistTutorialPresentationChanged& AHeistPlayerController::GetTutorialPresentationChangedDelegate()
{
	return TutorialPresentationChangedDelegate;
}

bool AHeistPlayerController::IsLocalTutorialActive() const
{
	return bLocalTutorialActive;
}

bool AHeistPlayerController::HasCompletedLocalTutorial() const
{
	const UHeistGameUserSettings* Settings = UHeistGameUserSettings::GetHeistGameUserSettings();
	return IsValid(Settings) && Settings->HasCompletedTutorial();
}

int32 AHeistPlayerController::GetLocalTutorialStepIndex() const
{
	return LocalTutorialStepIndex;
}

int32 AHeistPlayerController::GetLocalTutorialStepCount() const
{
	return 5;
}

FName AHeistPlayerController::GetLocalTutorialStepId() const
{
	switch (LocalTutorialStepIndex)
	{
	case 0:
		return TEXT("ProximityInteraction");
	case 1:
		return TEXT("Coin");
	case 2:
		return TEXT("Forgery");
	case 3:
		return TEXT("Alert");
	case 4:
		return TEXT("Extraction");
	default:
		return NAME_None;
	}
}

FText AHeistPlayerController::GetLocalTutorialTitleText() const
{
	switch (LocalTutorialStepIndex)
	{
	case 0:
		return NSLOCTEXT("HeistTutorial", "ProximityInteractionTitle", "상호작용 거리로 접근");
	case 1:
		return NSLOCTEXT("HeistTutorial", "CoinTitle", "경비의 주의를 분산");
	case 2:
		return NSLOCTEXT("HeistTutorial", "ForgeryTitle", "목표 작품 위조");
	case 3:
		return NSLOCTEXT("HeistTutorial", "AlertTitle", "경계 상황 확인");
	case 4:
		return NSLOCTEXT("HeistTutorial", "ExtractionTitle", "팀과 함께 탈출");
	default:
		return FText::GetEmpty();
	}
}

FText AHeistPlayerController::GetLocalTutorialBodyText() const
{
	switch (LocalTutorialStepIndex)
	{
	case 0:
		return NSLOCTEXT("HeistTutorial", "ProximityInteractionBody", "대상에 가까이 접근한 뒤 상호작용 안내가 나타나면 [E]를 누르세요.");
	case 1:
		return NSLOCTEXT("HeistTutorial", "CoinBody", "[Q]를 눌러 동전을 던지세요. 경비는 동전이 떨어진 곳을 조사합니다.");
	case 2:
		return NSLOCTEXT("HeistTutorial", "ForgeryBody", "목표를 관찰한 뒤 시간이 끝나기 전에 복제품을 만드세요.");
	case 3:
		return NSLOCTEXT("HeistTutorial", "AlertBody", "경비가 증거를 발견하면 경계 단계가 상승합니다. 경보 상태가 지속되면 봉쇄됩니다.");
	case 4:
		return NSLOCTEXT("HeistTutorial", "ExtractionBody", "원본을 확보하고 봉쇄 전에 탈출 지점에 도달하세요.");
	default:
		return FText::GetEmpty();
	}
}

void AHeistPlayerController::RefreshLocalTutorialFromMatchPhase()
{
	if (!IsLocalController())
	{
		return;
	}

	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (IsValid(HeistGameState) && HeistGameState->GetMatchPhase() == EHeistMatchPhase::InGame)
	{
		TryStartLocalTutorial();
	}
	else if (bLocalTutorialActive)
	{
		StopLocalTutorial(TEXT("MatchPhaseChanged"));
	}
}

void AHeistPlayerController::TryStartLocalTutorial(const bool bForceRestart)
{
	if (!IsLocalController())
	{
		return;
	}

	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistGameState) || HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame)
	{
		return;
	}

	UHeistGameUserSettings* Settings = UHeistGameUserSettings::GetHeistGameUserSettings();
	if (!bForceRestart && ((IsValid(Settings) && Settings->HasCompletedTutorial()) || bLocalTutorialActive))
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LocalTutorialStepTimerHandle);
	}
	LocalTutorialStepIndex = 0;
	bLocalTutorialActive = true;
	bLocalTutorialObservedForgerySession = false;
	TutorialPresentationChangedDelegate.Broadcast();
	ScheduleLocalTutorialAutoAdvance();
	UHeistDebugFunctionLibrary::DebugTutorialTransition(this, TEXT("Started"), GetLocalTutorialStepId(), LocalTutorialStepIndex, GetLocalTutorialStepCount(), true,
														HasCompletedLocalTutorial(), true);
}

void AHeistPlayerController::StopLocalTutorial(const FName TriggerId)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LocalTutorialStepTimerHandle);
	}
	const FName PreviousStepId = GetLocalTutorialStepId();
	bLocalTutorialActive = false;
	bLocalTutorialObservedForgerySession = false;
	LocalTutorialStepIndex = INDEX_NONE;
	TutorialPresentationChangedDelegate.Broadcast();
	UHeistDebugFunctionLibrary::DebugTutorialTransition(this, TriggerId, PreviousStepId, INDEX_NONE, GetLocalTutorialStepCount(), false, HasCompletedLocalTutorial(), true);
}

void AHeistPlayerController::CompleteLocalTutorial(const FName TriggerId)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LocalTutorialStepTimerHandle);
	}
	UHeistGameUserSettings* Settings = UHeistGameUserSettings::GetHeistGameUserSettings();
	if (IsValid(Settings))
	{
		Settings->SetTutorialCompleted(true);
		Settings->SaveSettings();
	}
	bLocalTutorialActive = false;
	bLocalTutorialObservedForgerySession = false;
	TutorialPresentationChangedDelegate.Broadcast();
	UHeistDebugFunctionLibrary::DebugTutorialTransition(this, TriggerId, GetLocalTutorialStepId(), LocalTutorialStepIndex, GetLocalTutorialStepCount(), false, true, true);
}

void AHeistPlayerController::AdvanceLocalTutorial(const FName TriggerId)
{
	if (!bLocalTutorialActive)
	{
		return;
	}

	if (LocalTutorialStepIndex >= GetLocalTutorialStepCount() - 1)
	{
		CompleteLocalTutorial(TriggerId);
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LocalTutorialStepTimerHandle);
	}
	++LocalTutorialStepIndex;
	bLocalTutorialObservedForgerySession = false;
	TutorialPresentationChangedDelegate.Broadcast();
	ScheduleLocalTutorialAutoAdvance();
	UHeistDebugFunctionLibrary::DebugTutorialTransition(this, TriggerId, GetLocalTutorialStepId(), LocalTutorialStepIndex, GetLocalTutorialStepCount(), true, false, true);
}

void AHeistPlayerController::NotifyLocalTutorialMilestone(const FName StepId, const FName TriggerId)
{
	if (IsCurrentLocalTutorialStep(StepId))
	{
		AdvanceLocalTutorial(TriggerId);
	}
}

void AHeistPlayerController::ScheduleLocalTutorialAutoAdvance(const float OverrideDelaySeconds)
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || !bLocalTutorialActive)
	{
		return;
	}

	const float DelaySeconds = OverrideDelaySeconds > 0.0f ? OverrideDelaySeconds : TutorialCardDurationSeconds;
	World->GetTimerManager().SetTimer(LocalTutorialStepTimerHandle, this, &AHeistPlayerController::HandleLocalTutorialAutoAdvance, DelaySeconds, false);
}

void AHeistPlayerController::HandleLocalTutorialAutoAdvance()
{
	if (IsCurrentLocalTutorialStep(TEXT("Forgery")) && LocalInputMode == EHeistInputMode::Forgery)
	{
		ScheduleLocalTutorialAutoAdvance(1.0f);
		return;
	}
	AdvanceLocalTutorial(TEXT("AutoAdvance"));
}

bool AHeistPlayerController::IsCurrentLocalTutorialStep(const FName StepId) const
{
	return bLocalTutorialActive && GetLocalTutorialStepId() == StepId;
}

void AHeistPlayerController::DebugResetLocalTutorial()
{
	UHeistGameUserSettings* Settings = UHeistGameUserSettings::GetHeistGameUserSettings();
	if (IsValid(Settings))
	{
		Settings->SetTutorialCompleted(false);
		Settings->SaveSettings();
	}

	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (IsValid(HeistGameState) && HeistGameState->GetMatchPhase() == EHeistMatchPhase::InGame)
	{
		TryStartLocalTutorial(true);
	}
	else
	{
		StopLocalTutorial(TEXT("DebugResetOutsideMatch"));
	}
}

void AHeistPlayerController::DebugAdvanceLocalTutorial()
{
	if (!bLocalTutorialActive)
	{
		TryStartLocalTutorial(true);
		return;
	}
	AdvanceLocalTutorial(TEXT("DebugAdvance"));
}

void AHeistPlayerController::DebugSkipLocalTutorial()
{
	CompleteLocalTutorial(TEXT("DebugSkip"));
}

#pragma endregion

#pragma region GameplayRequests

void AHeistPlayerController::RequestLeaveOnlineSession()
{
	UHeistGameInstance* HeistGameInstance = Cast<UHeistGameInstance>(GetGameInstance());
	const bool bAccepted = IsLocalController() && IsValid(HeistGameInstance) && HeistGameInstance->RequestLeaveSession();
	UHeistDebugFunctionLibrary::DebugOnlineSessionControllerRequest(this, TEXT("Leave"), bAccepted, bAccepted ? NAME_None : FName(TEXT("RequestRejected")));
}

void AHeistPlayerController::RequestSetLobbyMapSelection(const FName RequestedMapId)
{
	if (!IsLocalController())
	{
		UHeistDebugFunctionLibrary::DebugOnlineSessionControllerRequest(this, TEXT("SetMap"), false, FName(TEXT("NotLocalController")));
		return;
	}

	Server_RequestSetLobbyMapSelection(RequestedMapId);
}

void AHeistPlayerController::RequestStartSelectedGameplayMap()
{
	if (!IsLocalController())
	{
		UHeistDebugFunctionLibrary::DebugOnlineSessionControllerRequest(this, TEXT("StartGameplay"), false, FName(TEXT("NotLocalController")));
		return;
	}

	Server_RequestStartSelectedGameplayMap();
}

void AHeistPlayerController::RequestReturnToLobby()
{
	if (!IsLocalController())
	{
		UHeistDebugFunctionLibrary::DebugOnlineSessionControllerRequest(this, TEXT("ReturnLobby"), false, FName(TEXT("NotLocalController")));
		return;
	}

	Server_RequestReturnToLobby();
}

void AHeistPlayerController::Client_NotifyOnlineSessionEnded_Implementation(const FName Reason)
{
	if (UHeistGameInstance* HeistGameInstance = Cast<UHeistGameInstance>(GetGameInstance()))
	{
		HeistGameInstance->HandleHostSessionEnded(Reason);
	}
}

void AHeistPlayerController::RequestSetInventoryOpen(const bool bInventoryOpen)
{
	const AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	const UHeistForgeryComponent* ForgeryComponent = IsValid(HeistCharacter) ? HeistCharacter->GetForgeryComponent() : nullptr;
	const UHeistObjectAssemblyComponent* ObjectAssemblyComponent = IsValid(HeistCharacter) ? HeistCharacter->GetObjectAssemblyComponent() : nullptr;
	if (bInventoryOpen && ((IsValid(ForgeryComponent) && ForgeryComponent->IsSessionActive()) ||
						  (IsValid(ObjectAssemblyComponent) && ObjectAssemblyComponent->IsSessionActive())))
	{
		LogInventoryRequestRejected(TEXT("SetOpen"), INDEX_NONE, IsValid(ForgeryComponent) && ForgeryComponent->IsSessionActive() ? TEXT("ForgeryActive") : TEXT("ObjectAssemblyActive"));
		return;
	}

	const AHeistPlayerState* HeistPlayerState = GetPlayerState<AHeistPlayerState>();
	if (bInventoryOpen && IsValid(HeistPlayerState) && HeistPlayerState->IsArrested())
	{
		LogInventoryRequestRejected(TEXT("SetOpen"), INDEX_NONE, TEXT("PlayerArrested"));
		return;
	}

	if (bInventoryOpen && IsLocalController())
	{
		AHeistHUD* HeistHUD = GetHUD<AHeistHUD>();
		if (!IsValid(HeistHUD) || !HeistHUD->ShowInventoryScreen())
		{
			UHeistDebugFunctionLibrary::DebugInventoryOpenSkipped(this);
			return;
		}
	}

	Server_SetInventoryOpen(bInventoryOpen);
}

void AHeistPlayerController::RequestCancelForgery()
{
	Server_CancelForgery();
}

void AHeistPlayerController::RequestSubmitForgeryStrokes(const TArray<FVector2D>& NormalizedPoints, const TArray<int32>& StrokePointCounts, const TArray<uint8>& StrokePaletteIndices,
													 const TArray<uint8>& StrokeBrushPresetIndices, const int32 ClientSessionRevision)
{
	const AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	const UHeistForgeryComponent* ForgeryComponent = IsValid(HeistCharacter) ? HeistCharacter->GetForgeryComponent() : nullptr;
	const int32 ResolvedSessionRevision = ClientSessionRevision == INDEX_NONE && IsValid(ForgeryComponent) ? ForgeryComponent->GetSessionRevision() : ClientSessionRevision;

	TArray<uint32> PackedNormalizedPoints;
	PackedNormalizedPoints.Reserve(NormalizedPoints.Num());
	for (const FVector2D& NormalizedPoint : NormalizedPoints)
	{
		uint32 PackedPoint = 0;
		if (!TryPackForgeryPoint(NormalizedPoint, PackedPoint))
		{
			UE_LOG(LogHeistNetwork, Warning,
				TEXT("[%s] Forgery stroke submit rejected locally: Character=%s Points=%d Reason=InvalidNormalizedPoint CoordinateEncoding=UInt16Pair Result=REJECTED"), *GetName(),
				*GetNameSafe(HeistCharacter), NormalizedPoints.Num());
			return;
		}
		PackedNormalizedPoints.Add(PackedPoint);
	}

	UE_LOG(LogHeistNetwork, Log,
		   TEXT("[%s] Forgery stroke submit requested: Character=%s Strokes=%d Points=%d PaletteIndices=%d BrushPresetIndices=%d ClientSessionRevision=%d Local=%s Authority=%s CoordinateEncoding=UInt16Pair RenderTargetSent=false"),
		   *GetName(), *GetNameSafe(HeistCharacter), StrokePointCounts.Num(), NormalizedPoints.Num(), StrokePaletteIndices.Num(), StrokeBrushPresetIndices.Num(), ResolvedSessionRevision,
		   IsLocalController() ? TEXT("true") : TEXT("false"), HasAuthority() ? TEXT("true") : TEXT("false"));
	Server_SubmitForgeryStrokes(PackedNormalizedPoints, StrokePointCounts, StrokePaletteIndices, StrokeBrushPresetIndices, ResolvedSessionRevision);
}

void AHeistPlayerController::RequestConfirmForgeryReplicaSwap()
{
	Server_ConfirmForgeryReplicaSwap();
}

void AHeistPlayerController::RequestRestartForgeryFromPreview()
{
	Server_RestartForgeryFromPreview();
}

void AHeistPlayerController::RequestBeginObjectAssembly(AHeistObjectDisplayCaseActor* TargetDisplayCase, const float DurationSeconds)
{
	Server_RequestBeginObjectAssembly(TargetDisplayCase, DurationSeconds);
}

void AHeistPlayerController::RequestCancelObjectAssembly()
{
	Server_CancelObjectAssembly();
}

void AHeistPlayerController::RestoreGameplayInputAfterForcedForgeryClose()
{
	if (IsLocalController() && LocalInputMode == EHeistInputMode::Forgery)
	{
		ApplyLocalInputMode(EHeistInputMode::Gameplay);
	}
}

void AHeistPlayerController::RequestSubmitObjectAssembly(const TArray<FHeistObjectAssemblyEntry>& Entries, const int32 ClientSessionRevision)
{
	const AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	const UHeistObjectAssemblyComponent* ObjectAssemblyComponent = IsValid(HeistCharacter) ? HeistCharacter->GetObjectAssemblyComponent() : nullptr;
	const int32 ResolvedSessionRevision =
		ClientSessionRevision == INDEX_NONE && IsValid(ObjectAssemblyComponent) ? ObjectAssemblyComponent->GetSessionRevision() : ClientSessionRevision;
	Server_SubmitObjectAssembly(Entries, ResolvedSessionRevision);
}

void AHeistPlayerController::RequestConfirmObjectAssemblyReplicaSwap()
{
	Server_ConfirmObjectAssemblyReplicaSwap();
}

void AHeistPlayerController::RequestRestartObjectAssemblyFromPreview()
{
	Server_RestartObjectAssemblyFromPreview();
}

void AHeistPlayerController::RequestMoveInventoryItem(const int32 InstanceId, const FIntPoint TargetGridPosition)
{
	Server_RequestMoveInventoryItem(InstanceId, TargetGridPosition);
}

void AHeistPlayerController::RequestRotateInventoryItem(const int32 InstanceId)
{
	Server_RequestRotateInventoryItem(InstanceId);
}

void AHeistPlayerController::RequestDropInventoryItem(const int32 InstanceId)
{
	Server_RequestDropInventoryItem(InstanceId);
}

void AHeistPlayerController::RequestTakeOriginal(AHeistPaintingDisplayCaseActor* TargetDisplayCase)
{
	Server_RequestTakeOriginal(TargetDisplayCase);
}

void AHeistPlayerController::RequestTakeObjectOriginal(AHeistObjectDisplayCaseActor* TargetDisplayCase)
{
	Server_RequestTakeObjectOriginal(TargetDisplayCase);
}

void AHeistPlayerController::RequestDropCarriedOriginal()
{
	Server_RequestDropCarriedOriginal();
}

void AHeistPlayerController::RequestRescuePlayer(AHeistPlayerCharacter* TargetPlayerCharacter)
{
	if (!IsValid(TargetPlayerCharacter))
	{
		return;
	}
	Server_RequestRescuePlayer(TargetPlayerCharacter);
}

void AHeistPlayerController::RequestAssignQuickSlot(const EHeistQuickSlotType SlotType, const int32 InstanceId)
{
	Server_RequestAssignQuickSlot(SlotType, InstanceId);
}

void AHeistPlayerController::RequestClearQuickSlot(const EHeistQuickSlotType SlotType)
{
	Server_RequestClearQuickSlot(SlotType);
}

void AHeistPlayerController::RequestUseQuickSlot(const EHeistQuickSlotType SlotType)
{
	if (SlotType != EHeistQuickSlotType::Coin)
	{
		return;
	}

	const AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	if (!IsValid(HeistCharacter) || !HeistCharacter->CanPerformGameplayActions())
	{
		return;
	}

	FVector ResolvedTargetWorldLocation = FVector::ZeroVector;
	if (SlotType == EHeistQuickSlotType::Coin)
	{
		FVector ViewLocation;
		FVector CameraForward;
		if (!TryBuildCameraForwardAim(1000.0f, ViewLocation, CameraForward, ResolvedTargetWorldLocation))
		{
			return;
		}
	}
	NotifyLocalTutorialMilestone(TEXT("Coin"), TEXT("CoinUseRequested"));
	Server_RequestUseQuickSlot(SlotType, ResolvedTargetWorldLocation);
}

bool AHeistPlayerController::TryBuildCameraForwardAim(const float Distance, FVector& OutViewLocation, FVector& OutCameraForward, FVector& OutTargetWorldLocation) const
{
	OutViewLocation = FVector::ZeroVector;
	OutCameraForward = FVector::ZeroVector;
	OutTargetWorldLocation = FVector::ZeroVector;

	if (!IsValid(GetPawn()))
	{
		return false;
	}

	FRotator ViewRotation = FRotator::ZeroRotator;
	GetPlayerViewPoint(OutViewLocation, ViewRotation);
	OutCameraForward = ViewRotation.Vector().GetSafeNormal();
	if (OutCameraForward.IsNearlyZero())
	{
		return false;
	}

	OutTargetWorldLocation = OutViewLocation + OutCameraForward * FMath::Max(100.0f, Distance);
	return true;
}

void AHeistPlayerController::Server_RequestLootPickup_Implementation(AHeistLootActor* TargetLootActor)
{
	FHeistGameplayRequestContext RequestContext;
	const TCHAR* RejectReason = nullptr;
	if (!TryBuildGameplayRequestContext(RequestContext, RejectReason))
	{
		LogLootPickupRejected(TargetLootActor, RejectReason);
		return;
	}

	if (!IsValid(TargetLootActor))
	{
		LogLootPickupRejected(nullptr, TEXT("InvalidTarget"));
		return;
	}

	UHeistDebugFunctionLibrary::DebugLootPickupRequestReceived(this, RequestContext.Character, TargetLootActor);

	UHeistInteractionComponent* InteractionComponent = RequestContext.Character->GetInteractionComponent();
	const float Distance = FVector::Distance(RequestContext.Character->GetActorLocation(), TargetLootActor->GetActorLocation());

	if (!InteractionComponent->IsActorOverlappingInteractionArea(TargetLootActor))
	{
		LogLootPickupRejected(TargetLootActor, TEXT("OutOfRange"), Distance);
		return;
	}

	if (!TargetLootActor->IsLootAvailable())
	{
		LogLootPickupRejected(TargetLootActor, TEXT("AlreadyTaken"), Distance);
		return;
	}

	if (!CanUseHeistInteraction(TargetLootActor, RequestContext.Character))
	{
		LogLootPickupRejected(TargetLootActor, TEXT("InteractionUnavailable"), Distance);
		return;
	}

	const int32 ScoreDelta = TargetLootActor->GetScoreValue();
	const float WeightDelta = TargetLootActor->GetWeightValue();
	if (!RequestContext.PlayerState->CanAddLootScoreAndWeight(ScoreDelta, WeightDelta))
	{
		LogLootPickupRejected(TargetLootActor, TEXT("InvalidLootValues"), Distance);
		return;
	}

	if (!TargetLootActor->TryReserveForPickup(RequestContext.Character))
	{
		LogLootPickupRejected(TargetLootActor, TEXT("AlreadyTaken"), Distance);
		return;
	}

	int32 AddedInstanceId = INDEX_NONE;
	const TCHAR* InventoryRejectReason = nullptr;
	if (!RequestContext.InventoryComponent->TryAddItem(TargetLootActor->GetLootRowId(), AddedInstanceId, InventoryRejectReason))
	{
		TargetLootActor->ReleasePickupReservation(RequestContext.Character);
		LogLootPickupRejected(TargetLootActor, InventoryRejectReason != nullptr ? InventoryRejectReason : TEXT("InventoryRejected"), Distance);
		return;
	}

	checkf(RequestContext.PlayerState->AddLootScoreAndWeight(ScoreDelta, WeightDelta), TEXT("Validated loot score and weight must apply after inventory commit"));
	checkf(TargetLootActor->CommitPickupReservation(RequestContext.Character), TEXT("Reserved loot must commit after inventory and score/weight commit"));
	if (AHeistGameState* MutableHeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr)
	{
		MutableHeistGameState->RefreshContractCarriedValue();
	}

	UHeistDebugFunctionLibrary::DebugLootPickupRequestAccepted(this, TargetLootActor, TargetLootActor->GetLootRowId(), AddedInstanceId, Distance);
	SendPopupFeedback(FText::Format(
		NSLOCTEXT("HeistFeedback", "LootPickupAccepted", "전리품 획득 +{0}"),
		FText::AsNumber(ScoreDelta)));
}

void AHeistPlayerController::Server_RequestDroppedOriginalPickup_Implementation(AHeistDroppedOriginalActor* TargetDroppedOriginal)
{
	FHeistGameplayRequestContext RequestContext;
	const TCHAR* RejectReason = nullptr;
	if (!TryBuildGameplayRequestContext(RequestContext, RejectReason) || !IsValid(TargetDroppedOriginal))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Dropped Original pickup rejected: Actor=%s Reason=%s"), *GetNameSafe(TargetDroppedOriginal),
			   RejectReason != nullptr ? RejectReason : TEXT("InvalidTarget"));
		return;
	}

	UHeistInteractionComponent* InteractionComponent = RequestContext.Character->GetInteractionComponent();
	const float Distance = FVector::Distance(RequestContext.Character->GetActorLocation(), TargetDroppedOriginal->GetActorLocation());
	if (!InteractionComponent->IsActorOverlappingInteractionArea(TargetDroppedOriginal))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Dropped Original pickup rejected: Actor=%s PlayerId=%d Distance=%.1f Reason=OutOfRange"),
			   *GetNameSafe(TargetDroppedOriginal), RequestContext.PlayerState->HeistPlayerId, Distance);
		return;
	}

	if (!CanUseHeistInteraction(TargetDroppedOriginal, RequestContext.Character) || !TargetDroppedOriginal->TryReserveForPickup(RequestContext.Character))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Dropped Original pickup rejected: Actor=%s PlayerId=%d Distance=%.1f Reason=UnavailableOrReserved"),
			   *GetNameSafe(TargetDroppedOriginal), RequestContext.PlayerState->HeistPlayerId, Distance);
		return;
	}

	AActor* SourceDisplayCase = TargetDroppedOriginal->GetSourceDisplayCase();
	bool bClaimed = false;
	if (AHeistPaintingDisplayCaseActor* PaintingDisplayCase = Cast<AHeistPaintingDisplayCaseActor>(SourceDisplayCase))
	{
		bClaimed = PaintingDisplayCase->TryClaimDroppedOriginal(RequestContext.PlayerState, TargetDroppedOriginal);
	}
	else if (AHeistObjectDisplayCaseActor* ObjectDisplayCase = Cast<AHeistObjectDisplayCaseActor>(SourceDisplayCase))
	{
		bClaimed = ObjectDisplayCase->TryClaimDroppedOriginal(RequestContext.PlayerState, TargetDroppedOriginal);
	}

	if (!bClaimed)
	{
		TargetDroppedOriginal->ReleasePickupReservation(RequestContext.Character);
		UE_LOG(LogHeistNetwork, Warning, TEXT("Dropped Original pickup rejected: Actor=%s Source=%s PlayerId=%d Reason=SourceClaimRejected"),
			   *GetNameSafe(TargetDroppedOriginal), *GetNameSafe(SourceDisplayCase), RequestContext.PlayerState->HeistPlayerId);
		return;
	}

	checkf(TargetDroppedOriginal->CommitPickupReservation(RequestContext.Character), TEXT("Reserved Dropped Original must commit after source-case claim."));
	UE_LOG(LogHeistNetwork, Log,
		   TEXT("Dropped Original pickup committed: Actor=%s Source=%s Artifact=%s Value=%d Weight=%.1f Required=%s PlayerId=%d Distance=%.1f Authority=true Result=PASS"),
		   *GetNameSafe(TargetDroppedOriginal), *GetNameSafe(SourceDisplayCase), *TargetDroppedOriginal->GetArtifactId().ToString(), TargetDroppedOriginal->GetArtifactValue(),
		   TargetDroppedOriginal->GetWeight(), TargetDroppedOriginal->IsRequiredTarget() ? TEXT("true") : TEXT("false"), RequestContext.PlayerState->HeistPlayerId, Distance);
	TargetDroppedOriginal->Destroy();
}

void AHeistPlayerController::Server_RequestRescuePlayer_Implementation(AHeistPlayerCharacter* TargetPlayerCharacter)
{
	FHeistGameplayRequestContext RequestContext;
	const TCHAR* RejectReason = nullptr;
	if (!TryBuildGameplayRequestContext(RequestContext, RejectReason))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Player rescue rejected: Rescuer=%s Target=%s Reason=%s Authority=%s Result=FAIL"), *GetNameSafe(GetPlayerState<AHeistPlayerState>()),
			   *GetNameSafe(TargetPlayerCharacter), RejectReason != nullptr ? RejectReason : TEXT("InvalidRequestContext"), HasAuthority() ? TEXT("true") : TEXT("false"));
		return;
	}

	AHeistPlayerState* TargetPlayerState = IsValid(TargetPlayerCharacter) ? TargetPlayerCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	UHeistInteractionComponent* InteractionComponent = RequestContext.Character->GetInteractionComponent();
	const float Distance = IsValid(TargetPlayerCharacter) ? FVector::Distance(RequestContext.Character->GetActorLocation(), TargetPlayerCharacter->GetActorLocation()) : -1.0f;
	if (!IsValid(TargetPlayerCharacter) || TargetPlayerCharacter == RequestContext.Character || !IsValid(TargetPlayerState))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Player rescue rejected: RescuerId=%d Target=%s Distance=%.1f Reason=InvalidTarget Authority=true Result=FAIL"),
			   RequestContext.PlayerState->HeistPlayerId, *GetNameSafe(TargetPlayerCharacter), Distance);
		return;
	}
	if (!InteractionComponent->IsActorOverlappingInteractionArea(TargetPlayerCharacter))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Player rescue rejected: RescuerId=%d TargetId=%d Distance=%.1f Reason=OutOfRange Authority=true Result=FAIL"),
			   RequestContext.PlayerState->HeistPlayerId, TargetPlayerState->HeistPlayerId, Distance);
		return;
	}

	if (!CanUseHeistInteraction(TargetPlayerCharacter, RequestContext.Character))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Player rescue rejected: RescuerId=%d TargetId=%d Distance=%.1f Arrested=%s Reason=NotCurrentRescueTarget Authority=true Result=FAIL"),
			   RequestContext.PlayerState->HeistPlayerId, TargetPlayerState->HeistPlayerId, Distance, TargetPlayerState->IsArrested() ? TEXT("true") : TEXT("false"));
		return;
	}

	TargetPlayerCharacter->Interact(RequestContext.Character);
	const bool bRescued = !TargetPlayerState->IsArrested();
	if (bRescued)
	{
		RequestContext.PlayerState->RecordTeammateRescueContribution();
	}
	UE_LOG(LogHeistNetwork, Log, TEXT("Player rescue committed: RescuerId=%d TargetId=%d Distance=%.1f TargetArrested=%s TargetMovementRestored=%s Authority=true Result=%s"),
		   RequestContext.PlayerState->HeistPlayerId, TargetPlayerState->HeistPlayerId, Distance, TargetPlayerState->IsArrested() ? TEXT("true") : TEXT("false"),
		   bRescued ? TEXT("true") : TEXT("false"), bRescued ? TEXT("PASS") : TEXT("FAIL"));
}

void AHeistPlayerController::Server_RequestObservation_Implementation(AHeistPaintingDisplayCaseActor* TargetDisplayCase)
{
	FHeistGameplayRequestContext RequestContext;
	const TCHAR* RejectReason = nullptr;
	if (!TryBuildGameplayRequestContext(RequestContext, RejectReason))
	{
		UHeistDebugFunctionLibrary::DebugObservationRequestRejected(this, TargetDisplayCase, RejectReason != nullptr ? RejectReason : TEXT("InvalidRequestContext"));
		return;
	}

	if (!IsValid(TargetDisplayCase))
	{
		UHeistDebugFunctionLibrary::DebugObservationRequestRejected(this, nullptr, TEXT("InvalidTarget"));
		return;
	}

	UHeistInteractionComponent* InteractionComponent = RequestContext.Character->GetInteractionComponent();
	const float Distance = FVector::Distance(RequestContext.Character->GetActorLocation(), TargetDisplayCase->GetActorLocation());
	if (!InteractionComponent->IsActorOverlappingInteractionArea(TargetDisplayCase))
	{
		UHeistDebugFunctionLibrary::DebugObservationRequestRejected(this, TargetDisplayCase, TEXT("OutOfRange"), Distance);
		return;
	}

	if (!CanUseHeistInteraction(TargetDisplayCase, RequestContext.Character))
	{
		UHeistDebugFunctionLibrary::DebugObservationRequestRejected(this, TargetDisplayCase, TEXT("InteractionUnavailable"), Distance);
		return;
	}

	if (TargetDisplayCase->GetDisplayCaseState() != EHeistDisplayCaseState::Secured)
	{
		UHeistDebugFunctionLibrary::DebugObservationRequestRejected(this, TargetDisplayCase, TEXT("CaseNotSecured"), Distance);
		return;
	}

	UHeistActionComponent* ActionComponent = RequestContext.Character->GetActionComponent();
	if (!ActionComponent->TryBeginObservationRequest(TargetDisplayCase))
	{
		UHeistDebugFunctionLibrary::DebugObservationRequestRejected(this, TargetDisplayCase, TEXT("ObservationCastRejected"), Distance);
	}
}

void AHeistPlayerController::Server_RequestObjectObservation_Implementation(AHeistObjectDisplayCaseActor* TargetDisplayCase)
{
	FHeistGameplayRequestContext RequestContext;
	const TCHAR* RejectReason = nullptr;
	if (!TryBuildGameplayRequestContext(RequestContext, RejectReason))
	{
		UHeistDebugFunctionLibrary::DebugObservationRequestRejected(this, TargetDisplayCase, RejectReason != nullptr ? RejectReason : TEXT("InvalidRequestContext"));
		return;
	}

	if (!IsValid(TargetDisplayCase))
	{
		UHeistDebugFunctionLibrary::DebugObservationRequestRejected(this, nullptr, TEXT("InvalidTarget"));
		return;
	}

	UHeistInteractionComponent* InteractionComponent = RequestContext.Character->GetInteractionComponent();
	const float Distance = FVector::Distance(RequestContext.Character->GetActorLocation(), TargetDisplayCase->GetActorLocation());
	if (!InteractionComponent->IsActorOverlappingInteractionArea(TargetDisplayCase))
	{
		UHeistDebugFunctionLibrary::DebugObservationRequestRejected(this, TargetDisplayCase, TEXT("OutOfRange"), Distance);
		return;
	}

	if (!CanUseHeistInteraction(TargetDisplayCase, RequestContext.Character))
	{
		UHeistDebugFunctionLibrary::DebugObservationRequestRejected(this, TargetDisplayCase, TEXT("InteractionUnavailable"), Distance);
		return;
	}

	if (TargetDisplayCase->GetAssemblyState() != EHeistObjectAssemblyState::Secured)
	{
		UHeistDebugFunctionLibrary::DebugObservationRequestRejected(this, TargetDisplayCase, TEXT("CaseNotSecured"), Distance);
		return;
	}

	UHeistActionComponent* ActionComponent = RequestContext.Character->GetActionComponent();
	if (!ActionComponent->TryBeginObservationRequest(TargetDisplayCase))
	{
		UHeistDebugFunctionLibrary::DebugObservationRequestRejected(this, TargetDisplayCase, TEXT("ObservationCastRejected"), Distance);
	}
}

void AHeistPlayerController::Server_CancelObservation_Implementation()
{
	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	if (!IsValid(HeistCharacter) || HeistCharacter->GetController() != this)
	{
		return;
	}

	HeistCharacter->GetActionComponent()->CancelObservationRequest(TEXT("InputReleased"));
}

void AHeistPlayerController::Server_RequestBeginSecurityHold_Implementation(AHeistSecurityHoldButtonActor* TargetButton)
{
	FHeistGameplayRequestContext RequestContext;
	const TCHAR* RejectReason = nullptr;
	if (!TryBuildGameplayRequestContext(RequestContext, RejectReason) || !IsValid(TargetButton))
	{
		return;
	}

	UHeistInteractionComponent* InteractionComponent = RequestContext.Character->GetInteractionComponent();
	if (!IsValid(InteractionComponent) || !InteractionComponent->IsActorOverlappingInteractionArea(TargetButton) || !CanUseHeistInteraction(TargetButton, RequestContext.Character))
	{
		return;
	}

	TargetButton->TryBeginHold(RequestContext.Character);
}

void AHeistPlayerController::Server_RequestEndSecurityHold_Implementation(AHeistSecurityHoldButtonActor* TargetButton)
{
	AHeistPlayerState* RequestingPlayerState = GetPlayerState<AHeistPlayerState>();
	if (!IsValid(TargetButton) || !IsValid(RequestingPlayerState))
	{
		return;
	}

	// Release must remain valid after Stun/Arrest/Input cancellation, so it intentionally
	// does not require the normal action-eligible gameplay request context.
	TargetButton->TryEndHold(RequestingPlayerState, FName(TEXT("InputReleased")));
}

void AHeistPlayerController::Server_RequestTakeOriginal_Implementation(AHeistPaintingDisplayCaseActor* TargetDisplayCase)
{
	FHeistGameplayRequestContext RequestContext;
	const TCHAR* RejectReason = nullptr;
	if (!TryBuildGameplayRequestContext(RequestContext, RejectReason))
	{
		UHeistDebugFunctionLibrary::Message(
			this, FString::Printf(TEXT("Original take request rejected: Case=%s Reason=%s"), *GetNameSafe(TargetDisplayCase), RejectReason != nullptr ? RejectReason : TEXT("InvalidRequestContext")),
			EHeistDebugLevel::Warning);
		return;
	}
	if (!IsValid(TargetDisplayCase))
	{
		UHeistDebugFunctionLibrary::Message(this, TEXT("Original take request rejected: Reason=InvalidTarget"), EHeistDebugLevel::Warning);
		return;
	}

	UHeistInteractionComponent* InteractionComponent = RequestContext.Character->GetInteractionComponent();
	const float Distance = FVector::Distance(RequestContext.Character->GetActorLocation(), TargetDisplayCase->GetActorLocation());
	if (!InteractionComponent->IsActorOverlappingInteractionArea(TargetDisplayCase))
	{
		UHeistDebugFunctionLibrary::Message(this, FString::Printf(TEXT("Original take request rejected: Case=%s Distance=%.1f Reason=OutOfRange"), *GetNameSafe(TargetDisplayCase), Distance),
											EHeistDebugLevel::Warning);
		return;
	}

	if (!CanUseHeistInteraction(TargetDisplayCase, RequestContext.Character))
	{
		UHeistDebugFunctionLibrary::Message(this, FString::Printf(TEXT("Original take request rejected: Case=%s Distance=%.1f Reason=InteractionUnavailable"), *GetNameSafe(TargetDisplayCase), Distance),
											EHeistDebugLevel::Warning);
		return;
	}

	const float PreviousWeight = RequestContext.PlayerState->GetTotalLootWeight();
	if (!TargetDisplayCase->TryTakeOriginal(RequestContext.PlayerState))
	{
		UHeistDebugFunctionLibrary::Message(this,
											FString::Printf(TEXT("Original take request rejected: Case=%s Artifact=%s Reason=ServerValidationFailed"), *GetNameSafe(TargetDisplayCase),
															*TargetDisplayCase->GetTargetArtifactId().ToString()),
											EHeistDebugLevel::Warning);
		return;
	}

	FHeistInventoryItem OriginalItem;
	checkf(RequestContext.InventoryComponent->TryGetOriginalArtifactForSourceCase(TargetDisplayCase, OriginalItem), TEXT("Accepted painting Original must exist in the inventory grid."));
	UHeistDebugFunctionLibrary::Message(
		this, FString::Printf(TEXT("Original take request accepted: Case=%s Artifact=%s PlayerId=%d CarryWeight=%.1f PreviousWeight=%.1f TotalWeight=%.1f State=%s Authority=true Result=PASS"),
							  *GetNameSafe(TargetDisplayCase), *OriginalItem.ItemId.ToString(), RequestContext.PlayerState->HeistPlayerId, OriginalItem.Weight, PreviousWeight,
							  RequestContext.PlayerState->GetTotalLootWeight(), *UEnum::GetValueAsString(TargetDisplayCase->GetDisplayCaseState())));
}

void AHeistPlayerController::Server_RequestTakeObjectOriginal_Implementation(AHeistObjectDisplayCaseActor* TargetDisplayCase)
{
	FHeistGameplayRequestContext RequestContext;
	const TCHAR* RejectReason = nullptr;
	if (!TryBuildGameplayRequestContext(RequestContext, RejectReason))
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			FString::Printf(TEXT("Object original take request rejected: Case=%s Reason=%s"), *GetNameSafe(TargetDisplayCase),
							RejectReason != nullptr ? RejectReason : TEXT("InvalidRequestContext")),
			EHeistDebugLevel::Warning);
		return;
	}
	if (!IsValid(TargetDisplayCase))
	{
		UHeistDebugFunctionLibrary::Message(this, TEXT("Object original take request rejected: Reason=InvalidTarget"), EHeistDebugLevel::Warning);
		return;
	}

	UHeistInteractionComponent* InteractionComponent = RequestContext.Character->GetInteractionComponent();
	const float Distance = FVector::Distance(RequestContext.Character->GetActorLocation(), TargetDisplayCase->GetActorLocation());
	if (!InteractionComponent->IsActorOverlappingInteractionArea(TargetDisplayCase))
	{
		UHeistDebugFunctionLibrary::Message(
			this, FString::Printf(TEXT("Object original take request rejected: Case=%s Distance=%.1f Reason=OutOfRange"), *GetNameSafe(TargetDisplayCase), Distance),
			EHeistDebugLevel::Warning);
		return;
	}

	if (!CanUseHeistInteraction(TargetDisplayCase, RequestContext.Character))
	{
		UHeistDebugFunctionLibrary::Message(
			this, FString::Printf(TEXT("Object original take request rejected: Case=%s Distance=%.1f Reason=InteractionUnavailable"), *GetNameSafe(TargetDisplayCase), Distance),
			EHeistDebugLevel::Warning);
		return;
	}

	const float PreviousWeight = RequestContext.PlayerState->GetTotalLootWeight();
	if (!TargetDisplayCase->TryTakeOriginal(RequestContext.PlayerState))
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			FString::Printf(TEXT("Object original take request rejected: Case=%s Artifact=%s Reason=ServerValidationFailed"), *GetNameSafe(TargetDisplayCase),
							*TargetDisplayCase->GetTargetArtifactId().ToString()),
			EHeistDebugLevel::Warning);
		return;
	}

	FHeistInventoryItem OriginalItem;
	checkf(RequestContext.InventoryComponent->TryGetOriginalArtifactForSourceCase(TargetDisplayCase, OriginalItem), TEXT("Accepted object Original must exist in the inventory grid."));
	UHeistDebugFunctionLibrary::Message(
		this,
		FString::Printf(
			TEXT(
				"Object original take request accepted: Case=%s Artifact=%s PlayerId=%d CarryWeight=%.1f PreviousWeight=%.1f TotalWeight=%.1f State=%s Authority=true Result=PASS"),
			*GetNameSafe(TargetDisplayCase), *OriginalItem.ItemId.ToString(), RequestContext.PlayerState->HeistPlayerId, OriginalItem.Weight, PreviousWeight,
			RequestContext.PlayerState->GetTotalLootWeight(), *UEnum::GetValueAsString(TargetDisplayCase->GetAssemblyState())));
}

void AHeistPlayerController::Server_RequestDropCarriedOriginal_Implementation()
{
	FHeistGameplayRequestContext RequestContext;
	const TCHAR* RejectReason = nullptr;
	if (!TryBuildGameplayRequestContext(RequestContext, RejectReason))
	{
		UHeistDebugFunctionLibrary::Message(this, FString::Printf(TEXT("Original drop request rejected: Reason=%s"), RejectReason != nullptr ? RejectReason : TEXT("InvalidRequestContext")),
											EHeistDebugLevel::Warning);
		return;
	}

	FHeistInventoryItem OriginalItem;
	AActor* SourceDisplayCase = RequestContext.InventoryComponent->TryGetFirstOriginalArtifact(OriginalItem) ? OriginalItem.SourceDisplayCase.Get() : nullptr;
	if (!OriginalItem.HasValidOriginalData() || !IsValid(SourceDisplayCase))
	{
		UHeistDebugFunctionLibrary::Message(this, TEXT("Original drop request rejected: Reason=NotCarryingOriginal"), EHeistDebugLevel::Warning);
		return;
	}

	const float PreviousWeight = RequestContext.PlayerState->GetTotalLootWeight();
	bool bReleased = false;
	FString SourceStateAfterDrop = TEXT("UnsupportedSource");
	if (AHeistPaintingDisplayCaseActor* PaintingDisplayCase = Cast<AHeistPaintingDisplayCaseActor>(SourceDisplayCase))
	{
		bReleased = PaintingDisplayCase->DropOriginalForCarrier(RequestContext.PlayerState, FName(TEXT("OwnerDropped")));
		SourceStateAfterDrop = UEnum::GetValueAsString(PaintingDisplayCase->GetDisplayCaseState());
	}
	else if (AHeistObjectDisplayCaseActor* ObjectDisplayCase = Cast<AHeistObjectDisplayCaseActor>(SourceDisplayCase))
	{
		bReleased = ObjectDisplayCase->DropOriginalForCarrier(RequestContext.PlayerState, FName(TEXT("OwnerDropped")));
		SourceStateAfterDrop = UEnum::GetValueAsString(ObjectDisplayCase->GetAssemblyState());
	}
	if (!bReleased)
	{
		UHeistDebugFunctionLibrary::Message(
			this, FString::Printf(TEXT("Original drop request rejected: Case=%s Artifact=%s Reason=ServerValidationFailed"), *GetNameSafe(SourceDisplayCase), *OriginalItem.ItemId.ToString()),
			EHeistDebugLevel::Warning);
		return;
	}

	UHeistDebugFunctionLibrary::Message(
		this,
		FString::Printf(
			TEXT(
				"Original drop request accepted: Case=%s Artifact=%s PlayerId=%d ReleasedWeight=%.1f PreviousWeight=%.1f TotalWeight=%.1f SourceStateAfterDrop=%s Policy=NeutralWorldDrop Authority=true Result=PASS"),
			*GetNameSafe(SourceDisplayCase), *OriginalItem.ItemId.ToString(), RequestContext.PlayerState->HeistPlayerId, OriginalItem.Weight, PreviousWeight,
			RequestContext.PlayerState->GetTotalLootWeight(), *SourceStateAfterDrop));
}

void AHeistPlayerController::Server_RequestEscape_Implementation(AHeistVentActor* TargetVentActor)
{
	FHeistGameplayRequestContext RequestContext;
	const TCHAR* RejectReason = nullptr;
	if (!TryBuildGameplayRequestContext(RequestContext, RejectReason))
	{
		LogEscapeRequestRejected(TargetVentActor, RejectReason);
		return;
	}

	if (!IsValid(TargetVentActor))
	{
		LogEscapeRequestRejected(nullptr, TEXT("InvalidTarget"));
		return;
	}

	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistGameState))
	{
		LogEscapeRequestRejected(TargetVentActor, TEXT("InvalidGameState"));
		return;
	}

	if (!HeistGameState->IsEscapePhaseOpen())
	{
		LogEscapeRequestRejected(TargetVentActor, TEXT("EscapePhaseClosed"));
		return;
	}

	UHeistInteractionComponent* InteractionComponent = RequestContext.Character->GetInteractionComponent();
	const float Distance = FVector::Distance(RequestContext.Character->GetActorLocation(), TargetVentActor->GetActorLocation());

	if (!InteractionComponent->IsActorOverlappingInteractionArea(TargetVentActor))
	{
		LogEscapeRequestRejected(TargetVentActor, TEXT("OutOfRange"), Distance);
		return;
	}

	if (!TargetVentActor->CanUseVent(RequestContext.Character))
	{
		LogEscapeRequestRejected(TargetVentActor, TEXT("VentUnavailable"), Distance);
		return;
	}

	UHeistActionComponent* ActionComponent = RequestContext.Character->GetActionComponent();
	if (ActionComponent->HasPendingEscapeRequest())
	{
		LogEscapeRequestRejected(TargetVentActor, TEXT("DuplicateEscapeRequest"), Distance);
		return;
	}

	if (!ActionComponent->TryBeginEscapeRequest(TargetVentActor))
	{
		LogEscapeRequestRejected(TargetVentActor, TEXT("EscapeRequestStateRejected"), Distance);
		return;
	}

	UHeistDebugFunctionLibrary::DebugEscapeRequestAccepted(this, RequestContext.Character, TargetVentActor, Distance);
}

void AHeistPlayerController::Server_SetInventoryOpen_Implementation(const bool bInventoryOpen)
{
	FHeistGameplayRequestContext RequestContext;
	const TCHAR* RejectReason = nullptr;
	if (!TryBuildGameplayRequestContext(RequestContext, RejectReason))
	{
		LogInventoryRequestRejected(TEXT("SetOpen"), INDEX_NONE, RejectReason);
		return;
	}

	if (bInventoryOpen && RequestContext.Character->GetActionComponent()->IsGameplayCastActive())
	{
		LogInventoryRequestRejected(TEXT("SetOpen"), INDEX_NONE, TEXT("GameplayStateBlocked"));
		return;
	}

	if (!RequestContext.InventoryComponent->TrySetInventoryOpen(bInventoryOpen))
	{
		LogInventoryRequestRejected(TEXT("SetOpen"), INDEX_NONE, TEXT("MutationRejected"));
	}
}

void AHeistPlayerController::Server_RequestSetLobbyMapSelection_Implementation(const FName RequestedMapId)
{
	UHeistGameInstance* HeistGameInstance = Cast<UHeistGameInstance>(GetGameInstance());
	const bool bAccepted = HasAuthority() && IsLocalController() && IsValid(HeistGameInstance) && HeistGameInstance->RequestSetLobbyMapSelection(RequestedMapId);
	UHeistDebugFunctionLibrary::DebugOnlineSessionControllerRequest(this, TEXT("SetMap"), bAccepted,
																   bAccepted ? NAME_None : FName(IsLocalController() ? TEXT("AuthorityOrSessionRejected") : TEXT("HostOnly")));
}

void AHeistPlayerController::Server_RequestStartSelectedGameplayMap_Implementation()
{
	UHeistGameInstance* HeistGameInstance = Cast<UHeistGameInstance>(GetGameInstance());
	const bool bAccepted = HasAuthority() && IsLocalController() && IsValid(HeistGameInstance) && HeistGameInstance->RequestStartSelectedGameplayMap();
	const FName FailureReason =
		bAccepted ? NAME_None
				  : (IsValid(HeistGameInstance) && !HeistGameInstance->GetLastOnlineSessionFailure().IsNone()
						 ? HeistGameInstance->GetLastOnlineSessionFailure()
						 : FName(IsLocalController() ? TEXT("AuthorityOrSessionRejected") : TEXT("HostOnly")));
	UHeistDebugFunctionLibrary::DebugOnlineSessionControllerRequest(this, TEXT("StartGameplay"), bAccepted, FailureReason);
}

void AHeistPlayerController::Server_RequestReturnToLobby_Implementation()
{
	UHeistGameInstance* HeistGameInstance = Cast<UHeistGameInstance>(GetGameInstance());
	const bool bAccepted = HasAuthority() && IsLocalController() && IsValid(HeistGameInstance) && HeistGameInstance->RequestReturnToLobby();
	const FName FailureReason =
		bAccepted ? NAME_None
				  : (IsValid(HeistGameInstance) && !HeistGameInstance->GetLastOnlineSessionFailure().IsNone()
						 ? HeistGameInstance->GetLastOnlineSessionFailure()
						 : FName(IsLocalController() ? TEXT("AuthorityOrSessionRejected") : TEXT("HostOnly")));
	UHeistDebugFunctionLibrary::DebugOnlineSessionControllerRequest(this, TEXT("ReturnLobby"), bAccepted, FailureReason);
}

void AHeistPlayerController::Server_CancelForgery_Implementation()
{
	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	UHeistForgeryComponent* ForgeryComponent = IsValid(HeistCharacter) ? HeistCharacter->GetForgeryComponent() : nullptr;
	if (!HasAuthority() || !IsValid(HeistCharacter) || HeistCharacter->GetController() != this || !IsValid(ForgeryComponent) || !ForgeryComponent->IsSessionActive())
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("[%s] Forgery cancel rejected: Character=%s Active=%s Reason=InvalidOrInactiveSession"), *GetName(), *GetNameSafe(HeistCharacter),
			   IsValid(ForgeryComponent) && ForgeryComponent->IsSessionActive() ? TEXT("true") : TEXT("false"));
		return;
	}

	const bool bCancelled = ForgeryComponent->CancelForgerySession(FName(TEXT("InputCancelled")));
	UE_LOG(LogHeistNetwork, Log, TEXT("[%s] Forgery input cancel: Character=%s Result=%s"), *GetName(), *GetNameSafe(HeistCharacter), bCancelled ? TEXT("PASS") : TEXT("REJECTED"));
}

void AHeistPlayerController::Server_SubmitForgeryStrokes_Implementation(const TArray<uint32>& PackedNormalizedPoints, const TArray<int32>& StrokePointCounts,
																		const TArray<uint8>& StrokePaletteIndices, const TArray<uint8>& StrokeBrushPresetIndices,
																		const int32 ClientSessionRevision)
{
	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	UHeistForgeryComponent* ForgeryComponent = IsValid(HeistCharacter) ? HeistCharacter->GetForgeryComponent() : nullptr;
	if (!HasAuthority() || !IsValid(HeistCharacter) || HeistCharacter->GetController() != this || !IsValid(ForgeryComponent))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("[%s] Forgery stroke RPC rejected: Character=%s Reason=InvalidAuthorityContext"), *GetName(), *GetNameSafe(HeistCharacter));
		return;
	}

	TArray<FVector2D> NormalizedPoints;
	NormalizedPoints.Reserve(PackedNormalizedPoints.Num());
	for (const uint32 PackedPoint : PackedNormalizedPoints)
	{
		NormalizedPoints.Add(UnpackForgeryPoint(PackedPoint));
	}

	const bool bAccepted = ForgeryComponent->TrySubmitStrokePayload(NormalizedPoints, StrokePointCounts, StrokePaletteIndices, StrokeBrushPresetIndices, ClientSessionRevision);
	const FHeistForgeryResult& ForgeryResult = ForgeryComponent->GetAuthoritativeForgeryResult();
	UE_LOG(LogHeistNetwork, Log,
		TEXT(
			"[%s] Forgery stroke RPC processed: Character=%s Strokes=%d Points=%d ClientSessionRevision=%d ServerSessionRevision=%d Accepted=%s HasAuthoritativeScore=%s Score=%.2f ScoreRevision=%d Result=%s"),
		*GetName(), *GetNameSafe(HeistCharacter), StrokePointCounts.Num(), PackedNormalizedPoints.Num(), ClientSessionRevision, ForgeryComponent->GetSessionRevision(),
		bAccepted ? TEXT("true") : TEXT("false"), ForgeryComponent->HasAuthoritativeForgeryResult() ? TEXT("true") : TEXT("false"), ForgeryResult.SimilarityScore,
		ForgeryComponent->GetForgeryScoreRevision(), bAccepted ? TEXT("PASS") : TEXT("REJECTED"));
}

void AHeistPlayerController::Server_ConfirmForgeryReplicaSwap_Implementation()
{
	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	UHeistForgeryComponent* ForgeryComponent = IsValid(HeistCharacter) ? HeistCharacter->GetForgeryComponent() : nullptr;
	UHeistInteractionComponent* InteractionComponent = IsValid(HeistCharacter) ? HeistCharacter->GetInteractionComponent() : nullptr;
	AHeistPaintingDisplayCaseActor* TargetDisplayCase = IsValid(ForgeryComponent) ? ForgeryComponent->GetActiveDisplayCase() : nullptr;
	if (!HasAuthority() || !IsValid(HeistCharacter) || HeistCharacter->GetController() != this || !IsValid(ForgeryComponent) || !IsValid(InteractionComponent) ||
		!IsValid(TargetDisplayCase) || !InteractionComponent->IsActorOverlappingInteractionArea(TargetDisplayCase))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("[%s] Replica swap confirm rejected: Character=%s Case=%s Reason=InteractionOverlapRequired"), *GetName(), *GetNameSafe(HeistCharacter),
			   *GetNameSafe(TargetDisplayCase));
		return;
	}

	const bool bAccepted = ForgeryComponent->TryConfirmReplicaSwap();
	UE_LOG(LogHeistNetwork, Log, TEXT("[%s] Replica swap confirm processed: Character=%s Case=%s Overlap=true Result=%s"), *GetName(), *GetNameSafe(HeistCharacter),
		   *GetNameSafe(TargetDisplayCase), bAccepted ? TEXT("PASS") : TEXT("REJECTED"));
}

void AHeistPlayerController::Server_RestartForgeryFromPreview_Implementation()
{
	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	UHeistForgeryComponent* ForgeryComponent = IsValid(HeistCharacter) ? HeistCharacter->GetForgeryComponent() : nullptr;
	UHeistInteractionComponent* InteractionComponent = IsValid(HeistCharacter) ? HeistCharacter->GetInteractionComponent() : nullptr;
	AHeistPaintingDisplayCaseActor* TargetDisplayCase = IsValid(ForgeryComponent) ? ForgeryComponent->GetActiveDisplayCase() : nullptr;
	if (!HasAuthority() || !IsValid(HeistCharacter) || HeistCharacter->GetController() != this || !IsValid(ForgeryComponent) || !IsValid(InteractionComponent) ||
		!IsValid(TargetDisplayCase) || !InteractionComponent->IsActorOverlappingInteractionArea(TargetDisplayCase))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("[%s] Replica redraw rejected: Character=%s Case=%s Reason=InteractionOverlapRequired"), *GetName(), *GetNameSafe(HeistCharacter),
			   *GetNameSafe(TargetDisplayCase));
		return;
	}

	const bool bAccepted = ForgeryComponent->TryRestartForgeryFromPreview();
	UE_LOG(LogHeistNetwork, Log, TEXT("[%s] Replica redraw processed: Character=%s Case=%s Overlap=true Result=%s"), *GetName(), *GetNameSafe(HeistCharacter),
		   *GetNameSafe(TargetDisplayCase), bAccepted ? TEXT("PASS") : TEXT("REJECTED"));
}

void AHeistPlayerController::Server_RequestBeginObjectAssembly_Implementation(AHeistObjectDisplayCaseActor* TargetDisplayCase, const float DurationSeconds)
{
	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	UHeistObjectAssemblyComponent* ObjectAssemblyComponent = IsValid(HeistCharacter) ? HeistCharacter->GetObjectAssemblyComponent() : nullptr;
	UHeistInteractionComponent* InteractionComponent = IsValid(HeistCharacter) ? HeistCharacter->GetInteractionComponent() : nullptr;
	if (!HasAuthority() || !IsValid(HeistCharacter) || HeistCharacter->GetController() != this || !IsValid(ObjectAssemblyComponent) || !IsValid(InteractionComponent))
	{
		UHeistDebugFunctionLibrary::DebugObjectAssemblySessionSnapshot(ObjectAssemblyComponent, FName(TEXT("SessionBeginRPC")), FName(TEXT("InvalidAuthorityContext")), false);
		return;
	}
	if (!IsValid(TargetDisplayCase) || !InteractionComponent->IsActorOverlappingInteractionArea(TargetDisplayCase) || !CanUseHeistInteraction(TargetDisplayCase, HeistCharacter))
	{
		UHeistDebugFunctionLibrary::DebugObjectAssemblySessionSnapshot(ObjectAssemblyComponent, FName(TEXT("SessionBeginRPC")), FName(TEXT("InteractionOverlapRequired")), false);
		return;
	}

	ObjectAssemblyComponent->TryBeginAssemblySession(TargetDisplayCase, DurationSeconds);
}

void AHeistPlayerController::Server_CancelObjectAssembly_Implementation()
{
	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	UHeistObjectAssemblyComponent* ObjectAssemblyComponent = IsValid(HeistCharacter) ? HeistCharacter->GetObjectAssemblyComponent() : nullptr;
	if (!HasAuthority() || !IsValid(HeistCharacter) || HeistCharacter->GetController() != this || !IsValid(ObjectAssemblyComponent))
	{
		UHeistDebugFunctionLibrary::DebugObjectAssemblySessionSnapshot(ObjectAssemblyComponent, FName(TEXT("SessionCancelRPC")), FName(TEXT("InvalidAuthorityContext")), false);
		return;
	}

	ObjectAssemblyComponent->CancelAssemblySession(FName(TEXT("OwnerCancelled")));
}

void AHeistPlayerController::Server_SubmitObjectAssembly_Implementation(const TArray<FHeistObjectAssemblyEntry>& Entries, const int32 ClientSessionRevision)
{
	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	UHeistObjectAssemblyComponent* ObjectAssemblyComponent = IsValid(HeistCharacter) ? HeistCharacter->GetObjectAssemblyComponent() : nullptr;
	if (!HasAuthority() || !IsValid(HeistCharacter) || HeistCharacter->GetController() != this || !IsValid(ObjectAssemblyComponent))
	{
		UHeistDebugFunctionLibrary::DebugObjectAssemblyPayloadValidation(ObjectAssemblyComponent, false, FName(TEXT("InvalidAuthorityContext")), Entries.Num(), 0);
		return;
	}

	ObjectAssemblyComponent->TrySubmitAssemblyPayload(Entries, ClientSessionRevision);
}

void AHeistPlayerController::Server_ConfirmObjectAssemblyReplicaSwap_Implementation()
{
	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	UHeistObjectAssemblyComponent* ObjectAssemblyComponent = IsValid(HeistCharacter) ? HeistCharacter->GetObjectAssemblyComponent() : nullptr;
	UHeistInteractionComponent* InteractionComponent = IsValid(HeistCharacter) ? HeistCharacter->GetInteractionComponent() : nullptr;
	AHeistObjectDisplayCaseActor* TargetDisplayCase = IsValid(ObjectAssemblyComponent) ? ObjectAssemblyComponent->GetActiveDisplayCase() : nullptr;
	if (!HasAuthority() || !IsValid(HeistCharacter) || HeistCharacter->GetController() != this || !IsValid(ObjectAssemblyComponent) || !IsValid(InteractionComponent) ||
		!IsValid(TargetDisplayCase) || !InteractionComponent->IsActorOverlappingInteractionArea(TargetDisplayCase))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("[%s] Object replica swap rejected: Character=%s Case=%s Reason=InteractionOverlapRequired"), *GetName(),
			   *GetNameSafe(HeistCharacter), *GetNameSafe(TargetDisplayCase));
		return;
	}

	const bool bAccepted = ObjectAssemblyComponent->TryConfirmReplicaSwap();
	UE_LOG(LogHeistNetwork, Log, TEXT("[%s] Object replica swap processed: Character=%s Case=%s Overlap=true Result=%s"), *GetName(), *GetNameSafe(HeistCharacter),
		   *GetNameSafe(TargetDisplayCase), bAccepted ? TEXT("PASS") : TEXT("REJECTED"));
}

void AHeistPlayerController::Server_RestartObjectAssemblyFromPreview_Implementation()
{
	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	UHeistObjectAssemblyComponent* ObjectAssemblyComponent = IsValid(HeistCharacter) ? HeistCharacter->GetObjectAssemblyComponent() : nullptr;
	UHeistInteractionComponent* InteractionComponent = IsValid(HeistCharacter) ? HeistCharacter->GetInteractionComponent() : nullptr;
	AHeistObjectDisplayCaseActor* TargetDisplayCase = IsValid(ObjectAssemblyComponent) ? ObjectAssemblyComponent->GetActiveDisplayCase() : nullptr;
	if (!HasAuthority() || !IsValid(HeistCharacter) || HeistCharacter->GetController() != this || !IsValid(ObjectAssemblyComponent) || !IsValid(InteractionComponent) ||
		!IsValid(TargetDisplayCase) || !InteractionComponent->IsActorOverlappingInteractionArea(TargetDisplayCase))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("[%s] Object reassemble rejected: Character=%s Case=%s Reason=InteractionOverlapRequired"), *GetName(),
			   *GetNameSafe(HeistCharacter), *GetNameSafe(TargetDisplayCase));
		return;
	}

	const bool bAccepted = ObjectAssemblyComponent->TryRestartAssemblyFromPreview();
	UE_LOG(LogHeistNetwork, Log, TEXT("[%s] Object reassemble processed: Character=%s Case=%s Overlap=true Result=%s"), *GetName(), *GetNameSafe(HeistCharacter),
		   *GetNameSafe(TargetDisplayCase), bAccepted ? TEXT("PASS") : TEXT("REJECTED"));
}

void AHeistPlayerController::Server_RequestMoveInventoryItem_Implementation(const int32 InstanceId, const FIntPoint TargetGridPosition)
{
	FHeistGameplayRequestContext RequestContext;
	const TCHAR* RejectReason = nullptr;
	if (!TryBuildInventoryMutationRequestContext(RequestContext, RejectReason))
	{
		LogInventoryRequestRejected(TEXT("Move"), InstanceId, RejectReason);
		return;
	}

	if (!RequestContext.InventoryComponent->TryMoveItem(InstanceId, TargetGridPosition))
	{
		LogInventoryRequestRejected(TEXT("Move"), InstanceId, TEXT("InvalidTargetPlacement"));
	}
}

void AHeistPlayerController::Server_RequestRotateInventoryItem_Implementation(const int32 InstanceId)
{
	FHeistGameplayRequestContext RequestContext;
	const TCHAR* RejectReason = nullptr;
	if (!TryBuildInventoryMutationRequestContext(RequestContext, RejectReason))
	{
		LogInventoryRequestRejected(TEXT("Rotate"), InstanceId, RejectReason);
		return;
	}

	if (!RequestContext.InventoryComponent->TryRotateItem(InstanceId))
	{
		LogInventoryRequestRejected(TEXT("Rotate"), InstanceId, TEXT("RotationRejected"));
	}
}

void AHeistPlayerController::Server_RequestDropInventoryItem_Implementation(const int32 InstanceId)
{
	FHeistGameplayRequestContext RequestContext;
	const TCHAR* RejectReason = nullptr;
	if (!TryBuildInventoryMutationRequestContext(RequestContext, RejectReason))
	{
		LogInventoryRequestRejected(TEXT("Drop"), InstanceId, RejectReason);
		return;
	}

	FHeistInventoryItem InventoryItem;
	if (!RequestContext.InventoryComponent->TryGetItem(InstanceId, InventoryItem))
	{
		LogInventoryRequestRejected(TEXT("Drop"), InstanceId, TEXT("InvalidInstanceId"));
		return;
	}
	if (InventoryItem.IsOriginalArtifact())
	{
		AActor* SourceDisplayCase = InventoryItem.SourceDisplayCase.Get();
		bool bDropped = false;
		if (AHeistPaintingDisplayCaseActor* PaintingDisplayCase = Cast<AHeistPaintingDisplayCaseActor>(SourceDisplayCase))
		{
			bDropped = PaintingDisplayCase->DropOriginalForCarrier(RequestContext.PlayerState, FName(TEXT("OwnerDroppedFromGrid")));
		}
		else if (AHeistObjectDisplayCaseActor* ObjectDisplayCase = Cast<AHeistObjectDisplayCaseActor>(SourceDisplayCase))
		{
			bDropped = ObjectDisplayCase->DropOriginalForCarrier(RequestContext.PlayerState, FName(TEXT("OwnerDroppedFromGrid")));
		}
		if (!bDropped)
		{
			LogInventoryRequestRejected(TEXT("DropOriginal"), InstanceId, TEXT("OriginalSourceDropRejected"));
		}
		return;
	}

	AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	FHeistItemDataRow ItemDefinition;
	FHeistLootDataRow LootDefinition;
	if (!IsValid(HeistGameMode) || !HeistGameMode->TryGetItemDefinition(InventoryItem.ItemId, ItemDefinition) || ItemDefinition.ItemType != EHeistItemType::Loot ||
		!HeistGameMode->TryGetLootDefinition(InventoryItem.ItemId, LootDefinition) || !RequestContext.PlayerState->CanRemoveLootScoreAndWeight(LootDefinition.ScoreValue, ItemDefinition.Weight))
	{
		LogInventoryRequestRejected(TEXT("Drop"), InstanceId, TEXT("InvalidLootState"));
		return;
	}

	FHeistLootDropRequest DropRequest;
	DropRequest.DroppedBy = RequestContext.Character;
	DropRequest.ItemId = InventoryItem.ItemId;
	DropRequest.SourceInstanceId = InstanceId;
	DropRequest.DropOrigin = RequestContext.Character->GetActorLocation() + RequestContext.Character->GetActorForwardVector() * 100.0f;

	AHeistLootActor* DroppedLootActor = nullptr;
	if (!HeistGameMode->TrySpawnDroppedLoot(DropRequest, DroppedLootActor))
	{
		LogInventoryRequestRejected(TEXT("Drop"), InstanceId, TEXT("WorldSpawnFailed"));
		return;
	}

	FHeistInventoryItem RemovedItem;
	if (!RequestContext.InventoryComponent->TryRemoveItem(InstanceId, RemovedItem))
	{
		DroppedLootActor->Destroy();
		LogInventoryRequestRejected(TEXT("Drop"), InstanceId, TEXT("InventoryRemovalFailed"));
		return;
	}

	checkf(RemovedItem.ItemId == DropRequest.ItemId, TEXT("Validated inventory drop item changed during commit."));
	checkf(RequestContext.PlayerState->RemoveLootScoreAndWeight(LootDefinition.ScoreValue, ItemDefinition.Weight),
		   TEXT("Validated loot score and weight removal must succeed after inventory commit."));
	if (AHeistGameState* MutableHeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr)
	{
		MutableHeistGameState->RefreshContractCarriedValue();
	}

	UHeistDebugFunctionLibrary::DebugInventoryDropAccepted(this, RequestContext.Character, DropRequest.ItemId, InstanceId, DroppedLootActor, FVector(DropRequest.DropOrigin));
}

void AHeistPlayerController::Server_RequestAssignQuickSlot_Implementation(const EHeistQuickSlotType SlotType, const int32 InstanceId)
{
	FHeistGameplayRequestContext RequestContext;
	const TCHAR* RejectReason = nullptr;
	if (!TryBuildInventoryMutationRequestContext(RequestContext, RejectReason))
	{
		LogInventoryRequestRejected(TEXT("AssignQuickSlot"), InstanceId, RejectReason);
		return;
	}

	if (!RequestContext.InventoryComponent->TryAssignQuickSlot(SlotType, InstanceId))
	{
		LogInventoryRequestRejected(TEXT("AssignQuickSlot"), InstanceId, TEXT("InvalidSlotAssignment"));
	}
}

void AHeistPlayerController::Server_RequestClearQuickSlot_Implementation(const EHeistQuickSlotType SlotType)
{
	FHeistGameplayRequestContext RequestContext;
	const TCHAR* RejectReason = nullptr;
	if (!TryBuildInventoryMutationRequestContext(RequestContext, RejectReason))
	{
		LogInventoryRequestRejected(TEXT("ClearQuickSlot"), INDEX_NONE, RejectReason);
		return;
	}

	if (!RequestContext.InventoryComponent->TryClearQuickSlot(SlotType))
	{
		LogInventoryRequestRejected(TEXT("ClearQuickSlot"), INDEX_NONE, TEXT("InvalidSlot"));
	}
}

void AHeistPlayerController::Server_RequestUseQuickSlot_Implementation(const EHeistQuickSlotType SlotType, const FVector TargetWorldLocation)
{
	if (SlotType != EHeistQuickSlotType::Coin)
	{
		LogThrowableUseRejected(SlotType, NAME_None, TEXT("UnsupportedQuickSlot"));
		return;
	}

	FHeistGameplayRequestContext RequestContext;
	const TCHAR* RejectReason = nullptr;
	if (!TryBuildGameplayRequestContext(RequestContext, RejectReason))
	{
		LogThrowableUseRejected(SlotType, NAME_None, RejectReason);
		return;
	}

	if (RequestContext.Character->GetActionComponent()->IsGameplayCastActive())
	{
		LogThrowableUseRejected(SlotType, NAME_None, TEXT("Casting"));
		return;
	}

	if (RequestContext.InventoryComponent->IsInventoryOpen())
	{
		LogThrowableUseRejected(SlotType, NAME_None, TEXT("InventoryOpen"));
		return;
	}

	FName ItemId = NAME_None;
	if (!TryResolveQuickSlotItem(RequestContext, SlotType, ItemId, RejectReason))
	{
		LogThrowableUseRejected(SlotType, ItemId, RejectReason);
		return;
	}

	AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	FHeistUsableItemDataRow UsableItemDefinition;
	if (!IsValid(HeistGameMode) || !HeistGameMode->TryGetUsableItemDefinition(ItemId, UsableItemDefinition))
	{
		LogThrowableUseRejected(SlotType, ItemId, TEXT("InvalidUsableItem"));
		return;
	}

	if (UsableItemDefinition.UseType != EHeistUseType::Throw)
	{
		LogThrowableUseRejected(SlotType, ItemId, TEXT("UnsupportedUseType"));
		return;
	}

	AHeistThrowableProjectile* SpawnedProjectile = nullptr;
	if (!TrySpawnThrowableProjectile(RequestContext, ItemId, TargetWorldLocation, false, SpawnedProjectile, RejectReason))
	{
		LogThrowableUseRejected(SlotType, ItemId, RejectReason);
	}
}

void AHeistPlayerController::Server_UpdateFlashlightAimDirection_Implementation(const FVector_NetQuantizeNormal ClientCameraForward)
{
	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	if (!IsValid(HeistCharacter))
	{
		return;
	}

	const FVector RequestedDirection = FVector(ClientCameraForward).GetSafeNormal();
	if (RequestedDirection.IsNearlyZero())
	{
		return;
	}

	UHeistVisionComponent* VisionComponent = HeistCharacter->GetVisionComponent();
	checkf(IsValid(VisionComponent), TEXT("HeistPlayerCharacter requires HeistVisionComponent"));
	VisionComponent->UpdateFlashlightAimDirection(RequestedDirection);
}

void AHeistPlayerController::Server_SetSprintRequested_Implementation(const bool bRequested)
{
	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	if (!HasAuthority() || !IsValid(HeistCharacter) || HeistCharacter->GetController() != this)
	{
		return;
	}
	HeistCharacter->SetSprintRequested(bRequested);
}

void AHeistPlayerController::Server_SetVoicePushToTalk_Implementation(const bool bHeld)
{
	if (HasAuthority())
	{
		bServerVoicePushToTalkHeld = bHeld;
	}
}

void AHeistPlayerController::Server_ReportVoiceActivity_Implementation()
{
	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	if (!HasAuthority() || !bServerVoicePushToTalkHeld || !IsValid(HeistCharacter) || HeistCharacter->GetController() != this)
	{
		return;
	}

	UHeistNoiseEmitterComponent* NoiseEmitter = HeistCharacter->GetNoiseEmitterComponent();
	if (IsValid(NoiseEmitter))
	{
		NoiseEmitter->TryEmitVoiceNoise();
	}
}

#pragma endregion

#pragma region Debug

void AHeistPlayerController::DebugRequestAddInventoryItem(const FName ItemId)
{
	Server_DebugRequestAddInventoryItem(ItemId);
}

void AHeistPlayerController::DebugRequestThrowCoinAtWorldLocation(const FVector TargetWorldLocation)
{
	Server_DebugRequestThrowCoinAtWorldLocation(TargetWorldLocation);
}

void AHeistPlayerController::DebugRequestSpawnGuard(const float Distance)
{
	Server_DebugRequestSpawnGuard(Distance);
}

void AHeistPlayerController::DebugRequestSetNearestGuardState(const EHeistGuardState GuardState, const float DurationSeconds)
{
	Server_DebugRequestSetNearestGuardState(GuardState, DurationSeconds);
}

void AHeistPlayerController::DebugRequestEvaluateNearestGuardSight()
{
	Server_DebugRequestEvaluateNearestGuardSight();
}

void AHeistPlayerController::DebugRequestSetNearestGuardAutomaticSight(const bool bEnabled)
{
	Server_DebugRequestSetNearestGuardAutomaticSight(bEnabled);
}

void AHeistPlayerController::DebugRequestReportGuardNoise(const float Distance)
{
	Server_DebugRequestReportGuardNoise(Distance);
}

void AHeistPlayerController::DebugRequestDumpDifficultyBaseline()
{
	Server_DebugRequestDumpDifficultyBaseline();
}

void AHeistPlayerController::DebugRequestForgeryScoreTest()
{
	Server_DebugRequestForgeryScoreTest();
}

void AHeistPlayerController::DebugRequestRebuildResults()
{
	Server_DebugRequestRebuildResults();
}

void AHeistPlayerController::DebugRequestSeedResult(const bool bEscaped)
{
	Server_DebugRequestSeedResult(bEscaped);
}

void AHeistPlayerController::DebugRequestSeedContribution(const int32 SurfaceForgeries, const float BestSurfaceQuality, const int32 Assemblies,
	const float BestAssemblyQuality, const int32 ArtifactsRecovered, const float CarryTimeSeconds, const int32 SecuredLootValue, const int32 GuardsDistracted,
	const int32 TeammatesRescued, const int32 AlarmsTriggered)
{
	Server_DebugRequestSeedContribution(SurfaceForgeries, BestSurfaceQuality, Assemblies, BestAssemblyQuality, ArtifactsRecovered, CarryTimeSeconds,
		SecuredLootValue, GuardsDistracted, TeammatesRescued, AlarmsTriggered);
}

void AHeistPlayerController::DebugRequestFeedbackTest()
{
	Server_DebugRequestFeedbackTest();
}

void AHeistPlayerController::DebugRequestFillInventoryForFeedback(const FName ItemId)
{
	Server_DebugRequestFillInventoryForFeedback(ItemId);
}

void AHeistPlayerController::DebugRequestSetArrested(const bool bArrested)
{
	Server_DebugRequestSetArrested(bArrested);
}

void AHeistPlayerController::DebugRequestDumpArrestState()
{
	Server_DebugRequestDumpArrestState();
}

void AHeistPlayerController::DebugRequestSetFootstepWeight(const float TotalLootWeight)
{
	Server_DebugRequestSetFootstepWeight(TotalLootWeight);
}

void AHeistPlayerController::Server_DebugRequestAddInventoryItem_Implementation(const FName ItemId)
{
#if UE_BUILD_SHIPPING
	return;
#else
	FHeistGameplayRequestContext RequestContext;
	const TCHAR* RejectReason = nullptr;
	if (!TryBuildGameplayRequestContext(RequestContext, RejectReason))
	{
		LogInventoryRequestRejected(TEXT("DebugAddItem"), INDEX_NONE, RejectReason);
		return;
	}

	int32 AddedInstanceId = INDEX_NONE;
	const TCHAR* AddRejectReason = nullptr;
	if (!RequestContext.InventoryComponent->TryAddItem(ItemId, AddedInstanceId, AddRejectReason))
	{
		LogInventoryRequestRejected(TEXT("DebugAddItem"), INDEX_NONE, AddRejectReason != nullptr ? AddRejectReason : TEXT("AddRejected"));
		return;
	}

	AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	FHeistItemDataRow ItemDefinition;
	if (IsValid(HeistGameMode) && HeistGameMode->TryGetItemDefinition(ItemId, ItemDefinition) && ItemDefinition.ItemType == EHeistItemType::Loot)
	{
		FHeistLootDataRow LootDefinition;
		if (!HeistGameMode->TryGetLootDefinition(ItemId, LootDefinition) || !RequestContext.PlayerState->CanAddLootScoreAndWeight(LootDefinition.ScoreValue, ItemDefinition.Weight))
		{
			FHeistInventoryItem RolledBackItem;
			RequestContext.InventoryComponent->TryRemoveItem(AddedInstanceId, RolledBackItem);
			LogInventoryRequestRejected(TEXT("DebugAddItem"), AddedInstanceId, TEXT("InvalidLootTotals"));
			return;
		}

		checkf(RequestContext.PlayerState->AddLootScoreAndWeight(LootDefinition.ScoreValue, ItemDefinition.Weight), TEXT("Validated debug Loot totals must commit."));
		if (AHeistGameState* HeistGameState = GetWorld()->GetGameState<AHeistGameState>())
		{
			HeistGameState->RefreshContractCarriedValue();
		}
	}
#endif
}

void AHeistPlayerController::Server_DebugRequestForgeryScoreTest_Implementation()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugForgeryScoreTest(this);
#endif
}

void AHeistPlayerController::Server_DebugRequestThrowCoinAtWorldLocation_Implementation(const FVector TargetWorldLocation)
{
	FHeistGameplayRequestContext RequestContext;
	const TCHAR* RejectReason = nullptr;
	if (!TryBuildGameplayRequestContext(RequestContext, RejectReason))
	{
		LogThrowableUseRejected(EHeistQuickSlotType::Coin, FName(TEXT("Throwable_Coin")), RejectReason);
		return;
	}

	if (RequestContext.Character->GetActionComponent()->IsGameplayCastActive())
	{
		LogThrowableUseRejected(EHeistQuickSlotType::Coin, FName(TEXT("Throwable_Coin")), TEXT("Casting"));
		return;
	}

	AHeistThrowableProjectile* SpawnedProjectile = nullptr;
	if (!TrySpawnThrowableProjectile(RequestContext, FName(TEXT("Throwable_Coin")), TargetWorldLocation, true, SpawnedProjectile, RejectReason))
	{
		LogThrowableUseRejected(EHeistQuickSlotType::Coin, FName(TEXT("Throwable_Coin")), RejectReason);
	}
}

void AHeistPlayerController::Server_DebugRequestSpawnGuard_Implementation(const float Distance)
{
#if !UE_BUILD_SHIPPING
	APawn* RequestingPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!IsValid(RequestingPawn) || !IsValid(World))
	{
		UHeistDebugFunctionLibrary::Message(this, TEXT("Guard debug spawn rejected: missing pawn or world."), EHeistDebugLevel::Warning);
		return;
	}

	const float SafeDistance = FMath::Clamp(Distance, 100.0f, 3000.0f);
	const FVector DebugGuardSpawnLocation = RequestingPawn->GetActorLocation() + RequestingPawn->GetActorForwardVector() * SafeDistance;
	const FTransform SpawnTransform((-RequestingPawn->GetActorForwardVector()).Rotation(), DebugGuardSpawnLocation);
	AHeistGuardCharacter* SpawnedGuard =
		World->SpawnActorDeferred<AHeistGuardCharacter>(AHeistGuardCharacter::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!IsValid(SpawnedGuard))
	{
		UHeistDebugFunctionLibrary::Message(this, TEXT("Guard debug spawn rejected: deferred spawn failed."), EHeistDebugLevel::Warning);
		return;
	}

	SpawnedGuard->FinishSpawning(SpawnTransform);
	UHeistDebugFunctionLibrary::DebugDrawGuardSpawnMarker(this, SpawnedGuard);
	UHeistDebugFunctionLibrary::Message(this, FString::Printf(TEXT("Guard debug spawned: Guard=%s Location=(%.1f,%.1f,%.1f)"), *GetNameSafe(SpawnedGuard), SpawnedGuard->GetActorLocation().X,
															  SpawnedGuard->GetActorLocation().Y, SpawnedGuard->GetActorLocation().Z));
#endif
}

void AHeistPlayerController::Server_DebugRequestSetNearestGuardState_Implementation(const EHeistGuardState RequestedGuardState, const float DurationSeconds)
{
#if !UE_BUILD_SHIPPING
	APawn* RequestingPawn = GetPawn();
	if (!IsValid(RequestingPawn) || !IsValid(GetWorld()))
	{
		return;
	}

	AHeistGuardCharacter* NearestGuard = FindNearestGuard();

	if (!IsValid(NearestGuard))
	{
		UHeistDebugFunctionLibrary::Message(this, TEXT("Guard state debug request rejected: no Guard exists."), EHeistDebugLevel::Warning);
		return;
	}

	UHeistGuardStateComponent* GuardStateComponent = NearestGuard->GetGuardStateComponent();
	checkf(IsValid(GuardStateComponent), TEXT("Heist Guard requires GuardStateComponent."));

	const float SafeDuration = FMath::Max(0.0f, DurationSeconds);
	switch (RequestedGuardState)
	{
	case EHeistGuardState::Disabled:
		GuardStateComponent->SetDisabled(true);
		break;
	case EHeistGuardState::Patrol:
		if (GuardStateComponent->GetGuardState() == EHeistGuardState::Disabled)
		{
			GuardStateComponent->SetDisabled(false);
		}
		else
		{
			GuardStateComponent->EnterPatrol();
		}
		break;
	case EHeistGuardState::InvestigateNoise:
		GuardStateComponent->EnterInvestigateNoise(RequestingPawn->GetActorLocation(), SafeDuration);
		break;
	case EHeistGuardState::ChasePlayer:
		GuardStateComponent->EnterChasePlayer(RequestingPawn);
		break;
	case EHeistGuardState::SearchLastKnownLocation:
		GuardStateComponent->EnterSearchLastKnownLocation(RequestingPawn->GetActorLocation());
		break;
	case EHeistGuardState::ReturnToPatrol:
		GuardStateComponent->EnterReturnToPatrol();
		break;
	default:
		break;
	}
#endif
}

void AHeistPlayerController::Server_DebugRequestEvaluateNearestGuardSight_Implementation()
{
#if !UE_BUILD_SHIPPING
	AHeistGuardCharacter* NearestGuard = FindNearestGuard();
	AHeistGuardAIController* GuardAIController = IsValid(NearestGuard) ? Cast<AHeistGuardAIController>(NearestGuard->GetController()) : nullptr;
	if (!IsValid(GuardAIController) || !IsValid(GetPawn()))
	{
		UHeistDebugFunctionLibrary::Message(this, TEXT("Guard sight debug request rejected: missing Guard AIController or player pawn."), EHeistDebugLevel::Warning);
		return;
	}

	GuardAIController->DebugEvaluateSightTarget(GetPawn());
#endif
}

void AHeistPlayerController::Server_DebugRequestSetNearestGuardAutomaticSight_Implementation(const bool bEnabled)
{
#if !UE_BUILD_SHIPPING
	AHeistGuardCharacter* NearestGuard = FindNearestGuard();
	AHeistGuardAIController* GuardAIController = IsValid(NearestGuard) ? Cast<AHeistGuardAIController>(NearestGuard->GetController()) : nullptr;
	if (!IsValid(GuardAIController))
	{
		UHeistDebugFunctionLibrary::Message(this, TEXT("Guard automatic sight debug request rejected: missing Guard AIController."), EHeistDebugLevel::Warning);
		return;
	}

	GuardAIController->SetAutomaticSightEnabled(bEnabled);
	UHeistDebugFunctionLibrary::Message(
		this, FString::Printf(TEXT("Guard automatic sight changed: Guard=%s Enabled=%s"), *GetNameSafe(NearestGuard), GuardAIController->IsAutomaticSightEnabled() ? TEXT("true") : TEXT("false")),
		EHeistDebugLevel::Info);
#endif
}

void AHeistPlayerController::Server_DebugRequestReportGuardNoise_Implementation(const float Distance)
{
#if !UE_BUILD_SHIPPING
	APawn* RequestingPawn = GetPawn();
	AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(RequestingPawn) || !IsValid(HeistGameMode) || !IsValid(HeistGameState))
	{
		return;
	}

	const FName SoundPingId(TEXT("Ping_CoinImpact"));
	FHeistSoundPingDataRow SoundPingDefinition;
	if (!HeistGameMode->TryGetSoundPingDefinition(SoundPingId, SoundPingDefinition))
	{
		UHeistDebugFunctionLibrary::DebugSoundPingDefinitionRejected(this, SoundPingId, TEXT("MissingSoundPingDataRow"));
		return;
	}

	const float SafeDistance = FMath::Clamp(Distance, 0.0f, 5000.0f);
	FHeistSoundPingEvent SoundPingEvent;
	SoundPingEvent.SoundPingTag = SoundPingDefinition.SoundPingTag;
	SoundPingEvent.PingType = SoundPingDefinition.PingType;
	SoundPingEvent.WorldLocation = RequestingPawn->GetActorLocation() + RequestingPawn->GetActorForwardVector() * SafeDistance;
	SoundPingEvent.Radius = FMath::Max(0.0f, SoundPingDefinition.Radius);
	SoundPingEvent.Duration = FMath::Max(0.0f, SoundPingDefinition.Duration);
	SoundPingEvent.bAffectsGuards = SoundPingDefinition.bAffectsGuards;
	HeistGameState->ReportSoundPing(SoundPingEvent);
#endif
}

void AHeistPlayerController::Server_DebugRequestDumpDifficultyBaseline_Implementation()
{
#if !UE_BUILD_SHIPPING
	AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	if (!IsValid(HeistGameMode))
	{
		UHeistDebugFunctionLibrary::Message(this, TEXT("Difficulty baseline dump failed: missing Heist GameMode."), EHeistDebugLevel::Warning);
		return;
	}

	HeistGameMode->DebugDumpPlayerCountDifficultyBaseline();
#endif
}

void AHeistPlayerController::Server_DebugRequestRebuildResults_Implementation()
{
#if !UE_BUILD_SHIPPING
	AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistGameState))
	{
		UHeistDebugFunctionLibrary::Message(this, TEXT("Result debug rebuild rejected: missing Heist GameState."), EHeistDebugLevel::Warning);
		return;
	}

	HeistGameState->RebuildPlayerResults();
#endif
}

void AHeistPlayerController::Server_DebugRequestSeedResult_Implementation(const bool bEscaped)
{
#if !UE_BUILD_SHIPPING
	AHeistPlayerState* HeistPlayerState = GetPlayerState<AHeistPlayerState>();
	AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistPlayerState) || !IsValid(HeistGameState))
	{
		UHeistDebugFunctionLibrary::Message(this, TEXT("Result debug seed rejected: missing Heist PlayerState or GameState."), EHeistDebugLevel::Warning);
		return;
	}

	HeistPlayerState->DebugSetResultState(bEscaped);
	HeistGameState->RebuildPlayerResults();
#endif
}

void AHeistPlayerController::Server_DebugRequestSeedContribution_Implementation(const int32 SurfaceForgeries, const float BestSurfaceQuality,
	const int32 Assemblies, const float BestAssemblyQuality, const int32 ArtifactsRecovered, const float CarryTimeSeconds, const int32 SecuredLootValue,
	const int32 GuardsDistracted, const int32 TeammatesRescued, const int32 AlarmsTriggered)
{
#if !UE_BUILD_SHIPPING
	AHeistPlayerState* HeistPlayerState = GetPlayerState<AHeistPlayerState>();
	AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistPlayerState) || !IsValid(HeistGameState))
	{
		UHeistDebugFunctionLibrary::Message(this, TEXT("Contribution debug seed rejected: missing Heist PlayerState or GameState."), EHeistDebugLevel::Warning);
		return;
	}

	FHeistPlayerContribution SeededContribution;
	SeededContribution.SurfaceForgeries = SurfaceForgeries;
	SeededContribution.BestSurfaceQuality = BestSurfaceQuality;
	SeededContribution.Assemblies = Assemblies;
	SeededContribution.BestAssemblyQuality = BestAssemblyQuality;
	SeededContribution.ArtifactsRecovered = ArtifactsRecovered;
	SeededContribution.CarryTimeSeconds = CarryTimeSeconds;
	SeededContribution.SecuredLootValue = SecuredLootValue;
	SeededContribution.GuardsDistracted = GuardsDistracted;
	SeededContribution.TeammatesRescued = TeammatesRescued;
	SeededContribution.AlarmsTriggered = AlarmsTriggered;
	SeededContribution.bEscaped = HeistPlayerState->IsEscaped();
	SeededContribution.bArrested = HeistPlayerState->IsArrested();
	HeistPlayerState->DebugSetContributionState(SeededContribution);
	HeistGameState->RebuildPlayerResults();

	const FHeistPlayerContribution& CommittedContribution = HeistPlayerState->GetContribution();
	UHeistDebugFunctionLibrary::Message(
		this,
		FString::Printf(
			TEXT("Contribution debug seed committed: PlayerId=%d Surface=%d BestSurface=%.1f Assembly=%d BestAssembly=%.1f Artifacts=%d CarryTime=%.1f SecuredLoot=%d Distracted=%d Rescued=%d Alarms=%d Authority=true Result=PASS"),
			HeistPlayerState->HeistPlayerId, CommittedContribution.SurfaceForgeries, CommittedContribution.BestSurfaceQuality,
			CommittedContribution.Assemblies, CommittedContribution.BestAssemblyQuality, CommittedContribution.ArtifactsRecovered,
			CommittedContribution.CarryTimeSeconds, CommittedContribution.SecuredLootValue, CommittedContribution.GuardsDistracted,
			CommittedContribution.TeammatesRescued, CommittedContribution.AlarmsTriggered));
#endif
}

void AHeistPlayerController::Server_DebugRequestSetArrested_Implementation(const bool bArrested)
{
#if !UE_BUILD_SHIPPING
	AHeistPlayerState* HeistPlayerState = GetPlayerState<AHeistPlayerState>();
	if (!IsValid(HeistPlayerState))
	{
		UHeistDebugFunctionLibrary::Message(this, TEXT("Arrest debug request failed: missing PlayerState."), EHeistDebugLevel::Warning);
		return;
	}

	if (bArrested)
	{
		HeistPlayerState->MarkArrested(nullptr);
	}
	else
	{
		HeistPlayerState->ClearArrested();
	}
#endif
}

void AHeistPlayerController::Server_DebugRequestDumpArrestState_Implementation()
{
#if !UE_BUILD_SHIPPING
	AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (IsValid(HeistGameState))
	{
		HeistGameState->RebuildPlayerResults();
		UHeistDebugFunctionLibrary::Message(
			this,
			FString::Printf(TEXT("Server player lifecycle dump: Active=%d Escaped=%d Arrested=%d AllResolved=%s AllRemainingArrested=%s MatchPhase=%s Authority=true Result=PASS"),
							HeistGameState->GetActiveCrewCount(), HeistGameState->GetEscapedCrewCount(), HeistGameState->GetArrestedCrewCount(),
							HeistGameState->AreAllCrewMembersResolved() ? TEXT("true") : TEXT("false"), HeistGameState->AreAllRemainingCrewMembersArrested() ? TEXT("true") : TEXT("false"),
							*UEnum::GetValueAsString(HeistGameState->GetMatchPhase())));
	}
#endif
}

void AHeistPlayerController::Server_DebugRequestSetFootstepWeight_Implementation(const float TotalLootWeight)
{
#if !UE_BUILD_SHIPPING
	AHeistPlayerState* HeistPlayerState = GetPlayerState<AHeistPlayerState>();
	if (IsValid(HeistPlayerState))
	{
		HeistPlayerState->DebugSetTotalLootWeight(TotalLootWeight);
	}
#endif
}

void AHeistPlayerController::Server_DebugRequestFeedbackTest_Implementation()
{
#if !UE_BUILD_SHIPPING
	SendPopupFeedback(NSLOCTEXT("HeistFeedback", "ServerConfirmedTest", "서버 확인 피드백"), 3.0f);
#endif
}

void AHeistPlayerController::Server_DebugRequestFillInventoryForFeedback_Implementation(const FName ItemId)
{
#if !UE_BUILD_SHIPPING
	FHeistGameplayRequestContext RequestContext;
	const TCHAR* ContextRejectReason = nullptr;
	if (!TryBuildGameplayRequestContext(RequestContext, ContextRejectReason))
	{
		LogInventoryRequestRejected(TEXT("FeedbackBagFull"), INDEX_NONE, ContextRejectReason);
		return;
	}

	for (int32 AddAttempt = 0; AddAttempt < 64; ++AddAttempt)
	{
		int32 AddedInstanceId = INDEX_NONE;
		const TCHAR* AddRejectReason = nullptr;
		if (!RequestContext.InventoryComponent->TryAddItem(ItemId, AddedInstanceId, AddRejectReason))
		{
			LogInventoryRequestRejected(TEXT("FeedbackBagFull"), AddedInstanceId, AddRejectReason != nullptr ? AddRejectReason : TEXT("AddRejected"));
			return;
		}
	}

	LogInventoryRequestRejected(TEXT("FeedbackBagFull"), INDEX_NONE, TEXT("TestLimitReached"));
#endif
}

AHeistGuardCharacter* AHeistPlayerController::FindNearestGuard() const
{
	if (!IsValid(GetWorld()))
	{
		return nullptr;
	}

	const APawn* ReferencePawn = GetPawn();
	const FVector ReferenceLocation = IsValid(ReferencePawn) ? ReferencePawn->GetActorLocation() : FVector::ZeroVector;
	AHeistGuardCharacter* NearestGuard = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<AHeistGuardCharacter> GuardIterator(GetWorld()); GuardIterator; ++GuardIterator)
	{
		AHeistGuardCharacter* CandidateGuard = *GuardIterator;
		if (!IsValid(CandidateGuard))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(ReferenceLocation, CandidateGuard->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestGuard = CandidateGuard;
		}
	}

	return NearestGuard;
}

#pragma endregion

#pragma region Feedback

FHeistPopupFeedbackRequested& AHeistPlayerController::GetPopupFeedbackRequestedDelegate()
{
	return PopupFeedbackRequestedDelegate;
}

void AHeistPlayerController::Client_ReceivePopupFeedback_Implementation(const FText& Message, const float DurationSeconds)
{
	if (Message.IsEmpty())
	{
		return;
	}

	const float SafeDuration = FMath::Max(0.1f, DurationSeconds);
	PopupFeedbackRequestedDelegate.Broadcast(Message, SafeDuration);
	UE_LOG(LogHeistUI, Log, TEXT("[%s] Popup feedback received: Message=%s Duration=%.2f Local=%s"), *GetName(), *Message.ToString(), SafeDuration, IsLocalController() ? TEXT("true") : TEXT("false"));
}

void AHeistPlayerController::SendPopupFeedback(const FText& Message, const float DurationSeconds)
{
	if (Message.IsEmpty())
	{
		return;
	}

	Client_ReceivePopupFeedback(Message, FMath::Max(0.1f, DurationSeconds));
}

void AHeistPlayerController::SendPopupFeedbackForRejection(const TCHAR* RequestName, const TCHAR* Reason)
{
	SendPopupFeedback(ResolvePopupFeedbackText(RequestName, Reason));
}

FText AHeistPlayerController::ResolvePopupFeedbackText(const TCHAR* RequestName, const TCHAR* Reason)
{
	const FString Request = RequestName != nullptr ? RequestName : TEXT("Request");
	const FString Rejection = Reason != nullptr ? Reason : TEXT("Rejected");

	if (Rejection == TEXT("InventoryFull"))
	{
		return NSLOCTEXT("HeistFeedback", "BagFull", "가방이 가득 찼습니다");
	}
	if (Rejection == TEXT("AlreadyTaken"))
	{
		return NSLOCTEXT("HeistFeedback", "AlreadyTaken", "이미 획득한 전리품입니다");
	}
	if (Rejection == TEXT("OutOfRange") || Rejection == TEXT("NotCurrentTarget"))
	{
		return NSLOCTEXT("HeistFeedback", "TooFarAway", "너무 멀리 있습니다");
	}
	if (Rejection == TEXT("Stunned"))
	{
		return NSLOCTEXT("HeistFeedback", "BlockedByStun", "기절 상태에서는 행동할 수 없습니다");
	}
	if (Rejection == TEXT("Casting"))
	{
		return NSLOCTEXT("HeistFeedback", "BlockedByCast", "이미 다른 행동을 진행 중입니다");
	}
	if (Rejection == TEXT("InventoryClosed"))
	{
		return NSLOCTEXT("HeistFeedback", "InventoryClosed", "가방이 닫혀 있습니다");
	}
	if (Rejection == TEXT("InvalidTargetPlacement"))
	{
		return NSLOCTEXT("HeistFeedback", "InvalidPlacement", "배치할 수 없는 위치입니다");
	}
	if (Rejection == TEXT("RotationRejected"))
	{
		return NSLOCTEXT("HeistFeedback", "RotationBlocked", "회전할 수 없습니다");
	}
	if (Rejection == TEXT("InvalidSlotAssignment") || Rejection == TEXT("InvalidSlot"))
	{
		return NSLOCTEXT("HeistFeedback", "InvalidQuickSlot", "올바르지 않은 단축 슬롯입니다");
	}
	if (Rejection == TEXT("EmptyQuickSlot"))
	{
		return NSLOCTEXT("HeistFeedback", "EmptyQuickSlot", "단축 슬롯이 비어 있습니다");
	}
	if (Rejection == TEXT("EscapePhaseClosed"))
	{
		return NSLOCTEXT("HeistFeedback", "EscapeClosed", "아직 탈출할 수 없습니다");
	}
	if (Rejection == TEXT("Lockdown"))
	{
		return NSLOCTEXT("HeistFeedback", "Lockdown", "봉쇄 상태에서는 행동할 수 없습니다");
	}
	if (Rejection == TEXT("MatchNotInGame"))
	{
		return NSLOCTEXT("HeistFeedback", "MatchEnded", "현재 진행 중인 작전이 없습니다");
	}
	if (Request.Contains(TEXT("Loot")))
	{
		return NSLOCTEXT("HeistFeedback", "LootRejected", "전리품 획득 요청이 거부되었습니다");
	}
	if (Request.Contains(TEXT("Escape")))
	{
		return NSLOCTEXT("HeistFeedback", "EscapeRejected", "탈출 요청이 거부되었습니다");
	}
	if (Request.Contains(TEXT("Throwable")))
	{
		return NSLOCTEXT("HeistFeedback", "ItemUseRejected", "아이템 사용 요청이 거부되었습니다");
	}
	return NSLOCTEXT("HeistFeedback", "RequestRejected", "요청이 거부되었습니다");
}

#pragma endregion

#pragma region InternalHelpers

bool AHeistPlayerController::TryBuildGameplayRequestContext(FHeistGameplayRequestContext& OutContext, const TCHAR*& OutRejectReason) const
{
	OutContext = FHeistGameplayRequestContext();
	OutRejectReason = nullptr;

	if (!HasAuthority())
	{
		OutRejectReason = TEXT("InvalidController");
		return false;
	}

	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	if (!IsValid(HeistCharacter) || HeistCharacter->GetController() != this)
	{
		OutRejectReason = TEXT("InvalidCharacterOwnership");
		return false;
	}

	AHeistPlayerState* HeistPlayerState = GetPlayerState<AHeistPlayerState>();
	if (!IsValid(HeistPlayerState))
	{
		OutRejectReason = TEXT("InvalidPlayerState");
		return false;
	}

	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistGameState))
	{
		OutRejectReason = TEXT("MatchNotInGame");
		return false;
	}

	if (HeistGameState->IsLockdownActive())
	{
		OutRejectReason = TEXT("Lockdown");
		return false;
	}

	if (HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame)
	{
		OutRejectReason = TEXT("MatchNotInGame");
		return false;
	}

	if (HeistPlayerState->IsEscaped())
	{
		OutRejectReason = TEXT("AlreadyEscaped");
		return false;
	}

	if (HeistPlayerState->IsArrested())
	{
		OutRejectReason = TEXT("PlayerArrested");
		return false;
	}

	const UHeistForgeryComponent* ForgeryComponent = HeistCharacter->GetForgeryComponent();
	if (IsValid(ForgeryComponent) && ForgeryComponent->IsSessionActive())
	{
		OutRejectReason = TEXT("ForgeryActive");
		return false;
	}
	const UHeistObjectAssemblyComponent* ObjectAssemblyComponent = HeistCharacter->GetObjectAssemblyComponent();
	if (IsValid(ObjectAssemblyComponent) && ObjectAssemblyComponent->IsSessionActive())
	{
		OutRejectReason = TEXT("ObjectAssemblyActive");
		return false;
	}

	OutContext.Character = HeistCharacter;
	OutContext.PlayerState = HeistPlayerState;
	OutContext.InventoryComponent = HeistCharacter->GetInventoryComponent();
	checkf(IsValid(OutContext.InventoryComponent), TEXT("HeistPlayerCharacter requires HeistInventoryComponent"));
	return true;
}

bool AHeistPlayerController::TryBuildInventoryMutationRequestContext(FHeistGameplayRequestContext& OutContext, const TCHAR*& OutRejectReason) const
{
	if (!TryBuildGameplayRequestContext(OutContext, OutRejectReason))
	{
		return false;
	}

	if (OutContext.InventoryComponent->GetOwner() != OutContext.Character)
	{
		OutRejectReason = TEXT("InvalidInventoryOwnership");
		return false;
	}

	if (!OutContext.InventoryComponent->IsInventoryOpen())
	{
		OutRejectReason = TEXT("InventoryClosed");
		return false;
	}

	if (OutContext.Character->GetActionComponent()->IsGameplayCastActive())
	{
		OutRejectReason = TEXT("Casting");
		return false;
	}

	return true;
}

bool AHeistPlayerController::TryResolveQuickSlotItem(const FHeistGameplayRequestContext& RequestContext, const EHeistQuickSlotType SlotType, FName& OutItemId, const TCHAR*& OutRejectReason) const
{
	OutItemId = NAME_None;
	OutRejectReason = nullptr;

	const FHeistQuickSlotState* QuickSlot =
		RequestContext.InventoryComponent->GetQuickSlots().FindByPredicate([SlotType](const FHeistQuickSlotState& ExistingQuickSlot) { return ExistingQuickSlot.SlotType == SlotType; });
	if (QuickSlot == nullptr || QuickSlot->ItemInstanceId == INDEX_NONE)
	{
		OutRejectReason = TEXT("EmptyQuickSlot");
		return false;
	}

	FHeistInventoryItem InventoryItem;
	if (!RequestContext.InventoryComponent->TryGetItem(QuickSlot->ItemInstanceId, InventoryItem))
	{
		OutRejectReason = TEXT("InvalidQuickSlotItem");
		return false;
	}

	const FName ExpectedItemId = GetExpectedQuickSlotItemId(SlotType);
	if (ExpectedItemId.IsNone() || InventoryItem.ItemId != ExpectedItemId)
	{
		OutItemId = InventoryItem.ItemId;
		OutRejectReason = TEXT("QuickSlotItemMismatch");
		return false;
	}

	OutItemId = InventoryItem.ItemId;
	return true;
}

bool AHeistPlayerController::TrySpawnThrowableProjectile(const FHeistGameplayRequestContext& RequestContext, const FName ItemId, const FVector& TargetWorldLocation, const bool bDebugBypassInventory,
														 AHeistThrowableProjectile*& OutProjectile, const TCHAR*& OutRejectReason) const
{
	OutProjectile = nullptr;
	OutRejectReason = nullptr;

	AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	if (!IsValid(HeistGameMode))
	{
		OutRejectReason = TEXT("MissingAuthGameMode");
		return false;
	}

	const bool bIsDebugFallbackThrowable = bDebugBypassInventory && ItemId == FName(TEXT("Throwable_Coin"));

	FHeistItemDataRow ItemDefinition;
	if (!HeistGameMode->TryGetItemDefinition(ItemId, ItemDefinition) || ItemDefinition.ItemType != EHeistItemType::Throwable || !ItemDefinition.bCanUseQuickSlot)
	{
		if (!bIsDebugFallbackThrowable)
		{
			OutRejectReason = TEXT("InvalidThrowableItem");
			return false;
		}

		ItemDefinition.ItemId = ItemId;
		ItemDefinition.ItemType = EHeistItemType::Throwable;
		ItemDefinition.bCanUseQuickSlot = true;
	}

	FHeistUsableItemDataRow UsableItemDefinition;
	if (!HeistGameMode->TryGetUsableItemDefinition(ItemId, UsableItemDefinition) || UsableItemDefinition.UseType != EHeistUseType::Throw)
	{
		if (!bIsDebugFallbackThrowable)
		{
			OutRejectReason = TEXT("InvalidUsableItem");
			return false;
		}

		UsableItemDefinition.ItemId = ItemId;
		UsableItemDefinition.UseType = EHeistUseType::Throw;
		UsableItemDefinition.TargetType = EHeistTargetType::ActorHit;
		UsableItemDefinition.Duration = 3.0f;
		UsableItemDefinition.ProjectileSpeed = 1500.0f;
	}

	UClass* ProjectileClass = UsableItemDefinition.SpawnedActorClass.LoadSynchronous();
	if (!IsValid(ProjectileClass) && ItemId == FName(TEXT("Throwable_Coin")))
	{
		ProjectileClass = AHeistCoinProjectile::StaticClass();
	}

	if (!IsValid(ProjectileClass) || !ProjectileClass->IsChildOf(AHeistThrowableProjectile::StaticClass()))
	{
		OutRejectReason = TEXT("InvalidProjectileClass");
		return false;
	}

	if (TargetWorldLocation.ContainsNaN() || FVector::DistSquared(RequestContext.Character->GetActorLocation(), TargetWorldLocation) > FMath::Square(10000.0f))
	{
		OutRejectReason = TEXT("InvalidThrowTarget");
		return false;
	}

	const FVector ProjectileSpawnLocation = RequestContext.Character->GetActorLocation() + FVector::UpVector * 50.0f;
	const FVector LaunchDirection = (TargetWorldLocation - ProjectileSpawnLocation).GetSafeNormal();
	if (LaunchDirection.IsNearlyZero())
	{
		OutRejectReason = TEXT("InvalidThrowDirection");
		return false;
	}

	const float ProjectileSpeed = FMath::Max(1.0f, UsableItemDefinition.ProjectileSpeed);
	const FTransform SpawnTransform(LaunchDirection.Rotation(), ProjectileSpawnLocation);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = RequestContext.Character;
	SpawnParameters.Instigator = RequestContext.Character;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	OutProjectile = GetWorld()->SpawnActor<AHeistThrowableProjectile>(ProjectileClass, SpawnTransform, SpawnParameters);
	if (!IsValid(OutProjectile))
	{
		OutRejectReason = TEXT("ProjectileSpawnFailed");
		return false;
	}

	OutProjectile->InitializeThrowable(RequestContext.Character, ItemId, LaunchDirection, ProjectileSpeed, UsableItemDefinition.Duration);

	UHeistDebugFunctionLibrary::DebugThrowableProjectileSpawned(this, RequestContext.Character, OutProjectile, ItemId, TargetWorldLocation, LaunchDirection, ProjectileSpeed, bDebugBypassInventory);
	return true;
}

FName AHeistPlayerController::GetExpectedQuickSlotItemId(const EHeistQuickSlotType SlotType)
{
	switch (SlotType)
	{
	case EHeistQuickSlotType::Coin:
		return FName(TEXT("Throwable_Coin"));
	default:
		return NAME_None;
	}
}

void AHeistPlayerController::LogLootPickupRejected(const AHeistLootActor* TargetLootActor, const TCHAR* Reason, float Distance)
{
	UHeistDebugFunctionLibrary::DebugLootPickupRequestRejected(this, TargetLootActor, Reason, Distance);
	SendPopupFeedbackForRejection(TEXT("LootPickup"), Reason);
}

void AHeistPlayerController::LogEscapeRequestRejected(const AHeistVentActor* TargetVentActor, const TCHAR* Reason, float Distance)
{
	UHeistDebugFunctionLibrary::DebugEscapeRequestRejected(this, TargetVentActor, Reason, Distance);
	SendPopupFeedbackForRejection(TEXT("Escape"), Reason);
}

void AHeistPlayerController::LogInventoryRequestRejected(const TCHAR* RequestName, const int32 InstanceId, const TCHAR* Reason)
{
	UHeistDebugFunctionLibrary::DebugInventoryRequestRejected(this, RequestName, InstanceId, Reason);
	SendPopupFeedbackForRejection(RequestName, Reason);
}

void AHeistPlayerController::LogThrowableUseRejected(const EHeistQuickSlotType SlotType, const FName ItemId, const TCHAR* Reason)
{
	UHeistDebugFunctionLibrary::DebugThrowableUseRejected(this, SlotType, ItemId, Reason);
	SendPopupFeedbackForRejection(TEXT("ThrowableUse"), Reason);
}

#pragma endregion
