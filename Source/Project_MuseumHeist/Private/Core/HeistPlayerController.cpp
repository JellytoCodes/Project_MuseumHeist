#include "Core/HeistPlayerController.h"

#include "AI/HeistGuardAIController.h"
#include "AI/HeistGuardCharacter.h"
#include "AI/HeistGuardStateComponent.h"
#include "Character/Components/HeistActionComponent.h"
#include "Character/Components/HeistInteractionComponent.h"
#include "Character/Components/HeistInventoryComponent.h"
#include "Character/Components/HeistVisionComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameState.h"
#include "Core/HeistGameMode.h"
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
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Inventory/HeistItemDataTypes.h"
#include "Inventory/HeistInventoryTypes.h"
#include "World/Actors/Escape/HeistVentActor.h"
#include "World/Actors/Loot/HeistDisplayCaseActor.h"
#include "World/Actors/Loot/HeistLootActor.h"
#include "World/Actors/Projectile/HeistCoinProjectile.h"
#include "World/Actors/Projectile/HeistSmokeProjectile.h"
#include "World/Actors/Projectile/HeistThrowableProjectile.h"
#include "World/Actors/Trap/HeistGlueTrapActor.h"
#include "World/Actors/Trap/HeistTrapActor.h"

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

	RefreshLocalInputModeFromPawn();
	RefreshLocalHUDPresentation();
	UpdateFlashlightAimDirection();
}

void AHeistPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	RefreshLocalInputModeFromPawn();
	RefreshLocalHUDPresentation();
	UpdateFlashlightAimDirection();
}

void AHeistPlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();
	RefreshLocalInputModeFromPawn();
	RefreshLocalHUDPresentation();
	UpdateFlashlightAimDirection();
}

void AHeistPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
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
		EnhancedInputComponent->BindAction(
			MoveInputAction,
			ETriggerEvent::Triggered,
			this,
			&AHeistPlayerController::HandleMoveInput);
	}
	else
	{
		UHeistDebugFunctionLibrary::DebugMissingInputAsset(this, TEXT("MoveInputAction"));
	}

	if (LookInputAction != nullptr)
	{
		EnhancedInputComponent->BindAction(
			LookInputAction,
			ETriggerEvent::Triggered,
			this,
			&AHeistPlayerController::HandleLookInput);
	}
	else
	{
		UHeistDebugFunctionLibrary::DebugMissingInputAsset(this, TEXT("LookInputAction"));
	}

	if (InteractInputAction != nullptr)
	{
		EnhancedInputComponent->BindAction(
			InteractInputAction,
			ETriggerEvent::Started,
			this,
			&AHeistPlayerController::HandleInteractPressed);
		EnhancedInputComponent->BindAction(
			InteractInputAction,
			ETriggerEvent::Completed,
			this,
			&AHeistPlayerController::HandleInteractReleased);
		EnhancedInputComponent->BindAction(
			InteractInputAction,
			ETriggerEvent::Canceled,
			this,
			&AHeistPlayerController::HandleInteractReleased);
	}
	else
	{
		UHeistDebugFunctionLibrary::DebugMissingInputAsset(this, TEXT("InteractInputAction"));
	}

	if (InventoryInputAction != nullptr)
	{
		EnhancedInputComponent->BindAction(
			InventoryInputAction,
			ETriggerEvent::Started,
			this,
			&AHeistPlayerController::HandleInventoryToggle);
	}
	else
	{
		UHeistDebugFunctionLibrary::DebugMissingInputAsset(this, TEXT("InventoryInputAction"));
	}

	RefreshLocalInputModeFromPawn();
}

void AHeistPlayerController::RefreshLocalHUDPresentation()
{
	if (!IsLocalController())
	{
		return;
	}

	if (AHeistHUD* HeistHUD = GetHUD<AHeistHUD>())
	{
		HeistHUD->RefreshPresentationSources();
	}
}

#pragma endregion

#pragma region Input

void AHeistPlayerController::HandleLookInput(const FInputActionValue& InputValue)
{
	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	if (!ensureMsgf(HeistCharacter != nullptr, TEXT("Look input requires a possessed HeistPlayerCharacter"))
		|| !HeistCharacter->CanPerformGameplayActions())
	{
		return;
	}

	const FVector2D LookInput = InputValue.Get<FVector2D>();
	AddYawInput(LookInput.X);
	AddPitchInput(LookInput.Y);
	UpdateFlashlightAimDirection();
	HeistCharacter->GetInteractionComponent()->RefreshInteractionTarget();
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
	HeistCharacter->GetInteractionComponent()->RefreshInteractionTarget();
}

void AHeistPlayerController::HandleInventoryToggle()
{
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

void AHeistPlayerController::RefreshLocalInputModeFromPawn()
{
	if (!IsLocalController())
	{
		return;
	}

	const AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	const UHeistInventoryComponent* InventoryComponent = IsValid(HeistCharacter)
		? HeistCharacter->GetInventoryComponent()
		: nullptr;
	ApplyLocalInputMode(
		IsValid(InventoryComponent) && InventoryComponent->IsInventoryOpen()
			? EHeistInputMode::Inventory
			: EHeistInputMode::Gameplay);
}

void AHeistPlayerController::ApplyLocalInputMode(const EHeistInputMode NewInputMode)
{
	if (!IsLocalController())
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
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
	}

	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
	FlushPressedKeys();
	LocalInputMode = NewInputMode;

	if (NewInputMode == EHeistInputMode::Inventory)
	{
		if (IsValid(InputSubsystem))
		{
			if (IsValid(InventoryInputMappingContext.Get()))
			{
				InputSubsystem->AddMappingContext(InventoryInputMappingContext, 10);
			}
			else if (IsValid(GameplayInputMappingContext.Get()))
			{
				InputSubsystem->AddMappingContext(GameplayInputMappingContext, 0);
				UHeistDebugFunctionLibrary::DebugMissingInputAsset(this, TEXT("InventoryInputMappingContext"));
			}
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
	else
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

	const AHeistPlayerState* HeistPlayerState = GetPlayerState<AHeistPlayerState>();
	if (IsValid(HeistPlayerState) && HeistPlayerState->IsArrested())
	{
		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
	}

	UE_LOG(
		LogHeist,
		Verbose,
		TEXT("[%s] Local input mode applied: Mode=%s GameplayContext=%s InventoryContext=%s Cursor=%s IgnoreMove=%s IgnoreLook=%s"),
		*GetName(),
		NewInputMode == EHeistInputMode::Gameplay
			? TEXT("Gameplay")
			: NewInputMode == EHeistInputMode::Inventory
				? TEXT("Inventory")
				: TEXT("Forgery"),
		*GetNameSafe(GameplayInputMappingContext.Get()),
		*GetNameSafe(InventoryInputMappingContext.Get()),
		bShowMouseCursor ? TEXT("true") : TEXT("false"),
		IsMoveInputIgnored() ? TEXT("true") : TEXT("false"),
		IsLookInputIgnored() ? TEXT("true") : TEXT("false"));
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
	if (InventoryComponent->IsInventoryOpen())
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
	if (!InteractionComponent->RefreshInteractionTarget(true))
	{
		return;
	}

	AHeistLootActor* TargetLootActor = Cast<AHeistLootActor>(InteractionComponent->GetCurrentInteractionTarget());
	if (TargetLootActor != nullptr)
	{
		Server_RequestLootPickup(TargetLootActor);
		return;
	}

	AHeistDisplayCaseActor* TargetDisplayCase = Cast<AHeistDisplayCaseActor>(
		InteractionComponent->GetCurrentInteractionTarget());
	if (TargetDisplayCase != nullptr)
	{
		if (TargetDisplayCase->GetDisplayCaseState()
			== EHeistDisplayCaseState::OriginalAvailable)
		{
			Server_RequestTakeOriginal(TargetDisplayCase);
		}
		else
		{
			Server_RequestObservation(TargetDisplayCase);
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
	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	if (!IsValid(HeistCharacter))
	{
		return;
	}

	Server_CancelObservation();
}

#pragma endregion

#pragma region Networking

void AHeistPlayerController::HandleInventoryOpenStateChanged(const bool bInventoryOpen)
{
	if (!IsLocalController())
	{
		return;
	}

	ApplyLocalInputMode(
		bInventoryOpen
			? EHeistInputMode::Inventory
			: EHeistInputMode::Gameplay);
	if (!bInventoryOpen)
	{
		UpdateFlashlightAimDirection();
		if (AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>())
		{
			HeistCharacter->GetInteractionComponent()->RefreshInteractionTarget(true);
		}
	}
}

void AHeistPlayerController::HandleArrestStateChanged(const bool bArrested)
{
	if (!IsLocalController())
	{
		return;
	}

	ApplyLocalInputMode(EHeistInputMode::Gameplay);
	UHeistDebugFunctionLibrary::Message(
		this,
		FString::Printf(
			TEXT("Local arrest input state applied: Arrested=%s InputMode=Gameplay Cursor=false IgnoreMove=%s IgnoreLook=%s"),
			bArrested ? TEXT("true") : TEXT("false"),
			IsMoveInputIgnored() ? TEXT("true") : TEXT("false"),
			IsLookInputIgnored() ? TEXT("true") : TEXT("false")));
}

EHeistInputMode AHeistPlayerController::GetLocalInputMode() const
{
	return LocalInputMode;
}

void AHeistPlayerController::RequestSetInventoryOpen(const bool bInventoryOpen)
{
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

void AHeistPlayerController::RequestMoveInventoryItem(
	const int32 InstanceId,
	const FIntPoint TargetGridPosition)
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

void AHeistPlayerController::RequestTakeOriginal(AHeistDisplayCaseActor* TargetDisplayCase)
{
	Server_RequestTakeOriginal(TargetDisplayCase);
}

void AHeistPlayerController::RequestDropCarriedOriginal()
{
	Server_RequestDropCarriedOriginal();
}

void AHeistPlayerController::RequestAssignQuickSlot(
	const EHeistQuickSlotType SlotType,
	const int32 InstanceId)
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
	Server_RequestUseQuickSlot(SlotType, ResolvedTargetWorldLocation);
}

bool AHeistPlayerController::TryBuildCameraForwardAim(
	const float Distance,
	FVector& OutViewLocation,
	FVector& OutCameraForward,
	FVector& OutTargetWorldLocation) const
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

void AHeistPlayerController::DebugRequestSetNearestGuardState(
	const EHeistGuardState GuardState,
	const float DurationSeconds)
{
	Server_DebugRequestSetNearestGuardState(GuardState, DurationSeconds);
}

void AHeistPlayerController::DebugRequestEvaluateNearestGuardSight()
{
	Server_DebugRequestEvaluateNearestGuardSight();
}

void AHeistPlayerController::DebugRequestSetNearestGuardAutomaticSight(
	const bool bEnabled)
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

void AHeistPlayerController::DebugRequestRebuildResults()
{
	Server_DebugRequestRebuildResults();
}

void AHeistPlayerController::DebugRequestSeedResult(
	const int32 Score,
	const bool bEscaped,
	const float EscapeTimeSeconds)
{
	Server_DebugRequestSeedResult(Score, bEscaped, EscapeTimeSeconds);
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

	UHeistDebugFunctionLibrary::DebugLootPickupRequestReceived(
		this,
		RequestContext.Character,
		TargetLootActor);

	UHeistInteractionComponent* InteractionComponent = RequestContext.Character->GetInteractionComponent();
	const float Distance = FVector::Distance(
		RequestContext.Character->GetActorLocation(),
		TargetLootActor->GetActorLocation());

	if (!InteractionComponent->IsActorWithinInteractionRange(TargetLootActor))
	{
		LogLootPickupRejected(TargetLootActor, TEXT("OutOfRange"), Distance);
		return;
	}

	if (!TargetLootActor->IsLootAvailable())
	{
		LogLootPickupRejected(TargetLootActor, TEXT("AlreadyTaken"), Distance);
		return;
	}

	InteractionComponent->RefreshInteractionTarget(true);
	if (InteractionComponent->GetCurrentInteractionTarget() != TargetLootActor)
	{
		LogLootPickupRejected(TargetLootActor, TEXT("NotCurrentTarget"), Distance);
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
	if (!RequestContext.InventoryComponent->TryAddItem(
		TargetLootActor->GetLootRowId(),
		AddedInstanceId,
		InventoryRejectReason))
	{
		TargetLootActor->ReleasePickupReservation(RequestContext.Character);
		LogLootPickupRejected(
			TargetLootActor,
			InventoryRejectReason != nullptr ? InventoryRejectReason : TEXT("InventoryRejected"),
			Distance);
		return;
	}

	checkf(
		RequestContext.PlayerState->AddLootScoreAndWeight(ScoreDelta, WeightDelta),
		TEXT("Validated loot score and weight must apply after inventory commit"));
	checkf(
		TargetLootActor->CommitPickupReservation(RequestContext.Character),
		TEXT("Reserved loot must commit after inventory and score/weight commit"));

	UHeistDebugFunctionLibrary::DebugLootPickupRequestAccepted(
		this,
		TargetLootActor,
		TargetLootActor->GetLootRowId(),
		AddedInstanceId,
		Distance);
}

void AHeistPlayerController::Server_RequestObservation_Implementation(AHeistDisplayCaseActor* TargetDisplayCase)
{
	FHeistGameplayRequestContext RequestContext;
	const TCHAR* RejectReason = nullptr;
	if (!TryBuildGameplayRequestContext(RequestContext, RejectReason))
	{
		UHeistDebugFunctionLibrary::DebugObservationRequestRejected(
			this,
			TargetDisplayCase,
			RejectReason != nullptr ? RejectReason : TEXT("InvalidRequestContext"));
		return;
	}

	if (!IsValid(TargetDisplayCase))
	{
		UHeistDebugFunctionLibrary::DebugObservationRequestRejected(this, nullptr, TEXT("InvalidTarget"));
		return;
	}

	UHeistInteractionComponent* InteractionComponent = RequestContext.Character->GetInteractionComponent();
	const float Distance = FVector::Distance(
		RequestContext.Character->GetActorLocation(),
		TargetDisplayCase->GetActorLocation());
	if (!InteractionComponent->IsActorWithinInteractionRange(TargetDisplayCase))
	{
		UHeistDebugFunctionLibrary::DebugObservationRequestRejected(
			this,
			TargetDisplayCase,
			TEXT("OutOfRange"),
			Distance);
		return;
	}

	InteractionComponent->RefreshInteractionTarget(true);
	if (InteractionComponent->GetCurrentInteractionTarget() != TargetDisplayCase)
	{
		UHeistDebugFunctionLibrary::DebugObservationRequestRejected(
			this,
			TargetDisplayCase,
			TEXT("NotCurrentTarget"),
			Distance);
		return;
	}

	if (TargetDisplayCase->GetDisplayCaseState() != EHeistDisplayCaseState::Secured)
	{
		UHeistDebugFunctionLibrary::DebugObservationRequestRejected(
			this,
			TargetDisplayCase,
			TEXT("CaseNotSecured"),
			Distance);
		return;
	}

	UHeistActionComponent* ActionComponent = RequestContext.Character->GetActionComponent();
	if (!ActionComponent->TryBeginObservationRequest(TargetDisplayCase))
	{
		UHeistDebugFunctionLibrary::DebugObservationRequestRejected(
			this,
			TargetDisplayCase,
			TEXT("ObservationCastRejected"),
			Distance);
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

void AHeistPlayerController::Server_RequestTakeOriginal_Implementation(
	AHeistDisplayCaseActor* TargetDisplayCase)
{
	FHeistGameplayRequestContext RequestContext;
	const TCHAR* RejectReason = nullptr;
	if (!TryBuildGameplayRequestContext(RequestContext, RejectReason))
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			FString::Printf(
				TEXT("Original take request rejected: Case=%s Reason=%s"),
				*GetNameSafe(TargetDisplayCase),
				RejectReason != nullptr ? RejectReason : TEXT("InvalidRequestContext")),
			EHeistDebugLevel::Warning);
		return;
	}
	if (!IsValid(TargetDisplayCase))
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			TEXT("Original take request rejected: Reason=InvalidTarget"),
			EHeistDebugLevel::Warning);
		return;
	}

	UHeistInteractionComponent* InteractionComponent =
		RequestContext.Character->GetInteractionComponent();
	const float Distance = FVector::Distance(
		RequestContext.Character->GetActorLocation(),
		TargetDisplayCase->GetActorLocation());
	if (!InteractionComponent->IsActorWithinInteractionRange(TargetDisplayCase))
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			FString::Printf(
				TEXT("Original take request rejected: Case=%s Distance=%.1f Reason=OutOfRange"),
				*GetNameSafe(TargetDisplayCase),
				Distance),
			EHeistDebugLevel::Warning);
		return;
	}

	InteractionComponent->RefreshInteractionTarget(true);
	if (InteractionComponent->GetCurrentInteractionTarget() != TargetDisplayCase)
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			FString::Printf(
				TEXT("Original take request rejected: Case=%s Distance=%.1f Reason=NotCurrentTarget"),
				*GetNameSafe(TargetDisplayCase),
				Distance),
			EHeistDebugLevel::Warning);
		return;
	}

	const float PreviousWeight = RequestContext.PlayerState->GetTotalLootWeight();
	if (!TargetDisplayCase->TryTakeOriginal(RequestContext.PlayerState))
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			FString::Printf(
				TEXT("Original take request rejected: Case=%s Artifact=%s Reason=ServerValidationFailed"),
				*GetNameSafe(TargetDisplayCase),
				*TargetDisplayCase->GetTargetArtifactId().ToString()),
			EHeistDebugLevel::Warning);
		return;
	}

	const FHeistOriginalCarryEntry& CarryEntry =
		RequestContext.InventoryComponent->GetOriginalCarryEntry();
	UHeistDebugFunctionLibrary::Message(
		this,
		FString::Printf(
			TEXT("Original take request accepted: Case=%s Artifact=%s PlayerId=%d CarryWeight=%.1f PreviousWeight=%.1f TotalWeight=%.1f State=%s Authority=true Result=PASS"),
			*GetNameSafe(TargetDisplayCase),
			*CarryEntry.ArtifactId.ToString(),
			RequestContext.PlayerState->HeistPlayerId,
			CarryEntry.Weight,
			PreviousWeight,
			RequestContext.PlayerState->GetTotalLootWeight(),
			*UEnum::GetValueAsString(TargetDisplayCase->GetDisplayCaseState())));
}

void AHeistPlayerController::Server_RequestDropCarriedOriginal_Implementation()
{
	FHeistGameplayRequestContext RequestContext;
	const TCHAR* RejectReason = nullptr;
	if (!TryBuildGameplayRequestContext(RequestContext, RejectReason))
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			FString::Printf(
				TEXT("Original drop request rejected: Reason=%s"),
				RejectReason != nullptr ? RejectReason : TEXT("InvalidRequestContext")),
			EHeistDebugLevel::Warning);
		return;
	}

	const FHeistOriginalCarryEntry CarryEntry =
		RequestContext.InventoryComponent->GetOriginalCarryEntry();
	AHeistDisplayCaseActor* SourceDisplayCase = CarryEntry.SourceDisplayCase.Get();
	if (!CarryEntry.IsValid() || !IsValid(SourceDisplayCase))
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			TEXT("Original drop request rejected: Reason=NotCarryingOriginal"),
			EHeistDebugLevel::Warning);
		return;
	}

	const float PreviousWeight = RequestContext.PlayerState->GetTotalLootWeight();
	if (!SourceDisplayCase->ReleaseOriginalForCarrier(
		RequestContext.PlayerState,
		FName(TEXT("OwnerDropped"))))
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			FString::Printf(
				TEXT("Original drop request rejected: Case=%s Artifact=%s Reason=ServerValidationFailed"),
				*GetNameSafe(SourceDisplayCase),
				*CarryEntry.ArtifactId.ToString()),
			EHeistDebugLevel::Warning);
		return;
	}

	UHeistDebugFunctionLibrary::Message(
		this,
		FString::Printf(
			TEXT("Original drop request accepted: Case=%s Artifact=%s PlayerId=%d ReleasedWeight=%.1f PreviousWeight=%.1f TotalWeight=%.1f RestoredState=%s Policy=ReturnToSourceCase Authority=true Result=PASS"),
			*GetNameSafe(SourceDisplayCase),
			*CarryEntry.ArtifactId.ToString(),
			RequestContext.PlayerState->HeistPlayerId,
			CarryEntry.Weight,
			PreviousWeight,
			RequestContext.PlayerState->GetTotalLootWeight(),
			*UEnum::GetValueAsString(SourceDisplayCase->GetDisplayCaseState())));
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
	const float Distance = FVector::Distance(
		RequestContext.Character->GetActorLocation(),
		TargetVentActor->GetActorLocation());

	if (!InteractionComponent->IsActorWithinInteractionRange(TargetVentActor))
	{
		LogEscapeRequestRejected(TargetVentActor, TEXT("OutOfRange"), Distance);
		return;
	}

	if (!TargetVentActor->CanUseVent(RequestContext.Character))
	{
		LogEscapeRequestRejected(TargetVentActor, TEXT("VentUnavailable"), Distance);
		return;
	}

	InteractionComponent->RefreshInteractionTarget(true);
	if (InteractionComponent->GetCurrentInteractionTarget() != TargetVentActor)
	{
		LogEscapeRequestRejected(TargetVentActor, TEXT("NotCurrentTarget"), Distance);
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

	UHeistDebugFunctionLibrary::DebugEscapeRequestAccepted(
		this,
		RequestContext.Character,
		TargetVentActor,
		Distance);
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

	if (bInventoryOpen
		&& RequestContext.Character->GetActionComponent()->IsGameplayCastActive())
	{
		LogInventoryRequestRejected(TEXT("SetOpen"), INDEX_NONE, TEXT("GameplayStateBlocked"));
		return;
	}

	if (!RequestContext.InventoryComponent->TrySetInventoryOpen(bInventoryOpen))
	{
		LogInventoryRequestRejected(TEXT("SetOpen"), INDEX_NONE, TEXT("MutationRejected"));
	}
}

void AHeistPlayerController::Server_RequestMoveInventoryItem_Implementation(
	const int32 InstanceId,
	const FIntPoint TargetGridPosition)
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

	AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	FHeistItemDataRow ItemDefinition;
	FHeistLootDataRow LootDefinition;
	if (!IsValid(HeistGameMode)
		|| !HeistGameMode->TryGetItemDefinition(InventoryItem.ItemId, ItemDefinition)
		|| ItemDefinition.ItemType != EHeistItemType::Loot
		|| !HeistGameMode->TryGetLootDefinition(InventoryItem.ItemId, LootDefinition)
		|| !RequestContext.PlayerState->CanRemoveLootScoreAndWeight(LootDefinition.ScoreValue, ItemDefinition.Weight))
	{
		LogInventoryRequestRejected(TEXT("Drop"), InstanceId, TEXT("InvalidLootState"));
		return;
	}

	FHeistLootDropRequest DropRequest;
	DropRequest.DroppedBy = RequestContext.Character;
	DropRequest.ItemId = InventoryItem.ItemId;
	DropRequest.SourceInstanceId = InstanceId;
	DropRequest.DropOrigin = RequestContext.Character->GetActorLocation()
		+ RequestContext.Character->GetActorForwardVector() * 100.0f;

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
	checkf(
		RequestContext.PlayerState->RemoveLootScoreAndWeight(LootDefinition.ScoreValue, ItemDefinition.Weight),
		TEXT("Validated loot score and weight removal must succeed after inventory commit."));

	UHeistDebugFunctionLibrary::DebugInventoryDropAccepted(
		this,
		RequestContext.Character,
		DropRequest.ItemId,
		InstanceId,
		DroppedLootActor,
		FVector(DropRequest.DropOrigin));
}

void AHeistPlayerController::Server_RequestAssignQuickSlot_Implementation(
	const EHeistQuickSlotType SlotType,
	const int32 InstanceId)
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

void AHeistPlayerController::Server_RequestUseQuickSlot_Implementation(
	const EHeistQuickSlotType SlotType,
	const FVector TargetWorldLocation)
{
	if (SlotType != EHeistQuickSlotType::Coin)
	{
		LogThrowableUseRejected(SlotType, NAME_None, TEXT("LegacyToolDisabled"));
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
	int32 SourceInstanceId = INDEX_NONE;
	if (!TryResolveQuickSlotItem(RequestContext, SlotType, ItemId, SourceInstanceId, RejectReason))
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

	if (UsableItemDefinition.UseType == EHeistUseType::Throw)
	{
		AHeistThrowableProjectile* SpawnedProjectile = nullptr;
		if (!TrySpawnThrowableProjectile(
			RequestContext,
			ItemId,
			TargetWorldLocation,
			false,
			SpawnedProjectile,
			RejectReason))
		{
			LogThrowableUseRejected(SlotType, ItemId, RejectReason);
		}
		return;
	}

	if (UsableItemDefinition.UseType == EHeistUseType::PlaceTrap)
	{
		if (!TryBeginTrapPlacement(
			RequestContext,
			ItemId,
			SourceInstanceId,
			TargetWorldLocation,
			false,
			RejectReason))
		{
			LogThrowableUseRejected(SlotType, ItemId, RejectReason);
		}
		return;
	}

	LogThrowableUseRejected(SlotType, ItemId, TEXT("UnsupportedUseType"));
}

void AHeistPlayerController::Server_UpdateFlashlightAimDirection_Implementation(
	const FVector_NetQuantizeNormal ClientCameraForward)
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
		LogInventoryRequestRejected(
			TEXT("DebugAddItem"),
			INDEX_NONE,
			AddRejectReason != nullptr ? AddRejectReason : TEXT("AddRejected"));
	}
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
	if (!TrySpawnThrowableProjectile(
		RequestContext,
		FName(TEXT("Throwable_Coin")),
		TargetWorldLocation,
		true,
		SpawnedProjectile,
		RejectReason))
	{
		LogThrowableUseRejected(EHeistQuickSlotType::Coin, FName(TEXT("Throwable_Coin")), RejectReason);
	}
}

void AHeistPlayerController::Server_DebugRequestSpawnGuard_Implementation(
	const float Distance)
{
#if !UE_BUILD_SHIPPING
	APawn* RequestingPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!IsValid(RequestingPawn) || !IsValid(World))
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			TEXT("Guard debug spawn rejected: missing pawn or world."),
			EHeistDebugLevel::Warning);
		return;
	}

	const float SafeDistance = FMath::Clamp(Distance, 100.0f, 3000.0f);
	const FVector DebugGuardSpawnLocation =
		RequestingPawn->GetActorLocation()
		+ RequestingPawn->GetActorForwardVector() * SafeDistance;
	const FTransform SpawnTransform(
		(-RequestingPawn->GetActorForwardVector()).Rotation(),
		DebugGuardSpawnLocation);
	AHeistGuardCharacter* SpawnedGuard =
		World->SpawnActorDeferred<AHeistGuardCharacter>(
			AHeistGuardCharacter::StaticClass(),
			SpawnTransform,
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!IsValid(SpawnedGuard))
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			TEXT("Guard debug spawn rejected: deferred spawn failed."),
			EHeistDebugLevel::Warning);
		return;
	}

	SpawnedGuard->FinishSpawning(SpawnTransform);
	UHeistDebugFunctionLibrary::DebugDrawGuardSpawnMarker(this, SpawnedGuard);
	UHeistDebugFunctionLibrary::Message(
		this,
		FString::Printf(
			TEXT("Guard debug spawned: Guard=%s Location=(%.1f,%.1f,%.1f)"),
			*GetNameSafe(SpawnedGuard),
			SpawnedGuard->GetActorLocation().X,
			SpawnedGuard->GetActorLocation().Y,
			SpawnedGuard->GetActorLocation().Z));
#endif
}

void AHeistPlayerController::Server_DebugRequestSetNearestGuardState_Implementation(
	const EHeistGuardState RequestedGuardState,
	const float DurationSeconds)
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
		UHeistDebugFunctionLibrary::Message(
			this,
			TEXT("Guard state debug request rejected: no Guard exists."),
			EHeistDebugLevel::Warning);
		return;
	}

	UHeistGuardStateComponent* GuardStateComponent =
		NearestGuard->GetGuardStateComponent();
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
		GuardStateComponent->EnterInvestigateNoise(
			RequestingPawn->GetActorLocation(),
			SafeDuration);
		break;
	case EHeistGuardState::ChasePlayer:
		GuardStateComponent->EnterChasePlayer(RequestingPawn);
		break;
	case EHeistGuardState::SearchLastKnownLocation:
		GuardStateComponent->EnterSearchLastKnownLocation(
			RequestingPawn->GetActorLocation());
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
	AHeistGuardAIController* GuardAIController =
		IsValid(NearestGuard)
			? Cast<AHeistGuardAIController>(NearestGuard->GetController())
			: nullptr;
	if (!IsValid(GuardAIController) || !IsValid(GetPawn()))
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			TEXT("Guard sight debug request rejected: missing Guard AIController or player pawn."),
			EHeistDebugLevel::Warning);
		return;
	}

	GuardAIController->DebugEvaluateSightTarget(GetPawn());
#endif
}

void AHeistPlayerController::Server_DebugRequestSetNearestGuardAutomaticSight_Implementation(
	const bool bEnabled)
{
#if !UE_BUILD_SHIPPING
	AHeistGuardCharacter* NearestGuard = FindNearestGuard();
	AHeistGuardAIController* GuardAIController =
		IsValid(NearestGuard)
			? Cast<AHeistGuardAIController>(NearestGuard->GetController())
			: nullptr;
	if (!IsValid(GuardAIController))
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			TEXT("Guard automatic sight debug request rejected: missing Guard AIController."),
			EHeistDebugLevel::Warning);
		return;
	}

	GuardAIController->SetAutomaticSightEnabled(bEnabled);
	UHeistDebugFunctionLibrary::Message(
		this,
		FString::Printf(
			TEXT("Guard automatic sight changed: Guard=%s Enabled=%s"),
			*GetNameSafe(NearestGuard),
			GuardAIController->IsAutomaticSightEnabled()
				? TEXT("true")
				: TEXT("false")),
		EHeistDebugLevel::Info);
#endif
}

void AHeistPlayerController::Server_DebugRequestReportGuardNoise_Implementation(
	const float Distance)
{
#if !UE_BUILD_SHIPPING
	APawn* RequestingPawn = GetPawn();
	AHeistGameMode* HeistGameMode =
		GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	AHeistGameState* HeistGameState =
		GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(RequestingPawn)
		|| !IsValid(HeistGameMode)
		|| !IsValid(HeistGameState))
	{
		return;
	}

	const FName SoundPingId(TEXT("Ping_CoinImpact"));
	FHeistSoundPingDataRow SoundPingDefinition;
	if (!HeistGameMode->TryGetSoundPingDefinition(
		SoundPingId,
		SoundPingDefinition))
	{
		UHeistDebugFunctionLibrary::DebugSoundPingDefinitionRejected(
			this,
			SoundPingId,
			TEXT("MissingSoundPingDataRow"));
		return;
	}

	const float SafeDistance = FMath::Clamp(Distance, 0.0f, 5000.0f);
	FHeistSoundPingEvent SoundPingEvent;
	SoundPingEvent.SoundPingTag = SoundPingDefinition.SoundPingTag;
	SoundPingEvent.PingType = SoundPingDefinition.PingType;
	SoundPingEvent.WorldLocation =
		RequestingPawn->GetActorLocation()
		+ RequestingPawn->GetActorForwardVector() * SafeDistance;
	SoundPingEvent.Radius = FMath::Max(0.0f, SoundPingDefinition.Radius);
	SoundPingEvent.Duration = FMath::Max(0.0f, SoundPingDefinition.Duration);
	SoundPingEvent.bAffectsGuards = SoundPingDefinition.bAffectsGuards;
	SoundPingEvent.bAffectsPlayers = SoundPingDefinition.bAffectsPlayers;
	HeistGameState->ReportSoundPing(SoundPingEvent);
#endif
}

void AHeistPlayerController::Server_DebugRequestDumpDifficultyBaseline_Implementation()
{
#if !UE_BUILD_SHIPPING
	AHeistGameMode* HeistGameMode =
		GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	if (!IsValid(HeistGameMode))
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			TEXT("Difficulty baseline dump failed: missing Heist GameMode."),
			EHeistDebugLevel::Warning);
		return;
	}

	HeistGameMode->DebugDumpPlayerCountDifficultyBaseline();
#endif
}

void AHeistPlayerController::Server_DebugRequestRebuildResults_Implementation()
{
#if !UE_BUILD_SHIPPING
	AHeistGameState* HeistGameState =
		GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistGameState))
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			TEXT("Result debug rebuild rejected: missing Heist GameState."),
			EHeistDebugLevel::Warning);
		return;
	}

	HeistGameState->RebuildPlayerResults();
#endif
}

void AHeistPlayerController::Server_DebugRequestSeedResult_Implementation(
	const int32 Score,
	const bool bEscaped,
	const float EscapeTimeSeconds)
{
#if !UE_BUILD_SHIPPING
	AHeistPlayerState* HeistPlayerState = GetPlayerState<AHeistPlayerState>();
	AHeistGameState* HeistGameState =
		GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistPlayerState) || !IsValid(HeistGameState))
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			TEXT("Result debug seed rejected: missing Heist PlayerState or GameState."),
			EHeistDebugLevel::Warning);
		return;
	}

	HeistPlayerState->DebugSetResultState(
		FMath::Max(0, Score),
		bEscaped,
		FMath::Max(0.0f, EscapeTimeSeconds));
	HeistGameState->RebuildPlayerResults();
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

#pragma endregion

#pragma region Feedback

FHeistPopupFeedbackRequested& AHeistPlayerController::GetPopupFeedbackRequestedDelegate()
{
	return PopupFeedbackRequestedDelegate;
}

void AHeistPlayerController::Client_ReceivePopupFeedback_Implementation(
	const FText& Message,
	const float DurationSeconds)
{
	if (Message.IsEmpty())
	{
		return;
	}

	const float SafeDuration = FMath::Max(0.1f, DurationSeconds);
	PopupFeedbackRequestedDelegate.Broadcast(Message, SafeDuration);
	UE_LOG(
		LogHeistUI,
		Log,
		TEXT("[%s] Popup feedback received: Message=%s Duration=%.2f Local=%s"),
		*GetName(),
		*Message.ToString(),
		SafeDuration,
		IsLocalController() ? TEXT("true") : TEXT("false"));
}

void AHeistPlayerController::Server_DebugRequestFeedbackTest_Implementation()
{
#if !UE_BUILD_SHIPPING
	SendPopupFeedback(
		NSLOCTEXT("HeistFeedback", "ServerConfirmedTest", "SERVER CONFIRMED FEEDBACK"),
		3.0f);
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
			LogInventoryRequestRejected(
				TEXT("FeedbackBagFull"),
				AddedInstanceId,
				AddRejectReason != nullptr ? AddRejectReason : TEXT("AddRejected"));
			return;
		}
	}

	LogInventoryRequestRejected(TEXT("FeedbackBagFull"), INDEX_NONE, TEXT("TestLimitReached"));
#endif
}

void AHeistPlayerController::SendPopupFeedback(const FText& Message, const float DurationSeconds)
{
	if (Message.IsEmpty())
	{
		return;
	}

	Client_ReceivePopupFeedback(Message, FMath::Max(0.1f, DurationSeconds));
}

void AHeistPlayerController::SendPopupFeedbackForRejection(
	const TCHAR* RequestName,
	const TCHAR* Reason)
{
	SendPopupFeedback(ResolvePopupFeedbackText(RequestName, Reason));
}

FText AHeistPlayerController::ResolvePopupFeedbackText(const TCHAR* RequestName, const TCHAR* Reason)
{
	const FString Request = RequestName != nullptr ? RequestName : TEXT("Request");
	const FString Rejection = Reason != nullptr ? Reason : TEXT("Rejected");

	if (Rejection == TEXT("InventoryFull"))
	{
		return NSLOCTEXT("HeistFeedback", "BagFull", "BAG FULL");
	}
	if (Rejection == TEXT("AlreadyTaken"))
	{
		return NSLOCTEXT("HeistFeedback", "AlreadyTaken", "LOOT ALREADY TAKEN");
	}
	if (Rejection == TEXT("OutOfRange") || Rejection == TEXT("NotCurrentTarget"))
	{
		return NSLOCTEXT("HeistFeedback", "TooFarAway", "TOO FAR AWAY");
	}
	if (Rejection == TEXT("Stunned"))
	{
		return NSLOCTEXT("HeistFeedback", "BlockedByStun", "ACTION BLOCKED: STUNNED");
	}
	if (Rejection == TEXT("Casting"))
	{
		return NSLOCTEXT("HeistFeedback", "BlockedByCast", "ACTION ALREADY IN PROGRESS");
	}
	if (Rejection == TEXT("InventoryClosed"))
	{
		return NSLOCTEXT("HeistFeedback", "InventoryClosed", "INVENTORY CLOSED");
	}
	if (Rejection == TEXT("InvalidTargetPlacement"))
	{
		return NSLOCTEXT("HeistFeedback", "InvalidPlacement", "INVALID PLACEMENT");
	}
	if (Rejection == TEXT("RotationRejected"))
	{
		return NSLOCTEXT("HeistFeedback", "RotationBlocked", "ROTATION BLOCKED");
	}
	if (Rejection == TEXT("InvalidSlotAssignment") || Rejection == TEXT("InvalidSlot"))
	{
		return NSLOCTEXT("HeistFeedback", "InvalidQuickSlot", "INVALID QUICK SLOT");
	}
	if (Rejection == TEXT("EmptyQuickSlot"))
	{
		return NSLOCTEXT("HeistFeedback", "EmptyQuickSlot", "QUICK SLOT EMPTY");
	}
	if (Rejection == TEXT("EscapePhaseClosed"))
	{
		return NSLOCTEXT("HeistFeedback", "EscapeClosed", "ESCAPE NOT AVAILABLE");
	}
	if (Request.Contains(TEXT("Loot")))
	{
		return NSLOCTEXT("HeistFeedback", "LootRejected", "LOOT REQUEST REJECTED");
	}
	if (Request.Contains(TEXT("Escape")))
	{
		return NSLOCTEXT("HeistFeedback", "EscapeRejected", "ESCAPE REQUEST REJECTED");
	}
	if (Request.Contains(TEXT("Throwable")))
	{
		return NSLOCTEXT("HeistFeedback", "ItemUseRejected", "ITEM USE REJECTED");
	}
	return NSLOCTEXT("HeistFeedback", "RequestRejected", "REQUEST REJECTED");
}

#pragma endregion

#pragma region InternalHelpers

AHeistGuardCharacter* AHeistPlayerController::FindNearestGuard() const
{
	if (!IsValid(GetWorld()))
	{
		return nullptr;
	}

	const APawn* ReferencePawn = GetPawn();
	const FVector ReferenceLocation = IsValid(ReferencePawn)
		? ReferencePawn->GetActorLocation()
		: FVector::ZeroVector;
	AHeistGuardCharacter* NearestGuard = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<AHeistGuardCharacter> GuardIterator(GetWorld());
		GuardIterator;
		++GuardIterator)
	{
		AHeistGuardCharacter* CandidateGuard = *GuardIterator;
		if (!IsValid(CandidateGuard))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(
			ReferenceLocation,
			CandidateGuard->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestGuard = CandidateGuard;
		}
	}

	return NearestGuard;
}

bool AHeistPlayerController::TryBuildGameplayRequestContext(
	FHeistGameplayRequestContext& OutContext,
	const TCHAR*& OutRejectReason) const
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

	OutContext.Character = HeistCharacter;
	OutContext.PlayerState = HeistPlayerState;
	OutContext.InventoryComponent = HeistCharacter->GetInventoryComponent();
	checkf(IsValid(OutContext.InventoryComponent), TEXT("HeistPlayerCharacter requires HeistInventoryComponent"));
	return true;
}

bool AHeistPlayerController::TryBuildInventoryMutationRequestContext(
	FHeistGameplayRequestContext& OutContext,
	const TCHAR*& OutRejectReason) const
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

bool AHeistPlayerController::TryResolveQuickSlotItem(
	const FHeistGameplayRequestContext& RequestContext,
	const EHeistQuickSlotType SlotType,
	FName& OutItemId,
	int32& OutInstanceId,
	const TCHAR*& OutRejectReason) const
{
	OutItemId = NAME_None;
	OutInstanceId = INDEX_NONE;
	OutRejectReason = nullptr;

	const FHeistQuickSlotState* QuickSlot = RequestContext.InventoryComponent->GetQuickSlots().FindByPredicate(
		[SlotType](const FHeistQuickSlotState& ExistingQuickSlot)
		{
			return ExistingQuickSlot.SlotType == SlotType;
		});
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
	OutInstanceId = InventoryItem.InstanceId;
	return true;
}

bool AHeistPlayerController::TrySpawnThrowableProjectile(
	const FHeistGameplayRequestContext& RequestContext,
	const FName ItemId,
	const FVector& TargetWorldLocation,
	const bool bDebugBypassInventory,
	AHeistThrowableProjectile*& OutProjectile,
	const TCHAR*& OutRejectReason) const
{
	OutProjectile = nullptr;
	OutRejectReason = nullptr;

	AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	if (!IsValid(HeistGameMode))
	{
		OutRejectReason = TEXT("MissingAuthGameMode");
		return false;
	}

	const bool bIsDebugFallbackThrowable = bDebugBypassInventory
		&& (ItemId == FName(TEXT("Throwable_Coin")) || ItemId == FName(TEXT("Throwable_Smoke")));

	FHeistItemDataRow ItemDefinition;
	if (!HeistGameMode->TryGetItemDefinition(ItemId, ItemDefinition)
		|| ItemDefinition.ItemType != EHeistItemType::Throwable
		|| !ItemDefinition.bCanUseQuickSlot)
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
	if (!HeistGameMode->TryGetUsableItemDefinition(ItemId, UsableItemDefinition)
		|| UsableItemDefinition.UseType != EHeistUseType::Throw)
	{
		if (!bIsDebugFallbackThrowable)
		{
			OutRejectReason = TEXT("InvalidUsableItem");
			return false;
		}

		UsableItemDefinition.ItemId = ItemId;
		UsableItemDefinition.UseType = EHeistUseType::Throw;
		UsableItemDefinition.TargetType = EHeistTargetType::ActorHit;
		UsableItemDefinition.Duration = ItemId == FName(TEXT("Throwable_Smoke")) ? 5.0f : 3.0f;
		UsableItemDefinition.ProjectileSpeed = 1500.0f;
	}

	UClass* ProjectileClass = UsableItemDefinition.SpawnedActorClass.LoadSynchronous();
	if (!IsValid(ProjectileClass) && ItemId == FName(TEXT("Throwable_Coin")))
	{
		ProjectileClass = AHeistCoinProjectile::StaticClass();
	}
	else if (!IsValid(ProjectileClass) && ItemId == FName(TEXT("Throwable_Smoke")))
	{
		ProjectileClass = AHeistSmokeProjectile::StaticClass();
	}

	if (!IsValid(ProjectileClass) || !ProjectileClass->IsChildOf(AHeistThrowableProjectile::StaticClass()))
	{
		OutRejectReason = TEXT("InvalidProjectileClass");
		return false;
	}

	if (TargetWorldLocation.ContainsNaN()
		|| FVector::DistSquared(RequestContext.Character->GetActorLocation(), TargetWorldLocation) > FMath::Square(10000.0f))
	{
		OutRejectReason = TEXT("InvalidThrowTarget");
		return false;
	}

	const FVector ProjectileSpawnLocation = RequestContext.Character->GetActorLocation()
		+ FVector::UpVector * 50.0f;
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
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	OutProjectile = GetWorld()->SpawnActor<AHeistThrowableProjectile>(
		ProjectileClass,
		SpawnTransform,
		SpawnParameters);
	if (!IsValid(OutProjectile))
	{
		OutRejectReason = TEXT("ProjectileSpawnFailed");
		return false;
	}

	OutProjectile->InitializeThrowable(
		RequestContext.Character,
		ItemId,
		LaunchDirection,
		ProjectileSpeed,
		UsableItemDefinition.Duration);

	UHeistDebugFunctionLibrary::DebugThrowableProjectileSpawned(
		this,
		RequestContext.Character,
		OutProjectile,
		ItemId,
		TargetWorldLocation,
		LaunchDirection,
		ProjectileSpeed,
		bDebugBypassInventory);
	return true;
}

bool AHeistPlayerController::TryBeginTrapPlacement(
	const FHeistGameplayRequestContext& RequestContext,
	const FName ItemId,
	const int32 SourceInstanceId,
	const FVector& TargetWorldLocation,
	const bool bDebugBypassInventory,
	const TCHAR*& OutRejectReason) const
{
	OutRejectReason = nullptr;
	if (TargetWorldLocation.ContainsNaN()
		|| FVector::DistSquared(RequestContext.Character->GetActorLocation(), TargetWorldLocation)
			> FMath::Square(1200.0f))
	{
		OutRejectReason = TEXT("InvalidTrapTarget");
		return false;
	}

	AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	if (!IsValid(HeistGameMode))
	{
		OutRejectReason = TEXT("MissingAuthGameMode");
		return false;
	}

	FHeistItemDataRow ItemDefinition;
	if (!HeistGameMode->TryGetItemDefinition(ItemId, ItemDefinition)
		|| ItemDefinition.ItemType != EHeistItemType::Trap
		|| !ItemDefinition.bCanUseQuickSlot)
	{
		if (!bDebugBypassInventory || ItemId != FName(TEXT("Trap_Glue")))
		{
			OutRejectReason = TEXT("InvalidTrapItem");
			return false;
		}

		ItemDefinition.ItemId = ItemId;
		ItemDefinition.ItemType = EHeistItemType::Trap;
		ItemDefinition.bCanUseQuickSlot = true;
	}

	FHeistUsableItemDataRow UsableItemDefinition;
	if (!HeistGameMode->TryGetUsableItemDefinition(ItemId, UsableItemDefinition)
		|| UsableItemDefinition.UseType != EHeistUseType::PlaceTrap)
	{
		if (!bDebugBypassInventory || ItemId != FName(TEXT("Trap_Glue")))
		{
			OutRejectReason = TEXT("InvalidUsableItem");
			return false;
		}

		UsableItemDefinition.ItemId = ItemId;
		UsableItemDefinition.UseType = EHeistUseType::PlaceTrap;
		UsableItemDefinition.TargetType = EHeistTargetType::WorldLocation;
		UsableItemDefinition.CastTime = 1.5f;
		UsableItemDefinition.Duration = 3.0f;
	}

	UClass* TrapActorClass = UsableItemDefinition.SpawnedActorClass.LoadSynchronous();
	if (!IsValid(TrapActorClass) && ItemId == FName(TEXT("Trap_Glue")))
	{
		TrapActorClass = AHeistGlueTrapActor::StaticClass();
	}

	if (!IsValid(TrapActorClass) || !TrapActorClass->IsChildOf(AHeistTrapActor::StaticClass()))
	{
		OutRejectReason = TEXT("InvalidTrapClass");
		return false;
	}

	const float CastTimeSeconds = FMath::Max(0.0f, UsableItemDefinition.CastTime);
	const float EffectDurationSeconds = FMath::Max(0.0f, UsableItemDefinition.Duration);
	if (!RequestContext.Character->GetActionComponent()->TryBeginTrapPlacementRequest(
		ItemId,
		SourceInstanceId,
		TargetWorldLocation,
		CastTimeSeconds,
		EffectDurationSeconds,
		TrapActorClass,
		!bDebugBypassInventory))
	{
		OutRejectReason = TEXT("TrapPlacementCastRejected");
		return false;
	}

	return true;
}

FName AHeistPlayerController::GetExpectedQuickSlotItemId(const EHeistQuickSlotType SlotType)
{
	switch (SlotType)
	{
	case EHeistQuickSlotType::Coin:
		return FName(TEXT("Throwable_Coin"));
	case EHeistQuickSlotType::SmokeGrenade:
		return FName(TEXT("Throwable_Smoke"));
	case EHeistQuickSlotType::GlueTrap:
		return FName(TEXT("Trap_Glue"));
	default:
		return NAME_None;
	}
}

void AHeistPlayerController::LogLootPickupRejected(
	const AHeistLootActor* TargetLootActor,
	const TCHAR* Reason,
	float Distance)
{
	UHeistDebugFunctionLibrary::DebugLootPickupRequestRejected(this, TargetLootActor, Reason, Distance);
	SendPopupFeedbackForRejection(TEXT("LootPickup"), Reason);
}

void AHeistPlayerController::LogEscapeRequestRejected(
	const AHeistVentActor* TargetVentActor,
	const TCHAR* Reason,
	float Distance)
{
	UHeistDebugFunctionLibrary::DebugEscapeRequestRejected(this, TargetVentActor, Reason, Distance);
	SendPopupFeedbackForRejection(TEXT("Escape"), Reason);
}

void AHeistPlayerController::LogInventoryRequestRejected(
	const TCHAR* RequestName,
	const int32 InstanceId,
	const TCHAR* Reason)
{
	UHeistDebugFunctionLibrary::DebugInventoryRequestRejected(this, RequestName, InstanceId, Reason);
	SendPopupFeedbackForRejection(RequestName, Reason);
}

void AHeistPlayerController::LogThrowableUseRejected(
	const EHeistQuickSlotType SlotType,
	const FName ItemId,
	const TCHAR* Reason)
{
	UHeistDebugFunctionLibrary::DebugThrowableUseRejected(this, SlotType, ItemId, Reason);
	SendPopupFeedbackForRejection(TEXT("ThrowableUse"), Reason);
}

#pragma endregion
