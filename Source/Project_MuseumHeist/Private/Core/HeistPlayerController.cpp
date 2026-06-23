#include "Core/HeistPlayerController.h"

#include "Character/Components/HeistActionComponent.h"
#include "Character/Components/HeistInteractionComponent.h"
#include "Character/Components/HeistInventoryComponent.h"
#include "Character/Components/HeistStatusComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameState.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistHUD.h"
#include "Core/HeistPlayerState.h"
#if !UE_BUILD_SHIPPING
#include "Debug/HeistCheatManager.h"
#endif
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Inventory/HeistItemDataTypes.h"
#include "World/HeistLootActor.h"
#include "World/HeistVentActor.h"

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
		UHeistDebugFunctionLibrary::DebugMissingInputAsset(this, TEXT("MoveInputAction"));
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

	if (GameplayInputMappingContext == nullptr)
	{
		UHeistDebugFunctionLibrary::DebugMissingInputAsset(this, TEXT("GameplayInputMappingContext"));
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
	if (bRequestOpen)
	{
		AHeistHUD* HeistHUD = GetHUD<AHeistHUD>();
		if (!IsValid(HeistHUD) || !HeistHUD->ShowInventoryScreen())
		{
			UHeistDebugFunctionLibrary::DebugInventoryOpenSkipped(this);
			return;
		}
	}

	RequestSetInventoryOpen(bRequestOpen);
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

void AHeistPlayerController::RequestSetInventoryOpen(const bool bInventoryOpen)
{
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

	if (RequestContext.Character->GetStatusComponent()->IsStunned())
	{
		LogLootPickupRejected(TargetLootActor, TEXT("Stunned"));
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

	int32 AddedInstanceId = INDEX_NONE;
	if (!RequestContext.InventoryComponent->TryAddItem(TargetLootActor->GetLootRowId(), AddedInstanceId))
	{
		TargetLootActor->ReleasePickupReservation(RequestContext.Character);
		LogLootPickupRejected(TargetLootActor, TEXT("InventoryRejected"), Distance);
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

	if (RequestContext.Character->GetStatusComponent()->IsStunned())
	{
		LogEscapeRequestRejected(TargetVentActor, TEXT("Stunned"));
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
		&& (RequestContext.Character->GetStatusComponent()->IsStunned()
			|| RequestContext.Character->GetActionComponent()->IsGameplayCastActive()))
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
	FHeistLootDataRow LootDefinition;
	if (!IsValid(HeistGameMode)
		|| !HeistGameMode->TryGetLootDefinition(InventoryItem.ItemId, LootDefinition)
		|| !RequestContext.PlayerState->CanRemoveLootScoreAndWeight(LootDefinition.ScoreValue, LootDefinition.Weight))
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
		RequestContext.PlayerState->RemoveLootScoreAndWeight(LootDefinition.ScoreValue, LootDefinition.Weight),
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

	if (OutContext.Character->GetStatusComponent()->IsStunned())
	{
		OutRejectReason = TEXT("Stunned");
		return false;
	}

	if (OutContext.Character->GetActionComponent()->IsGameplayCastActive())
	{
		OutRejectReason = TEXT("Casting");
		return false;
	}

	return true;
}

void AHeistPlayerController::LogLootPickupRejected(
	const AHeistLootActor* TargetLootActor,
	const TCHAR* Reason,
	float Distance) const
{
	UHeistDebugFunctionLibrary::DebugLootPickupRequestRejected(this, TargetLootActor, Reason, Distance);
}

void AHeistPlayerController::LogEscapeRequestRejected(
	const AHeistVentActor* TargetVentActor,
	const TCHAR* Reason,
	float Distance) const
{
	UHeistDebugFunctionLibrary::DebugEscapeRequestRejected(this, TargetVentActor, Reason, Distance);
}

void AHeistPlayerController::LogInventoryRequestRejected(
	const TCHAR* RequestName,
	const int32 InstanceId,
	const TCHAR* Reason) const
{
	UHeistDebugFunctionLibrary::DebugInventoryRequestRejected(this, RequestName, InstanceId, Reason);
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
