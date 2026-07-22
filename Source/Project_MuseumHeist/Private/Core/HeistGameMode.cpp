#include "Core/HeistGameMode.h"

#include "Character/HeistPlayerCharacter.h"
#include "Character/Components/HeistForgeryComponent.h"
#include "Core/HeistGameState.h"
#include "Core/HeistHUD.h"
#include "Core/HeistLogChannels.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistPlayerState.h"
#include "Data/HeistArtifactDataTypes.h"
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
#include "World/Actors/Loot/HeistPaintingDisplayCaseActor.h"
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
	if (AHeistGameState* HeistGameState = GetGameState<AHeistGameState>())
	{
		HeistGameState->SetMatchPhase(EHeistMatchPhase::InGame);
	}
	InitializeObjectiveFromPlacedTargetCase();
	ValidateItemDataTables();
	StartEscapePhaseTimer();
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

void AHeistGameMode::Logout(AController* Exiting)
{
	AHeistPlayerState* ExitingPlayerState = IsValid(Exiting)
		? Exiting->GetPlayerState<AHeistPlayerState>()
		: nullptr;
	if (HasAuthority() && IsValid(ExitingPlayerState))
	{
		AHeistPlayerCharacter* ExitingCharacter =
			Cast<AHeistPlayerCharacter>(Exiting->GetPawn());
		UHeistForgeryComponent* ForgeryComponent =
			IsValid(ExitingCharacter)
				? ExitingCharacter->GetForgeryComponent()
				: nullptr;
		if (IsValid(ForgeryComponent))
		{
			ForgeryComponent->CancelForgerySession(
				FName(TEXT("OwnerDisconnected")));
		}

		// Keep the case sweep as a safety net for pawn-less disconnects and
		// partially torn-down ownership state.
		for (TActorIterator<AHeistPaintingDisplayCaseActor> DisplayCaseIterator(GetWorld()); DisplayCaseIterator; ++DisplayCaseIterator)
		{
			if (AHeistPaintingDisplayCaseActor* DisplayCase = *DisplayCaseIterator; IsValid(DisplayCase))
			{
				DisplayCase->CancelSessionForOwner(ExitingPlayerState, FName(TEXT("OwnerDisconnected")));
				DisplayCase->ReleaseOriginalForCarrier(
					ExitingPlayerState,
					FName(TEXT("OwnerDisconnected")));
			}
		}
	}

	Super::Logout(Exiting);
}

#pragma endregion

#pragma region Objective

void AHeistGameMode::InitializeObjectiveFromPlacedTargetCase()
{
	if (!HasAuthority())
	{
		return;
	}

	AHeistGameState* HeistGameState = GetGameState<AHeistGameState>();
	if (!IsValid(HeistGameState))
	{
		UE_LOG(
			LogHeist,
			Error,
			TEXT("Objective initialization: ConfiguredTargetCaseId=%s Result=FAIL Reason=MissingGameState"),
			*ObjectiveTargetCaseId.ToString());
		return;
	}

	TArray<AHeistPaintingDisplayCaseActor*> MatchingTargetCases;
	for (TActorIterator<AHeistPaintingDisplayCaseActor> DisplayCaseIterator(GetWorld());
		DisplayCaseIterator;
		++DisplayCaseIterator)
	{
		AHeistPaintingDisplayCaseActor* DisplayCase = *DisplayCaseIterator;
		if (!IsValid(DisplayCase))
		{
			continue;
		}

		const FName DisplayCaseId = DisplayCase->GetDisplayCaseId();
		const bool bMatchesConfiguredId = !ObjectiveTargetCaseId.IsNone()
			&& DisplayCaseId == ObjectiveTargetCaseId;
		const bool bMatchesMapTargetConvention = ObjectiveTargetCaseId.IsNone()
			&& DisplayCaseId.ToString().EndsWith(TEXT("_Target"), ESearchCase::IgnoreCase);
		if (bMatchesConfiguredId || bMatchesMapTargetConvention)
		{
			MatchingTargetCases.Add(DisplayCase);
		}
	}

	if (MatchingTargetCases.Num() != 1)
	{
		UE_LOG(
			LogHeist,
			Error,
			TEXT("Objective initialization: ConfiguredTargetCaseId=%s MatchingCases=%d Result=FAIL Reason=%s"),
			*ObjectiveTargetCaseId.ToString(),
			MatchingTargetCases.Num(),
			MatchingTargetCases.IsEmpty() ? TEXT("MissingTargetCase") : TEXT("DuplicateTargetCaseId"));
		return;
	}

	AHeistPaintingDisplayCaseActor* TargetDisplayCase = MatchingTargetCases[0];
	const FName TargetArtifactId = TargetDisplayCase->GetTargetArtifactId();
	FHeistArtifactDataRow ArtifactDefinition;
	const bool bArtifactValid = TryGetArtifactDefinition(TargetArtifactId, ArtifactDefinition);
	const bool bCaseStateValid =
		TargetDisplayCase->GetDisplayCaseState() == EHeistDisplayCaseState::Secured;
	const bool bObjectiveInitialized = bArtifactValid
		&& bCaseStateValid
		&& HeistGameState->SetObjectiveSnapshot(
			TargetArtifactId,
			TargetDisplayCase->GetDisplayCaseId(),
			EHeistObjectiveState::Available,
			nullptr);

	const FString InitializationMessage = FString::Printf(
		TEXT("Objective initialization: TargetCase=%s CaseId=%s ArtifactId=%s Location=%s CaseState=%s CaseStateValid=%s ArtifactValid=%s ObjectiveState=%s Result=%s"),
		*GetNameSafe(TargetDisplayCase),
		*TargetDisplayCase->GetDisplayCaseId().ToString(),
		*TargetArtifactId.ToString(),
		*TargetDisplayCase->GetActorLocation().ToCompactString(),
		*UEnum::GetValueAsString(TargetDisplayCase->GetDisplayCaseState()),
		bCaseStateValid ? TEXT("true") : TEXT("false"),
		bArtifactValid ? TEXT("true") : TEXT("false"),
		*UEnum::GetValueAsString(HeistGameState->GetObjectiveState()),
		bObjectiveInitialized ? TEXT("PASS") : TEXT("FAIL"));
	if (bObjectiveInitialized)
	{
		UE_LOG(LogHeist, Log, TEXT("%s"), *InitializationMessage);
	}
	else
	{
		UE_LOG(LogHeist, Error, TEXT("%s"), *InitializationMessage);
	}
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

UDataTable* AHeistGameMode::GetArtifactDataTable() const
{
	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset)
		? GameBalanceDataAsset.Get()
		: GetDefault<UHeistGameBalanceDataAsset>();

	return ResolvedBalanceData->ArtifactDataTable.LoadSynchronous();
}

UDataTable* AHeistGameMode::GetForgeryTemplateDataTable() const
{
	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset)
		? GameBalanceDataAsset.Get()
		: GetDefault<UHeistGameBalanceDataAsset>();

	return ResolvedBalanceData->ForgeryTemplateDataTable.LoadSynchronous();
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

bool AHeistGameMode::TryGetArtifactDefinition(
	const FName ArtifactId,
	FHeistArtifactDataRow& OutArtifactDefinition) const
{
	OutArtifactDefinition = FHeistArtifactDataRow();
	if (ArtifactId.IsNone())
	{
		UE_LOG(LogHeist, Warning, TEXT("Artifact definition lookup rejected: Reason=MissingArtifactId"));
		return false;
	}

	const UDataTable* ArtifactDataTable = GetArtifactDataTable();
	if (!IsValid(ArtifactDataTable)
		|| ArtifactDataTable->GetRowStruct() != FHeistArtifactDataRow::StaticStruct())
	{
		UE_LOG(
			LogHeist,
			Error,
			TEXT("Artifact definition lookup rejected: ArtifactId=%s Reason=MissingOrInvalidArtifactDataTable"),
			*ArtifactId.ToString());
		return false;
	}

	const FHeistArtifactDataRow* ArtifactDefinition =
		ArtifactDataTable->FindRow<FHeistArtifactDataRow>(
			ArtifactId,
			TEXT("AHeistGameMode::TryGetArtifactDefinition"),
			false);
	if (ArtifactDefinition == nullptr
		|| ArtifactDefinition->ArtifactId != ArtifactId
		|| ArtifactDefinition->ArtifactValue < 0
		|| !FMath::IsFinite(ArtifactDefinition->Weight)
		|| ArtifactDefinition->Weight < 0.0f)
	{
		UE_LOG(
			LogHeist,
			Error,
			TEXT("Artifact definition lookup rejected: ArtifactId=%s Reason=MissingOrInvalidDefinition"),
			*ArtifactId.ToString());
		return false;
	}

	OutArtifactDefinition = *ArtifactDefinition;
	return true;
}

bool AHeistGameMode::TryGetForgeryTemplateDefinition(
	const FName TemplateId,
	FHeistForgeryTemplateRow& OutTemplateDefinition) const
{
	OutTemplateDefinition = FHeistForgeryTemplateRow();
	if (TemplateId.IsNone())
	{
		UE_LOG(LogHeist, Warning, TEXT("Forgery template lookup rejected: Reason=MissingTemplateId"));
		return false;
	}

	const UDataTable* TemplateDataTable = GetForgeryTemplateDataTable();
	if (!IsValid(TemplateDataTable)
		|| TemplateDataTable->GetRowStruct() != FHeistForgeryTemplateRow::StaticStruct())
	{
		UE_LOG(
			LogHeist,
			Error,
			TEXT("Forgery template lookup rejected: TemplateId=%s Reason=MissingOrInvalidTemplateDataTable"),
			*TemplateId.ToString());
		return false;
	}

	const FHeistForgeryTemplateRow* TemplateDefinition =
		TemplateDataTable->FindRow<FHeistForgeryTemplateRow>(
			TemplateId,
			TEXT("AHeistGameMode::TryGetForgeryTemplateDefinition"),
			false);
	if (TemplateDefinition == nullptr
		|| TemplateDefinition->TemplateId != TemplateId
		|| TemplateDefinition->ReferenceImage.IsNull()
		|| TemplateDefinition->ReferenceMask.IsNull()
		|| TemplateDefinition->ObservationDuration < 0.0f
		|| TemplateDefinition->ForgeryDuration <= 0.0f
		|| TemplateDefinition->StrokeLimit <= 0
		|| TemplateDefinition->BrushSize <= 0.0f)
	{
		UE_LOG(
			LogHeist,
			Error,
			TEXT("Forgery template lookup rejected: TemplateId=%s Reason=MissingOrInvalidDefinition"),
			*TemplateId.ToString());
		return false;
	}

	OutTemplateDefinition = *TemplateDefinition;
	return true;
}

bool AHeistGameMode::TryGetLootDefinition(
	const FName ItemId,
	FHeistLootDataRow& OutLootDefinition) const
{
	OutLootDefinition = FHeistLootDataRow();
	if (ItemId.IsNone())
	{
		return false;
	}

	FHeistItemDataRow ItemDefinition;
	if (!TryGetItemDefinition(ItemId, ItemDefinition)
		|| ItemDefinition.ItemType != EHeistItemType::Loot)
	{
		return false;
	}

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
	if (LootDefinition == nullptr
		|| LootDefinition->ItemId != ItemId
		|| LootDefinition->ScoreValue < 0
		|| LootDefinition->SpawnCategory == EHeistSpawnCategory::None
		|| LootDefinition->SpawnWeight < 0.0f
		|| (ItemDefinition.bAvailableInV1 && LootDefinition->WorldLootActorClass.IsNull()))
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
	if (ItemId.IsNone())
	{
		return false;
	}

	FHeistItemDataRow ItemDefinition;
	if (!TryGetItemDefinition(ItemId, ItemDefinition)
		|| (ItemDefinition.ItemType != EHeistItemType::Trap
			&& ItemDefinition.ItemType != EHeistItemType::Throwable))
	{
		return false;
	}

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
	const bool bUseTypeMatchesItemType = UsableItemDefinition != nullptr
		&& ((ItemDefinition.ItemType == EHeistItemType::Throwable
				&& UsableItemDefinition->UseType == EHeistUseType::Throw)
			|| (ItemDefinition.ItemType == EHeistItemType::Trap
				&& UsableItemDefinition->UseType == EHeistUseType::PlaceTrap));
	if (UsableItemDefinition == nullptr
		|| UsableItemDefinition->ItemId != ItemId
		|| !bUseTypeMatchesItemType
		|| UsableItemDefinition->TargetType == EHeistTargetType::None
		|| UsableItemDefinition->Cooldown < 0.0f
		|| UsableItemDefinition->CastTime < 0.0f
		|| UsableItemDefinition->Duration < 0.0f
		|| UsableItemDefinition->ProjectileSpeed < 0.0f
		|| (ItemDefinition.bAvailableInV1 && UsableItemDefinition->SpawnedActorClass.IsNull()))
	{
		return false;
	}

	OutUsableItemDefinition = *UsableItemDefinition;
	return true;
}

bool AHeistGameMode::TryGetGuardDefinition(
	const FName GuardProfileId,
	FHeistGuardDataRow& OutGuardDefinition) const
{
	OutGuardDefinition = FHeistGuardDataRow();
	if (GuardProfileId.IsNone())
	{
		return false;
	}

	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset)
		? GameBalanceDataAsset.Get()
		: GetDefault<UHeistGameBalanceDataAsset>();
	const UDataTable* GuardDataTable = ResolvedBalanceData->GuardDataTable.LoadSynchronous();
	if (!IsValid(GuardDataTable)
		|| GuardDataTable->GetRowStruct() != FHeistGuardDataRow::StaticStruct())
	{
		return false;
	}

	const FHeistGuardDataRow* GuardDefinition =
		GuardDataTable->FindRow<FHeistGuardDataRow>(
			GuardProfileId,
			TEXT("AHeistGameMode::TryGetGuardDefinition"),
			false);
	if (GuardDefinition == nullptr
		|| GuardDefinition->GuardProfileId != GuardProfileId)
	{
		return false;
	}

	OutGuardDefinition = *GuardDefinition;
	return true;
}

bool AHeistGameMode::TryGetSoundPingDefinition(
	const FName SoundPingId,
	FHeistSoundPingDataRow& OutSoundPingDefinition) const
{
	OutSoundPingDefinition = FHeistSoundPingDataRow();
	if (SoundPingId.IsNone())
	{
		return false;
	}

	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset)
		? GameBalanceDataAsset.Get()
		: GetDefault<UHeistGameBalanceDataAsset>();
	const UDataTable* SoundPingDataTable =
		ResolvedBalanceData->SoundPingDataTable.LoadSynchronous();
	if (!IsValid(SoundPingDataTable)
		|| SoundPingDataTable->GetRowStruct() != FHeistSoundPingDataRow::StaticStruct())
	{
		return false;
	}

	const FHeistSoundPingDataRow* SoundPingDefinition =
		SoundPingDataTable->FindRow<FHeistSoundPingDataRow>(
			SoundPingId,
			TEXT("AHeistGameMode::TryGetSoundPingDefinition"),
			false);
	if (SoundPingDefinition == nullptr
		|| SoundPingDefinition->SoundPingId != SoundPingId)
	{
		return false;
	}

	OutSoundPingDefinition = *SoundPingDefinition;
	return true;
}

bool AHeistGameMode::TryGetPlayerCountDifficultyBaseline(
	const int32 PlayerCount,
	FHeistPlayerCountDifficultyBaseline& OutBaseline) const
{
	const UHeistGameBalanceDataAsset* BalanceData = ResolveGameBalanceData();
	return IsValid(BalanceData)
		&& BalanceData->TryGetPlayerCountDifficultyBaseline(PlayerCount, OutBaseline);
}

void AHeistGameMode::DebugDumpPlayerCountDifficultyBaseline() const
{
#if !UE_BUILD_SHIPPING
	const UHeistGameBalanceDataAsset* BalanceData = ResolveGameBalanceData();
	const AHeistGameState* HeistGameState = GetGameState<AHeistGameState>();
	if (!IsValid(BalanceData) || !IsValid(HeistGameState))
	{
		UE_LOG(LogHeist, Warning, TEXT("Difficulty baseline dump: Result=FAIL Reason=MissingBalanceOrGameState"));
		return;
	}

	bool bValid = BalanceData->bAllowSoloProgression
		&& BalanceData->PlayerCountDifficultyBaselines.Num() == 4;
	TSet<int32> SeenPlayerCounts;
	for (const FHeistPlayerCountDifficultyBaseline& Baseline : BalanceData->PlayerCountDifficultyBaselines)
	{
		const bool bRowValid = Baseline.PlayerCount >= 1
			&& Baseline.PlayerCount <= 4
			&& !SeenPlayerCounts.Contains(Baseline.PlayerCount)
			&& FMath::IsFinite(Baseline.GuardCountMultiplier)
			&& Baseline.GuardCountMultiplier > 0.0f
			&& FMath::IsFinite(Baseline.DetectionMultiplier)
			&& Baseline.DetectionMultiplier > 0.0f
			&& FMath::IsFinite(Baseline.InspectionDurationMultiplier)
			&& Baseline.InspectionDurationMultiplier > 0.0f;
		SeenPlayerCounts.Add(Baseline.PlayerCount);
		bValid = bValid && bRowValid;
		UE_LOG(
			LogHeist,
			Log,
			TEXT("Difficulty baseline row: Players=%d GuardCount=%.2f Detection=%.2f InspectionDuration=%.2f Valid=%s"),
			Baseline.PlayerCount,
			Baseline.GuardCountMultiplier,
			Baseline.DetectionMultiplier,
			Baseline.InspectionDurationMultiplier,
			bRowValid ? TEXT("true") : TEXT("false"));
	}

	const int32 ConnectedPlayerCount = HeistGameState->GetConnectedPlayerCount();
	FHeistPlayerCountDifficultyBaseline ResolvedBaseline;
	const bool bResolved = TryGetPlayerCountDifficultyBaseline(
		ConnectedPlayerCount,
		ResolvedBaseline);
	bValid = bValid && SeenPlayerCounts.Num() == 4 && bResolved;
	const FString Summary = FString::Printf(
		TEXT("Difficulty baseline dump: ConnectedPlayers=%d ResolvedPlayers=%d GuardCount=%.2f Detection=%.2f InspectionDuration=%.2f SoloAllowed=%s MandatoryPlayers=1 Rows=%d Result=%s"),
		ConnectedPlayerCount,
		ResolvedBaseline.PlayerCount,
		ResolvedBaseline.GuardCountMultiplier,
		ResolvedBaseline.DetectionMultiplier,
		ResolvedBaseline.InspectionDurationMultiplier,
		BalanceData->bAllowSoloProgression ? TEXT("true") : TEXT("false"),
		BalanceData->PlayerCountDifficultyBaselines.Num(),
		bValid ? TEXT("PASS") : TEXT("FAIL"));
	if (bValid)
	{
		UE_LOG(LogHeist, Log, TEXT("%s"), *Summary);
	}
	else
	{
		UE_LOG(LogHeist, Warning, TEXT("%s"), *Summary);
	}
#endif
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

	UClass* LootActorClass = LootDefinition.WorldLootActorClass.LoadSynchronous();
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

void AHeistGameMode::ValidateItemDataTables() const
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

	const UHeistGameBalanceDataAsset* BalanceData = ResolveGameBalanceData();
	const UDataTable* LootDataTable = IsValid(BalanceData)
		? BalanceData->LootDataTable.LoadSynchronous()
		: nullptr;
	const UDataTable* UsableItemDataTable = IsValid(BalanceData)
		? BalanceData->UsableItemDataTable.LoadSynchronous()
		: nullptr;
	if (ItemDataTable->GetRowStruct() != FHeistItemDataRow::StaticStruct()
		|| !IsValid(LootDataTable)
		|| LootDataTable->GetRowStruct() != FHeistLootDataRow::StaticStruct()
		|| !IsValid(UsableItemDataTable)
		|| UsableItemDataTable->GetRowStruct() != FHeistUsableItemDataRow::StaticStruct())
	{
		UE_LOG(
			LogHeistInventory,
			Error,
			TEXT("Item data validation completed: Result=FAIL Reason=MissingOrInvalidTableSchema ItemTable=%s LootTable=%s UsableTable=%s"),
			*GetNameSafe(ItemDataTable),
			*GetNameSafe(LootDataTable),
			*GetNameSafe(UsableItemDataTable));
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
		if (!TryGetItemDefinition(RowName, ItemDefinition))
		{
			continue;
		}

		const bool bHasLootExtension = LootDataTable->FindRowUnchecked(RowName) != nullptr;
		const bool bHasUsableExtension = UsableItemDataTable->FindRowUnchecked(RowName) != nullptr;
		bool bValidExtension = false;
		if (ItemDefinition.ItemType == EHeistItemType::Loot)
		{
			FHeistLootDataRow LootDefinition;
			bValidExtension = bHasLootExtension
				&& !bHasUsableExtension
				&& TryGetLootDefinition(RowName, LootDefinition);
		}
		else if (ItemDefinition.ItemType == EHeistItemType::Trap
			|| ItemDefinition.ItemType == EHeistItemType::Throwable)
		{
			FHeistUsableItemDataRow UsableItemDefinition;
			bValidExtension = !bHasLootExtension
				&& bHasUsableExtension
				&& TryGetUsableItemDefinition(RowName, UsableItemDefinition);
		}

		if (bValidExtension)
		{
			++ValidRowCount;
		}
		else
		{
			UE_LOG(
				LogHeistInventory,
				Error,
				TEXT("Item data validation rejected row: ItemId=%s Type=%d HasLootExtension=%s HasUsableExtension=%s"),
				*RowName.ToString(),
				static_cast<int32>(ItemDefinition.ItemType),
				bHasLootExtension ? TEXT("true") : TEXT("false"),
				bHasUsableExtension ? TEXT("true") : TEXT("false"));
		}
	}

	int32 OrphanExtensionCount = 0;
	for (const FName RowName : LootDataTable->GetRowNames())
	{
		if (!RowNames.Contains(RowName))
		{
			++OrphanExtensionCount;
			UE_LOG(
				LogHeistInventory,
				Error,
				TEXT("Item data validation rejected orphan Loot row: ItemId=%s"),
				*RowName.ToString());
		}
	}
	for (const FName RowName : UsableItemDataTable->GetRowNames())
	{
		if (!RowNames.Contains(RowName))
		{
			++OrphanExtensionCount;
			UE_LOG(
				LogHeistInventory,
				Error,
				TEXT("Item data validation rejected orphan Usable row: ItemId=%s"),
				*RowName.ToString());
		}
	}

	const int32 InvalidRowCount = RowNames.Num() - ValidRowCount + OrphanExtensionCount;
	if (InvalidRowCount > 0)
	{
		UE_LOG(
			LogHeistInventory,
			Error,
			TEXT("Item data validation completed: ItemTable=%s LootTable=%s UsableTable=%s TotalItems=%d ValidItems=%d InvalidRows=%d OrphanExtensions=%d Result=FAIL"),
			*GetNameSafe(ItemDataTable),
			*GetNameSafe(LootDataTable),
			*GetNameSafe(UsableItemDataTable),
			RowNames.Num(),
			ValidRowCount,
			InvalidRowCount,
			OrphanExtensionCount);
		return;
	}

	UE_LOG(
		LogHeistInventory,
		Log,
		TEXT("Item data validation completed: ItemTable=%s LootTable=%s UsableTable=%s TotalItems=%d ValidItems=%d InvalidRows=0 OrphanExtensions=0 Result=PASS"),
		*GetNameSafe(ItemDataTable),
		*GetNameSafe(LootDataTable),
		*GetNameSafe(UsableItemDataTable),
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
	UClass* LootActorClass = LootDefinition.WorldLootActorClass.LoadSynchronous();
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
