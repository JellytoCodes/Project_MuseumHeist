#include "Core/HeistGameState.h"
#include "Core/HeistGameInstance.h"
#include "Core/HeistLogChannels.h"

#include "Character/Components/HeistInventoryComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistPlayerState.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Net/UnrealNetwork.h"

namespace
{
#if !UE_BUILD_SHIPPING
void LogSoundPingEvent(const TCHAR* Phase, const FHeistSoundPingEvent& SoundPingEvent)
{
	const FString Message =
		FString::Printf(TEXT("Sound ping %s: SequenceId=%d Type=%d Tag=%s Location=(%.1f,%.1f,%.1f) Radius=%.1f Duration=%.2f AffectsGuards=%s ServerTime=%.2f"), Phase, SoundPingEvent.SequenceId,
						static_cast<int32>(SoundPingEvent.PingType), *SoundPingEvent.SoundPingTag.ToString(), SoundPingEvent.WorldLocation.X, SoundPingEvent.WorldLocation.Y,
						SoundPingEvent.WorldLocation.Z, SoundPingEvent.Radius, SoundPingEvent.Duration, SoundPingEvent.bAffectsGuards ? TEXT("true") : TEXT("false"), SoundPingEvent.ServerTimeSeconds);

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
	RefreshContractCarriedValue();
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
	if (NewMatchPhase == EHeistMatchPhase::Lobby)
	{
		ClearContractSnapshot();
		SurfaceTemplatePoolId = NAME_None;
		SelectedSurfaceTemplateId = NAME_None;
		SurfaceTemplatePoolSize = 0;
		SurfaceTemplateBagCycle = 0;
		SurfaceTemplateRemainingCount = 0;
		SurfaceTemplateSelectionRevision = 0;
		BroadcastSurfaceTemplateSelection(TEXT("ServerCleared"), true);
	}
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

void AHeistGameState::BroadcastMatchPhaseChanged(const EHeistMatchPhase PreviousMatchPhase, const TCHAR* ChangeSource)
{
	MatchPhaseChangedDelegate.Broadcast(PreviousMatchPhase, MatchPhase);
	UE_LOG(LogHeistNetwork, Log, TEXT("Match phase %s: Previous=%s New=%s Authority=%s"), ChangeSource, *UEnum::GetValueAsString(PreviousMatchPhase), *UEnum::GetValueAsString(MatchPhase),
		   HasAuthority() ? TEXT("true") : TEXT("false"));
}

#pragma endregion

#pragma region LobbyMapSelection

FName AHeistGameState::GetSelectedLobbyMapId() const
{
	return SelectedLobbyMapId;
}

bool AHeistGameState::IsRandomLobbyMapSelection() const
{
	return bRandomLobbyMapSelection;
}

int32 AHeistGameState::GetLobbyMapSelectionRevision() const
{
	return LobbyMapSelectionRevision;
}

bool AHeistGameState::SetLobbyMapSelection(const FName NewSelectedMapId, const bool bNewRandomSelection)
{
	const bool bValidMapId = NewSelectedMapId == FName(TEXT("M01")) || NewSelectedMapId == FName(TEXT("M02")) || NewSelectedMapId == FName(TEXT("M03"));
	if (!HasAuthority() || MatchPhase != EHeistMatchPhase::Lobby || !bValidMapId)
	{
		UHeistDebugFunctionLibrary::DebugLobbyMapSelectionState(this, TEXT("ServerRejected"), NewSelectedMapId, bNewRandomSelection, LobbyMapSelectionRevision, false);
		return false;
	}

	if (SelectedLobbyMapId == NewSelectedMapId && bRandomLobbyMapSelection == bNewRandomSelection)
	{
		return true;
	}

	SelectedLobbyMapId = NewSelectedMapId;
	bRandomLobbyMapSelection = bNewRandomSelection;
	++LobbyMapSelectionRevision;
	ForceNetUpdate();
	BroadcastLobbyMapSelection(TEXT("Server"));
	return true;
}

bool AHeistGameState::InitializeSessionMapSelection(const FName NewSelectedMapId, const bool bNewRandomSelection)
{
	const bool bValidMapId = NewSelectedMapId == FName(TEXT("M01")) || NewSelectedMapId == FName(TEXT("M02")) || NewSelectedMapId == FName(TEXT("M03"));
	const bool bValidSessionPhase = MatchPhase == EHeistMatchPhase::Lobby || MatchPhase == EHeistMatchPhase::InGame;
	const bool bWouldReplaceInitializedSelection =
		LobbyMapSelectionRevision > 0 && (SelectedLobbyMapId != NewSelectedMapId || bRandomLobbyMapSelection != bNewRandomSelection);
	if (!HasAuthority() || !bValidSessionPhase || !bValidMapId || bWouldReplaceInitializedSelection)
	{
		UHeistDebugFunctionLibrary::DebugLobbyMapSelectionState(this, TEXT("SessionTravelInitRejected"), NewSelectedMapId, bNewRandomSelection, LobbyMapSelectionRevision, false);
		return false;
	}

	if (SelectedLobbyMapId == NewSelectedMapId && bRandomLobbyMapSelection == bNewRandomSelection)
	{
		return true;
	}

	SelectedLobbyMapId = NewSelectedMapId;
	bRandomLobbyMapSelection = bNewRandomSelection;
	++LobbyMapSelectionRevision;
	ForceNetUpdate();
	BroadcastLobbyMapSelection(TEXT("SessionTravelInit"));
	return true;
}

FHeistLobbyMapSelectionChanged& AHeistGameState::GetLobbyMapSelectionChangedDelegate()
{
	return LobbyMapSelectionChangedDelegate;
}

void AHeistGameState::OnRep_LobbyMapSelectionRevision()
{
	BroadcastLobbyMapSelection(TEXT("Replicated"));
}

void AHeistGameState::BroadcastLobbyMapSelection(const TCHAR* ChangeSource)
{
	if (UHeistGameInstance* HeistGameInstance = Cast<UHeistGameInstance>(GetGameInstance()))
	{
		HeistGameInstance->SynchronizeSessionMapSelection(this, SelectedLobbyMapId, bRandomLobbyMapSelection);
	}

	LobbyMapSelectionChangedDelegate.Broadcast(SelectedLobbyMapId, bRandomLobbyMapSelection, LobbyMapSelectionRevision);
	UHeistDebugFunctionLibrary::DebugLobbyMapSelectionState(this, ChangeSource, SelectedLobbyMapId, bRandomLobbyMapSelection, LobbyMapSelectionRevision, true);
}

#pragma endregion

#pragma region SurfaceTemplateSelection

FName AHeistGameState::GetSurfaceTemplatePoolId() const
{
	return SurfaceTemplatePoolId;
}

FName AHeistGameState::GetSelectedSurfaceTemplateId() const
{
	return SelectedSurfaceTemplateId;
}

int32 AHeistGameState::GetSurfaceTemplatePoolSize() const
{
	return SurfaceTemplatePoolSize;
}

int32 AHeistGameState::GetSurfaceTemplateBagCycle() const
{
	return SurfaceTemplateBagCycle;
}

int32 AHeistGameState::GetSurfaceTemplateRemainingCount() const
{
	return SurfaceTemplateRemainingCount;
}

int32 AHeistGameState::GetSurfaceTemplateSelectionRevision() const
{
	return SurfaceTemplateSelectionRevision;
}

FHeistSurfaceTemplateSelectionChanged& AHeistGameState::GetSurfaceTemplateSelectionChangedDelegate()
{
	return SurfaceTemplateSelectionChangedDelegate;
}

bool AHeistGameState::InitializeSurfaceTemplateSelection(const FName PoolId, const FName TemplateId, const int32 PoolSize, const int32 BagCycle, const int32 RemainingCount,
														 const int32 SelectionRevision)
{
	const bool bValidPoolId = PoolId == FName(TEXT("M01")) || PoolId == FName(TEXT("M02")) || PoolId == FName(TEXT("M03"));
	const bool bValidSnapshot = bValidPoolId && !TemplateId.IsNone() && PoolSize > 0 && BagCycle > 0 && FMath::IsWithinInclusive(RemainingCount, 0, PoolSize - 1) &&
								SelectionRevision > 0;
	const bool bMatchesInitializedSnapshot =
		SurfaceTemplateSelectionRevision > 0 && SurfaceTemplatePoolId == PoolId && SelectedSurfaceTemplateId == TemplateId && SurfaceTemplatePoolSize == PoolSize &&
		SurfaceTemplateBagCycle == BagCycle && SurfaceTemplateRemainingCount == RemainingCount && SurfaceTemplateSelectionRevision == SelectionRevision;
	if (!HasAuthority() || MatchPhase != EHeistMatchPhase::InGame || !bValidSnapshot || (SurfaceTemplateSelectionRevision > 0 && !bMatchesInitializedSnapshot))
	{
		UHeistDebugFunctionLibrary::DebugSurfaceTemplateSelectionState(this, TEXT("ServerInitRejected"), PoolId, TemplateId, PoolSize, BagCycle, RemainingCount,
																	  SelectionRevision, false);
		return false;
	}
	if (bMatchesInitializedSnapshot)
	{
		return true;
	}

	SurfaceTemplatePoolId = PoolId;
	SelectedSurfaceTemplateId = TemplateId;
	SurfaceTemplatePoolSize = PoolSize;
	SurfaceTemplateBagCycle = BagCycle;
	SurfaceTemplateRemainingCount = RemainingCount;
	SurfaceTemplateSelectionRevision = SelectionRevision;
	ForceNetUpdate();
	BroadcastSurfaceTemplateSelection(TEXT("ServerInitialized"), true);
	return true;
}

void AHeistGameState::OnRep_SurfaceTemplateSelectionRevision()
{
	BroadcastSurfaceTemplateSelection(SurfaceTemplateSelectionRevision > 0 ? TEXT("Replicated") : TEXT("ReplicatedClear"), true);
}

void AHeistGameState::BroadcastSurfaceTemplateSelection(const TCHAR* ChangeSource, const bool bAccepted)
{
	if (bAccepted)
	{
		SurfaceTemplateSelectionChangedDelegate.Broadcast(SurfaceTemplatePoolId, SelectedSurfaceTemplateId, SurfaceTemplateSelectionRevision);
	}
	UHeistDebugFunctionLibrary::DebugSurfaceTemplateSelectionState(this, ChangeSource, SurfaceTemplatePoolId, SelectedSurfaceTemplateId, SurfaceTemplatePoolSize,
																  SurfaceTemplateBagCycle, SurfaceTemplateRemainingCount, SurfaceTemplateSelectionRevision, bAccepted);
}

#pragma endregion

#pragma region Contract

FHeistContractSnapshot AHeistGameState::GetContractSnapshot() const
{
	return ContractSnapshot;
}

bool AHeistGameState::IsContractInitialized() const
{
	return ContractSnapshot.IsInitialized();
}

bool AHeistGameState::IsContractSuccessConditionMet() const
{
	return ContractSnapshot.IsSuccessConditionMet();
}

bool AHeistGameState::InitializeContractSnapshot(const FName ContractId, const FName MapId, const int32 AssignmentSeed, const FName RequiredTargetArtifactId,
											 const FName RequiredTargetCaseId, const int32 LootValueQuota)
{
	if (!HasAuthority() || MatchPhase != EHeistMatchPhase::InGame || ContractId.IsNone() || MapId.IsNone() || RequiredTargetArtifactId.IsNone() || RequiredTargetCaseId.IsNone() || LootValueQuota <= 0)
	{
		UE_LOG(LogHeistNetwork, Error,
			   TEXT("Contract snapshot initialization rejected: Contract=%s Map=%s Seed=%d TargetArtifact=%s TargetCase=%s Quota=%d MatchPhase=%s Authority=%s Result=FAIL"),
			   *ContractId.ToString(), *MapId.ToString(), AssignmentSeed, *RequiredTargetArtifactId.ToString(), *RequiredTargetCaseId.ToString(), LootValueQuota,
			   *UEnum::GetValueAsString(MatchPhase), HasAuthority() ? TEXT("true") : TEXT("false"));
		return false;
	}

	const int32 PreviousRevision = ContractSnapshot.Revision;
	ContractSnapshot = FHeistContractSnapshot();
	ContractSnapshot.ContractId = ContractId;
	ContractSnapshot.MapId = MapId;
	ContractSnapshot.AssignmentSeed = AssignmentSeed;
	ContractSnapshot.RequiredTargetArtifactId = RequiredTargetArtifactId;
	ContractSnapshot.RequiredTargetCaseId = RequiredTargetCaseId;
	ContractSnapshot.LootValueQuota = LootValueQuota;
	ContractSnapshot.Revision = PreviousRevision == MAX_int32 ? 1 : FMath::Max(1, PreviousRevision + 1);

	ForceNetUpdate();
	BroadcastContractSnapshot(TEXT("ServerInitialized"));
	return true;
}

bool AHeistGameState::SetContractProgress(const int32 CarriedValue, const int32 SecuredValue, const bool bRequiredTargetSecured)
{
	if (!HasAuthority() || !ContractSnapshot.IsInitialized() || ContractSnapshot.Outcome != EHeistContractOutcome::None || CarriedValue < 0 || SecuredValue < 0)
	{
		UE_LOG(LogHeistNetwork, Warning,
			   TEXT("Contract progress rejected: Carried=%d Secured=%d RequiredSecured=%s Outcome=%s Initialized=%s Authority=%s Result=FAIL"), CarriedValue, SecuredValue,
			   bRequiredTargetSecured ? TEXT("true") : TEXT("false"), *UEnum::GetValueAsString(ContractSnapshot.Outcome), ContractSnapshot.IsInitialized() ? TEXT("true") : TEXT("false"),
			   HasAuthority() ? TEXT("true") : TEXT("false"));
		return false;
	}

	if (ContractSnapshot.CarriedValue == CarriedValue && ContractSnapshot.SecuredValue == SecuredValue && ContractSnapshot.bRequiredTargetSecured == bRequiredTargetSecured)
	{
		return true;
	}

	ContractSnapshot.CarriedValue = CarriedValue;
	ContractSnapshot.SecuredValue = SecuredValue;
	ContractSnapshot.bRequiredTargetSecured = bRequiredTargetSecured;
	ContractSnapshot.Revision = ContractSnapshot.Revision == MAX_int32 ? 1 : ContractSnapshot.Revision + 1;
	ForceNetUpdate();
	BroadcastContractSnapshot(TEXT("ServerProgress"));
	return true;
}

bool AHeistGameState::RefreshContractCarriedValue()
{
	if (!HasAuthority() || !ContractSnapshot.IsInitialized() || ContractSnapshot.Outcome != EHeistContractOutcome::None)
	{
		return false;
	}

	int64 ResolvedCarriedValue = 0;
	for (APlayerState* PlayerState : PlayerArray)
	{
		const AHeistPlayerState* HeistPlayerState = Cast<AHeistPlayerState>(PlayerState);
		if (!IsValid(HeistPlayerState))
		{
			continue;
		}

		ResolvedCarriedValue += static_cast<int64>(HeistPlayerState->GetTotalLootScore());
		const AHeistPlayerCharacter* PlayerCharacter = Cast<AHeistPlayerCharacter>(HeistPlayerState->GetPawn());
		const UHeistInventoryComponent* InventoryComponent = IsValid(PlayerCharacter) ? PlayerCharacter->GetInventoryComponent() : nullptr;
		if (IsValid(InventoryComponent) && InventoryComponent->IsCarryingOriginal())
		{
			ResolvedCarriedValue += static_cast<int64>(InventoryComponent->GetOriginalCarryEntry().ArtifactValue);
		}

		if (ResolvedCarriedValue > MAX_int32)
		{
			UE_LOG(LogHeistNetwork, Error, TEXT("Contract carried value refresh rejected: Resolved=%lld Result=FAIL Reason=Overflow"), ResolvedCarriedValue);
			return false;
		}
	}

	return SetContractProgress(static_cast<int32>(ResolvedCarriedValue), ContractSnapshot.SecuredValue, ContractSnapshot.bRequiredTargetSecured);
}

bool AHeistGameState::CanCommitPlayerDeposit(const AHeistPlayerState* DepositingPlayerState, const int32 DepositValue, const bool bRequiredTargetDeposited,
											 const TCHAR*& OutRejectReason) const
{
	OutRejectReason = nullptr;
	if (!HasAuthority())
	{
		OutRejectReason = TEXT("NotAuthority");
	}
	else if (!ContractSnapshot.IsInitialized() || ContractSnapshot.Outcome != EHeistContractOutcome::None)
	{
		OutRejectReason = TEXT("InvalidContractState");
	}
	else if (!IsValid(DepositingPlayerState) || !PlayerArray.ContainsByPredicate([DepositingPlayerState](const TObjectPtr<APlayerState>& Candidate)
																								{ return Candidate.Get() == DepositingPlayerState; }))
	{
		OutRejectReason = TEXT("PlayerNotInMatch");
	}
	else if (DepositingPlayerState->IsArrested() || DepositingPlayerState->IsEscaped())
	{
		OutRejectReason = DepositingPlayerState->IsArrested() ? TEXT("PlayerArrested") : TEXT("AlreadyEscaped");
	}
	else if (DepositValue <= 0 || DepositValue > ContractSnapshot.CarriedValue)
	{
		OutRejectReason = TEXT("InvalidDepositValue");
	}
	else if (bRequiredTargetDeposited && ContractSnapshot.bRequiredTargetSecured)
	{
		OutRejectReason = TEXT("RequiredTargetAlreadySecured");
	}
	else if (static_cast<int64>(ContractSnapshot.SecuredValue) + static_cast<int64>(DepositValue) > MAX_int32)
	{
		OutRejectReason = TEXT("SecuredValueOverflow");
	}

	return OutRejectReason == nullptr;
}

bool AHeistGameState::CommitPlayerDeposit(AHeistPlayerState* DepositingPlayerState, const int32 DepositValue, const bool bRequiredTargetDeposited)
{
	const TCHAR* RejectReason = nullptr;
	if (!CanCommitPlayerDeposit(DepositingPlayerState, DepositValue, bRequiredTargetDeposited, RejectReason))
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			FString::Printf(TEXT("Player extraction deposit rejected: PlayerId=%d DepositValue=%d Required=%s Carried=%d Secured=%d Reason=%s Authority=%s Result=FAIL"),
							IsValid(DepositingPlayerState) ? DepositingPlayerState->HeistPlayerId : INDEX_NONE, DepositValue, bRequiredTargetDeposited ? TEXT("true") : TEXT("false"),
							ContractSnapshot.CarriedValue, ContractSnapshot.SecuredValue, RejectReason != nullptr ? RejectReason : TEXT("Unknown"), HasAuthority() ? TEXT("true") : TEXT("false")),
			EHeistDebugLevel::Warning);
		return false;
	}

	ContractSnapshot.CarriedValue -= DepositValue;
	ContractSnapshot.SecuredValue += DepositValue;
	ContractSnapshot.bRequiredTargetSecured |= bRequiredTargetDeposited;
	ContractSnapshot.Revision = ContractSnapshot.Revision == MAX_int32 ? 1 : ContractSnapshot.Revision + 1;
	ForceNetUpdate();
	BroadcastContractSnapshot(TEXT("ServerDeposit"));

	UHeistDebugFunctionLibrary::Message(
		this,
		FString::Printf(TEXT("Player extraction deposit committed: PlayerId=%d DepositValue=%d Required=%s Carried=%d Secured=%d Quota=%d Authority=true Result=PASS"),
						DepositingPlayerState->HeistPlayerId, DepositValue, bRequiredTargetDeposited ? TEXT("true") : TEXT("false"), ContractSnapshot.CarriedValue,
						ContractSnapshot.SecuredValue, ContractSnapshot.LootValueQuota));
	return true;
}

bool AHeistGameState::CommitContractOutcome(const EHeistContractOutcome Outcome)
{
	if (!HasAuthority() || !ContractSnapshot.IsInitialized() || ContractSnapshot.Outcome != EHeistContractOutcome::None || Outcome == EHeistContractOutcome::None)
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Contract outcome rejected: Requested=%s Current=%s Initialized=%s Authority=%s Result=FAIL Reason=InvalidState"),
			   *UEnum::GetValueAsString(Outcome), *UEnum::GetValueAsString(ContractSnapshot.Outcome), ContractSnapshot.IsInitialized() ? TEXT("true") : TEXT("false"),
			   HasAuthority() ? TEXT("true") : TEXT("false"));
		return false;
	}

	FHeistContractSnapshot CandidateSnapshot = ContractSnapshot;
	CandidateSnapshot.Outcome = Outcome;
	if (!CandidateSnapshot.IsOutcomeConsistent())
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Contract outcome rejected: Requested=%s RequiredSecured=%s Secured=%d Quota=%d Result=FAIL Reason=OutcomeMismatch"),
			   *UEnum::GetValueAsString(Outcome), ContractSnapshot.bRequiredTargetSecured ? TEXT("true") : TEXT("false"), ContractSnapshot.SecuredValue, ContractSnapshot.LootValueQuota);
		return false;
	}

	ContractSnapshot.Outcome = Outcome;
	ContractSnapshot.Revision = ContractSnapshot.Revision == MAX_int32 ? 1 : ContractSnapshot.Revision + 1;
	ForceNetUpdate();
	BroadcastContractSnapshot(TEXT("ServerOutcome"));
	return true;
}

FHeistContractSnapshotChanged& AHeistGameState::GetContractSnapshotChangedDelegate()
{
	return ContractSnapshotChangedDelegate;
}

void AHeistGameState::OnRep_ContractSnapshot()
{
	BroadcastContractSnapshot(TEXT("Replicated"));
}

void AHeistGameState::BroadcastContractSnapshot(const TCHAR* ChangeSource)
{
	ContractSnapshotChangedDelegate.Broadcast(ContractSnapshot);
	UE_LOG(LogHeistNetwork, Log,
		   TEXT("Contract snapshot %s: Contract=%s Map=%s Seed=%d TargetArtifact=%s TargetCase=%s Quota=%d Carried=%d Secured=%d RequiredSecured=%s Outcome=%s Revision=%d Authority=%s Result=%s"),
		   ChangeSource, *ContractSnapshot.ContractId.ToString(), *ContractSnapshot.MapId.ToString(), ContractSnapshot.AssignmentSeed,
		   *ContractSnapshot.RequiredTargetArtifactId.ToString(), *ContractSnapshot.RequiredTargetCaseId.ToString(), ContractSnapshot.LootValueQuota, ContractSnapshot.CarriedValue,
		   ContractSnapshot.SecuredValue, ContractSnapshot.bRequiredTargetSecured ? TEXT("true") : TEXT("false"), *UEnum::GetValueAsString(ContractSnapshot.Outcome), ContractSnapshot.Revision,
		   HasAuthority() ? TEXT("true") : TEXT("false"), ContractSnapshot.IsInitialized() && ContractSnapshot.IsProgressValid() && ContractSnapshot.IsOutcomeConsistent() ? TEXT("PASS") : TEXT("PENDING"));
}

void AHeistGameState::ClearContractSnapshot()
{
	if (!ContractSnapshot.IsInitialized() && ContractSnapshot.Revision == 0)
	{
		return;
	}

	ContractSnapshot = FHeistContractSnapshot();
	ForceNetUpdate();
	BroadcastContractSnapshot(TEXT("ServerCleared"));
}

#pragma endregion

#pragma region Alert

EHeistAlertLevel AHeistGameState::GetAlertLevel() const
{
	return AlertLevel;
}

float AHeistGameState::GetAlertNextTransitionServerTime() const
{
	return AlertNextTransitionServerTime;
}

float AHeistGameState::GetAlertTransitionRemainingSeconds() const
{
	return AlertNextTransitionServerTime > 0.0f ? FMath::Max(0.0f, AlertNextTransitionServerTime - GetServerWorldTimeSeconds()) : 0.0f;
}

bool AHeistGameState::IsLockdownCountdownActive() const
{
	return AlertLevel == EHeistAlertLevel::Alarmed && GetAlertTransitionRemainingSeconds() > 0.0f;
}

float AHeistGameState::GetLockdownCountdownRemainingSeconds() const
{
	return IsLockdownCountdownActive() ? GetAlertTransitionRemainingSeconds() : 0.0f;
}

bool AHeistGameState::IsLockdownActive() const
{
	return AlertLevel == EHeistAlertLevel::Lockdown;
}

bool AHeistGameState::AreWorldInteractionsRestricted() const
{
	return IsLockdownActive() || MatchPhase == EHeistMatchPhase::End;
}

int32 AHeistGameState::GetAlertRevision() const
{
	return AlertRevision;
}

FName AHeistGameState::GetLastAlertTriggerId() const
{
	return LastAlertTriggerId;
}

bool AHeistGameState::SetAlertSnapshot(const EHeistAlertLevel NewAlertLevel, const float NewNextTransitionServerTime, const FName TriggerId)
{
	const UEnum* AlertLevelEnum = StaticEnum<EHeistAlertLevel>();
	if (!HasAuthority() || !IsValid(AlertLevelEnum) || !AlertLevelEnum->IsValidEnumValue(static_cast<int64>(NewAlertLevel)) || !FMath::IsFinite(NewNextTransitionServerTime) ||
		NewNextTransitionServerTime < 0.0f)
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Alert snapshot rejected: GameState=%s Level=%s NextTransitionServerTime=%.2f Trigger=%s Authority=%s Result=FAIL"), *GetNameSafe(this),
			   *UEnum::GetValueAsString(NewAlertLevel), NewNextTransitionServerTime, *TriggerId.ToString(), HasAuthority() ? TEXT("true") : TEXT("false"));
		return false;
	}

	if (AlertLevel == NewAlertLevel && FMath::IsNearlyEqual(AlertNextTransitionServerTime, NewNextTransitionServerTime, KINDA_SMALL_NUMBER) && LastAlertTriggerId == TriggerId)
	{
		return true;
	}

	const EHeistAlertLevel PreviousAlertLevel = AlertLevel;
	AlertLevel = NewAlertLevel;
	AlertNextTransitionServerTime = NewNextTransitionServerTime;
	LastAlertTriggerId = TriggerId;
	++AlertRevision;
	ForceNetUpdate();
	BroadcastAlertState(PreviousAlertLevel, TEXT("Server"));
	return true;
}

FHeistAlertStateChanged& AHeistGameState::GetAlertStateChangedDelegate()
{
	return AlertStateChangedDelegate;
}

void AHeistGameState::OnRep_AlertRevision()
{
	BroadcastAlertState(LastBroadcastAlertLevel, TEXT("Replicated"));
}

void AHeistGameState::BroadcastAlertState(const EHeistAlertLevel PreviousAlertLevel, const TCHAR* ChangeSource)
{
	LastBroadcastAlertLevel = AlertLevel;
	AlertStateChangedDelegate.Broadcast(PreviousAlertLevel, AlertLevel, AlertRevision, LastAlertTriggerId);
	UE_LOG(LogHeistNetwork, Log, TEXT("Global alert state %s: GameState=%s Previous=%s New=%s NextTransitionServerTime=%.2f Remaining=%.2f Trigger=%s Revision=%d Authority=%s Result=PASS"),
		   ChangeSource, *GetNameSafe(this), *UEnum::GetValueAsString(PreviousAlertLevel), *UEnum::GetValueAsString(AlertLevel), AlertNextTransitionServerTime, GetAlertTransitionRemainingSeconds(),
		   *LastAlertTriggerId.ToString(), AlertRevision, HasAuthority() ? TEXT("true") : TEXT("false"));
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

bool AHeistGameState::SetObjectiveSnapshot(const FName InActiveTargetArtifactId, const FName InActiveTargetCaseId, const EHeistObjectiveState InObjectiveState,
										   AHeistPlayerState* InOriginalCarrierCandidate)
{
	if (!HasAuthority())
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Objective state change rejected: GameState=%s Reason=NotAuthority"), *GetNameSafe(this));
		return false;
	}

	const bool bCarrierBelongsToMatch = !IsValid(InOriginalCarrierCandidate) || PlayerArray.ContainsByPredicate([InOriginalCarrierCandidate](const TObjectPtr<APlayerState>& CandidatePlayerState)
																												{ return CandidatePlayerState.Get() == InOriginalCarrierCandidate; });
	if (!bCarrierBelongsToMatch)
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Objective state change rejected: GameState=%s Carrier=%s Reason=CarrierNotInPlayerArray"), *GetNameSafe(this), *GetNameSafe(InOriginalCarrierCandidate));
		return false;
	}

	if (ActiveTargetArtifactId == InActiveTargetArtifactId && ActiveTargetCaseId == InActiveTargetCaseId && ObjectiveState == InObjectiveState &&
		OriginalCarrierCandidate.Get() == InOriginalCarrierCandidate)
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
	ObjectiveStateChangedDelegate.Broadcast(ActiveTargetArtifactId, ActiveTargetCaseId, ObjectiveState, OriginalCarrierCandidate.Get());

	UE_LOG(LogHeistNetwork, Log, TEXT("Objective state %s: GameState=%s Target=%s CaseId=%s State=%s CarrierCandidate=%s Revision=%d Authority=%s"), ChangeSource, *GetNameSafe(this),
		   *ActiveTargetArtifactId.ToString(), *ActiveTargetCaseId.ToString(), *UEnum::GetValueAsString(ObjectiveState), *GetNameSafe(OriginalCarrierCandidate.Get()), ObjectiveRevision,
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
		UE_LOG(LogHeistNetwork, Warning, TEXT("Escape phase initialization rejected: GameState=%s Reason=NotAuthority"), *GetNameSafe(this));
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
		UE_LOG(LogHeistNetwork, Warning, TEXT("Escape phase open rejected: GameState=%s Reason=NotAuthority"), *GetNameSafe(this));
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

	UE_LOG(LogHeist, Log, TEXT("Escape phase opened: GameState=%s ServerTime=%.2f Delay=%.2f"), *GetNameSafe(this), EscapePhaseOpenTimeSeconds, EscapePhaseDelaySeconds);
}

FHeistEscapePhaseStateChanged& AHeistGameState::GetEscapePhaseStateChangedDelegate()
{
	return EscapePhaseStateChangedDelegate;
}

void AHeistGameState::OnRep_EscapePhaseOpen()
{
	EscapePhaseStateChangedDelegate.Broadcast(bEscapePhaseOpen);

	UE_LOG(LogHeistNetwork, Log, TEXT("Escape phase replicated: GameState=%s IsOpen=%s OpenTime=%.2f Delay=%.2f"), *GetNameSafe(this), bEscapePhaseOpen ? TEXT("true") : TEXT("false"),
		   EscapePhaseOpenTimeSeconds, EscapePhaseDelaySeconds);
}

#pragma endregion

#pragma region RareLootEvent

const FHeistRareLootEventState& AHeistGameState::GetRareLootEventState() const
{
	return RareLootEventState;
}

void AHeistGameState::BeginRareLootWarning(const int32 EventIndex, const FName ItemId, const float SpawnServerTime)
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

void AHeistGameState::ActivateRareLootMarker(const int32 EventIndex, const FName ItemId, const FVector& WorldLocation)
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
	UE_LOG(LogHeistNetwork, Log, TEXT("Rare Loot state replicated: EventIndex=%d ItemId=%s Incoming=%s MarkerActive=%s SpawnServerTime=%.2f Location=(%.1f,%.1f,%.1f)"), RareLootEventState.EventIndex,
		   *RareLootEventState.ItemId.ToString(), RareLootEventState.bIncomingWarningActive ? TEXT("true") : TEXT("false"), RareLootEventState.bDirectionMarkerActive ? TEXT("true") : TEXT("false"),
		   RareLootEventState.SpawnServerTime, RareLootEventState.WorldLocation.X, RareLootEventState.WorldLocation.Y, RareLootEventState.WorldLocation.Z);
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

	FHeistSoundPingEvent ReportedEvent = SoundPingEvent;
	ReportedEvent.SequenceId = NextSoundPingSequenceId++;
	ReportedEvent.ServerTimeSeconds = GetServerWorldTimeSeconds();
	SoundPingEventReportedDelegate.Broadcast(ReportedEvent);
#if !UE_BUILD_SHIPPING
	LogSoundPingEvent(TEXT("reported"), ReportedEvent);
#endif
}

FHeistSoundPingEventReported& AHeistGameState::GetSoundPingEventReportedDelegate()
{
	return SoundPingEventReportedDelegate;
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

	NewPlayerResults.Sort([](const FHeistPlayerResult& Left, const FHeistPlayerResult& Right) { return Left.PlayerId < Right.PlayerId; });

	PlayerResults = MoveTemp(NewPlayerResults);
	ForceNetUpdate();
	PlayerResultsChangedDelegate.Broadcast();

	for (const FHeistPlayerResult& PlayerResult : PlayerResults)
	{
		UE_LOG(LogHeist, Log, TEXT("Player contribution result: PlayerId=%d Escaped=%s Arrested=%s LootScore=%d FinalScore=%d LootWeight=%.2f EscapeTime=%.2f"), PlayerResult.PlayerId,
			   PlayerResult.bEscaped ? TEXT("true") : TEXT("false"), PlayerResult.bArrested ? TEXT("true") : TEXT("false"), PlayerResult.LootScore, PlayerResult.FinalScore, PlayerResult.LootWeight,
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
	UE_LOG(LogHeist, Log, TEXT("Team arrest state: Players=%d Arrested=%d Escaped=%d Active=%d AllArrested=%s FailureEligible=%s"), PlayerResults.Num(), ArrestedPlayerCount, EscapedPlayerCount,
		   ActivePlayerCount, bAllPlayersArrested ? TEXT("true") : TEXT("false"), bAllPlayersArrested ? TEXT("true") : TEXT("false"));

	UE_LOG(LogHeist, Log, TEXT("Player contribution results rebuilt: GameState=%s PlayerCount=%d"), *GetNameSafe(this), PlayerResults.Num());
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
		UE_LOG(LogHeistNetwork, Log, TEXT("Player contribution result replicated: PlayerId=%d Escaped=%s Arrested=%s LootScore=%d FinalScore=%d LootWeight=%.2f EscapeTime=%.2f"),
			   PlayerResult.PlayerId, PlayerResult.bEscaped ? TEXT("true") : TEXT("false"), PlayerResult.bArrested ? TEXT("true") : TEXT("false"), PlayerResult.LootScore, PlayerResult.FinalScore,
			   PlayerResult.LootWeight, PlayerResult.EscapeTimeSeconds);
	}

	UE_LOG(LogHeistNetwork, Log, TEXT("Player contribution results replicated: GameState=%s PlayerCount=%d"), *GetNameSafe(this), PlayerResults.Num());
}

#pragma endregion

#pragma region Replication

void AHeistGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHeistGameState, MatchPhase);
	DOREPLIFETIME(AHeistGameState, SelectedLobbyMapId);
	DOREPLIFETIME(AHeistGameState, bRandomLobbyMapSelection);
	DOREPLIFETIME(AHeistGameState, LobbyMapSelectionRevision);
	DOREPLIFETIME(AHeistGameState, SurfaceTemplatePoolId);
	DOREPLIFETIME(AHeistGameState, SelectedSurfaceTemplateId);
	DOREPLIFETIME(AHeistGameState, SurfaceTemplatePoolSize);
	DOREPLIFETIME(AHeistGameState, SurfaceTemplateBagCycle);
	DOREPLIFETIME(AHeistGameState, SurfaceTemplateRemainingCount);
	DOREPLIFETIME(AHeistGameState, SurfaceTemplateSelectionRevision);
	DOREPLIFETIME(AHeistGameState, ContractSnapshot);
	DOREPLIFETIME(AHeistGameState, AlertLevel);
	DOREPLIFETIME(AHeistGameState, AlertNextTransitionServerTime);
	DOREPLIFETIME(AHeistGameState, LastAlertTriggerId);
	DOREPLIFETIME(AHeistGameState, AlertRevision);

	DOREPLIFETIME(AHeistGameState, bEscapePhaseOpen);
	DOREPLIFETIME(AHeistGameState, ActiveTargetArtifactId);
	DOREPLIFETIME(AHeistGameState, ActiveTargetCaseId);
	DOREPLIFETIME(AHeistGameState, ObjectiveState);
	DOREPLIFETIME(AHeistGameState, OriginalCarrierCandidate);
	DOREPLIFETIME(AHeistGameState, ObjectiveRevision);
	DOREPLIFETIME(AHeistGameState, EscapePhaseDelaySeconds);
	DOREPLIFETIME(AHeistGameState, EscapePhaseOpenTimeSeconds);
	DOREPLIFETIME(AHeistGameState, PlayerResults);
}

#pragma endregion
