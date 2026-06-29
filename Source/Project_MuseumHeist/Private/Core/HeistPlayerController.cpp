#include "Core/HeistPlayerController.h"

#include "AI/HeistGuardAIController.h"
#include "AI/HeistGuardCharacter.h"
#include "AI/HeistGuardStateComponent.h"
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
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Inventory/HeistItemDataTypes.h"
#include "Inventory/HeistInventoryTypes.h"
#include "Kismet/GameplayStatics.h"
#include "World/Actors/Escape/HeistVentActor.h"
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

void AHeistPlayerController::RequestUseQuickSlotAtWorldLocation(
	const EHeistQuickSlotType SlotType,
	const FVector TargetWorldLocation)
{
	Server_RequestUseQuickSlotAtWorldLocation(SlotType, TargetWorldLocation);
}

void AHeistPlayerController::DebugRequestThrowCoinAtWorldLocation(const FVector TargetWorldLocation)
{
	Server_DebugRequestThrowCoinAtWorldLocation(TargetWorldLocation);
}

void AHeistPlayerController::DebugRequestThrowSmokeAtWorldLocation(const FVector TargetWorldLocation)
{
	Server_DebugRequestThrowSmokeAtWorldLocation(TargetWorldLocation);
}

void AHeistPlayerController::DebugRequestPlaceGlueTrapAtWorldLocation(const FVector TargetWorldLocation)
{
	Server_DebugRequestPlaceGlueTrapAtWorldLocation(TargetWorldLocation);
}

void AHeistPlayerController::DebugRequestForceRareLootEvent(const float WarningDelaySeconds)
{
	Server_DebugRequestForceRareLootEvent(WarningDelaySeconds);
}

void AHeistPlayerController::DebugRequestSetGapTrackerScore(const int32 Score)
{
	Server_DebugRequestSetGapTrackerScore(Score);
}

void AHeistPlayerController::DebugRequestForceGapTracker(const bool bActive)
{
	Server_DebugRequestForceGapTracker(bActive);
}

void AHeistPlayerController::DebugRequestClearGapTrackerOverride()
{
	Server_DebugRequestClearGapTrackerOverride();
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

void AHeistPlayerController::Server_RequestUseQuickSlotAtWorldLocation_Implementation(
	const EHeistQuickSlotType SlotType,
	const FVector TargetWorldLocation)
{
	FHeistGameplayRequestContext RequestContext;
	const TCHAR* RejectReason = nullptr;
	if (!TryBuildGameplayRequestContext(RequestContext, RejectReason))
	{
		LogThrowableUseRejected(SlotType, NAME_None, RejectReason);
		return;
	}

	if (RequestContext.Character->GetStatusComponent()->IsStunned())
	{
		LogThrowableUseRejected(SlotType, NAME_None, TEXT("Stunned"));
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

void AHeistPlayerController::Server_DebugRequestThrowCoinAtWorldLocation_Implementation(const FVector TargetWorldLocation)
{
	FHeistGameplayRequestContext RequestContext;
	const TCHAR* RejectReason = nullptr;
	if (!TryBuildGameplayRequestContext(RequestContext, RejectReason))
	{
		LogThrowableUseRejected(EHeistQuickSlotType::Coin, FName(TEXT("Throwable_Coin")), RejectReason);
		return;
	}

	if (RequestContext.Character->GetStatusComponent()->IsStunned())
	{
		LogThrowableUseRejected(EHeistQuickSlotType::Coin, FName(TEXT("Throwable_Coin")), TEXT("Stunned"));
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

void AHeistPlayerController::Server_DebugRequestThrowSmokeAtWorldLocation_Implementation(const FVector TargetWorldLocation)
{
	const FName DebugSmokeItemId(TEXT("Throwable_Smoke"));

	FHeistGameplayRequestContext RequestContext;
	const TCHAR* RejectReason = nullptr;
	if (!TryBuildGameplayRequestContext(RequestContext, RejectReason))
	{
		LogThrowableUseRejected(EHeistQuickSlotType::SmokeGrenade, DebugSmokeItemId, RejectReason);
		return;
	}

	if (RequestContext.Character->GetStatusComponent()->IsStunned())
	{
		LogThrowableUseRejected(EHeistQuickSlotType::SmokeGrenade, DebugSmokeItemId, TEXT("Stunned"));
		return;
	}

	if (RequestContext.Character->GetActionComponent()->IsGameplayCastActive())
	{
		LogThrowableUseRejected(EHeistQuickSlotType::SmokeGrenade, DebugSmokeItemId, TEXT("Casting"));
		return;
	}

	AHeistThrowableProjectile* SpawnedProjectile = nullptr;
	if (!TrySpawnThrowableProjectile(
		RequestContext,
		DebugSmokeItemId,
		TargetWorldLocation,
		true,
		SpawnedProjectile,
		RejectReason))
	{
		LogThrowableUseRejected(EHeistQuickSlotType::SmokeGrenade, DebugSmokeItemId, RejectReason);
	}
}

void AHeistPlayerController::Server_DebugRequestPlaceGlueTrapAtWorldLocation_Implementation(const FVector TargetWorldLocation)
{
	FHeistGameplayRequestContext RequestContext;
	const TCHAR* RejectReason = nullptr;
	if (!TryBuildGameplayRequestContext(RequestContext, RejectReason))
	{
		LogThrowableUseRejected(EHeistQuickSlotType::GlueTrap, FName(TEXT("Trap_Glue")), RejectReason);
		return;
	}

	if (RequestContext.Character->GetStatusComponent()->IsStunned())
	{
		LogThrowableUseRejected(EHeistQuickSlotType::GlueTrap, FName(TEXT("Trap_Glue")), TEXT("Stunned"));
		return;
	}

	if (RequestContext.Character->GetActionComponent()->IsGameplayCastActive())
	{
		LogThrowableUseRejected(EHeistQuickSlotType::GlueTrap, FName(TEXT("Trap_Glue")), TEXT("Casting"));
		return;
	}

	if (!TryBeginTrapPlacement(
		RequestContext,
		FName(TEXT("Trap_Glue")),
		INDEX_NONE,
		TargetWorldLocation,
		true,
		RejectReason))
	{
		LogThrowableUseRejected(EHeistQuickSlotType::GlueTrap, FName(TEXT("Trap_Glue")), RejectReason);
	}
}

void AHeistPlayerController::Server_DebugRequestForceRareLootEvent_Implementation(const float WarningDelaySeconds)
{
#if !UE_BUILD_SHIPPING
	AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	if (!IsValid(HeistGameMode))
	{
		UHeistDebugFunctionLibrary::DebugRareLootEventFailed(this, 0, TEXT("MissingAuthGameMode"));
		return;
	}

	HeistGameMode->ForceRareLootEvent(FMath::Max(0.0f, WarningDelaySeconds));
#endif
}

void AHeistPlayerController::Server_DebugRequestSetGapTrackerScore_Implementation(const int32 Score)
{
#if !UE_BUILD_SHIPPING
	AHeistPlayerState* HeistPlayerState = GetPlayerState<AHeistPlayerState>();
	AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	if (!IsValid(HeistPlayerState) || !IsValid(HeistGameMode))
	{
		return;
	}

	HeistPlayerState->DebugSetTotalLootScore(FMath::Max(0, Score));
	HeistGameMode->RefreshGapTrackerState();
#endif
}

void AHeistPlayerController::Server_DebugRequestForceGapTracker_Implementation(const bool bActive)
{
#if !UE_BUILD_SHIPPING
	if (AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr)
	{
		HeistGameMode->DebugForceGapTracker(bActive);
	}
#endif
}

void AHeistPlayerController::Server_DebugRequestClearGapTrackerOverride_Implementation()
{
#if !UE_BUILD_SHIPPING
	if (AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr)
	{
		HeistGameMode->DebugClearGapTrackerOverride();
	}
#endif
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
	case EHeistGuardState::Stunned:
		GuardStateComponent->ApplyStun(SafeDuration);
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

	const FVector ProjectileSpawnLocation = RequestContext.Character->GetActorLocation()
		+ RequestContext.Character->GetActorForwardVector() * 80.0f
		+ FVector::UpVector * 40.0f;
	FVector LaunchDirection = TargetWorldLocation - ProjectileSpawnLocation;
	if (!LaunchDirection.Normalize())
	{
		LaunchDirection = RequestContext.Character->GetActorForwardVector().GetSafeNormal2D();
	}

	if (LaunchDirection.IsNearlyZero())
	{
		OutRejectReason = TEXT("InvalidThrowDirection");
		return false;
	}

	const float ProjectileSpeed = FMath::Max(1.0f, UsableItemDefinition.ProjectileSpeed);
	const FTransform SpawnTransform(LaunchDirection.Rotation(), ProjectileSpawnLocation);
	AHeistThrowableProjectile* Projectile = GetWorld()->SpawnActorDeferred<AHeistThrowableProjectile>(
		ProjectileClass,
		SpawnTransform,
		RequestContext.Character,
		RequestContext.Character,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!IsValid(Projectile))
	{
		OutRejectReason = TEXT("ProjectileSpawnFailed");
		return false;
	}

	Projectile->InitializeThrowable(
		RequestContext.Character,
		ItemId,
		LaunchDirection,
		ProjectileSpeed,
		UsableItemDefinition.Duration);
	OutProjectile = Cast<AHeistThrowableProjectile>(UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform));
	if (!IsValid(OutProjectile))
	{
		OutRejectReason = TEXT("ProjectileFinishSpawnFailed");
		return false;
	}

	UHeistDebugFunctionLibrary::DebugThrowableProjectileSpawned(
		this,
		RequestContext.Character,
		OutProjectile,
		ItemId,
		TargetWorldLocation,
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

void AHeistPlayerController::LogThrowableUseRejected(
	const EHeistQuickSlotType SlotType,
	const FName ItemId,
	const TCHAR* Reason) const
{
	UHeistDebugFunctionLibrary::DebugThrowableUseRejected(this, SlotType, ItemId, Reason);
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
