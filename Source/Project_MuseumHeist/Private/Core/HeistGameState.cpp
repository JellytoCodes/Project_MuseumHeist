#include "Core/HeistGameState.h"

#include "Core/HeistLogChannels.h"
#include "Core/HeistPlayerState.h"
#include "Net/UnrealNetwork.h"

namespace
{
#if !UE_BUILD_SHIPPING
void LogSoundPingEvent(const TCHAR* Phase, const FHeistSoundPingEvent& SoundPingEvent)
{
	const FString Message = FString::Printf(
		TEXT("Sound ping %s: SequenceId=%d Type=%d Tag=%s Location=(%.1f,%.1f,%.1f) Radius=%.1f Duration=%.2f AffectsGuards=%s ServerTime=%.2f"),
		Phase,
		SoundPingEvent.SequenceId,
		static_cast<int32>(SoundPingEvent.PingType),
		*SoundPingEvent.SoundPingTag.ToString(),
		SoundPingEvent.WorldLocation.X,
		SoundPingEvent.WorldLocation.Y,
		SoundPingEvent.WorldLocation.Z,
		SoundPingEvent.Radius,
		SoundPingEvent.Duration,
		SoundPingEvent.bAffectsGuards ? TEXT("true") : TEXT("false"),
		SoundPingEvent.ServerTimeSeconds);

	if (SoundPingEvent.PingType == EHeistSoundPingType::Footstep)
	{
		UE_LOG(LogHeistNetwork, Verbose, TEXT("%s"), *Message);
		return;
	}

	UE_LOG(LogHeistNetwork, Log, TEXT("%s"), *Message);
}
#endif
}

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

#pragma region MatchPhase

EHeistMatchPhase AHeistGameState::GetMatchPhase() const
{
	return MatchPhase;
}

bool AHeistGameState::SetMatchPhase(const EHeistMatchPhase NewMatchPhase)
{
	if (!HasAuthority())
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Match phase change rejected: GameState=%s Reason=NotAuthority"), *GetNameSafe(this));
		return false;
	}

	if (MatchPhase == NewMatchPhase)
	{
		return true;
	}

	const EHeistMatchPhase PreviousMatchPhase = MatchPhase;
	MatchPhase = NewMatchPhase;
	ForceNetUpdate();
	BroadcastMatchPhaseChanged(PreviousMatchPhase, TEXT("Server"));
	return true;
}

FHeistMatchPhaseChanged& AHeistGameState::GetMatchPhaseChangedDelegate()
{
	return MatchPhaseChangedDelegate;
}

void AHeistGameState::OnRep_MatchPhase(const EHeistMatchPhase PreviousMatchPhase)
{
	BroadcastMatchPhaseChanged(PreviousMatchPhase, TEXT("Replicated"));
}

void AHeistGameState::BroadcastMatchPhaseChanged(
	const EHeistMatchPhase PreviousMatchPhase,
	const TCHAR* ChangeSource)
{
	MatchPhaseChangedDelegate.Broadcast(PreviousMatchPhase, MatchPhase);
	UE_LOG(
		LogHeistNetwork,
		Log,
		TEXT("Match phase %s: Previous=%s New=%s Authority=%s"),
		ChangeSource,
		*UEnum::GetValueAsString(PreviousMatchPhase),
		*UEnum::GetValueAsString(MatchPhase),
		HasAuthority() ? TEXT("true") : TEXT("false"));
}

#pragma endregion

#pragma region Objective

FName AHeistGameState::GetActiveTargetArtifactId() const
{
	return ActiveTargetArtifactId;
}

FName AHeistGameState::GetActiveTargetCaseId() const
{
	return ActiveTargetCaseId;
}

EHeistObjectiveState AHeistGameState::GetObjectiveState() const
{
	return ObjectiveState;
}

AHeistPlayerState* AHeistGameState::GetOriginalCarrierCandidate() const
{
	return OriginalCarrierCandidate.Get();
}

int32 AHeistGameState::GetObjectiveRevision() const
{
	return ObjectiveRevision;
}

bool AHeistGameState::SetObjectiveSnapshot(
	const FName InActiveTargetArtifactId,
	const FName InActiveTargetCaseId,
	const EHeistObjectiveState InObjectiveState,
	AHeistPlayerState* InOriginalCarrierCandidate)
{
	if (!HasAuthority())
	{
		UE_LOG(
			LogHeistNetwork,
			Warning,
			TEXT("Objective state change rejected: GameState=%s Reason=NotAuthority"),
			*GetNameSafe(this));
		return false;
	}

	const bool bCarrierBelongsToMatch = !IsValid(InOriginalCarrierCandidate)
		|| PlayerArray.ContainsByPredicate(
			[InOriginalCarrierCandidate](const TObjectPtr<APlayerState>& CandidatePlayerState)
			{
				return CandidatePlayerState.Get() == InOriginalCarrierCandidate;
			});
	if (!bCarrierBelongsToMatch)
	{
		UE_LOG(
			LogHeistNetwork,
			Warning,
			TEXT("Objective state change rejected: GameState=%s Carrier=%s Reason=CarrierNotInPlayerArray"),
			*GetNameSafe(this),
			*GetNameSafe(InOriginalCarrierCandidate));
		return false;
	}

	if (ActiveTargetArtifactId == InActiveTargetArtifactId
		&& ActiveTargetCaseId == InActiveTargetCaseId
		&& ObjectiveState == InObjectiveState
		&& OriginalCarrierCandidate.Get() == InOriginalCarrierCandidate)
	{
		return true;
	}

	ActiveTargetArtifactId = InActiveTargetArtifactId;
	ActiveTargetCaseId = InActiveTargetCaseId;
	ObjectiveState = InObjectiveState;
	OriginalCarrierCandidate = InOriginalCarrierCandidate;
	++ObjectiveRevision;
	ForceNetUpdate();
	BroadcastObjectiveState(TEXT("Server"));
	return true;
}

FHeistObjectiveStateChanged& AHeistGameState::GetObjectiveStateChangedDelegate()
{
	return ObjectiveStateChangedDelegate;
}

void AHeistGameState::OnRep_ObjectiveRevision()
{
	BroadcastObjectiveState(TEXT("Replicated"));
}

void AHeistGameState::BroadcastObjectiveState(const TCHAR* ChangeSource)
{
	ObjectiveStateChangedDelegate.Broadcast(
		ActiveTargetArtifactId,
		ActiveTargetCaseId,
		ObjectiveState,
		OriginalCarrierCandidate.Get());

	UE_LOG(
		LogHeistNetwork,
		Log,
		TEXT("Objective state %s: GameState=%s Target=%s CaseId=%s State=%s CarrierCandidate=%s Revision=%d Authority=%s"),
		ChangeSource,
		*GetNameSafe(this),
		*ActiveTargetArtifactId.ToString(),
		*ActiveTargetCaseId.ToString(),
		*UEnum::GetValueAsString(ObjectiveState),
		*GetNameSafe(OriginalCarrierCandidate.Get()),
		ObjectiveRevision,
		HasAuthority() ? TEXT("true") : TEXT("false"));
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
#if !UE_BUILD_SHIPPING
	UE_LOG(
		LogHeistNetwork,
		Log,
		TEXT("Rare Loot state replicated: EventIndex=%d ItemId=%s Incoming=%s MarkerActive=%s SpawnServerTime=%.2f Location=(%.1f,%.1f,%.1f)"),
		RareLootEventState.EventIndex,
		*RareLootEventState.ItemId.ToString(),
		RareLootEventState.bIncomingWarningActive ? TEXT("true") : TEXT("false"),
		RareLootEventState.bDirectionMarkerActive ? TEXT("true") : TEXT("false"),
		RareLootEventState.SpawnServerTime,
		RareLootEventState.WorldLocation.X,
		RareLootEventState.WorldLocation.Y,
		RareLootEventState.WorldLocation.Z);
#endif
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
#if !UE_BUILD_SHIPPING
	LogSoundPingEvent(TEXT("reported"), LastSoundPingEvent);
#endif
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
#if !UE_BUILD_SHIPPING
	LogSoundPingEvent(TEXT("replicated"), LastSoundPingEvent);
#endif
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
	DOREPLIFETIME(AHeistGameState, MatchPhase);

	DOREPLIFETIME(AHeistGameState, bEscapePhaseOpen);
	DOREPLIFETIME(AHeistGameState, ActiveTargetArtifactId);
	DOREPLIFETIME(AHeistGameState, ActiveTargetCaseId);
	DOREPLIFETIME(AHeistGameState, ObjectiveState);
	DOREPLIFETIME(AHeistGameState, OriginalCarrierCandidate);
	DOREPLIFETIME(AHeistGameState, ObjectiveRevision);
	DOREPLIFETIME(AHeistGameState, EscapePhaseDelaySeconds);
	DOREPLIFETIME(AHeistGameState, EscapePhaseOpenTimeSeconds);
	DOREPLIFETIME(AHeistGameState, LastSoundPingEvent);
	DOREPLIFETIME(AHeistGameState, PlayerResults);
}

#pragma endregion
