#include "Core/HeistGameMode.h"

#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameState.h"
#include "Core/HeistHUD.h"
#include "Core/HeistLogChannels.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistPlayerState.h"
#include "Data/HeistGameBalanceDataAsset.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Inventory/HeistItemDataTypes.h"
#include "Inventory/HeistInventoryTypes.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "World/Actors/Loot/HeistLootActor.h"
#include "World/Spawn/HeistLootSpawnPoint.h"

#pragma region InternalHelpers

namespace
{
	const FLinearColor VerificationPlayerColors[] =
	{
		FLinearColor::Red,
		FLinearColor::Green,
		FLinearColor::Blue,
		FLinearColor::Yellow
	};
}

#pragma endregion

#pragma region Construction

AHeistGameMode::AHeistGameMode()
{
	PlayerControllerClass = AHeistPlayerController::StaticClass();
	PlayerStateClass = AHeistPlayerState::StaticClass();
	GameStateClass = AHeistGameState::StaticClass();
	HUDClass = AHeistHUD::StaticClass();
	DefaultPawnClass = AHeistPlayerCharacter::StaticClass();
}

#pragma endregion

#pragma region Lifecycle

void AHeistGameMode::StartPlay()
{
	Super::StartPlay();
	ValidateItemDataTable();
	StartEscapePhaseTimer();
	StartRareLootEventTimers();
	StartGapTrackerTimer();
}

void AHeistGameMode::RestartPlayer(AController* NewPlayer)
{
	AHeistPlayerState* HeistPlayerState = NewPlayer ? NewPlayer->GetPlayerState<AHeistPlayerState>() : nullptr;
	if (HeistPlayerState && HeistPlayerState->HeistPlayerId == INDEX_NONE)
	{
		const int32 AssignedPlayerId = NextHeistPlayerId++;
		const int32 ColorIndex = (AssignedPlayerId - 1) % UE_ARRAY_COUNT(VerificationPlayerColors);
		HeistPlayerState->InitializeVerificationIdentity(AssignedPlayerId, VerificationPlayerColors[ColorIndex]);
	}

	Super::RestartPlayer(NewPlayer);
}

#pragma endregion

#pragma region Balance

UDataTable* AHeistGameMode::GetItemDataTable() const
{
	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset)
		? GameBalanceDataAsset.Get()
		: GetDefault<UHeistGameBalanceDataAsset>();

	return ResolvedBalanceData->ItemDataTable.LoadSynchronous();
}

bool AHeistGameMode::TryGetItemDefinition(
	const FName ItemId,
	FHeistItemDataRow& OutItemDefinition) const
{
	OutItemDefinition = FHeistItemDataRow();

	if (ItemId.IsNone())
	{
		UE_LOG(LogHeistInventory, Warning, TEXT("Item definition lookup rejected: Reason=MissingItemId"));
		return false;
	}

	const UDataTable* ItemDataTable = GetItemDataTable();
	if (!IsValid(ItemDataTable))
	{
		UE_LOG(
			LogHeistInventory,
			Warning,
			TEXT("Item definition lookup rejected: ItemId=%s Reason=MissingItemDataTable"),
			*ItemId.ToString());
		return false;
	}

	if (ItemDataTable->GetRowStruct() != FHeistItemDataRow::StaticStruct())
	{
		UE_LOG(
			LogHeistInventory,
			Error,
			TEXT("Item definition lookup rejected: ItemId=%s Reason=InvalidRowStruct Table=%s RowStruct=%s"),
			*ItemId.ToString(),
			*GetNameSafe(ItemDataTable),
			*GetNameSafe(ItemDataTable->GetRowStruct()));
		return false;
	}

	const FHeistItemDataRow* ItemDefinition = ItemDataTable->FindRow<FHeistItemDataRow>(
		ItemId,
		TEXT("AHeistGameMode::TryGetItemDefinition"),
		false);
	if (!ItemDefinition)
	{
		UE_LOG(
			LogHeistInventory,
			Warning,
			TEXT("Item definition lookup rejected: ItemId=%s Reason=MissingRow Table=%s"),
			*ItemId.ToString(),
			*GetNameSafe(ItemDataTable));
		return false;
	}

	if (ItemDefinition->ItemId != ItemId)
	{
		UE_LOG(
			LogHeistInventory,
			Error,
			TEXT("Item definition lookup rejected: RowName=%s RowItemId=%s Reason=RowNameItemIdMismatch"),
			*ItemId.ToString(),
			*ItemDefinition->ItemId.ToString());
		return false;
	}

	if (ItemDefinition->ItemType == EHeistItemType::None
		|| ItemDefinition->GridSize.X <= 0
		|| ItemDefinition->GridSize.Y <= 0
		|| ItemDefinition->Weight < 0.0f)
	{
		UE_LOG(
			LogHeistInventory,
			Error,
			TEXT("Item definition lookup rejected: ItemId=%s Reason=InvalidDefinition Type=%d Grid=%dx%d Weight=%.2f"),
			*ItemId.ToString(),
			static_cast<int32>(ItemDefinition->ItemType),
			ItemDefinition->GridSize.X,
			ItemDefinition->GridSize.Y,
			ItemDefinition->Weight);
		return false;
	}

	OutItemDefinition = *ItemDefinition;
	return true;
}

bool AHeistGameMode::TryGetLootDefinition(
	const FName ItemId,
	FHeistLootDataRow& OutLootDefinition) const
{
	OutLootDefinition = FHeistLootDataRow();
	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset)
		? GameBalanceDataAsset.Get()
		: GetDefault<UHeistGameBalanceDataAsset>();
	const UDataTable* LootDataTable = ResolvedBalanceData->LootDataTable.LoadSynchronous();
	if (!IsValid(LootDataTable) || LootDataTable->GetRowStruct() != FHeistLootDataRow::StaticStruct())
	{
		return false;
	}

	const FHeistLootDataRow* LootDefinition = LootDataTable->FindRow<FHeistLootDataRow>(
		ItemId,
		TEXT("AHeistGameMode::TryGetLootDefinition"),
		false);
	if (LootDefinition == nullptr || LootDefinition->ItemId != ItemId)
	{
		return false;
	}

	OutLootDefinition = *LootDefinition;
	return true;
}

bool AHeistGameMode::TryGetUsableItemDefinition(
	const FName ItemId,
	FHeistUsableItemDataRow& OutUsableItemDefinition) const
{
	OutUsableItemDefinition = FHeistUsableItemDataRow();
	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset)
		? GameBalanceDataAsset.Get()
		: GetDefault<UHeistGameBalanceDataAsset>();
	const UDataTable* UsableItemDataTable = ResolvedBalanceData->UsableItemDataTable.LoadSynchronous();
	if (!IsValid(UsableItemDataTable) || UsableItemDataTable->GetRowStruct() != FHeistUsableItemDataRow::StaticStruct())
	{
		return false;
	}

	const FHeistUsableItemDataRow* UsableItemDefinition = UsableItemDataTable->FindRow<FHeistUsableItemDataRow>(
		ItemId,
		TEXT("AHeistGameMode::TryGetUsableItemDefinition"),
		false);
	if (UsableItemDefinition == nullptr || UsableItemDefinition->ItemId != ItemId)
	{
		return false;
	}

	OutUsableItemDefinition = *UsableItemDefinition;
	return true;
}

bool AHeistGameMode::TrySpawnDroppedLoot(
	const FHeistLootDropRequest& DropRequest,
	AHeistLootActor*& OutDroppedLootActor) const
{
	OutDroppedLootActor = nullptr;
	if (!HasAuthority() || DropRequest.ItemId.IsNone() || !IsValid(DropRequest.DroppedBy))
	{
		return false;
	}

	FHeistItemDataRow ItemDefinition;
	FHeistLootDataRow LootDefinition;
	if (!TryGetItemDefinition(DropRequest.ItemId, ItemDefinition)
		|| ItemDefinition.ItemType != EHeistItemType::Loot
		|| !TryGetLootDefinition(DropRequest.ItemId, LootDefinition))
	{
		return false;
	}

	UClass* LootActorClass = ItemDefinition.WorldLootActorClass.LoadSynchronous();
	if (!IsValid(LootActorClass) || !LootActorClass->IsChildOf(AHeistLootActor::StaticClass()))
	{
		return false;
	}

	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset)
		? GameBalanceDataAsset.Get()
		: GetDefault<UHeistGameBalanceDataAsset>();
	UDataTable* LootDataTable = ResolvedBalanceData->LootDataTable.LoadSynchronous();
	const FTransform SpawnTransform(FRotator::ZeroRotator, FVector(DropRequest.DropOrigin));
	AHeistLootActor* DroppedLootActor = GetWorld()->SpawnActorDeferred<AHeistLootActor>(
		LootActorClass,
		SpawnTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!IsValid(DroppedLootActor))
	{
		return false;
	}

	DroppedLootActor->InitializeLootData(LootDataTable, DropRequest.ItemId);
	OutDroppedLootActor = Cast<AHeistLootActor>(UGameplayStatics::FinishSpawningActor(DroppedLootActor, SpawnTransform));
	return IsValid(OutDroppedLootActor);
}

void AHeistGameMode::ValidateItemDataTable() const
{
	if (!HasAuthority())
	{
		return;
	}

	const UDataTable* ItemDataTable = GetItemDataTable();
	if (!IsValid(ItemDataTable))
	{
		UE_LOG(LogHeistInventory, Error, TEXT("Item data validation completed: Result=FAIL Reason=MissingItemDataTable"));
		return;
	}

	const TArray<FName> RowNames = ItemDataTable->GetRowNames();
	if (RowNames.IsEmpty())
	{
		UE_LOG(
			LogHeistInventory,
			Error,
			TEXT("Item data validation completed: Table=%s TotalRows=0 ValidRows=0 InvalidRows=0 Result=FAIL Reason=EmptyTable"),
			*GetNameSafe(ItemDataTable));
		return;
	}

	int32 ValidRowCount = 0;
	for (const FName RowName : RowNames)
	{
		FHeistItemDataRow ItemDefinition;
		if (TryGetItemDefinition(RowName, ItemDefinition))
		{
			++ValidRowCount;
		}
	}

	const int32 InvalidRowCount = RowNames.Num() - ValidRowCount;
	if (InvalidRowCount > 0)
	{
		UE_LOG(
			LogHeistInventory,
			Error,
			TEXT("Item data validation completed: Table=%s TotalRows=%d ValidRows=%d InvalidRows=%d Result=FAIL"),
			*GetNameSafe(ItemDataTable),
			RowNames.Num(),
			ValidRowCount,
			InvalidRowCount);
		return;
	}

	UE_LOG(
		LogHeistInventory,
		Log,
		TEXT("Item data validation completed: Table=%s TotalRows=%d ValidRows=%d InvalidRows=0 Result=PASS"),
		*GetNameSafe(ItemDataTable),
		RowNames.Num(),
		ValidRowCount);
}

#pragma endregion

#pragma region RareLootEvent

void AHeistGameMode::ForceRareLootEvent(const float WarningDelaySeconds)
{
#if !UE_BUILD_SHIPPING
	if (!HasAuthority())
	{
		return;
	}

	while (TriggeredRareLootEventIndices.Contains(NextForcedRareLootEventIndex))
	{
		++NextForcedRareLootEventIndex;
	}

	const int32 EventIndex = NextForcedRareLootEventIndex;
	const float SafeWarningDelay = FMath::Max(0.0f, WarningDelaySeconds);
	const AHeistGameState* HeistGameState = GetGameState<AHeistGameState>();
	const float SpawnServerTime = IsValid(HeistGameState)
		? HeistGameState->GetServerWorldTimeSeconds() + SafeWarningDelay
		: GetWorld()->GetTimeSeconds() + SafeWarningDelay;
	BeginRareLootWarning(EventIndex, SpawnServerTime);

	if (SafeWarningDelay <= 0.0f)
	{
		TriggerRareLootEvent(EventIndex);
		return;
	}

	FTimerHandle& SpawnTimerHandle = RareLootSpawnTimerHandles.AddDefaulted_GetRef();
	FTimerDelegate SpawnDelegate;
	SpawnDelegate.BindUObject(this, &AHeistGameMode::TriggerRareLootEvent, EventIndex);
	GetWorldTimerManager().SetTimer(SpawnTimerHandle, SpawnDelegate, SafeWarningDelay, false);
#endif
}

void AHeistGameMode::StartRareLootEventTimers()
{
	if (!HasAuthority())
	{
		return;
	}

	const UHeistGameBalanceDataAsset* BalanceData = ResolveGameBalanceData();
	AHeistGameState* HeistGameState = GetGameState<AHeistGameState>();
	if (!IsValid(BalanceData) || !IsValid(HeistGameState))
	{
		UHeistDebugFunctionLibrary::DebugRareLootEventFailed(this, 0, TEXT("MissingBalanceOrGameState"));
		return;
	}

	const float WarningLeadTime = FMath::Max(0.0f, BalanceData->RareLootWarningLeadTime);
	for (int32 EventArrayIndex = 0; EventArrayIndex < BalanceData->RareLootEventTimes.Num(); ++EventArrayIndex)
	{
		const int32 EventIndex = EventArrayIndex + 1;
		const float SpawnDelay = FMath::Max(0.0f, BalanceData->RareLootEventTimes[EventArrayIndex]);
		const float WarningDelay = FMath::Max(0.0f, SpawnDelay - WarningLeadTime);
		const float ScheduledSpawnServerTime = HeistGameState->GetServerWorldTimeSeconds() + SpawnDelay;

		FTimerHandle& WarningTimerHandle = RareLootWarningTimerHandles.AddDefaulted_GetRef();
		FTimerDelegate WarningDelegate;
		WarningDelegate.BindUObject(
			this,
			&AHeistGameMode::BeginRareLootWarning,
			EventIndex,
			ScheduledSpawnServerTime);
		GetWorldTimerManager().SetTimer(WarningTimerHandle, WarningDelegate, WarningDelay, false);

		FTimerHandle& SpawnTimerHandle = RareLootSpawnTimerHandles.AddDefaulted_GetRef();
		FTimerDelegate SpawnDelegate;
		SpawnDelegate.BindUObject(this, &AHeistGameMode::TriggerRareLootEvent, EventIndex);
		GetWorldTimerManager().SetTimer(SpawnTimerHandle, SpawnDelegate, SpawnDelay, false);
	}

	UHeistDebugFunctionLibrary::DebugRareLootTimersStarted(
		this,
		BalanceData->RareLootEventTimes,
		WarningLeadTime);
}

void AHeistGameMode::BeginRareLootWarning(const int32 EventIndex, const float ScheduledSpawnTime)
{
	if (!HasAuthority() || TriggeredRareLootEventIndices.Contains(EventIndex))
	{
		return;
	}

	AHeistGameState* HeistGameState = GetGameState<AHeistGameState>();
	const UHeistGameBalanceDataAsset* BalanceData = ResolveGameBalanceData();
	if (!IsValid(HeistGameState) || !IsValid(BalanceData) || BalanceData->RareLootItemId.IsNone())
	{
		UHeistDebugFunctionLibrary::DebugRareLootEventFailed(this, EventIndex, TEXT("InvalidWarningState"));
		return;
	}

	HeistGameState->BeginRareLootWarning(EventIndex, BalanceData->RareLootItemId, ScheduledSpawnTime);
	UHeistDebugFunctionLibrary::DebugRareLootWarningStarted(
		this,
		EventIndex,
		BalanceData->RareLootItemId,
		ScheduledSpawnTime);
}

void AHeistGameMode::TriggerRareLootEvent(const int32 EventIndex)
{
	if (!HasAuthority() || TriggeredRareLootEventIndices.Contains(EventIndex))
	{
		return;
	}

	AHeistLootActor* RareLootActor = nullptr;
	AHeistLootSpawnPoint* SpawnPoint = nullptr;
	if (!TrySpawnRareLoot(EventIndex, RareLootActor, SpawnPoint))
	{
		if (AHeistGameState* HeistGameState = GetGameState<AHeistGameState>())
		{
			HeistGameState->DeactivateRareLootMarker(EventIndex);
		}
		return;
	}

	TriggeredRareLootEventIndices.Add(EventIndex);
	ActiveRareLootEventIndices.Add(RareLootActor, EventIndex);
	NextForcedRareLootEventIndex = FMath::Max(NextForcedRareLootEventIndex, EventIndex + 1);
	RareLootActor->GetLootPickupCommittedDelegate().AddUObject(
		this,
		&AHeistGameMode::HandleRareLootPickedUp);

	AHeistGameState* HeistGameState = GetGameState<AHeistGameState>();
	const UHeistGameBalanceDataAsset* BalanceData = ResolveGameBalanceData();
	checkf(IsValid(HeistGameState), TEXT("Rare Loot event requires AHeistGameState."));
	checkf(IsValid(BalanceData), TEXT("Rare Loot event requires balance data."));
	HeistGameState->ActivateRareLootMarker(
		EventIndex,
		BalanceData->RareLootItemId,
		RareLootActor->GetActorLocation());
	UHeistDebugFunctionLibrary::DebugRareLootSpawned(
		this,
		EventIndex,
		RareLootActor,
		SpawnPoint,
		BalanceData->RareLootItemId,
		RareLootActor->GetActorLocation());
}

bool AHeistGameMode::TrySpawnRareLoot(
	const int32 EventIndex,
	AHeistLootActor*& OutRareLootActor,
	AHeistLootSpawnPoint*& OutSpawnPoint)
{
	OutRareLootActor = nullptr;
	OutSpawnPoint = nullptr;

	const UHeistGameBalanceDataAsset* BalanceData = ResolveGameBalanceData();
	if (!IsValid(BalanceData) || BalanceData->RareLootItemId.IsNone())
	{
		UHeistDebugFunctionLibrary::DebugRareLootEventFailed(this, EventIndex, TEXT("MissingRareLootConfig"));
		return false;
	}

	FHeistItemDataRow ItemDefinition;
	FHeistLootDataRow LootDefinition;
	if (!TryGetItemDefinition(BalanceData->RareLootItemId, ItemDefinition)
		|| ItemDefinition.ItemType != EHeistItemType::Loot
		|| !TryGetLootDefinition(BalanceData->RareLootItemId, LootDefinition)
		|| LootDefinition.SpawnCategory != EHeistSpawnCategory::RareEvent)
	{
		UHeistDebugFunctionLibrary::DebugRareLootEventFailed(this, EventIndex, TEXT("InvalidRareLootData"));
		return false;
	}

	TArray<AHeistLootSpawnPoint*> CandidateSpawnPoints;
	for (TActorIterator<AHeistLootSpawnPoint> It(GetWorld()); It; ++It)
	{
		AHeistLootSpawnPoint* SpawnPoint = *It;
		if (IsValid(SpawnPoint) && SpawnPoint->CanSpawnCategory(EHeistSpawnCategory::RareEvent))
		{
			CandidateSpawnPoints.Add(SpawnPoint);
		}
	}

	if (CandidateSpawnPoints.IsEmpty())
	{
		UHeistDebugFunctionLibrary::DebugRareLootEventFailed(this, EventIndex, TEXT("NoEmptyRareEventSpawnPoint"));
		return false;
	}

	OutSpawnPoint = CandidateSpawnPoints[FMath::RandRange(0, CandidateSpawnPoints.Num() - 1)];
	UClass* LootActorClass = ItemDefinition.WorldLootActorClass.LoadSynchronous();
	if (!IsValid(LootActorClass) || !LootActorClass->IsChildOf(AHeistLootActor::StaticClass()))
	{
		LootActorClass = AHeistLootActor::StaticClass();
	}

	UDataTable* LootDataTable = BalanceData->LootDataTable.LoadSynchronous();
	if (!IsValid(LootDataTable))
	{
		UHeistDebugFunctionLibrary::DebugRareLootEventFailed(this, EventIndex, TEXT("MissingLootDataTable"));
		return false;
	}

	const FTransform SpawnTransform = OutSpawnPoint->GetActorTransform();
	AHeistLootActor* DeferredLootActor = GetWorld()->SpawnActorDeferred<AHeistLootActor>(
		LootActorClass,
		SpawnTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!IsValid(DeferredLootActor))
	{
		UHeistDebugFunctionLibrary::DebugRareLootEventFailed(this, EventIndex, TEXT("DeferredSpawnFailed"));
		return false;
	}

	DeferredLootActor->InitializeLootData(LootDataTable, BalanceData->RareLootItemId);
	OutRareLootActor = Cast<AHeistLootActor>(
		UGameplayStatics::FinishSpawningActor(DeferredLootActor, SpawnTransform));
	if (!IsValid(OutRareLootActor))
	{
		UHeistDebugFunctionLibrary::DebugRareLootEventFailed(this, EventIndex, TEXT("FinishSpawnFailed"));
		return false;
	}

	return true;
}

void AHeistGameMode::HandleRareLootPickedUp(AHeistLootActor* LootActor, AActor* Requester)
{
	if (!HasAuthority() || !IsValid(LootActor))
	{
		return;
	}

	AHeistGameState* HeistGameState = GetGameState<AHeistGameState>();
	if (!IsValid(HeistGameState))
	{
		return;
	}

	const int32* EventIndexPtr = ActiveRareLootEventIndices.Find(LootActor);
	if (EventIndexPtr == nullptr)
	{
		return;
	}

	const int32 EventIndex = *EventIndexPtr;
	if (HeistGameState->GetRareLootEventState().EventIndex == EventIndex)
	{
		HeistGameState->DeactivateRareLootMarker(EventIndex);
	}
	ActiveRareLootEventIndices.Remove(LootActor);
	LootActor->GetLootPickupCommittedDelegate().RemoveAll(this);
	UHeistDebugFunctionLibrary::DebugRareLootPickedUp(
		this,
		EventIndex,
		LootActor,
		Requester,
		LootActor->GetLootRowId());
}

const UHeistGameBalanceDataAsset* AHeistGameMode::ResolveGameBalanceData() const
{
	return IsValid(GameBalanceDataAsset)
		? GameBalanceDataAsset.Get()
		: GetDefault<UHeistGameBalanceDataAsset>();
}

#pragma endregion

#pragma region GapTracker

void AHeistGameMode::StartGapTrackerTimer()
{
	if (!HasAuthority())
	{
		return;
	}

	const UHeistGameBalanceDataAsset* BalanceData = ResolveGameBalanceData();
	const float UpdateInterval = IsValid(BalanceData)
		? FMath::Max(0.01f, BalanceData->GapTrackerUpdateInterval)
		: 0.1f;
	GetWorldTimerManager().SetTimer(
		GapTrackerUpdateTimerHandle,
		this,
		&AHeistGameMode::RefreshGapTrackerState,
		UpdateInterval,
		true);
	RefreshGapTrackerState();
	UHeistDebugFunctionLibrary::DebugGapTrackerTimerStarted(this, UpdateInterval);
}

void AHeistGameMode::RefreshGapTrackerState()
{
	if (!HasAuthority())
	{
		return;
	}

	AHeistGameState* HeistGameState = GetGameState<AHeistGameState>();
	const UHeistGameBalanceDataAsset* BalanceData = ResolveGameBalanceData();
	if (!IsValid(HeistGameState) || !IsValid(BalanceData))
	{
		return;
	}

	TArray<AHeistPlayerState*> RankedPlayerStates;
	RankedPlayerStates.Reserve(HeistGameState->PlayerArray.Num());
	for (APlayerState* PlayerState : HeistGameState->PlayerArray)
	{
		AHeistPlayerState* HeistPlayerState = Cast<AHeistPlayerState>(PlayerState);
		if (IsValid(HeistPlayerState) && !HeistPlayerState->IsEscaped())
		{
			RankedPlayerStates.Add(HeistPlayerState);
		}
	}

	RankedPlayerStates.Sort([](const AHeistPlayerState& Left, const AHeistPlayerState& Right)
	{
		if (Left.GetTotalLootScore() != Right.GetTotalLootScore())
		{
			return Left.GetTotalLootScore() > Right.GetTotalLootScore();
		}

		return Left.HeistPlayerId < Right.HeistPlayerId;
	});

	const bool bHasLeader = !RankedPlayerStates.IsEmpty();
	const int32 ScoreGap = RankedPlayerStates.Num() >= 2
		? RankedPlayerStates[0]->GetTotalLootScore() - RankedPlayerStates[1]->GetTotalLootScore()
		: 0;
	const bool bScoreThresholdMet = RankedPlayerStates.Num() >= 2
		&& ScoreGap >= FMath::Max(0, BalanceData->GapTrackerScoreThreshold);
	const bool bShouldActivate = bGapTrackerDebugOverride
		? bGapTrackerDebugForcedActive && bHasLeader
		: bScoreThresholdMet;
	AHeistPlayerState* LeaderPlayerState = bShouldActivate ? RankedPlayerStates[0] : nullptr;
	const int32 LeaderPlayerId = IsValid(LeaderPlayerState)
		? LeaderPlayerState->HeistPlayerId
		: INDEX_NONE;

	HeistGameState->SetGapTrackerState(bShouldActivate, LeaderPlayerId);
	UpdateGapTrackerDirections(LeaderPlayerState, RankedPlayerStates);
}

void AHeistGameMode::UpdateGapTrackerDirections(
	AHeistPlayerState* LeaderPlayerState,
	const TArray<AHeistPlayerState*>& RankedPlayerStates)
{
	const AHeistPlayerCharacter* LeaderCharacter = IsValid(LeaderPlayerState)
		? Cast<AHeistPlayerCharacter>(LeaderPlayerState->GetPawn())
		: nullptr;
	const FVector LeaderLocation = IsValid(LeaderCharacter)
		? LeaderCharacter->GetActorLocation()
		: FVector::ZeroVector;

	for (AHeistPlayerState* HeistPlayerState : RankedPlayerStates)
	{
		if (!IsValid(HeistPlayerState)
			|| !IsValid(LeaderCharacter)
			|| HeistPlayerState == LeaderPlayerState)
		{
			if (IsValid(HeistPlayerState))
			{
				HeistPlayerState->SetGapTrackerDirection(FVector::ZeroVector);
			}
			continue;
		}

		const AHeistPlayerCharacter* FollowerCharacter = Cast<AHeistPlayerCharacter>(HeistPlayerState->GetPawn());
		const FVector Direction = IsValid(FollowerCharacter)
			? (LeaderLocation - FollowerCharacter->GetActorLocation()).GetSafeNormal()
			: FVector::ZeroVector;
		HeistPlayerState->SetGapTrackerDirection(Direction);
	}

	AHeistGameState* HeistGameState = GetGameState<AHeistGameState>();
	if (IsValid(HeistGameState))
	{
		for (APlayerState* PlayerState : HeistGameState->PlayerArray)
		{
			AHeistPlayerState* HeistPlayerState = Cast<AHeistPlayerState>(PlayerState);
			if (IsValid(HeistPlayerState) && !RankedPlayerStates.Contains(HeistPlayerState))
			{
				HeistPlayerState->SetGapTrackerDirection(FVector::ZeroVector);
			}
		}
	}
}

void AHeistGameMode::DebugForceGapTracker(const bool bActive)
{
#if !UE_BUILD_SHIPPING
	if (!HasAuthority())
	{
		return;
	}

	bGapTrackerDebugOverride = true;
	bGapTrackerDebugForcedActive = bActive;
	RefreshGapTrackerState();
	UHeistDebugFunctionLibrary::DebugGapTrackerOverrideChanged(this, true, bActive);
#endif
}

void AHeistGameMode::DebugClearGapTrackerOverride()
{
#if !UE_BUILD_SHIPPING
	if (!HasAuthority())
	{
		return;
	}

	bGapTrackerDebugOverride = false;
	bGapTrackerDebugForcedActive = false;
	RefreshGapTrackerState();
	UHeistDebugFunctionLibrary::DebugGapTrackerOverrideChanged(this, false, false);
#endif
}

#pragma endregion

#pragma region EscapePhase

float AHeistGameMode::GetEscapeCastTimeSeconds() const
{
	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset)
		? GameBalanceDataAsset.Get()
		: GetDefault<UHeistGameBalanceDataAsset>();

	return FMath::Max(0.0f, ResolvedBalanceData->EscapeCastTime);
}

void AHeistGameMode::StartEscapePhaseTimer()
{
	if (!HasAuthority())
	{
		return;
	}

	AHeistGameState* HeistGameState = GetGameState<AHeistGameState>();
	if (!IsValid(HeistGameState))
	{
		UE_LOG(LogHeist, Warning, TEXT("Escape phase timer skipped: Reason=MissingGameState"));
		return;
	}

	FTimerManager& TimerManager = GetWorldTimerManager();
	if (TimerManager.IsTimerActive(EscapePhaseTimerHandle) || HeistGameState->IsEscapePhaseOpen())
	{
		return;
	}

	const float EscapePhaseDelaySeconds = ResolveEscapePhaseDelaySeconds();
	HeistGameState->InitializeEscapePhase(EscapePhaseDelaySeconds);

	if (EscapePhaseDelaySeconds <= 0.0f)
	{
		HeistGameState->OpenEscapePhase();
		return;
	}

	TimerManager.SetTimer(
		EscapePhaseTimerHandle,
		this,
		&AHeistGameMode::HandleEscapePhaseTimerElapsed,
		EscapePhaseDelaySeconds,
		false);

	UE_LOG(
		LogHeist,
		Log,
		TEXT("Escape phase timer started: Delay=%.2f BalanceData=%s"),
		EscapePhaseDelaySeconds,
		*GetNameSafe(GameBalanceDataAsset));
}

void AHeistGameMode::HandleEscapePhaseTimerElapsed()
{
	if (AHeistGameState* HeistGameState = GetGameState<AHeistGameState>())
	{
		HeistGameState->OpenEscapePhase();
	}
	else
	{
		UE_LOG(LogHeist, Warning, TEXT("Escape phase open skipped: Reason=MissingGameState"));
	}
}

float AHeistGameMode::ResolveEscapePhaseDelaySeconds() const
{
	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset)
		? GameBalanceDataAsset.Get()
		: GetDefault<UHeistGameBalanceDataAsset>();

	return FMath::Max(0.0f, ResolvedBalanceData->VentUnlockTime);
}

#pragma endregion
