#include "Core/HeistGameMode.h"

#include "Character/Components/HeistActionComponent.h"
#include "Character/Components/HeistForgeryComponent.h"
#include "Character/Components/HeistInventoryComponent.h"
#include "Character/Components/HeistObjectAssemblyComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameInstance.h"
#include "Core/HeistGameState.h"
#include "Core/HeistHUD.h"
#include "Core/HeistLogChannels.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistPlayerState.h"
#include "Data/HeistArtifactDataTypes.h"
#include "Data/HeistContractDataTypes.h"
#include "Data/HeistGameBalanceDataAsset.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Inventory/HeistItemDataTypes.h"
#include "Inventory/HeistInventoryTypes.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "World/Actors/Loot/HeistLootActor.h"
#include "World/Actors/Loot/HeistObjectDisplayCaseActor.h"
#include "World/Actors/Loot/HeistPaintingDisplayCaseActor.h"
#include "World/Spawn/HeistLootSpawnPoint.h"

#pragma region InternalHelpers

namespace
{
const FLinearColor VerificationPlayerColors[] = {FLinearColor::Red, FLinearColor::Green, FLinearColor::Blue, FLinearColor::Yellow};

int32 FindLowestAvailableHeistPlayerId(const AHeistGameState* HeistGameState, const int32 MaxPlayerSlots)
{
	bool bOccupiedSlots[UE_ARRAY_COUNT(VerificationPlayerColors)] = {};
	const int32 ClampedMaxPlayerSlots = FMath::Clamp(MaxPlayerSlots, 1, UE_ARRAY_COUNT(VerificationPlayerColors));

	if (IsValid(HeistGameState))
	{
		for (const APlayerState* PlayerState : HeistGameState->PlayerArray)
		{
			const AHeistPlayerState* HeistPlayerState = Cast<AHeistPlayerState>(PlayerState);
			if (!IsValid(HeistPlayerState)
				|| HeistPlayerState->HeistPlayerId < 1
				|| HeistPlayerState->HeistPlayerId > ClampedMaxPlayerSlots)
			{
				continue;
			}

			bOccupiedSlots[HeistPlayerState->HeistPlayerId - 1] = true;
		}
	}

	for (int32 SlotIndex = 0; SlotIndex < ClampedMaxPlayerSlots; ++SlotIndex)
	{
		if (!bOccupiedSlots[SlotIndex])
		{
			return SlotIndex + 1;
		}
	}

	return INDEX_NONE;
}

FName ResolveContractMapId(const UObject* WorldContextObject, const AHeistGameState* HeistGameState)
{
	const FString LevelName = UGameplayStatics::GetCurrentLevelName(WorldContextObject, true);
	if (LevelName.StartsWith(TEXT("M01"), ESearchCase::IgnoreCase))
	{
		return FName(TEXT("M01"));
	}
	if (LevelName.StartsWith(TEXT("M02"), ESearchCase::IgnoreCase))
	{
		return FName(TEXT("M02"));
	}
	if (LevelName.StartsWith(TEXT("M03"), ESearchCase::IgnoreCase))
	{
		return FName(TEXT("M03"));
	}

	const FName SelectedMapId = IsValid(HeistGameState) ? HeistGameState->GetSelectedLobbyMapId() : NAME_None;
	return SelectedMapId == FName(TEXT("M01")) || SelectedMapId == FName(TEXT("M02")) || SelectedMapId == FName(TEXT("M03")) ? SelectedMapId : NAME_None;
}

UClass* ResolveWorldLootShellClass(const UHeistGameBalanceDataAsset* BalanceData)
{
	UClass* ResolvedClass = IsValid(BalanceData) ? BalanceData->WorldLootActorClass.LoadSynchronous() : nullptr;
	return IsValid(ResolvedClass) && ResolvedClass->IsChildOf(AHeistLootActor::StaticClass()) ? ResolvedClass : nullptr;
}
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
	bUseSeamlessTravel = true;
}

#pragma endregion

#pragma region Lifecycle

void AHeistGameMode::StartPlay()
{
	Super::StartPlay();
	UHeistGameInstance* HeistGameInstance = Cast<UHeistGameInstance>(GetGameInstance());
	const bool bStartAsTitleMenu = IsValid(HeistGameInstance) && HeistGameInstance->IsCurrentWorldTitleMenu();
	const bool bStartAsOnlineLobby =
		!bStartAsTitleMenu
		&& (UGameplayStatics::HasOption(OptionsString, TEXT("HeistLobby"))
			|| (IsValid(HeistGameInstance) && HeistGameInstance->IsCurrentWorldLobby()));
	const bool bOnlineSessionActive =
		IsValid(HeistGameInstance) && (HeistGameInstance->IsHostingOnlineSession() || HeistGameInstance->IsJoinedOnlineSession());
	if (AHeistGameState* HeistGameState = GetGameState<AHeistGameState>())
	{
		HeistGameState->GetMatchPhaseChangedDelegate().RemoveAll(this);
		HeistGameState->GetMatchPhaseChangedDelegate().AddUObject(this, &AHeistGameMode::HandleMatchPhaseChanged);
		HeistGameState->SetMatchPhase(bStartAsTitleMenu ? EHeistMatchPhase::None
													  : (bStartAsOnlineLobby ? EHeistMatchPhase::Lobby : EHeistMatchPhase::InGame));
		if (!bStartAsTitleMenu && (bStartAsOnlineLobby || bOnlineSessionActive))
		{
			HeistGameState->InitializeSessionMapSelection(HeistGameInstance->GetSelectedMapId(), HeistGameInstance->IsRandomMapSelection());
		}
	}
	if (bOnlineSessionActive)
	{
		HeistGameInstance->NotifySessionWorldReady();
	}

	if (bStartAsTitleMenu)
	{
		return;
	}

	ValidateItemDataTables();
	if (bStartAsOnlineLobby)
	{
		return;
	}

	bAnyPlayerEscapedThisMatch = false;
	bMatchHadPlayer = IsValid(GetGameState<AHeistGameState>()) && GetGameState<AHeistGameState>()->GetConnectedPlayerCount() > 0;

	InitializeSurfaceTemplateSelection();
	InitializeAlertState();
	InitializeContractFromPlacedTargetCase();
	StartEscapePhaseTimer();
	StartContractDurationTimer();
}

void AHeistGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AHeistGameState* HeistGameState = GetGameState<AHeistGameState>())
	{
		HeistGameState->GetMatchPhaseChangedDelegate().RemoveAll(this);
	}
	ClearMatchScopedTimers();
	ProcessedAlertTriggerIds.Reset();
	bLockdownWorldRestrictionsApplied = false;
	bAnyPlayerEscapedThisMatch = false;
	bMatchHadPlayer = false;
	Super::EndPlay(EndPlayReason);
}

APawn* AHeistGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	const UHeistGameInstance* HeistGameInstance = Cast<UHeistGameInstance>(GetGameInstance());
	if (IsValid(HeistGameInstance) && HeistGameInstance->IsCurrentWorldTitleMenu())
	{
		return nullptr;
	}

	return Super::SpawnDefaultPawnFor_Implementation(NewPlayer, StartSpot);
}

void AHeistGameMode::HandleMatchPhaseChanged(const EHeistMatchPhase PreviousMatchPhase, const EHeistMatchPhase NewMatchPhase)
{
	if (!HasAuthority() || PreviousMatchPhase == NewMatchPhase || NewMatchPhase == EHeistMatchPhase::InGame)
	{
		return;
	}

	const int32 ClearedTimerCount = ClearMatchScopedTimers();
	ScheduledAlertSourceLevel = EHeistAlertLevel::Quiet;
	ScheduledAlertRevision = 0;
	UE_LOG(LogHeistNetwork, Log, TEXT("Match timer cleanup: PreviousPhase=%s NewPhase=%s ClearedTimers=%d RemainingTimers=%d Authority=true Result=%s"),
		   *UEnum::GetValueAsString(PreviousMatchPhase), *UEnum::GetValueAsString(NewMatchPhase), ClearedTimerCount, GetActiveMatchTimerCount(),
		   GetActiveMatchTimerCount() == 0 ? TEXT("PASS") : TEXT("FAIL"));
}

int32 AHeistGameMode::ClearMatchScopedTimers()
{
	FTimerManager& TimerManager = GetWorldTimerManager();
	int32 ClearedTimerCount = 0;
	const auto ClearTimer = [&TimerManager, &ClearedTimerCount](FTimerHandle& TimerHandle)
	{
		if (TimerManager.TimerExists(TimerHandle))
		{
			++ClearedTimerCount;
		}
		TimerManager.ClearTimer(TimerHandle);
		TimerHandle.Invalidate();
	};

	ClearTimer(AlertTransitionTimerHandle);
	ClearTimer(EscapePhaseTimerHandle);
	ClearTimer(ContractDurationTimerHandle);
	for (FTimerHandle& TimerHandle : RareLootWarningTimerHandles)
	{
		ClearTimer(TimerHandle);
	}
	for (FTimerHandle& TimerHandle : RareLootSpawnTimerHandles)
	{
		ClearTimer(TimerHandle);
	}
	RareLootWarningTimerHandles.Reset();
	RareLootSpawnTimerHandles.Reset();
	return ClearedTimerCount;
}

void AHeistGameMode::RestartPlayer(AController* NewPlayer)
{
	const UHeistGameInstance* HeistGameInstance = Cast<UHeistGameInstance>(GetGameInstance());
	if (IsValid(HeistGameInstance) && HeistGameInstance->IsCurrentWorldTitleMenu())
	{
		return;
	}

	AHeistPlayerState* HeistPlayerState = NewPlayer ? NewPlayer->GetPlayerState<AHeistPlayerState>() : nullptr;
	if (HeistPlayerState && HeistPlayerState->HeistPlayerId == INDEX_NONE)
	{
		const int32 MaxPlayerSlots =
			IsValid(HeistGameInstance) ? HeistGameInstance->GetMaxPublicConnections() : UE_ARRAY_COUNT(VerificationPlayerColors);
		const int32 AssignedPlayerId = FindLowestAvailableHeistPlayerId(GetGameState<AHeistGameState>(), MaxPlayerSlots);
		if (AssignedPlayerId != INDEX_NONE)
		{
			HeistPlayerState->InitializeVerificationIdentity(AssignedPlayerId, VerificationPlayerColors[AssignedPlayerId - 1]);
		}
	}

	Super::RestartPlayer(NewPlayer);
	if (HasAuthority() && IsValid(NewPlayer))
	{
		bMatchHadPlayer = true;
	}
}

void AHeistGameMode::Logout(AController* Exiting)
{
	AHeistPlayerState* ExitingPlayerState = IsValid(Exiting) ? Exiting->GetPlayerState<AHeistPlayerState>() : nullptr;
	const bool bExitingPlayerEscaped = IsValid(ExitingPlayerState) && ExitingPlayerState->IsEscaped();
	if (HasAuthority() && IsValid(ExitingPlayerState))
	{
		int32 CancelledActionCount = 0;
		int32 CancelledForgeryCount = 0;
		int32 ClosedInventoryCount = 0;
		int32 ReleasedOriginalCount = 0;
		int32 ClearedCaseLockCount = 0;
		AHeistPlayerCharacter* ExitingCharacter = Cast<AHeistPlayerCharacter>(Exiting->GetPawn());
		UHeistActionComponent* ActionComponent = IsValid(ExitingCharacter) ? ExitingCharacter->GetActionComponent() : nullptr;
		UHeistForgeryComponent* ForgeryComponent = IsValid(ExitingCharacter) ? ExitingCharacter->GetForgeryComponent() : nullptr;
		UHeistObjectAssemblyComponent* ObjectAssemblyComponent = IsValid(ExitingCharacter) ? ExitingCharacter->GetObjectAssemblyComponent() : nullptr;
		UHeistInventoryComponent* InventoryComponent = IsValid(ExitingCharacter) ? ExitingCharacter->GetInventoryComponent() : nullptr;
		if (IsValid(ActionComponent) && ActionComponent->IsGameplayCastActive())
		{
			ActionComponent->CancelGameplayActions(TEXT("OwnerDisconnected"));
			++CancelledActionCount;
		}
		if (IsValid(ForgeryComponent) && (ForgeryComponent->IsSessionActive() || ForgeryComponent->HasPendingReplicaReview()) &&
			ForgeryComponent->CancelForgerySession(FName(TEXT("OwnerDisconnected"))))
		{
			++CancelledForgeryCount;
		}
		if (IsValid(ObjectAssemblyComponent) && (ObjectAssemblyComponent->IsSessionActive() || ObjectAssemblyComponent->HasPendingReplicaReview()) &&
			ObjectAssemblyComponent->CancelAssemblySession(FName(TEXT("OwnerDisconnected"))))
		{
			++CancelledForgeryCount;
		}
		if (IsValid(InventoryComponent) && InventoryComponent->IsInventoryOpen() && InventoryComponent->TrySetInventoryOpen(false))
		{
			++ClosedInventoryCount;
		}

		// Keep the case sweep as a safety net for pawn-less disconnects and
		// partially torn-down ownership state.
		for (TActorIterator<AHeistPaintingDisplayCaseActor> DisplayCaseIterator(GetWorld()); DisplayCaseIterator; ++DisplayCaseIterator)
		{
			if (AHeistPaintingDisplayCaseActor* DisplayCase = *DisplayCaseIterator; IsValid(DisplayCase))
			{
				ClearedCaseLockCount += DisplayCase->CancelSessionForOwner(ExitingPlayerState, FName(TEXT("OwnerDisconnected"))) ? 1 : 0;
				ReleasedOriginalCount += DisplayCase->DropOriginalForCarrier(ExitingPlayerState, FName(TEXT("OwnerDisconnected"))) ? 1 : 0;
			}
		}
		for (TActorIterator<AHeistObjectDisplayCaseActor> DisplayCaseIterator(GetWorld()); DisplayCaseIterator; ++DisplayCaseIterator)
		{
			if (AHeistObjectDisplayCaseActor* DisplayCase = *DisplayCaseIterator; IsValid(DisplayCase))
			{
				ClearedCaseLockCount += DisplayCase->CancelSessionForOwner(ExitingPlayerState, FName(TEXT("OwnerDisconnected"))) ? 1 : 0;
				ReleasedOriginalCount += DisplayCase->DropOriginalForCarrier(ExitingPlayerState, FName(TEXT("OwnerDisconnected"))) ? 1 : 0;
			}
		}
		UHeistDebugFunctionLibrary::DebugOnlineSessionShutdownCleanup(this, FName(TEXT("OwnerDisconnected")), CancelledActionCount, CancelledForgeryCount, ClosedInventoryCount,
																	 ReleasedOriginalCount, ClearedCaseLockCount, 0, true);
	}

	Super::Logout(Exiting);
	if (HasAuthority())
	{
		bAnyPlayerEscapedThisMatch |= bExitingPlayerEscaped;
		TryResolveContractOutcome(FName(TEXT("PlayerDisconnected")));
	}
}

void AHeistGameMode::PrepareForOnlineSessionShutdown(const FName Reason)
{
	if (!HasAuthority())
	{
		UHeistDebugFunctionLibrary::DebugOnlineSessionShutdownCleanup(this, Reason, 0, 0, 0, 0, 0, 0, false);
		return;
	}

	int32 ActiveCaseLockCount = 0;
	int32 ActiveCaseTimerCount = 0;
	int32 ReleasedOriginalCount = 0;
	for (TActorIterator<AHeistPaintingDisplayCaseActor> DisplayCaseIterator(GetWorld()); DisplayCaseIterator; ++DisplayCaseIterator)
	{
		AHeistPaintingDisplayCaseActor* DisplayCase = *DisplayCaseIterator;
		if (!IsValid(DisplayCase))
		{
			continue;
		}

		ActiveCaseLockCount += DisplayCase->IsSessionLocked() ? 1 : 0;
		ActiveCaseTimerCount += DisplayCase->IsInspectionDelayTimerActive() || DisplayCase->IsInspectionClaimActive() ? 1 : 0;
		if (AHeistPlayerState* OriginalCarrier = DisplayCase->GetOriginalCarrier(); IsValid(OriginalCarrier)
			&& DisplayCase->ReleaseOriginalForCarrier(OriginalCarrier, Reason))
		{
			++ReleasedOriginalCount;
		}
	}
	for (TActorIterator<AHeistObjectDisplayCaseActor> DisplayCaseIterator(GetWorld()); DisplayCaseIterator; ++DisplayCaseIterator)
	{
		if (AHeistObjectDisplayCaseActor* DisplayCase = *DisplayCaseIterator; IsValid(DisplayCase))
		{
			ActiveCaseLockCount += DisplayCase->IsSessionLocked() ? 1 : 0;
			ActiveCaseTimerCount += DisplayCase->IsInspectionDelayTimerActive() || DisplayCase->IsInspectionClaimActive() ? 1 : 0;
			if (AHeistPlayerState* OriginalCarrier = DisplayCase->GetOriginalCarrier();
				IsValid(OriginalCarrier) && DisplayCase->ReleaseOriginalForCarrier(OriginalCarrier, Reason))
			{
				++ReleasedOriginalCount;
			}
		}
	}

	int32 CancelledActionCount = 0;
	int32 CancelledForgeryCount = 0;
	int32 ClosedInventoryCount = 0;
	for (TActorIterator<AHeistPlayerCharacter> CharacterIterator(GetWorld()); CharacterIterator; ++CharacterIterator)
	{
		AHeistPlayerCharacter* PlayerCharacter = *CharacterIterator;
		if (!IsValid(PlayerCharacter))
		{
			continue;
		}

		if (UHeistActionComponent* ActionComponent = PlayerCharacter->GetActionComponent(); IsValid(ActionComponent) && ActionComponent->IsGameplayCastActive())
		{
			ActionComponent->CancelGameplayActions(TEXT("OnlineSessionShutdown"));
			++CancelledActionCount;
		}
		if (UHeistForgeryComponent* ForgeryComponent = PlayerCharacter->GetForgeryComponent();
			IsValid(ForgeryComponent) && (ForgeryComponent->IsSessionActive() || ForgeryComponent->HasPendingReplicaReview()) && ForgeryComponent->CancelForgerySession(Reason))
		{
			++CancelledForgeryCount;
		}
		if (UHeistObjectAssemblyComponent* ObjectAssemblyComponent = PlayerCharacter->GetObjectAssemblyComponent();
			IsValid(ObjectAssemblyComponent) && (ObjectAssemblyComponent->IsSessionActive() || ObjectAssemblyComponent->HasPendingReplicaReview()) &&
			ObjectAssemblyComponent->CancelAssemblySession(Reason))
		{
			++CancelledForgeryCount;
		}
		if (UHeistInventoryComponent* InventoryComponent = PlayerCharacter->GetInventoryComponent(); IsValid(InventoryComponent) && InventoryComponent->IsInventoryOpen()
			&& InventoryComponent->TrySetInventoryOpen(false))
		{
			++ClosedInventoryCount;
		}
	}

	const int32 ClearedMatchTimerCount = ClearMatchScopedTimers();
	if (AHeistGameState* HeistGameState = GetGameState<AHeistGameState>())
	{
		HeistGameState->SetMatchPhase(EHeistMatchPhase::End);
	}
	UHeistDebugFunctionLibrary::DebugOnlineSessionShutdownCleanup(this, Reason, CancelledActionCount, CancelledForgeryCount, ClosedInventoryCount, ReleasedOriginalCount,
																 ActiveCaseLockCount, ActiveCaseTimerCount + ClearedMatchTimerCount, true);
}

#pragma endregion

#pragma region ContractOutcome

void AHeistGameMode::NotifyPlayerTerminalStateChanged(AHeistPlayerState* PlayerState, const FName TerminalTrigger)
{
	if (!HasAuthority() || !IsValid(PlayerState) || TerminalTrigger.IsNone())
	{
		return;
	}

	bMatchHadPlayer = true;
	bAnyPlayerEscapedThisMatch |= PlayerState->IsEscaped();
	TryResolveContractOutcome(TerminalTrigger);
}

#if !UE_BUILD_SHIPPING
bool AHeistGameMode::ForceContractOutcomeForDebug(const FName TerminalTrigger, const bool bTreatAsCrewEscaped,
	const bool bTreatAsAllRemainingCrewArrested, const bool bTreatAsAllCrewDisconnected)
{
	return TryResolveContractOutcome(TerminalTrigger, true, bTreatAsCrewEscaped, bTreatAsAllRemainingCrewArrested, bTreatAsAllCrewDisconnected);
}
#endif

void AHeistGameMode::StartContractDurationTimer()
{
	AHeistGameState* HeistGameState = GetGameState<AHeistGameState>();
	if (!HasAuthority() || !IsValid(HeistGameState) || HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame || !HeistGameState->IsContractInitialized())
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Contract duration timer skipped: Authority=%s ContractInitialized=%s Phase=%s Result=PENDING Reason=InvalidRuntimeState"),
			   HasAuthority() ? TEXT("true") : TEXT("false"), IsValid(HeistGameState) && HeistGameState->IsContractInitialized() ? TEXT("true") : TEXT("false"),
			   IsValid(HeistGameState) ? *UEnum::GetValueAsString(HeistGameState->GetMatchPhase()) : TEXT("MissingGameState"));
		return;
	}

	FHeistContractDataRow ContractDefinition;
	FString FailureReason;
	const FName ContractId = HeistGameState->GetContractSnapshot().ContractId;
	if (!TryGetContractDefinition(ContractId, ContractDefinition) || !ContractDefinition.IsRuntimeDefinitionValid(&FailureReason))
	{
		UE_LOG(LogHeistNetwork, Error, TEXT("Contract duration timer skipped: Contract=%s Result=FAIL Reason=%s"), *ContractId.ToString(),
			   FailureReason.IsEmpty() ? TEXT("MissingContractDefinition") : *FailureReason);
		return;
	}

	FTimerManager& TimerManager = GetWorldTimerManager();
	TimerManager.ClearTimer(ContractDurationTimerHandle);
	TimerManager.SetTimer(ContractDurationTimerHandle, this, &AHeistGameMode::HandleContractDurationTimerElapsed, ContractDefinition.MatchDurationSeconds, false);
	UE_LOG(LogHeistNetwork, Log,
		   TEXT("Contract duration timer started: Contract=%s Duration=%.2f DeadlineServerTime=%.2f Authority=true Priority=CommittedEscapeThenLockdownThenMatchTimer Result=PASS"),
		   *ContractId.ToString(), ContractDefinition.MatchDurationSeconds, HeistGameState->GetServerWorldTimeSeconds() + ContractDefinition.MatchDurationSeconds);
}

void AHeistGameMode::HandleContractDurationTimerElapsed()
{
	TryResolveContractOutcome(FName(TEXT("MatchTimerExpired")), true);
}

bool AHeistGameMode::TryResolveContractOutcome(const FName TerminalTrigger, const bool bForceTerminal, const bool bTreatAsCrewEscaped,
	const bool bTreatAsAllRemainingCrewArrested, const bool bTreatAsAllCrewDisconnected)
{
	AHeistGameState* HeistGameState = GetGameState<AHeistGameState>();
	if (!HasAuthority() || !IsValid(HeistGameState) || TerminalTrigger.IsNone() || HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame ||
		!HeistGameState->IsContractInitialized() || HeistGameState->GetContractSnapshot().Outcome != EHeistContractOutcome::None)
	{
		return false;
	}

	const bool bAllCrewResolved = HeistGameState->AreAllCrewMembersResolved();
	const bool bAllCrewDisconnected = bTreatAsAllCrewDisconnected || (bMatchHadPlayer && HeistGameState->GetConnectedPlayerCount() == 0);
	const bool bTerminalTrigger = bForceTerminal || TerminalTrigger == FName(TEXT("Lockdown")) || TerminalTrigger == FName(TEXT("MatchTimerExpired"));
	if (!bTerminalTrigger && !bAllCrewResolved && !bAllCrewDisconnected)
	{
		return false;
	}

	const FHeistContractSnapshot ContractSnapshot = HeistGameState->GetContractSnapshot();
	const bool bAtLeastOneCrewEscaped = bTreatAsCrewEscaped || bAnyPlayerEscapedThisMatch || HeistGameState->GetEscapedCrewCount() > 0;
	const bool bAllRemainingCrewArrested = bTreatAsAllRemainingCrewArrested || HeistGameState->AreAllRemainingCrewMembersArrested();
	const EHeistContractOutcome Outcome = ContractSnapshot.ResolveTerminalOutcome(bAtLeastOneCrewEscaped);
	const FName OutcomeReasonId = HeistContractOutcomeReasons::Resolve(Outcome, ContractSnapshot.bRequiredTargetSecured, bAtLeastOneCrewEscaped,
		bAllRemainingCrewArrested, bAllCrewDisconnected, TerminalTrigger);

	UE_LOG(LogHeistNetwork, Log,
		   TEXT("Contract outcome matrix resolved: Trigger=%s EscapedAtLeastOne=%s AllResolved=%s AllRemainingArrested=%s AllDisconnected=%s RequiredSecured=%s Secured=%d Quota=%d Outcome=%s ReasonId=%s Priority=CommittedEscapeThenLockdownThenMatchTimer Authority=true Result=%s"),
		   *TerminalTrigger.ToString(), bAtLeastOneCrewEscaped ? TEXT("true") : TEXT("false"), bAllCrewResolved ? TEXT("true") : TEXT("false"),
		   bAllRemainingCrewArrested ? TEXT("true") : TEXT("false"), bAllCrewDisconnected ? TEXT("true") : TEXT("false"),
		   ContractSnapshot.bRequiredTargetSecured ? TEXT("true") : TEXT("false"), ContractSnapshot.SecuredValue, ContractSnapshot.LootValueQuota,
		   *UEnum::GetValueAsString(Outcome), *OutcomeReasonId.ToString(), OutcomeReasonId.IsNone() ? TEXT("FAIL") : TEXT("PASS"));

	return !OutcomeReasonId.IsNone() && FinalizeContractOutcome(Outcome, OutcomeReasonId, TerminalTrigger);
}

bool AHeistGameMode::FinalizeContractOutcome(const EHeistContractOutcome Outcome, const FName OutcomeReasonId, const FName TerminalTrigger)
{
	AHeistGameState* HeistGameState = GetGameState<AHeistGameState>();
	FHeistTeamResult TeamResultSnapshot;
	if (!HasAuthority() || !IsValid(HeistGameState) || !BuildTeamResultSnapshot(Outcome, OutcomeReasonId, TeamResultSnapshot) ||
		!HeistGameState->CommitContractOutcome(Outcome, OutcomeReasonId) || !HeistGameState->CommitTeamResult(MoveTemp(TeamResultSnapshot)))
	{
		return false;
	}

	int32 PlayerCount = 0;
	int32 CancelledActionCount = 0;
	int32 CancelledForgeryCount = 0;
	int32 ClosedInventoryCount = 0;
	int32 DisabledMovementCount = 0;
	GetWorldTimerManager().ClearTimer(EscapePhaseTimerHandle);
	HeistGameState->InitializeEscapePhase(HeistGameState->GetEscapePhaseDelaySeconds());
	const FString CancellationReason = TerminalTrigger.ToString();

	for (TActorIterator<AHeistPlayerCharacter> PlayerIterator(GetWorld()); PlayerIterator; ++PlayerIterator)
	{
		AHeistPlayerCharacter* PlayerCharacter = *PlayerIterator;
		if (!IsValid(PlayerCharacter))
		{
			continue;
		}

		++PlayerCount;
		if (UHeistActionComponent* ActionComponent = PlayerCharacter->GetActionComponent(); IsValid(ActionComponent) && ActionComponent->IsGameplayCastActive())
		{
			ActionComponent->CancelGameplayActions(*CancellationReason);
			++CancelledActionCount;
		}
		if (UHeistForgeryComponent* ForgeryComponent = PlayerCharacter->GetForgeryComponent();
			IsValid(ForgeryComponent) && (ForgeryComponent->IsSessionActive() || ForgeryComponent->HasPendingReplicaReview()) &&
			ForgeryComponent->CancelForgerySession(TerminalTrigger))
		{
			++CancelledForgeryCount;
		}
		if (UHeistObjectAssemblyComponent* ObjectAssemblyComponent = PlayerCharacter->GetObjectAssemblyComponent();
			IsValid(ObjectAssemblyComponent) && (ObjectAssemblyComponent->IsSessionActive() || ObjectAssemblyComponent->HasPendingReplicaReview()) &&
			ObjectAssemblyComponent->CancelAssemblySession(TerminalTrigger))
		{
			++CancelledForgeryCount;
		}
		if (UHeistInventoryComponent* InventoryComponent = PlayerCharacter->GetInventoryComponent();
			IsValid(InventoryComponent) && InventoryComponent->IsInventoryOpen() && InventoryComponent->TrySetInventoryOpen(false))
		{
			++ClosedInventoryCount;
		}
		if (const UHeistInventoryComponent* InventoryComponent = PlayerCharacter->GetInventoryComponent(); IsValid(InventoryComponent) && InventoryComponent->IsCarryingOriginal())
		{
			if (AHeistPlayerState* PlayerState = PlayerCharacter->GetPlayerState<AHeistPlayerState>())
			{
				PlayerState->EndOriginalCarryContribution(0);
			}
		}
		if (UCharacterMovementComponent* MovementComponent = PlayerCharacter->GetCharacterMovement(); IsValid(MovementComponent))
		{
			MovementComponent->StopMovementImmediately();
			MovementComponent->DisableMovement();
			++DisabledMovementCount;
		}
	}

	const EHeistObjectiveState TerminalObjectiveState = Outcome == EHeistContractOutcome::Failed ? EHeistObjectiveState::Failed : EHeistObjectiveState::Completed;
	const bool bObjectiveUpdated = HeistGameState->SetObjectiveSnapshot(HeistGameState->GetActiveTargetArtifactId(), HeistGameState->GetActiveTargetCaseId(),
		TerminalObjectiveState, nullptr);
	HeistGameState->RebuildPlayerResults();
	const bool bMatchEnded = HeistGameState->SetMatchPhase(EHeistMatchPhase::End);
	const bool bFinalized = bObjectiveUpdated && bMatchEnded;
	if (TerminalTrigger == FName(TEXT("Lockdown")))
	{
		bLockdownWorldRestrictionsApplied = bFinalized;
	}

	UE_LOG(LogHeistNetwork, Log,
		   TEXT("Contract outcome finalized: Trigger=%s Outcome=%s ReasonId=%s Reason=\"%s\" Players=%d ActionsCancelled=%d ForgeriesCancelled=%d InventoriesClosed=%d MovementsDisabled=%d Objective=%s MatchPhase=%s Authority=true Result=%s"),
		   *TerminalTrigger.ToString(), *UEnum::GetValueAsString(Outcome), *OutcomeReasonId.ToString(), *HeistGameState->GetContractOutcomeReasonText().ToString(), PlayerCount,
		   CancelledActionCount, CancelledForgeryCount, ClosedInventoryCount, DisabledMovementCount, *UEnum::GetValueAsString(HeistGameState->GetObjectiveState()),
		   *UEnum::GetValueAsString(HeistGameState->GetMatchPhase()), bFinalized ? TEXT("PASS") : TEXT("FAIL"));
	return bFinalized;
}

bool AHeistGameMode::BuildTeamResultSnapshot(const EHeistContractOutcome Outcome, const FName OutcomeReasonId, FHeistTeamResult& OutTeamResult) const
{
	OutTeamResult = FHeistTeamResult();
	const AHeistGameState* HeistGameState = GetGameState<AHeistGameState>();
	const UHeistGameBalanceDataAsset* BalanceData = ResolveGameBalanceData();
	if (!HasAuthority() || !IsValid(HeistGameState) || !IsValid(BalanceData) || Outcome == EHeistContractOutcome::None || OutcomeReasonId.IsNone())
	{
		return false;
	}

	const FHeistContractSnapshot ContractSnapshot = HeistGameState->GetContractSnapshot();
	FHeistArtifactDataRow RequiredTargetDefinition;
	if (!TryGetArtifactDefinition(ContractSnapshot.RequiredTargetArtifactId, RequiredTargetDefinition))
	{
		UE_LOG(LogHeistNetwork, Error, TEXT("Team result calculation rejected: TargetArtifact=%s Result=FAIL Reason=MissingArtifactDefinition"),
			*ContractSnapshot.RequiredTargetArtifactId.ToString());
		return false;
	}

	float RequiredTargetQuality = 50.0f;
	bool bFoundRequiredTargetReplica = false;
	TArray<FHeistReplicaRecapEntry> ReplicaRecap;
	for (TActorIterator<AHeistPaintingDisplayCaseActor> DisplayCaseIterator(GetWorld()); DisplayCaseIterator; ++DisplayCaseIterator)
	{
		const AHeistPaintingDisplayCaseActor* DisplayCase = *DisplayCaseIterator;
		if (!IsValid(DisplayCase) || !DisplayCase->HasCommittedForgeryResult())
		{
			continue;
		}
		const FHeistForgeryResult Result = DisplayCase->GetCommittedForgeryResult();
		FHeistReplicaRecapEntry& Recap = ReplicaRecap.AddDefaulted_GetRef();
		Recap.CaseId = DisplayCase->GetDisplayCaseId();
		Recap.ArtifactId = Result.ArtifactId;
		Recap.TemplateId = Result.TemplateId;
		Recap.ForgeryType = Result.ForgeryType == EHeistForgeryType::None ? EHeistForgeryType::Drawing : Result.ForgeryType;
		Recap.QualityScore = FMath::Clamp(Result.SimilarityScore, 0.0f, 100.0f);
		Recap.bRequiredTarget = Recap.CaseId == ContractSnapshot.RequiredTargetCaseId && Recap.ArtifactId == ContractSnapshot.RequiredTargetArtifactId;
		const FHeistReplicaPaintingData PaintingData = DisplayCase->GetReplicaPaintingData();
		Recap.PaintingResolution = PaintingData.Resolution;
		Recap.PaintingPalette = PaintingData.Palette;
		Recap.PaintingPackedPaletteIndices = PaintingData.PackedPaletteIndices;
		if (Recap.bRequiredTarget)
		{
			RequiredTargetQuality = Recap.QualityScore;
			bFoundRequiredTargetReplica = true;
		}
	}
	for (TActorIterator<AHeistObjectDisplayCaseActor> DisplayCaseIterator(GetWorld()); DisplayCaseIterator; ++DisplayCaseIterator)
	{
		const AHeistObjectDisplayCaseActor* DisplayCase = *DisplayCaseIterator;
		if (!IsValid(DisplayCase) || !DisplayCase->HasCommittedAssemblyResult())
		{
			continue;
		}
		const FHeistObjectAssemblyResult Result = DisplayCase->GetCommittedAssemblyResult();
		FHeistReplicaRecapEntry& Recap = ReplicaRecap.AddDefaulted_GetRef();
		Recap.CaseId = DisplayCase->GetObjectCaseId();
		Recap.ArtifactId = Result.ArtifactId;
		Recap.TemplateId = Result.TemplateId;
		Recap.ForgeryType = EHeistForgeryType::Assembly;
		Recap.QualityScore = FMath::Clamp(Result.QualityScore, 0.0f, 100.0f);
		Recap.bRequiredTarget = Recap.CaseId == ContractSnapshot.RequiredTargetCaseId && Recap.ArtifactId == ContractSnapshot.RequiredTargetArtifactId;
		Recap.AssemblyEntries = DisplayCase->GetAssemblyReplicaData().Entries;
		if (Recap.bRequiredTarget)
		{
			RequiredTargetQuality = Recap.QualityScore;
			bFoundRequiredTargetReplica = true;
		}
	}
	ReplicaRecap.Sort([](const FHeistReplicaRecapEntry& Left, const FHeistReplicaRecapEntry& Right)
	{
		return Left.CaseId.ToString() < Right.CaseId.ToString();
	});

	const int32 AlertLevelIndex = static_cast<int32>(HeistGameState->GetAlertLevel());
	const int32 TargetValue = ContractSnapshot.bRequiredTargetSecured ? FMath::Min(ContractSnapshot.SecuredValue, FMath::Max(0, RequiredTargetDefinition.ArtifactValue)) : 0;
	const int32 LooseLootValue = FMath::Max(0, ContractSnapshot.SecuredValue - TargetValue);
	const int32 ArrestedCrewCount = HeistGameState->GetArrestedCrewCount();
	float ForgeryMultiplier = 1.0f;
	float StealthMultiplier = 1.0f;
	int32 ArrestPenalty = 0;
	int32 TeamReward = 0;
	if (!HeistTeamReward::Calculate(TargetValue, LooseLootValue, RequiredTargetQuality, BalanceData->MinimumForgeryRewardMultiplier,
		BalanceData->MaximumForgeryRewardMultiplier, AlertLevelIndex, BalanceData->AlertLevelRewardPenalty, BalanceData->MinimumStealthRewardMultiplier,
		ArrestedCrewCount, BalanceData->ArrestRewardPenaltyPerPlayer, ForgeryMultiplier, StealthMultiplier, ArrestPenalty, TeamReward))
	{
		UE_LOG(LogHeistNetwork, Error, TEXT("Team result calculation rejected: Result=FAIL Reason=InvalidRewardBalance"));
		return false;
	}

	OutTeamResult.Outcome = Outcome;
	OutTeamResult.OutcomeReasonId = OutcomeReasonId;
	OutTeamResult.RequiredTargetArtifactId = ContractSnapshot.RequiredTargetArtifactId;
	OutTeamResult.bRequiredTargetSecured = ContractSnapshot.bRequiredTargetSecured;
	OutTeamResult.LootValueQuota = ContractSnapshot.LootValueQuota;
	OutTeamResult.SecuredValue = ContractSnapshot.SecuredValue;
	OutTeamResult.ExtraValue = FMath::Max(0, ContractSnapshot.SecuredValue - ContractSnapshot.LootValueQuota);
	OutTeamResult.RequiredTargetValue = TargetValue;
	OutTeamResult.SecuredLooseLootValue = LooseLootValue;
	OutTeamResult.RequiredTargetQuality = RequiredTargetQuality;
	OutTeamResult.ForgeryRewardMultiplier = ForgeryMultiplier;
	OutTeamResult.StealthRewardMultiplier = StealthMultiplier;
	OutTeamResult.ArrestPenalty = ArrestPenalty;
	OutTeamResult.TeamReward = TeamReward;
	OutTeamResult.CrewCount = HeistGameState->GetConnectedPlayerCount();
	OutTeamResult.EscapedCrewCount = HeistGameState->GetEscapedCrewCount();
	OutTeamResult.ArrestedCrewCount = ArrestedCrewCount;
	OutTeamResult.ReplicaRecap = MoveTemp(ReplicaRecap);

	UE_LOG(LogHeistNetwork, Log,
		TEXT("Team reward calculated: TargetValue=%d LooseValue=%d RequiredQuality=%.1f RequiredReplica=%s ForgeryMultiplier=%.3f Alert=%s StealthMultiplier=%.3f Arrested=%d ArrestPenalty=%d Reward=%d QuotaUntouched=%d SecuredUntouched=%d Authority=true Result=PASS"),
		TargetValue, LooseLootValue, RequiredTargetQuality, bFoundRequiredTargetReplica ? TEXT("true") : TEXT("false"), ForgeryMultiplier,
		*UEnum::GetValueAsString(HeistGameState->GetAlertLevel()), StealthMultiplier, ArrestedCrewCount, OutTeamResult.ArrestPenalty, OutTeamResult.TeamReward,
		ContractSnapshot.LootValueQuota, ContractSnapshot.SecuredValue);
	return true;
}

#pragma endregion

#pragma region ContractObjective

void AHeistGameMode::InitializeContractFromPlacedTargetCase()
{
	if (!HasAuthority())
	{
		return;
	}

	AHeistGameState* HeistGameState = GetGameState<AHeistGameState>();
	if (!IsValid(HeistGameState))
	{
		UE_LOG(LogHeist, Error, TEXT("Objective initialization: ConfiguredTargetCaseId=%s Result=FAIL Reason=MissingGameState"), *ObjectiveTargetCaseId.ToString());
		return;
	}

	struct FPlacedTargetCase
	{
		AActor* Actor = nullptr;
		FName CaseId = NAME_None;
		FName ArtifactId = NAME_None;
		FString CaseState;
		bool bCaseStateValid = false;
	};

	TArray<FPlacedTargetCase> MatchingTargetCases;
	const auto IsConfiguredTargetCase = [this](const FName CaseId)
	{
		return (!ObjectiveTargetCaseId.IsNone() && CaseId == ObjectiveTargetCaseId) ||
			(ObjectiveTargetCaseId.IsNone() && CaseId.ToString().EndsWith(TEXT("_Target"), ESearchCase::IgnoreCase));
	};

	for (TActorIterator<AHeistPaintingDisplayCaseActor> DisplayCaseIterator(GetWorld()); DisplayCaseIterator; ++DisplayCaseIterator)
	{
		AHeistPaintingDisplayCaseActor* DisplayCase = *DisplayCaseIterator;
		if (!IsValid(DisplayCase) || !IsConfiguredTargetCase(DisplayCase->GetDisplayCaseId()))
		{
			continue;
		}

		FPlacedTargetCase& Candidate = MatchingTargetCases.AddDefaulted_GetRef();
		Candidate.Actor = DisplayCase;
		Candidate.CaseId = DisplayCase->GetDisplayCaseId();
		Candidate.ArtifactId = DisplayCase->GetTargetArtifactId();
		Candidate.CaseState = UEnum::GetValueAsString(DisplayCase->GetDisplayCaseState());
		Candidate.bCaseStateValid = DisplayCase->GetDisplayCaseState() == EHeistDisplayCaseState::Secured;
	}

	for (TActorIterator<AHeistObjectDisplayCaseActor> DisplayCaseIterator(GetWorld()); DisplayCaseIterator; ++DisplayCaseIterator)
	{
		AHeistObjectDisplayCaseActor* DisplayCase = *DisplayCaseIterator;
		if (!IsValid(DisplayCase) || !IsConfiguredTargetCase(DisplayCase->GetObjectCaseId()))
		{
			continue;
		}

		FPlacedTargetCase& Candidate = MatchingTargetCases.AddDefaulted_GetRef();
		Candidate.Actor = DisplayCase;
		Candidate.CaseId = DisplayCase->GetObjectCaseId();
		Candidate.ArtifactId = DisplayCase->GetTargetArtifactId();
		Candidate.CaseState = UEnum::GetValueAsString(DisplayCase->GetAssemblyState());
		Candidate.bCaseStateValid = DisplayCase->GetAssemblyState() == EHeistObjectAssemblyState::Secured;
	}

	if (MatchingTargetCases.Num() != 1)
	{
		UE_LOG(LogHeist, Error, TEXT("Objective initialization: ConfiguredTargetCaseId=%s MatchingCases=%d Result=FAIL Reason=%s"), *ObjectiveTargetCaseId.ToString(), MatchingTargetCases.Num(),
			   MatchingTargetCases.IsEmpty() ? TEXT("MissingTargetCase") : TEXT("DuplicateTargetCaseId"));
		return;
	}

	const FPlacedTargetCase& TargetDisplayCase = MatchingTargetCases[0];
	const FName TargetArtifactId = TargetDisplayCase.ArtifactId;
	FHeistArtifactDataRow ArtifactDefinition;
	const bool bArtifactValid = TryGetArtifactDefinition(TargetArtifactId, ArtifactDefinition);
	const bool bCaseStateValid = TargetDisplayCase.bCaseStateValid;

	const UHeistGameBalanceDataAsset* BalanceData = ResolveGameBalanceData();
	const FName ContractId = IsValid(BalanceData) ? BalanceData->DefaultContractDefinition.ContractId : NAME_None;
	FHeistContractDataRow ContractDefinition;
	FString ContractFailureReason;
	const bool bContractDefinitionValid = TryGetContractDefinition(ContractId, ContractDefinition) && ContractDefinition.IsRuntimeDefinitionValid(&ContractFailureReason);
	const int32 PlayerCount = FMath::Clamp(HeistGameState->GetConnectedPlayerCount(), 1, 4);
	const int32 LootValueQuota = bContractDefinitionValid ? ContractDefinition.ResolveLootValueQuota(PlayerCount) : 0;
	const bool bRequiredTargetNeedsOptionalLoot = bArtifactValid && ArtifactDefinition.ArtifactValue < LootValueQuota;
	const FName MapId = ResolveContractMapId(this, HeistGameState);
	const int32 AssignmentSeed = FMath::Rand();
	const bool bContractInitialized = bArtifactValid && bCaseStateValid && bContractDefinitionValid && bRequiredTargetNeedsOptionalLoot && !MapId.IsNone() &&
		HeistGameState->InitializeContractSnapshot(ContractDefinition.ContractId, MapId, AssignmentSeed, TargetArtifactId, TargetDisplayCase.CaseId, LootValueQuota);
	const bool bObjectiveInitialized = bContractInitialized &&
		HeistGameState->SetObjectiveSnapshot(TargetArtifactId, TargetDisplayCase.CaseId, EHeistObjectiveState::Available, nullptr);

	const FString InitializationMessage =
		FString::Printf(TEXT("Contract objective initialization: Contract=%s Map=%s Seed=%d Players=%d Quota=%d TargetValue=%d RequiresOptionalLoot=%s TargetCase=%s CaseId=%s ArtifactId=%s Location=%s CaseState=%s CaseStateValid=%s ArtifactValid=%s ContractDefinitionValid=%s ContractFailure=%s ContractInitialized=%s ObjectiveState=%s Result=%s"),
						*ContractDefinition.ContractId.ToString(), *MapId.ToString(), AssignmentSeed, PlayerCount, LootValueQuota, ArtifactDefinition.ArtifactValue,
						bRequiredTargetNeedsOptionalLoot ? TEXT("true") : TEXT("false"),
						*GetNameSafe(TargetDisplayCase.Actor), *TargetDisplayCase.CaseId.ToString(), *TargetArtifactId.ToString(), *TargetDisplayCase.Actor->GetActorLocation().ToCompactString(),
						*TargetDisplayCase.CaseState, bCaseStateValid ? TEXT("true") : TEXT("false"), bArtifactValid ? TEXT("true") : TEXT("false"),
						bContractDefinitionValid ? TEXT("true") : TEXT("false"), ContractFailureReason.IsEmpty() ? TEXT("None") : *ContractFailureReason,
						bContractInitialized ? TEXT("true") : TEXT("false"),
						*UEnum::GetValueAsString(HeistGameState->GetObjectiveState()), bObjectiveInitialized ? TEXT("PASS") : TEXT("FAIL"));
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

#pragma region Alert

bool AHeistGameMode::RequestAlertEscalation(const EHeistAlertLevel RequestedAlertLevel, const FName TriggerId)
{
	AHeistGameState* HeistGameState = GetGameState<AHeistGameState>();
	const UEnum* AlertLevelEnum = StaticEnum<EHeistAlertLevel>();
	if (!HasAuthority() || !IsValid(HeistGameState) || HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame || TriggerId.IsNone() || !IsValid(AlertLevelEnum) ||
		!AlertLevelEnum->IsValidEnumValue(static_cast<int64>(RequestedAlertLevel)))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Global alert trigger rejected: Requested=%s Trigger=%s Authority=%s MatchPhase=%s Result=FAIL Reason=InvalidRequest"),
			   *UEnum::GetValueAsString(RequestedAlertLevel), *TriggerId.ToString(), HasAuthority() ? TEXT("true") : TEXT("false"),
			   IsValid(HeistGameState) ? *UEnum::GetValueAsString(HeistGameState->GetMatchPhase()) : TEXT("MissingGameState"));
		return false;
	}

	if (ProcessedAlertTriggerIds.Contains(TriggerId))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Global alert trigger blocked: Current=%s Requested=%s Trigger=%s ProcessedTriggers=%d Authority=true Result=PASS Reason=DuplicateTrigger"),
			   *UEnum::GetValueAsString(HeistGameState->GetAlertLevel()), *UEnum::GetValueAsString(RequestedAlertLevel), *TriggerId.ToString(), ProcessedAlertTriggerIds.Num());
		return true;
	}

	ProcessedAlertTriggerIds.Add(TriggerId);
	const EHeistAlertLevel CurrentAlertLevel = HeistGameState->GetAlertLevel();
	if (static_cast<uint8>(RequestedAlertLevel) <= static_cast<uint8>(CurrentAlertLevel))
	{
		UE_LOG(LogHeistNetwork, Log, TEXT("Global alert trigger consumed: Current=%s Requested=%s Trigger=%s ProcessedTriggers=%d Authority=true Result=PASS Reason=NoDowngradeOrDuplicateLevel"),
			   *UEnum::GetValueAsString(CurrentAlertLevel), *UEnum::GetValueAsString(RequestedAlertLevel), *TriggerId.ToString(), ProcessedAlertTriggerIds.Num());
		return true;
	}

	if (ApplyAlertLevel(RequestedAlertLevel, TriggerId))
	{
		return true;
	}

	ProcessedAlertTriggerIds.Remove(TriggerId);
	return false;
}

bool AHeistGameMode::IsAlertTransitionTimerActive() const
{
	return GetWorldTimerManager().IsTimerActive(AlertTransitionTimerHandle);
}

int32 AHeistGameMode::GetProcessedAlertTriggerCount() const
{
	return ProcessedAlertTriggerIds.Num();
}

int32 AHeistGameMode::GetActiveMatchTimerCount() const
{
	const FTimerManager& TimerManager = GetWorldTimerManager();
	int32 ActiveTimerCount = TimerManager.TimerExists(AlertTransitionTimerHandle) ? 1 : 0;
	ActiveTimerCount += TimerManager.TimerExists(EscapePhaseTimerHandle) ? 1 : 0;
	ActiveTimerCount += TimerManager.TimerExists(ContractDurationTimerHandle) ? 1 : 0;
	for (const FTimerHandle& TimerHandle : RareLootWarningTimerHandles)
	{
		ActiveTimerCount += TimerManager.TimerExists(TimerHandle) ? 1 : 0;
	}
	for (const FTimerHandle& TimerHandle : RareLootSpawnTimerHandles)
	{
		ActiveTimerCount += TimerManager.TimerExists(TimerHandle) ? 1 : 0;
	}
	return ActiveTimerCount;
}

void AHeistGameMode::InitializeAlertState()
{
	GetWorldTimerManager().ClearTimer(AlertTransitionTimerHandle);
	ProcessedAlertTriggerIds.Reset();
	ScheduledAlertSourceLevel = EHeistAlertLevel::Quiet;
	ScheduledAlertRevision = 0;
	bLockdownWorldRestrictionsApplied = false;
	if (AHeistGameState* HeistGameState = GetGameState<AHeistGameState>())
	{
		HeistGameState->SetAlertSnapshot(EHeistAlertLevel::Quiet, 0.0f, FName(TEXT("MatchStart")));
	}
}

bool AHeistGameMode::ApplyAlertLevel(const EHeistAlertLevel NewAlertLevel, const FName TriggerId)
{
	AHeistGameState* HeistGameState = GetGameState<AHeistGameState>();
	if (!HasAuthority() || !IsValid(HeistGameState) || HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame)
	{
		return false;
	}

	GetWorldTimerManager().ClearTimer(AlertTransitionTimerHandle);
	const EHeistAlertLevel NextAlertLevel = GetNextAlertLevel(NewAlertLevel);
	const float TransitionDelay = ResolveAlertTransitionDelay(NewAlertLevel);
	const bool bHasNextTransition = NextAlertLevel != NewAlertLevel;
	const float NextTransitionServerTime = bHasNextTransition ? HeistGameState->GetServerWorldTimeSeconds() + TransitionDelay : 0.0f;
	if (!HeistGameState->SetAlertSnapshot(NewAlertLevel, NextTransitionServerTime, TriggerId))
	{
		return false;
	}

	ScheduledAlertSourceLevel = NewAlertLevel;
	ScheduledAlertRevision = HeistGameState->GetAlertRevision();
	if (!bHasNextTransition)
	{
		const bool bRestrictionsApplied = NewAlertLevel != EHeistAlertLevel::Lockdown || ApplyLockdownWorldRestrictions(TriggerId);
		UE_LOG(LogHeistNetwork, Log, TEXT("Global alert terminal level reached: Level=%s Trigger=%s Revision=%d Authority=true Result=%s"), *UEnum::GetValueAsString(NewAlertLevel),
			   *TriggerId.ToString(), ScheduledAlertRevision, bRestrictionsApplied ? TEXT("PASS") : TEXT("FAIL"));
		return bRestrictionsApplied;
	}

	if (TransitionDelay <= KINDA_SMALL_NUMBER)
	{
		HandleAlertTransitionTimerElapsed();
		return true;
	}

	GetWorldTimerManager().SetTimer(AlertTransitionTimerHandle, this, &AHeistGameMode::HandleAlertTransitionTimerElapsed, TransitionDelay, false);
	UE_LOG(LogHeistNetwork, Log, TEXT("Global alert timer started: Current=%s Next=%s Delay=%.2f NextTransitionServerTime=%.2f Trigger=%s Revision=%d Authority=true Result=PASS"),
		   *UEnum::GetValueAsString(NewAlertLevel), *UEnum::GetValueAsString(NextAlertLevel), TransitionDelay, NextTransitionServerTime, *TriggerId.ToString(), ScheduledAlertRevision);
	return true;
}

bool AHeistGameMode::ApplyLockdownWorldRestrictions(const FName TriggerId)
{
	AHeistGameState* HeistGameState = GetGameState<AHeistGameState>();
	if (!HasAuthority() || !IsValid(HeistGameState) || !HeistGameState->IsLockdownActive())
	{
		UE_LOG(LogHeistNetwork, Error, TEXT("Lockdown world restriction rejected: Trigger=%s Authority=%s Alert=%s Result=FAIL Reason=InvalidAuthorityState"), *TriggerId.ToString(),
			   HasAuthority() ? TEXT("true") : TEXT("false"), IsValid(HeistGameState) ? *UEnum::GetValueAsString(HeistGameState->GetAlertLevel()) : TEXT("MissingGameState"));
		return false;
	}

	if (bLockdownWorldRestrictionsApplied)
	{
		const bool bStateValid = HeistGameState->GetMatchPhase() == EHeistMatchPhase::End && HeistGameState->GetContractSnapshot().Outcome != EHeistContractOutcome::None;
		UE_LOG(LogHeistNetwork, Log, TEXT("Lockdown world restriction duplicate blocked: Trigger=%s MatchPhase=%s Objective=%s Authority=true Result=%s Reason=AlreadyApplied"),
			   *TriggerId.ToString(), *UEnum::GetValueAsString(HeistGameState->GetMatchPhase()), *UEnum::GetValueAsString(HeistGameState->GetObjectiveState()),
			   bStateValid ? TEXT("PASS") : TEXT("FAIL"));
		return bStateValid;
	}

	const bool bOutcomeFinalized = TryResolveContractOutcome(FName(TEXT("Lockdown")), true);
	bLockdownWorldRestrictionsApplied = bOutcomeFinalized;
	UE_LOG(LogHeistNetwork, Log, TEXT("Lockdown world restriction applied: SourceTrigger=%s Outcome=%s ReasonId=%s MatchPhase=%s Authority=true Result=%s"),
		   *TriggerId.ToString(), *UEnum::GetValueAsString(HeistGameState->GetContractSnapshot().Outcome),
		   *HeistGameState->GetContractSnapshot().OutcomeReasonId.ToString(), *UEnum::GetValueAsString(HeistGameState->GetMatchPhase()),
		   bOutcomeFinalized ? TEXT("PASS") : TEXT("FAIL"));
	return bOutcomeFinalized;
}

void AHeistGameMode::HandleAlertTransitionTimerElapsed()
{
	AHeistGameState* HeistGameState = GetGameState<AHeistGameState>();
	if (!HasAuthority() || !IsValid(HeistGameState) || HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame || HeistGameState->GetAlertLevel() != ScheduledAlertSourceLevel ||
		HeistGameState->GetAlertRevision() != ScheduledAlertRevision)
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Global alert timer blocked: ScheduledSource=%s ScheduledRevision=%d Current=%s CurrentRevision=%d Authority=%s Result=PASS Reason=StaleOrMatchEnded"),
			   *UEnum::GetValueAsString(ScheduledAlertSourceLevel), ScheduledAlertRevision,
			   IsValid(HeistGameState) ? *UEnum::GetValueAsString(HeistGameState->GetAlertLevel()) : TEXT("MissingGameState"),
			   IsValid(HeistGameState) ? HeistGameState->GetAlertRevision() : INDEX_NONE, HasAuthority() ? TEXT("true") : TEXT("false"));
		return;
	}

	const EHeistAlertLevel NextAlertLevel = GetNextAlertLevel(ScheduledAlertSourceLevel);
	const FName TimerTriggerId(*FString::Printf(TEXT("AlertTimer_%s_%d"), *UEnum::GetValueAsString(ScheduledAlertSourceLevel), ScheduledAlertRevision));
	ApplyAlertLevel(NextAlertLevel, TimerTriggerId);
}

EHeistAlertLevel AHeistGameMode::GetNextAlertLevel(const EHeistAlertLevel CurrentAlertLevel) const
{
	switch (CurrentAlertLevel)
	{
	case EHeistAlertLevel::Quiet:
		return EHeistAlertLevel::Suspicious;
	case EHeistAlertLevel::Suspicious:
		return EHeistAlertLevel::Searching;
	case EHeistAlertLevel::Searching:
		return EHeistAlertLevel::Alarmed;
	case EHeistAlertLevel::Alarmed:
		return EHeistAlertLevel::Lockdown;
	case EHeistAlertLevel::Lockdown:
	default:
		return EHeistAlertLevel::Lockdown;
	}
}

float AHeistGameMode::ResolveAlertTransitionDelay(const EHeistAlertLevel CurrentAlertLevel) const
{
	const UHeistGameBalanceDataAsset* BalanceData = ResolveGameBalanceData();
	if (!IsValid(BalanceData))
	{
		return 0.0f;
	}

	switch (CurrentAlertLevel)
	{
	case EHeistAlertLevel::Suspicious:
		return FMath::Max(0.0f, BalanceData->SuspiciousToSearchingDelay);
	case EHeistAlertLevel::Searching:
		return FMath::Max(0.0f, BalanceData->SearchingToAlarmedDelay);
	case EHeistAlertLevel::Alarmed:
		return FMath::Max(0.0f, BalanceData->AlarmedToLockdownDelay);
	case EHeistAlertLevel::Quiet:
	case EHeistAlertLevel::Lockdown:
	default:
		return 0.0f;
	}
}

#pragma endregion

#pragma region Balance

UDataTable* AHeistGameMode::GetItemDataTable() const
{
	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset) ? GameBalanceDataAsset.Get() : GetDefault<UHeistGameBalanceDataAsset>();

	return ResolvedBalanceData->ItemDataTable.LoadSynchronous();
}

UDataTable* AHeistGameMode::GetContractDataTable() const
{
	const UHeistGameBalanceDataAsset* ResolvedBalanceData = ResolveGameBalanceData();
	return IsValid(ResolvedBalanceData) && !ResolvedBalanceData->ContractDataTable.IsNull() ? ResolvedBalanceData->ContractDataTable.LoadSynchronous() : nullptr;
}

UDataTable* AHeistGameMode::GetArtifactDataTable() const
{
	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset) ? GameBalanceDataAsset.Get() : GetDefault<UHeistGameBalanceDataAsset>();

	return ResolvedBalanceData->ArtifactDataTable.LoadSynchronous();
}

UDataTable* AHeistGameMode::GetForgeryTemplateDataTable() const
{
	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset) ? GameBalanceDataAsset.Get() : GetDefault<UHeistGameBalanceDataAsset>();

	return ResolvedBalanceData->ForgeryTemplateDataTable.LoadSynchronous();
}

UDataTable* AHeistGameMode::GetObjectAssemblyPartDataTable() const
{
	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset) ? GameBalanceDataAsset.Get() : GetDefault<UHeistGameBalanceDataAsset>();

	return ResolvedBalanceData->ObjectAssemblyPartDataTable.LoadSynchronous();
}

UDataTable* AHeistGameMode::GetObjectAssemblyTemplateDataTable() const
{
	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset) ? GameBalanceDataAsset.Get() : GetDefault<UHeistGameBalanceDataAsset>();

	return ResolvedBalanceData->ObjectAssemblyTemplateDataTable.LoadSynchronous();
}

bool AHeistGameMode::TryGetItemDefinition(const FName ItemId, FHeistItemDataRow& OutItemDefinition) const
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
		UE_LOG(LogHeistInventory, Warning, TEXT("Item definition lookup rejected: ItemId=%s Reason=MissingItemDataTable"), *ItemId.ToString());
		return false;
	}

	if (ItemDataTable->GetRowStruct() != FHeistItemDataRow::StaticStruct())
	{
		UE_LOG(LogHeistInventory, Error, TEXT("Item definition lookup rejected: ItemId=%s Reason=InvalidRowStruct Table=%s RowStruct=%s"), *ItemId.ToString(), *GetNameSafe(ItemDataTable),
			   *GetNameSafe(ItemDataTable->GetRowStruct()));
		return false;
	}

	const FHeistItemDataRow* ItemDefinition = ItemDataTable->FindRow<FHeistItemDataRow>(ItemId, TEXT("AHeistGameMode::TryGetItemDefinition"), false);
	if (!ItemDefinition)
	{
		UE_LOG(LogHeistInventory, Warning, TEXT("Item definition lookup rejected: ItemId=%s Reason=MissingRow Table=%s"), *ItemId.ToString(), *GetNameSafe(ItemDataTable));
		return false;
	}

	if (ItemDefinition->ItemId != ItemId)
	{
		UE_LOG(LogHeistInventory, Error, TEXT("Item definition lookup rejected: RowName=%s RowItemId=%s Reason=RowNameItemIdMismatch"), *ItemId.ToString(), *ItemDefinition->ItemId.ToString());
		return false;
	}

	if (ItemDefinition->ItemType == EHeistItemType::None || ItemDefinition->GridSize.X <= 0 || ItemDefinition->GridSize.Y <= 0 || ItemDefinition->Weight < 0.0f)
	{
		UE_LOG(LogHeistInventory, Error, TEXT("Item definition lookup rejected: ItemId=%s Reason=InvalidDefinition Type=%d Grid=%dx%d Weight=%.2f"), *ItemId.ToString(),
			   static_cast<int32>(ItemDefinition->ItemType), ItemDefinition->GridSize.X, ItemDefinition->GridSize.Y, ItemDefinition->Weight);
		return false;
	}

	OutItemDefinition = *ItemDefinition;
	return true;
}

bool AHeistGameMode::TryGetContractDefinition(const FName ContractId, FHeistContractDataRow& OutContractDefinition) const
{
	OutContractDefinition = FHeistContractDataRow();
	if (ContractId.IsNone())
	{
		UE_LOG(LogHeist, Error, TEXT("Contract definition lookup rejected: Reason=MissingContractId"));
		return false;
	}

	const UDataTable* ContractDataTable = GetContractDataTable();
	if (IsValid(ContractDataTable))
	{
		if (ContractDataTable->GetRowStruct() != FHeistContractDataRow::StaticStruct())
		{
			UE_LOG(LogHeist, Error, TEXT("Contract definition lookup rejected: ContractId=%s Reason=InvalidRowStruct Table=%s RowStruct=%s"), *ContractId.ToString(),
				   *GetNameSafe(ContractDataTable), *GetNameSafe(ContractDataTable->GetRowStruct()));
			return false;
		}

		const FHeistContractDataRow* ContractDefinition =
			ContractDataTable->FindRow<FHeistContractDataRow>(ContractId, TEXT("AHeistGameMode::TryGetContractDefinition"), false);
		FString FailureReason;
		if (ContractDefinition == nullptr || ContractDefinition->ContractId != ContractId || !ContractDefinition->IsRuntimeDefinitionValid(&FailureReason))
		{
			UE_LOG(LogHeist, Error, TEXT("Contract definition lookup rejected: ContractId=%s Reason=%s"), *ContractId.ToString(),
				   ContractDefinition == nullptr ? TEXT("MissingRow") : (FailureReason.IsEmpty() ? TEXT("RowNameContractIdMismatch") : *FailureReason));
			return false;
		}

		OutContractDefinition = *ContractDefinition;
		return true;
	}

	const UHeistGameBalanceDataAsset* BalanceData = ResolveGameBalanceData();
	if (!IsValid(BalanceData) || BalanceData->DefaultContractDefinition.ContractId != ContractId)
	{
		UE_LOG(LogHeist, Error, TEXT("Contract definition lookup rejected: ContractId=%s Reason=MissingDataTableAndFallback"), *ContractId.ToString());
		return false;
	}

	FString FailureReason;
	if (!BalanceData->DefaultContractDefinition.IsRuntimeDefinitionValid(&FailureReason))
	{
		UE_LOG(LogHeist, Error, TEXT("Contract definition lookup rejected: ContractId=%s Reason=InvalidFallback Detail=%s"), *ContractId.ToString(), *FailureReason);
		return false;
	}

	OutContractDefinition = BalanceData->DefaultContractDefinition;
	UE_LOG(LogHeist, Log, TEXT("Contract definition lookup: ContractId=%s Source=DefaultBalanceFallback Result=PASS"), *ContractId.ToString());
	return true;
}

bool AHeistGameMode::TryGetArtifactDefinition(const FName ArtifactId, FHeistArtifactDataRow& OutArtifactDefinition) const
{
	OutArtifactDefinition = FHeistArtifactDataRow();
	if (ArtifactId.IsNone())
	{
		UE_LOG(LogHeist, Warning, TEXT("Artifact definition lookup rejected: Reason=MissingArtifactId"));
		return false;
	}

	const UDataTable* ArtifactDataTable = GetArtifactDataTable();
	if (!IsValid(ArtifactDataTable) || ArtifactDataTable->GetRowStruct() != FHeistArtifactDataRow::StaticStruct())
	{
		UE_LOG(LogHeist, Error, TEXT("Artifact definition lookup rejected: ArtifactId=%s Reason=MissingOrInvalidArtifactDataTable"), *ArtifactId.ToString());
		return false;
	}

	const FHeistArtifactDataRow* ArtifactDefinition = ArtifactDataTable->FindRow<FHeistArtifactDataRow>(ArtifactId, TEXT("AHeistGameMode::TryGetArtifactDefinition"), false);
	if (ArtifactDefinition == nullptr || ArtifactDefinition->ArtifactId != ArtifactId || ArtifactDefinition->ArtifactValue < 0 || !FMath::IsFinite(ArtifactDefinition->Weight) ||
		ArtifactDefinition->Weight < 0.0f || ArtifactDefinition->DisplayName.IsEmpty() || ArtifactDefinition->ForgeryType == EHeistForgeryType::None)
	{
		UE_LOG(LogHeist, Error, TEXT("Artifact definition lookup rejected: ArtifactId=%s Reason=MissingOrInvalidDefinition"), *ArtifactId.ToString());
		return false;
	}

	OutArtifactDefinition = *ArtifactDefinition;
	return true;
}

bool AHeistGameMode::TryGetForgeryTemplateDefinition(const FName TemplateId, FHeistForgeryTemplateRow& OutTemplateDefinition) const
{
	OutTemplateDefinition = FHeistForgeryTemplateRow();
	if (TemplateId.IsNone())
	{
		UE_LOG(LogHeist, Warning, TEXT("Forgery template lookup rejected: Reason=MissingTemplateId"));
		return false;
	}

	const UDataTable* TemplateDataTable = GetForgeryTemplateDataTable();
	if (!IsValid(TemplateDataTable) || TemplateDataTable->GetRowStruct() != FHeistForgeryTemplateRow::StaticStruct())
	{
		UE_LOG(LogHeist, Error, TEXT("Forgery template lookup rejected: TemplateId=%s Reason=MissingOrInvalidTemplateDataTable"), *TemplateId.ToString());
		return false;
	}

	const FHeistForgeryTemplateRow* TemplateDefinition = TemplateDataTable->FindRow<FHeistForgeryTemplateRow>(TemplateId, TEXT("AHeistGameMode::TryGetForgeryTemplateDefinition"), false);
	if (TemplateDefinition == nullptr || TemplateDefinition->TemplateId != TemplateId || TemplateDefinition->ReferenceImage.IsNull() || TemplateDefinition->ReferenceMask.IsNull() ||
		TemplateDefinition->ObservationDuration < 0.0f || !FMath::IsFinite(TemplateDefinition->ForgeryDuration) ||
		!FMath::IsWithinInclusive(TemplateDefinition->ForgeryDuration, 20.0f, 45.0f) || TemplateDefinition->StrokeLimit <= 0 || TemplateDefinition->BrushSize <= 0.0f)
	{
		UE_LOG(LogHeist, Error, TEXT("Forgery template lookup rejected: TemplateId=%s Reason=MissingOrInvalidDefinition"), *TemplateId.ToString());
		return false;
	}

	OutTemplateDefinition = *TemplateDefinition;
	return true;
}

void AHeistGameMode::InitializeSurfaceTemplateSelection()
{
	if (!HasAuthority())
	{
		return;
	}

	AHeistGameState* HeistGameState = GetGameState<AHeistGameState>();
	UHeistGameInstance* HeistGameInstance = Cast<UHeistGameInstance>(GetGameInstance());
	if (IsValid(HeistGameState) && HeistGameState->GetSurfaceTemplateSelectionRevision() > 0)
	{
		return;
	}
	const FName PoolId = IsValid(HeistGameState) && !HeistGameState->GetSelectedLobbyMapId().IsNone()
							 ? HeistGameState->GetSelectedLobbyMapId()
							 : (IsValid(HeistGameInstance) ? HeistGameInstance->GetSelectedMapId() : NAME_None);
	TArray<FName> CandidateTemplateIds;
	if (!IsValid(HeistGameState) || !IsValid(HeistGameInstance) || !GatherSurfaceTemplatePool(PoolId, CandidateTemplateIds))
	{
		UHeistDebugFunctionLibrary::DebugSurfaceTemplateSelectionState(this, TEXT("ServerPoolRejected"), PoolId, NAME_None, CandidateTemplateIds.Num(), 0, 0, 0, false);
		return;
	}

	FName SelectedTemplateId = NAME_None;
	int32 SelectionRevision = 0;
	int32 BagCycle = 0;
	int32 RemainingTemplateCount = 0;
	if (!HeistGameInstance->SelectSurfaceTemplateForMatch(PoolId, CandidateTemplateIds, SelectedTemplateId, SelectionRevision, BagCycle, RemainingTemplateCount) ||
		!HeistGameState->InitializeSurfaceTemplateSelection(PoolId, SelectedTemplateId, CandidateTemplateIds.Num(), BagCycle, RemainingTemplateCount, SelectionRevision))
	{
		UHeistDebugFunctionLibrary::DebugSurfaceTemplateSelectionState(this, TEXT("ServerSelectionRejected"), PoolId, SelectedTemplateId, CandidateTemplateIds.Num(), BagCycle,
																	  RemainingTemplateCount, SelectionRevision, false);
	}
}

bool AHeistGameMode::GatherSurfaceTemplatePool(const FName PoolId, TArray<FName>& OutTemplateIds) const
{
	OutTemplateIds.Reset();
	const bool bValidPoolId = PoolId == FName(TEXT("M01")) || PoolId == FName(TEXT("M02")) || PoolId == FName(TEXT("M03"));
	const UDataTable* TemplateDataTable = GetForgeryTemplateDataTable();
	if (!bValidPoolId || !IsValid(TemplateDataTable) || TemplateDataTable->GetRowStruct() != FHeistForgeryTemplateRow::StaticStruct())
	{
		return false;
	}

	TSet<FName> UniqueTemplateIds;
	TArray<FName> LegacyM01TemplateIds;
	for (const FName RowName : TemplateDataTable->GetRowNames())
	{
		const FHeistForgeryTemplateRow* TemplateDefinition =
			TemplateDataTable->FindRow<FHeistForgeryTemplateRow>(RowName, TEXT("AHeistGameMode::GatherSurfaceTemplatePool"), false);
		if (TemplateDefinition == nullptr || TemplateDefinition->TemplateId.IsNone() || TemplateDefinition->TemplateId != RowName || TemplateDefinition->ReferenceImage.IsNull() ||
			(TemplateDefinition->BackgroundFilterMode == EHeistForgeryBackgroundFilter::None && TemplateDefinition->ReferenceMask.IsNull()))
		{
			continue;
		}

		if (TemplateDefinition->SurfacePoolId == PoolId && !UniqueTemplateIds.Contains(TemplateDefinition->TemplateId))
		{
			UniqueTemplateIds.Add(TemplateDefinition->TemplateId);
			OutTemplateIds.Add(TemplateDefinition->TemplateId);
		}
		else if (PoolId == FName(TEXT("M01")) && TemplateDefinition->SurfacePoolId.IsNone())
		{
			LegacyM01TemplateIds.AddUnique(TemplateDefinition->TemplateId);
		}
	}

	// Keeps the pre-W5 prototype usable until its JSON source is reimported with SurfacePoolId=M01.
	if (OutTemplateIds.IsEmpty())
	{
		OutTemplateIds = MoveTemp(LegacyM01TemplateIds);
	}
	OutTemplateIds.Sort(
		[](const FName Left, const FName Right)
		{
			return Left.ToString() < Right.ToString();
		});
	return !OutTemplateIds.IsEmpty();
}

bool AHeistGameMode::TryGetObjectAssemblyPartDefinition(const FName PartId, FHeistObjectAssemblyPartRow& OutPartDefinition) const
{
	OutPartDefinition = FHeistObjectAssemblyPartRow();
	if (PartId.IsNone())
	{
		return false;
	}

	const UDataTable* PartDataTable = GetObjectAssemblyPartDataTable();
	if (!IsValid(PartDataTable) || PartDataTable->GetRowStruct() != FHeistObjectAssemblyPartRow::StaticStruct())
	{
		return false;
	}

	const FHeistObjectAssemblyPartRow* PartDefinition =
		PartDataTable->FindRow<FHeistObjectAssemblyPartRow>(PartId, TEXT("AHeistGameMode::TryGetObjectAssemblyPartDefinition"), false);
	if (PartDefinition == nullptr || PartDefinition->PartId != PartId || PartDefinition->FamilyId.IsNone() || PartDefinition->StaticMesh.IsNull() ||
		PartDefinition->CompatibleSocketIds.IsEmpty() || PartDefinition->AllowedOrientationSteps.IsEmpty())
	{
		return false;
	}

	OutPartDefinition = *PartDefinition;
	return true;
}

bool AHeistGameMode::TryGetObjectAssemblyTemplateDefinition(const FName TemplateId, FHeistObjectAssemblyTemplateRow& OutTemplateDefinition) const
{
	OutTemplateDefinition = FHeistObjectAssemblyTemplateRow();
	if (TemplateId.IsNone())
	{
		return false;
	}

	const UDataTable* TemplateDataTable = GetObjectAssemblyTemplateDataTable();
	if (!IsValid(TemplateDataTable) || TemplateDataTable->GetRowStruct() != FHeistObjectAssemblyTemplateRow::StaticStruct())
	{
		return false;
	}

	const FHeistObjectAssemblyTemplateRow* TemplateDefinition =
		TemplateDataTable->FindRow<FHeistObjectAssemblyTemplateRow>(TemplateId, TEXT("AHeistGameMode::TryGetObjectAssemblyTemplateDefinition"), false);
	const float ScoreWeightTotal = TemplateDefinition == nullptr
									   ? 0.0f
									   : TemplateDefinition->RequiredPartWeight + TemplateDefinition->SocketTopologyWeight + TemplateDefinition->OrientationWeight +
											 TemplateDefinition->MaterialWeight;
	if (TemplateDefinition == nullptr || TemplateDefinition->TemplateId != TemplateId || TemplateDefinition->FamilyId.IsNone() || TemplateDefinition->DisplayName.IsEmpty() ||
		TemplateDefinition->CorePartId.IsNone() || !FMath::IsWithinInclusive(TemplateDefinition->RequiredParts.Num(), 3, 5) ||
		!FMath::IsFinite(TemplateDefinition->AssemblyDuration) || !FMath::IsWithinInclusive(TemplateDefinition->AssemblyDuration, 25.0f, 35.0f) ||
		!FMath::IsFinite(ScoreWeightTotal) || ScoreWeightTotal <= 0.0f)
	{
		return false;
	}

	OutTemplateDefinition = *TemplateDefinition;
	return true;
}

bool AHeistGameMode::TryGetLootDefinition(const FName ItemId, FHeistLootDataRow& OutLootDefinition) const
{
	OutLootDefinition = FHeistLootDataRow();
	if (ItemId.IsNone())
	{
		return false;
	}

	FHeistItemDataRow ItemDefinition;
	if (!TryGetItemDefinition(ItemId, ItemDefinition) || ItemDefinition.ItemType != EHeistItemType::Loot)
	{
		return false;
	}

	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset) ? GameBalanceDataAsset.Get() : GetDefault<UHeistGameBalanceDataAsset>();
	const UDataTable* LootDataTable = ResolvedBalanceData->LootDataTable.LoadSynchronous();
	if (!IsValid(LootDataTable) || LootDataTable->GetRowStruct() != FHeistLootDataRow::StaticStruct())
	{
		return false;
	}

	const FHeistLootDataRow* LootDefinition = LootDataTable->FindRow<FHeistLootDataRow>(ItemId, TEXT("AHeistGameMode::TryGetLootDefinition"), false);
	if (LootDefinition == nullptr || LootDefinition->ItemId != ItemId || LootDefinition->ScoreValue < 0 || LootDefinition->SpawnCategory == EHeistSpawnCategory::None ||
		LootDefinition->SpawnWeight < 0.0f || (ItemDefinition.bAvailableInV1 && LootDefinition->WorldMesh.IsNull()) || LootDefinition->WorldVisualRelativeTransform.ContainsNaN() ||
		LootDefinition->WorldMaterials.ContainsByPredicate([](const TSoftObjectPtr<UMaterialInterface>& Material) { return Material.IsNull(); }))
	{
		return false;
	}

	OutLootDefinition = *LootDefinition;
	return true;
}

bool AHeistGameMode::TryGetUsableItemDefinition(const FName ItemId, FHeistUsableItemDataRow& OutUsableItemDefinition) const
{
	OutUsableItemDefinition = FHeistUsableItemDataRow();
	if (ItemId.IsNone())
	{
		return false;
	}

	FHeistItemDataRow ItemDefinition;
	if (!TryGetItemDefinition(ItemId, ItemDefinition) || (ItemDefinition.ItemType != EHeistItemType::Throwable))
	{
		return false;
	}

	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset) ? GameBalanceDataAsset.Get() : GetDefault<UHeistGameBalanceDataAsset>();
	const UDataTable* UsableItemDataTable = ResolvedBalanceData->UsableItemDataTable.LoadSynchronous();
	if (!IsValid(UsableItemDataTable) || UsableItemDataTable->GetRowStruct() != FHeistUsableItemDataRow::StaticStruct())
	{
		return false;
	}

	const FHeistUsableItemDataRow* UsableItemDefinition = UsableItemDataTable->FindRow<FHeistUsableItemDataRow>(ItemId, TEXT("AHeistGameMode::TryGetUsableItemDefinition"), false);
	const bool bUseTypeMatchesItemType = UsableItemDefinition != nullptr && ((ItemDefinition.ItemType == EHeistItemType::Throwable && UsableItemDefinition->UseType == EHeistUseType::Throw));
	if (UsableItemDefinition == nullptr || UsableItemDefinition->ItemId != ItemId || !bUseTypeMatchesItemType || UsableItemDefinition->TargetType == EHeistTargetType::None ||
		UsableItemDefinition->Cooldown < 0.0f || UsableItemDefinition->CastTime < 0.0f || UsableItemDefinition->Duration < 0.0f || UsableItemDefinition->ProjectileSpeed < 0.0f ||
		(ItemDefinition.bAvailableInV1 && UsableItemDefinition->SpawnedActorClass.IsNull()))
	{
		return false;
	}

	OutUsableItemDefinition = *UsableItemDefinition;
	return true;
}

bool AHeistGameMode::TryGetGuardDefinition(const FName GuardProfileId, FHeistGuardDataRow& OutGuardDefinition) const
{
	OutGuardDefinition = FHeistGuardDataRow();
	if (GuardProfileId.IsNone())
	{
		return false;
	}

	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset) ? GameBalanceDataAsset.Get() : GetDefault<UHeistGameBalanceDataAsset>();
	const UDataTable* GuardDataTable = ResolvedBalanceData->GuardDataTable.LoadSynchronous();
	if (!IsValid(GuardDataTable) || GuardDataTable->GetRowStruct() != FHeistGuardDataRow::StaticStruct())
	{
		return false;
	}

	const FHeistGuardDataRow* GuardDefinition = GuardDataTable->FindRow<FHeistGuardDataRow>(GuardProfileId, TEXT("AHeistGameMode::TryGetGuardDefinition"), false);
	if (GuardDefinition == nullptr || GuardDefinition->GuardProfileId != GuardProfileId)
	{
		return false;
	}

	OutGuardDefinition = *GuardDefinition;
	return true;
}

bool AHeistGameMode::TryGetSoundPingDefinition(const FName SoundPingId, FHeistSoundPingDataRow& OutSoundPingDefinition) const
{
	OutSoundPingDefinition = FHeistSoundPingDataRow();
	if (SoundPingId.IsNone())
	{
		return false;
	}

	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset) ? GameBalanceDataAsset.Get() : GetDefault<UHeistGameBalanceDataAsset>();
	const UDataTable* SoundPingDataTable = ResolvedBalanceData->SoundPingDataTable.LoadSynchronous();
	if (!IsValid(SoundPingDataTable) || SoundPingDataTable->GetRowStruct() != FHeistSoundPingDataRow::StaticStruct())
	{
		return false;
	}

	const FHeistSoundPingDataRow* SoundPingDefinition = SoundPingDataTable->FindRow<FHeistSoundPingDataRow>(SoundPingId, TEXT("AHeistGameMode::TryGetSoundPingDefinition"), false);
	if (SoundPingDefinition == nullptr || SoundPingDefinition->SoundPingId != SoundPingId)
	{
		return false;
	}

	OutSoundPingDefinition = *SoundPingDefinition;
	return true;
}

bool AHeistGameMode::TryGetPlayerCountDifficultyBaseline(const int32 PlayerCount, FHeistPlayerCountDifficultyBaseline& OutBaseline) const
{
	const UHeistGameBalanceDataAsset* BalanceData = ResolveGameBalanceData();
	return IsValid(BalanceData) && BalanceData->TryGetPlayerCountDifficultyBaseline(PlayerCount, OutBaseline);
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

	bool bValid = BalanceData->bAllowSoloProgression && BalanceData->PlayerCountDifficultyBaselines.Num() == 4;
	TSet<int32> SeenPlayerCounts;
	for (const FHeistPlayerCountDifficultyBaseline& Baseline : BalanceData->PlayerCountDifficultyBaselines)
	{
		const bool bRowValid = Baseline.PlayerCount >= 1 && Baseline.PlayerCount <= 4 && !SeenPlayerCounts.Contains(Baseline.PlayerCount) && FMath::IsFinite(Baseline.GuardCountMultiplier) &&
							   Baseline.GuardCountMultiplier > 0.0f && FMath::IsFinite(Baseline.DetectionMultiplier) && Baseline.DetectionMultiplier > 0.0f &&
							   FMath::IsFinite(Baseline.InspectionDurationMultiplier) && Baseline.InspectionDurationMultiplier > 0.0f;
		SeenPlayerCounts.Add(Baseline.PlayerCount);
		bValid = bValid && bRowValid;
		UE_LOG(LogHeist, Log, TEXT("Difficulty baseline row: Players=%d GuardCount=%.2f Detection=%.2f InspectionDuration=%.2f Valid=%s"), Baseline.PlayerCount, Baseline.GuardCountMultiplier,
			   Baseline.DetectionMultiplier, Baseline.InspectionDurationMultiplier, bRowValid ? TEXT("true") : TEXT("false"));
	}

	const int32 ConnectedPlayerCount = HeistGameState->GetConnectedPlayerCount();
	FHeistPlayerCountDifficultyBaseline ResolvedBaseline;
	const bool bResolved = TryGetPlayerCountDifficultyBaseline(ConnectedPlayerCount, ResolvedBaseline);
	bValid = bValid && SeenPlayerCounts.Num() == 4 && bResolved;
	const FString Summary = FString::Printf(
		TEXT("Difficulty baseline dump: ConnectedPlayers=%d ResolvedPlayers=%d GuardCount=%.2f Detection=%.2f InspectionDuration=%.2f SoloAllowed=%s MandatoryPlayers=1 Rows=%d Result=%s"),
		ConnectedPlayerCount, ResolvedBaseline.PlayerCount, ResolvedBaseline.GuardCountMultiplier, ResolvedBaseline.DetectionMultiplier, ResolvedBaseline.InspectionDurationMultiplier,
		BalanceData->bAllowSoloProgression ? TEXT("true") : TEXT("false"), BalanceData->PlayerCountDifficultyBaselines.Num(), bValid ? TEXT("PASS") : TEXT("FAIL"));
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

bool AHeistGameMode::TrySpawnDroppedLoot(const FHeistLootDropRequest& DropRequest, AHeistLootActor*& OutDroppedLootActor) const
{
	OutDroppedLootActor = nullptr;
	if (!HasAuthority() || DropRequest.ItemId.IsNone() || !IsValid(DropRequest.DroppedBy))
	{
		return false;
	}

	FHeistItemDataRow ItemDefinition;
	FHeistLootDataRow LootDefinition;
	if (!TryGetItemDefinition(DropRequest.ItemId, ItemDefinition) || ItemDefinition.ItemType != EHeistItemType::Loot || !TryGetLootDefinition(DropRequest.ItemId, LootDefinition))
	{
		return false;
	}

	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset) ? GameBalanceDataAsset.Get() : GetDefault<UHeistGameBalanceDataAsset>();
	UClass* LootActorClass = ResolveWorldLootShellClass(ResolvedBalanceData);
	if (!IsValid(LootActorClass))
	{
		return false;
	}

	UDataTable* LootDataTable = ResolvedBalanceData->LootDataTable.LoadSynchronous();
	const FTransform SpawnTransform(FRotator::ZeroRotator, FVector(DropRequest.DropOrigin));
	AHeistLootActor* DroppedLootActor =
		GetWorld()->SpawnActorDeferred<AHeistLootActor>(LootActorClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
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
	const UDataTable* LootDataTable = IsValid(BalanceData) ? BalanceData->LootDataTable.LoadSynchronous() : nullptr;
	const UDataTable* UsableItemDataTable = IsValid(BalanceData) ? BalanceData->UsableItemDataTable.LoadSynchronous() : nullptr;
	if (ItemDataTable->GetRowStruct() != FHeistItemDataRow::StaticStruct() || !IsValid(LootDataTable) || LootDataTable->GetRowStruct() != FHeistLootDataRow::StaticStruct() ||
		!IsValid(UsableItemDataTable) || UsableItemDataTable->GetRowStruct() != FHeistUsableItemDataRow::StaticStruct())
	{
		UE_LOG(LogHeistInventory, Error, TEXT("Item data validation completed: Result=FAIL Reason=MissingOrInvalidTableSchema ItemTable=%s LootTable=%s UsableTable=%s"), *GetNameSafe(ItemDataTable),
			   *GetNameSafe(LootDataTable), *GetNameSafe(UsableItemDataTable));
		return;
	}

	const TArray<FName> RowNames = ItemDataTable->GetRowNames();
	if (RowNames.IsEmpty())
	{
		UE_LOG(LogHeistInventory, Error, TEXT("Item data validation completed: Table=%s TotalRows=0 ValidRows=0 InvalidRows=0 Result=FAIL Reason=EmptyTable"), *GetNameSafe(ItemDataTable));
		return;
	}

	int32 ValidRowCount = 0;
	int32 ReleaseLootRowCount = 0;
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
			bValidExtension = bHasLootExtension && !bHasUsableExtension && TryGetLootDefinition(RowName, LootDefinition);
		}
		else if (ItemDefinition.ItemType == EHeistItemType::Throwable)
		{
			FHeistUsableItemDataRow UsableItemDefinition;
			bValidExtension = !bHasLootExtension && bHasUsableExtension && TryGetUsableItemDefinition(RowName, UsableItemDefinition);
		}

		if (bValidExtension)
		{
			++ValidRowCount;
			if (ItemDefinition.ItemType == EHeistItemType::Loot && ItemDefinition.bAvailableInV1)
			{
				++ReleaseLootRowCount;
			}
		}
		else
		{
			UE_LOG(LogHeistInventory, Error, TEXT("Item data validation rejected row: ItemId=%s Type=%d HasLootExtension=%s HasUsableExtension=%s"), *RowName.ToString(),
				   static_cast<int32>(ItemDefinition.ItemType), bHasLootExtension ? TEXT("true") : TEXT("false"), bHasUsableExtension ? TEXT("true") : TEXT("false"));
		}
	}

	int32 OrphanExtensionCount = 0;
	for (const FName RowName : LootDataTable->GetRowNames())
	{
		if (!RowNames.Contains(RowName))
		{
			++OrphanExtensionCount;
			UE_LOG(LogHeistInventory, Error, TEXT("Item data validation rejected orphan Loot row: ItemId=%s"), *RowName.ToString());
		}
	}
	for (const FName RowName : UsableItemDataTable->GetRowNames())
	{
		if (!RowNames.Contains(RowName))
		{
			++OrphanExtensionCount;
			UE_LOG(LogHeistInventory, Error, TEXT("Item data validation rejected orphan Usable row: ItemId=%s"), *RowName.ToString());
		}
	}

	constexpr int32 MinimumReleaseLootRowCount = 5;
	const bool bReleaseLootCountValid = ReleaseLootRowCount >= MinimumReleaseLootRowCount;
	if (!bReleaseLootCountValid)
	{
		UE_LOG(LogHeistInventory, Error, TEXT("Item data validation rejected release loot set: ReleaseLootRows=%d RequiredMinimum=%d"), ReleaseLootRowCount, MinimumReleaseLootRowCount);
	}

	const int32 InvalidRowCount = RowNames.Num() - ValidRowCount + OrphanExtensionCount + (bReleaseLootCountValid ? 0 : 1);
	if (InvalidRowCount > 0)
	{
		UE_LOG(LogHeistInventory, Error,
			   TEXT("Item data validation completed: ItemTable=%s LootTable=%s UsableTable=%s TotalItems=%d ValidItems=%d ReleaseLootRows=%d InvalidRows=%d OrphanExtensions=%d Result=FAIL"),
			   *GetNameSafe(ItemDataTable), *GetNameSafe(LootDataTable), *GetNameSafe(UsableItemDataTable), RowNames.Num(), ValidRowCount, ReleaseLootRowCount, InvalidRowCount,
			   OrphanExtensionCount);
		return;
	}

	UE_LOG(LogHeistInventory, Log,
		   TEXT("Item data validation completed: ItemTable=%s LootTable=%s UsableTable=%s TotalItems=%d ValidItems=%d ReleaseLootRows=%d InvalidRows=0 OrphanExtensions=0 Result=PASS"),
		   *GetNameSafe(ItemDataTable), *GetNameSafe(LootDataTable), *GetNameSafe(UsableItemDataTable), RowNames.Num(), ValidRowCount, ReleaseLootRowCount);
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
	const float SpawnServerTime = IsValid(HeistGameState) ? HeistGameState->GetServerWorldTimeSeconds() + SafeWarningDelay : GetWorld()->GetTimeSeconds() + SafeWarningDelay;
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
		WarningDelegate.BindUObject(this, &AHeistGameMode::BeginRareLootWarning, EventIndex, ScheduledSpawnServerTime);
		GetWorldTimerManager().SetTimer(WarningTimerHandle, WarningDelegate, WarningDelay, false);

		FTimerHandle& SpawnTimerHandle = RareLootSpawnTimerHandles.AddDefaulted_GetRef();
		FTimerDelegate SpawnDelegate;
		SpawnDelegate.BindUObject(this, &AHeistGameMode::TriggerRareLootEvent, EventIndex);
		GetWorldTimerManager().SetTimer(SpawnTimerHandle, SpawnDelegate, SpawnDelay, false);
	}

	UHeistDebugFunctionLibrary::DebugRareLootTimersStarted(this, BalanceData->RareLootEventTimes, WarningLeadTime);
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
	UHeistDebugFunctionLibrary::DebugRareLootWarningStarted(this, EventIndex, BalanceData->RareLootItemId, ScheduledSpawnTime);
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
	RareLootActor->GetLootPickupCommittedDelegate().AddUObject(this, &AHeistGameMode::HandleRareLootPickedUp);

	AHeistGameState* HeistGameState = GetGameState<AHeistGameState>();
	const UHeistGameBalanceDataAsset* BalanceData = ResolveGameBalanceData();
	checkf(IsValid(HeistGameState), TEXT("Rare Loot event requires AHeistGameState."));
	checkf(IsValid(BalanceData), TEXT("Rare Loot event requires balance data."));
	HeistGameState->ActivateRareLootMarker(EventIndex, BalanceData->RareLootItemId, RareLootActor->GetActorLocation());
	UHeistDebugFunctionLibrary::DebugRareLootSpawned(this, EventIndex, RareLootActor, SpawnPoint, BalanceData->RareLootItemId, RareLootActor->GetActorLocation());
}

bool AHeistGameMode::TrySpawnRareLoot(const int32 EventIndex, AHeistLootActor*& OutRareLootActor, AHeistLootSpawnPoint*& OutSpawnPoint)
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
	if (!TryGetItemDefinition(BalanceData->RareLootItemId, ItemDefinition) || ItemDefinition.ItemType != EHeistItemType::Loot || !TryGetLootDefinition(BalanceData->RareLootItemId, LootDefinition) ||
		LootDefinition.SpawnCategory != EHeistSpawnCategory::RareEvent)
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
	UClass* LootActorClass = ResolveWorldLootShellClass(BalanceData);
	if (!IsValid(LootActorClass))
	{
		UHeistDebugFunctionLibrary::DebugRareLootEventFailed(this, EventIndex, TEXT("MissingWorldLootShell"));
		return false;
	}

	UDataTable* LootDataTable = BalanceData->LootDataTable.LoadSynchronous();
	if (!IsValid(LootDataTable))
	{
		UHeistDebugFunctionLibrary::DebugRareLootEventFailed(this, EventIndex, TEXT("MissingLootDataTable"));
		return false;
	}

	const FTransform SpawnTransform = OutSpawnPoint->GetActorTransform();
	AHeistLootActor* DeferredLootActor =
		GetWorld()->SpawnActorDeferred<AHeistLootActor>(LootActorClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!IsValid(DeferredLootActor))
	{
		UHeistDebugFunctionLibrary::DebugRareLootEventFailed(this, EventIndex, TEXT("DeferredSpawnFailed"));
		return false;
	}

	DeferredLootActor->InitializeLootData(LootDataTable, BalanceData->RareLootItemId);
	OutRareLootActor = Cast<AHeistLootActor>(UGameplayStatics::FinishSpawningActor(DeferredLootActor, SpawnTransform));
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
	UHeistDebugFunctionLibrary::DebugRareLootPickedUp(this, EventIndex, LootActor, Requester, LootActor->GetLootRowId());
}

const UHeistGameBalanceDataAsset* AHeistGameMode::ResolveGameBalanceData() const
{
	return IsValid(GameBalanceDataAsset) ? GameBalanceDataAsset.Get() : GetDefault<UHeistGameBalanceDataAsset>();
}

#pragma endregion

#pragma region EscapePhase

float AHeistGameMode::GetEscapeCastTimeSeconds() const
{
	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset) ? GameBalanceDataAsset.Get() : GetDefault<UHeistGameBalanceDataAsset>();

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

	TimerManager.SetTimer(EscapePhaseTimerHandle, this, &AHeistGameMode::HandleEscapePhaseTimerElapsed, EscapePhaseDelaySeconds, false);

	UE_LOG(LogHeist, Log, TEXT("Escape phase timer started: Delay=%.2f BalanceData=%s"), EscapePhaseDelaySeconds, *GetNameSafe(GameBalanceDataAsset));
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
	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset) ? GameBalanceDataAsset.Get() : GetDefault<UHeistGameBalanceDataAsset>();

	return FMath::Max(0.0f, ResolvedBalanceData->VentUnlockTime);
}

#pragma endregion
