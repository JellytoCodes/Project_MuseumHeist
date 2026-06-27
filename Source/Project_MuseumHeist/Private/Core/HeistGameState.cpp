#include "Core/HeistGameState.h"

#include "Debug/HeistDebugFunctionLibrary.h"
#include "Core/HeistLogChannels.h"
#include "Core/HeistPlayerState.h"
#include "Net/UnrealNetwork.h"

#pragma region Construction

AHeistGameState::AHeistGameState()
{
}

#pragma endregion

#pragma region PlayerConnections

int32 AHeistGameState::GetConnectedPlayerCount() const
{
	return PlayerArray.Num();
}

#pragma endregion

#pragma region EscapePhase

bool AHeistGameState::IsEscapePhaseOpen() const
{
	return bEscapePhaseOpen;
}

float AHeistGameState::GetEscapePhaseDelaySeconds() const
{
	return EscapePhaseDelaySeconds;
}

float AHeistGameState::GetEscapePhaseOpenTimeSeconds() const
{
	return EscapePhaseOpenTimeSeconds;
}

void AHeistGameState::InitializeEscapePhase(float InDelaySeconds)
{
	if (!HasAuthority())
	{
		UE_LOG(
			LogHeistNetwork,
			Warning,
			TEXT("Escape phase initialization rejected: GameState=%s Reason=NotAuthority"),
			*GetNameSafe(this));
		return;
	}

	const bool bWasEscapePhaseOpen = bEscapePhaseOpen;
	bEscapePhaseOpen = false;
	EscapePhaseDelaySeconds = FMath::Max(0.0f, InDelaySeconds);
	EscapePhaseOpenTimeSeconds = -1.0f;
	ForceNetUpdate();

	if (bWasEscapePhaseOpen)
	{
		EscapePhaseStateChangedDelegate.Broadcast(false);
	}
}

void AHeistGameState::OpenEscapePhase()
{
	if (!HasAuthority())
	{
		UE_LOG(
			LogHeistNetwork,
			Warning,
			TEXT("Escape phase open rejected: GameState=%s Reason=NotAuthority"),
			*GetNameSafe(this));
		return;
	}

	if (bEscapePhaseOpen)
	{
		return;
	}

	bEscapePhaseOpen = true;
	EscapePhaseOpenTimeSeconds = GetServerWorldTimeSeconds();
	ForceNetUpdate();
	EscapePhaseStateChangedDelegate.Broadcast(true);

	UE_LOG(
		LogHeist,
		Log,
		TEXT("Escape phase opened: GameState=%s ServerTime=%.2f Delay=%.2f"),
		*GetNameSafe(this),
		EscapePhaseOpenTimeSeconds,
		EscapePhaseDelaySeconds);
}

FHeistEscapePhaseStateChanged& AHeistGameState::GetEscapePhaseStateChangedDelegate()
{
	return EscapePhaseStateChangedDelegate;
}

void AHeistGameState::OnRep_EscapePhaseOpen()
{
	EscapePhaseStateChangedDelegate.Broadcast(bEscapePhaseOpen);

	UE_LOG(
		LogHeistNetwork,
		Log,
		TEXT("Escape phase replicated: GameState=%s IsOpen=%s OpenTime=%.2f Delay=%.2f"),
		*GetNameSafe(this),
		bEscapePhaseOpen ? TEXT("true") : TEXT("false"),
		EscapePhaseOpenTimeSeconds,
		EscapePhaseDelaySeconds);
}

#pragma endregion

#pragma region RareLootEvent

const FHeistRareLootEventState& AHeistGameState::GetRareLootEventState() const
{
	return RareLootEventState;
}

void AHeistGameState::BeginRareLootWarning(
	const int32 EventIndex,
	const FName ItemId,
	const float SpawnServerTime)
{
	if (!HasAuthority())
	{
		return;
	}

	RareLootEventState.EventIndex = EventIndex;
	RareLootEventState.ItemId = ItemId;
	RareLootEventState.WorldLocation = FVector::ZeroVector;
	RareLootEventState.SpawnServerTime = SpawnServerTime;
	RareLootEventState.bIncomingWarningActive = true;
	RareLootEventState.bDirectionMarkerActive = false;
	ForceNetUpdate();
	BroadcastRareLootEventState();
}

void AHeistGameState::ActivateRareLootMarker(
	const int32 EventIndex,
	const FName ItemId,
	const FVector& WorldLocation)
{
	if (!HasAuthority())
	{
		return;
	}

	RareLootEventState.EventIndex = EventIndex;
	RareLootEventState.ItemId = ItemId;
	RareLootEventState.WorldLocation = WorldLocation;
	RareLootEventState.SpawnServerTime = GetServerWorldTimeSeconds();
	RareLootEventState.bIncomingWarningActive = false;
	RareLootEventState.bDirectionMarkerActive = true;
	ForceNetUpdate();
	BroadcastRareLootEventState();
}

void AHeistGameState::DeactivateRareLootMarker(const int32 EventIndex)
{
	if (!HasAuthority() || RareLootEventState.EventIndex != EventIndex)
	{
		return;
	}

	RareLootEventState.bIncomingWarningActive = false;
	RareLootEventState.bDirectionMarkerActive = false;
	ForceNetUpdate();
	BroadcastRareLootEventState();
}

FHeistRareLootEventStateChanged& AHeistGameState::GetRareLootEventStateChangedDelegate()
{
	return RareLootEventStateChangedDelegate;
}

void AHeistGameState::OnRep_RareLootEventState()
{
	BroadcastRareLootEventState();
	UHeistDebugFunctionLibrary::DebugRareLootStateReplicated(this, RareLootEventState);
}

void AHeistGameState::BroadcastRareLootEventState()
{
	RareLootEventStateChangedDelegate.Broadcast(RareLootEventState);
}

#pragma endregion

#pragma region GapTracker

bool AHeistGameState::IsGapTrackerActive() const
{
	return bGapTrackerActive;
}

int32 AHeistGameState::GetGapTrackerLeaderPlayerId() const
{
	return GapTrackerLeaderPlayerId;
}

void AHeistGameState::SetGapTrackerState(const bool bInActive, const int32 InLeaderPlayerId)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 ResolvedLeaderPlayerId = bInActive ? InLeaderPlayerId : INDEX_NONE;
	if (bGapTrackerActive == bInActive && GapTrackerLeaderPlayerId == ResolvedLeaderPlayerId)
	{
		return;
	}

	bGapTrackerActive = bInActive;
	GapTrackerLeaderPlayerId = ResolvedLeaderPlayerId;
	ForceNetUpdate();
	BroadcastGapTrackerState();
	UHeistDebugFunctionLibrary::DebugGapTrackerStateChanged(
		this,
		bGapTrackerActive,
		GapTrackerLeaderPlayerId);
}

FHeistGapTrackerStateChanged& AHeistGameState::GetGapTrackerStateChangedDelegate()
{
	return GapTrackerStateChangedDelegate;
}

void AHeistGameState::OnRep_GapTrackerState()
{
	BroadcastGapTrackerState();
	UHeistDebugFunctionLibrary::DebugGapTrackerStateReplicated(
		this,
		bGapTrackerActive,
		GapTrackerLeaderPlayerId);
}

void AHeistGameState::BroadcastGapTrackerState()
{
	GapTrackerStateChangedDelegate.Broadcast(bGapTrackerActive, GapTrackerLeaderPlayerId);
}

#pragma endregion

#pragma region SoundPing

void AHeistGameState::ReportSoundPing(const FHeistSoundPingEvent& SoundPingEvent)
{
	if (!HasAuthority())
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Sound ping report rejected: GameState=%s Reason=NotAuthority"), *GetNameSafe(this));
		return;
	}

	LastSoundPingEvent = SoundPingEvent;
	LastSoundPingEvent.SequenceId = NextSoundPingSequenceId++;
	LastSoundPingEvent.ServerTimeSeconds = GetServerWorldTimeSeconds();
	ForceNetUpdate();
	SoundPingEventReportedDelegate.Broadcast(LastSoundPingEvent);
	UHeistDebugFunctionLibrary::DebugSoundPingReported(this, LastSoundPingEvent);
}

const FHeistSoundPingEvent& AHeistGameState::GetLastSoundPingEvent() const
{
	return LastSoundPingEvent;
}

FHeistSoundPingEventReported& AHeistGameState::GetSoundPingEventReportedDelegate()
{
	return SoundPingEventReportedDelegate;
}

void AHeistGameState::OnRep_LastSoundPingEvent()
{
	SoundPingEventReportedDelegate.Broadcast(LastSoundPingEvent);
	UHeistDebugFunctionLibrary::DebugSoundPingReplicated(this, LastSoundPingEvent);
}

#pragma endregion

#pragma region ResultData

void AHeistGameState::RebuildPlayerResults()
{
	if (!HasAuthority())
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Player result rebuild rejected: GameState=%s Reason=NotAuthority"), *GetNameSafe(this));
		return;
	}

	TArray<FHeistPlayerResult> NewPlayerResults;
	NewPlayerResults.Reserve(PlayerArray.Num());

	for (APlayerState* PlayerState : PlayerArray)
	{
		const AHeistPlayerState* HeistPlayerState = Cast<AHeistPlayerState>(PlayerState);
		if (!IsValid(HeistPlayerState))
		{
			continue;
		}

		FHeistPlayerResult& PlayerResult = NewPlayerResults.AddDefaulted_GetRef();
		PlayerResult.PlayerId = HeistPlayerState->HeistPlayerId;
		PlayerResult.LootScore = HeistPlayerState->GetTotalLootScore();
		PlayerResult.FinalScore = HeistPlayerState->GetFinalScore();
		PlayerResult.LootWeight = HeistPlayerState->GetTotalLootWeight();
		PlayerResult.EscapeTimeSeconds = HeistPlayerState->GetEscapeTimeSeconds();
		PlayerResult.bEscaped = HeistPlayerState->IsEscaped();
	}

	NewPlayerResults.Sort([](const FHeistPlayerResult& Left, const FHeistPlayerResult& Right)
	{
		if (Left.bEscaped != Right.bEscaped)
		{
			return Left.bEscaped;
		}

		if (Left.FinalScore != Right.FinalScore)
		{
			return Left.FinalScore > Right.FinalScore;
		}

		if (Left.bEscaped && Left.EscapeTimeSeconds != Right.EscapeTimeSeconds)
		{
			return Left.EscapeTimeSeconds < Right.EscapeTimeSeconds;
		}

		return Left.PlayerId < Right.PlayerId;
	});

	for (int32 ResultIndex = 0; ResultIndex < NewPlayerResults.Num(); ++ResultIndex)
	{
		FHeistPlayerResult& PlayerResult = NewPlayerResults[ResultIndex];
		PlayerResult.Rank = ResultIndex + 1;

		for (APlayerState* PlayerState : PlayerArray)
		{
			AHeistPlayerState* HeistPlayerState = Cast<AHeistPlayerState>(PlayerState);
			if (IsValid(HeistPlayerState) && HeistPlayerState->HeistPlayerId == PlayerResult.PlayerId)
			{
				HeistPlayerState->SetPlayerRank(PlayerResult.Rank);
				break;
			}
		}
	}

	PlayerResults = MoveTemp(NewPlayerResults);
	ForceNetUpdate();
	PlayerResultsChangedDelegate.Broadcast();

	for (const FHeistPlayerResult& PlayerResult : PlayerResults)
	{
		UE_LOG(
			LogHeist,
			Log,
			TEXT("Player result ranked: PlayerId=%d Rank=%d Escaped=%s LootScore=%d FinalScore=%d LootWeight=%.2f EscapeTime=%.2f"),
			PlayerResult.PlayerId,
			PlayerResult.Rank,
			PlayerResult.bEscaped ? TEXT("true") : TEXT("false"),
			PlayerResult.LootScore,
			PlayerResult.FinalScore,
			PlayerResult.LootWeight,
			PlayerResult.EscapeTimeSeconds);
	}

	UE_LOG(
		LogHeist,
		Log,
		TEXT("Player results rebuilt: GameState=%s PlayerCount=%d WinnerPlayerId=%d"),
		*GetNameSafe(this),
		PlayerResults.Num(),
		GetWinnerPlayerId());
}

const TArray<FHeistPlayerResult>& AHeistGameState::GetPlayerResults() const
{
	return PlayerResults;
}

int32 AHeistGameState::GetWinnerPlayerId() const
{
	return !PlayerResults.IsEmpty() && PlayerResults[0].bEscaped
		? PlayerResults[0].PlayerId
		: INDEX_NONE;
}

FHeistPlayerResultsChanged& AHeistGameState::GetPlayerResultsChangedDelegate()
{
	return PlayerResultsChangedDelegate;
}

void AHeistGameState::OnRep_PlayerResults()
{
	PlayerResultsChangedDelegate.Broadcast();

	for (const FHeistPlayerResult& PlayerResult : PlayerResults)
	{
		UE_LOG(
			LogHeistNetwork,
			Log,
			TEXT("Player result replicated entry: PlayerId=%d Rank=%d Escaped=%s LootScore=%d FinalScore=%d LootWeight=%.2f EscapeTime=%.2f"),
			PlayerResult.PlayerId,
			PlayerResult.Rank,
			PlayerResult.bEscaped ? TEXT("true") : TEXT("false"),
			PlayerResult.LootScore,
			PlayerResult.FinalScore,
			PlayerResult.LootWeight,
			PlayerResult.EscapeTimeSeconds);
	}

	UE_LOG(
		LogHeistNetwork,
		Log,
		TEXT("Player results replicated: GameState=%s PlayerCount=%d WinnerPlayerId=%d"),
		*GetNameSafe(this),
		PlayerResults.Num(),
		GetWinnerPlayerId());
}

#pragma endregion

#pragma region Replication

void AHeistGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeistGameState, bEscapePhaseOpen);
	DOREPLIFETIME(AHeistGameState, EscapePhaseDelaySeconds);
	DOREPLIFETIME(AHeistGameState, EscapePhaseOpenTimeSeconds);
	DOREPLIFETIME(AHeistGameState, RareLootEventState);
	DOREPLIFETIME(AHeistGameState, bGapTrackerActive);
	DOREPLIFETIME(AHeistGameState, GapTrackerLeaderPlayerId);
	DOREPLIFETIME(AHeistGameState, LastSoundPingEvent);
	DOREPLIFETIME(AHeistGameState, PlayerResults);
}

#pragma endregion
