#include "Core/HeistPlayerController.h"

#include "Character/Components/HeistInteractionComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistPlayerState.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "World/HeistLootActor.h"

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

	UHeistInteractionComponent* InteractionComponent = HeistCharacter->GetInteractionComponent();
	if (!ensureMsgf(InteractionComponent != nullptr, TEXT("HeistPlayerCharacter requires HeistInteractionComponent")))
	{
		return;
	}

	if (!InteractionComponent->RefreshInteractionTarget())
	{
		return;
	}

	AHeistLootActor* TargetLootActor = Cast<AHeistLootActor>(InteractionComponent->GetCurrentInteractionTarget());
	if (TargetLootActor != nullptr)
	{
		Server_RequestLootPickup(TargetLootActor);
	}
}

#pragma endregion

#pragma region Networking

void AHeistPlayerController::Server_RequestLootPickup_Implementation(AHeistLootActor* TargetLootActor)
{
	if (!HasAuthority())
	{
		LogLootPickupRejected(TargetLootActor, TEXT("InvalidController"));
		return;
	}

	AHeistPlayerCharacter* HeistCharacter = GetPawn<AHeistPlayerCharacter>();
	if (!IsValid(HeistCharacter))
	{
		LogLootPickupRejected(TargetLootActor, TEXT("InvalidCharacter"));
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
			*GetNameSafe(HeistCharacter),
			*GetNameSafe(TargetLootActor)));

	UHeistInteractionComponent* InteractionComponent = HeistCharacter->GetInteractionComponent();
	if (!IsValid(InteractionComponent))
	{
		LogLootPickupRejected(TargetLootActor, TEXT("InvalidInteractionComponent"));
		return;
	}

	const float Distance = FVector::Distance(
		HeistCharacter->GetActorLocation(),
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

	AHeistPlayerState* HeistPlayerState = GetPlayerState<AHeistPlayerState>();
	if (!IsValid(HeistPlayerState))
	{
		LogLootPickupRejected(TargetLootActor, TEXT("InvalidPlayerState"), Distance);
		return;
	}

	const int32 ScoreDelta = TargetLootActor->GetScoreValue();
	const float WeightDelta = TargetLootActor->GetWeightValue();
	if (!HeistPlayerState->CanAddLootScoreAndWeight(ScoreDelta, WeightDelta))
	{
		LogLootPickupRejected(TargetLootActor, TEXT("InvalidLootValues"), Distance);
		return;
	}

	if (!TargetLootActor->TryReserveForPickup(HeistCharacter))
	{
		LogLootPickupRejected(TargetLootActor, TEXT("AlreadyTaken"), Distance);
		return;
	}

	if (!ensureMsgf(
		HeistPlayerState->AddLootScoreAndWeight(ScoreDelta, WeightDelta),
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

#pragma endregion

#pragma region InternalHelpers

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
