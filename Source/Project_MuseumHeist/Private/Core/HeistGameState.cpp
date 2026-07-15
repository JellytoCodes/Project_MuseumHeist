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

void AHeistGameState::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);
	PlayerConnectionsChangedDelegate.Broadcast(GetConnectedPlayerCount());
}

void AHeistGameState::RemovePlayerState(APlayerState* PlayerState)
{
	Super::RemovePlayerState(PlayerState);
	PlayerConnectionsChangedDelegate.Broadcast(GetConnectedPlayerCount());
}

int32 AHeistGameState::GetConnectedPlayerCount() const
{
	return PlayerArray.Num();
}

FHeistPlayerConnectionsChanged& AHeistGameState::GetPlayerConnectionsChangedDelegate()
{
	return PlayerConnectionsChangedDelegate;
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
		PlayerResult.bArrested = HeistPlayerState->IsArrested();
	}

	NewPlayerResults.Sort([](const FHeistPlayerResult& Left, const FHeistPlayerResult& Right)
	{
		return Left.PlayerId < Right.PlayerId;
	});

	PlayerResults = MoveTemp(NewPlayerResults);
	ForceNetUpdate();
	PlayerResultsChangedDelegate.Broadcast();

	for (const FHeistPlayerResult& PlayerResult : PlayerResults)
	{
		UE_LOG(
			LogHeist,
			Log,
			TEXT("Player contribution result: PlayerId=%d Escaped=%s Arrested=%s LootScore=%d FinalScore=%d LootWeight=%.2f EscapeTime=%.2f"),
			PlayerResult.PlayerId,
			PlayerResult.bEscaped ? TEXT("true") : TEXT("false"),
			PlayerResult.bArrested ? TEXT("true") : TEXT("false"),
			PlayerResult.LootScore,
			PlayerResult.FinalScore,
			PlayerResult.LootWeight,
			PlayerResult.EscapeTimeSeconds);
	}

	int32 ArrestedPlayerCount = 0;
	int32 EscapedPlayerCount = 0;
	for (const FHeistPlayerResult& PlayerResult : PlayerResults)
	{
		ArrestedPlayerCount += PlayerResult.bArrested ? 1 : 0;
		EscapedPlayerCount += PlayerResult.bEscaped ? 1 : 0;
	}
	const int32 ActivePlayerCount = PlayerResults.Num() - ArrestedPlayerCount - EscapedPlayerCount;
	const bool bAllPlayersArrested = PlayerResults.Num() > 0 && ArrestedPlayerCount == PlayerResults.Num();
	UE_LOG(
		LogHeist,
		Log,
		TEXT("Team arrest state: Players=%d Arrested=%d Escaped=%d Active=%d AllArrested=%s FailureEligible=%s"),
		PlayerResults.Num(),
		ArrestedPlayerCount,
		EscapedPlayerCount,
		ActivePlayerCount,
		bAllPlayersArrested ? TEXT("true") : TEXT("false"),
		bAllPlayersArrested ? TEXT("true") : TEXT("false"));

	UE_LOG(
		LogHeist,
		Log,
		TEXT("Player contribution results rebuilt: GameState=%s PlayerCount=%d"),
		*GetNameSafe(this),
		PlayerResults.Num());
}

const TArray<FHeistPlayerResult>& AHeistGameState::GetPlayerResults() const
{
	return PlayerResults;
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
			TEXT("Player contribution result replicated: PlayerId=%d Escaped=%s Arrested=%s LootScore=%d FinalScore=%d LootWeight=%.2f EscapeTime=%.2f"),
			PlayerResult.PlayerId,
			PlayerResult.bEscaped ? TEXT("true") : TEXT("false"),
			PlayerResult.bArrested ? TEXT("true") : TEXT("false"),
			PlayerResult.LootScore,
			PlayerResult.FinalScore,
			PlayerResult.LootWeight,
			PlayerResult.EscapeTimeSeconds);
	}

	UE_LOG(
		LogHeistNetwork,
		Log,
		TEXT("Player contribution results replicated: GameState=%s PlayerCount=%d"),
		*GetNameSafe(this),
		PlayerResults.Num());
}

#pragma endregion

#pragma region Replication

void AHeistGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeistGameState, bEscapePhaseOpen);
	DOREPLIFETIME(AHeistGameState, EscapePhaseDelaySeconds);
	DOREPLIFETIME(AHeistGameState, EscapePhaseOpenTimeSeconds);
	DOREPLIFETIME(AHeistGameState, LastSoundPingEvent);
	DOREPLIFETIME(AHeistGameState, PlayerResults);
}

#pragma endregion
