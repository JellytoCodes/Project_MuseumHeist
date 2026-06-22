#include "Core/HeistPlayerController.h"

#include "Character/Components/HeistActionComponent.h"
#include "Character/Components/HeistInteractionComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerState.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "World/HeistLootActor.h"
#include "World/HeistVentActor.h"

#pragma region Lifecycle

void AHeistPlayerController::BeginPlay()
{
	Super::BeginPlay();

	ConfigureMouseCursorDefaults();
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
		UHeistDebugFunctionLibrary::Message(
			this,
			TEXT("MoveInputAction is not assigned in the PlayerController Blueprint."),
			EHeistDebugLevel::Warning);
	}

	if (InteractInputAction != nullptr)
	{
		EnhancedInputComponent->BindAction(
			InteractInputAction,
			ETriggerEvent::Started,
			this,
			&AHeistPlayerController::HandleInteractPressed);
	}
	else
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			TEXT("InteractInputAction is not assigned in the PlayerController Blueprint."),
			EHeistDebugLevel::Warning);
	}

	if (GameplayInputMappingContext == nullptr)
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			TEXT("GameplayInputMappingContext is not assigned in the PlayerController Blueprint."),
			EHeistDebugLevel::Warning);
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		InputSubsystem->AddMappingContext(GameplayInputMappingContext, 0);
	}
}

#pragma endregion

#pragma region Input

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

	AHeistLootActor* TargetLootActor = Cast<AHeistLootActor>(InteractionComponent->GetCurrentInteractionTarget());
	if (TargetLootActor != nullptr)
	{
		Server_RequestLootPickup(TargetLootActor);
		return;
	}

	AHeistVentActor* TargetVentActor = Cast<AHeistVentActor>(InteractionComponent->GetCurrentInteractionTarget());
	if (TargetVentActor != nullptr)
	{
		Server_RequestEscape(TargetVentActor);
	}
}

#pragma endregion

#pragma region Networking

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

	UHeistDebugFunctionLibrary::Message(
		this,
		FString::Printf(
			TEXT("Loot pickup request received: Character=%s Target=%s"),
			*GetNameSafe(RequestContext.Character),
			*GetNameSafe(TargetLootActor)));

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

	InteractionComponent->RefreshInteractionTarget();
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

	if (!ensureMsgf(
		RequestContext.PlayerState->AddLootScoreAndWeight(ScoreDelta, WeightDelta),
		TEXT("Validated loot score and weight must apply after a successful reservation")))
	{
		LogLootPickupRejected(TargetLootActor, TEXT("ScoreWeightApplicationFailed"), Distance);
		return;
	}

	UHeistDebugFunctionLibrary::Message(
		this,
		FString::Printf(
			TEXT("Loot pickup request accepted: Target=%s Distance=%.1f"),
			*GetNameSafe(TargetLootActor),
			Distance));
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

	InteractionComponent->RefreshInteractionTarget();
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

	UHeistDebugFunctionLibrary::Message(
		this,
		FString::Printf(
			TEXT("Escape request accepted: Character=%s Vent=%s Distance=%.1f State=Casting"),
			*GetNameSafe(RequestContext.Character),
			*GetNameSafe(TargetVentActor),
			Distance));
}

#pragma endregion

#pragma region InternalHelpers

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

	OutContext.Character = HeistCharacter;
	OutContext.PlayerState = HeistPlayerState;
	return true;
}

void AHeistPlayerController::LogLootPickupRejected(
	const AHeistLootActor* TargetLootActor,
	const TCHAR* Reason,
	float Distance) const
{
	const FString DistanceText = Distance >= 0.0f
		? FString::Printf(TEXT(" Distance=%.1f"), Distance)
		: FString();

	UHeistDebugFunctionLibrary::Message(
		this,
		FString::Printf(
			TEXT("Loot pickup request rejected: Target=%s Reason=%s%s"),
			*GetNameSafe(TargetLootActor),
			Reason,
			*DistanceText),
		EHeistDebugLevel::Warning);
}

void AHeistPlayerController::LogEscapeRequestRejected(
	const AHeistVentActor* TargetVentActor,
	const TCHAR* Reason,
	float Distance) const
{
	const FString DistanceText = Distance >= 0.0f
		? FString::Printf(TEXT(" Distance=%.1f"), Distance)
		: FString();

	UHeistDebugFunctionLibrary::Message(
		this,
		FString::Printf(
			TEXT("Escape request rejected: Vent=%s Reason=%s%s"),
			*GetNameSafe(TargetVentActor),
			Reason,
			*DistanceText),
		EHeistDebugLevel::Warning);
}

#pragma endregion

#pragma region Cursor

bool AHeistPlayerController::GetCursorWorldHit(FHitResult& OutHitResult) const
{
	OutHitResult = FHitResult();

	if (!IsLocalController())
	{
		return false;
	}

	return GetHitResultUnderCursor(ECC_Visibility, false, OutHitResult);
}

bool AHeistPlayerController::GetCursorWorldLocation(FVector& OutWorldLocation) const
{
	FHitResult CursorHit;
	if (!GetCursorWorldHit(CursorHit))
	{
		OutWorldLocation = FVector::ZeroVector;
		return false;
	}

	OutWorldLocation = CursorHit.ImpactPoint;
	return true;
}

void AHeistPlayerController::ConfigureMouseCursorDefaults()
{
	if (!IsLocalController())
	{
		return;
	}

	bShowMouseCursor = true;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

#pragma endregion
