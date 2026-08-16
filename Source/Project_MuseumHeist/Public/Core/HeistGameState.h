#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "GameFramework/GameStateBase.h"

#include "HeistGameState.generated.h"

class AHeistPlayerState;

DECLARE_MULTICAST_DELEGATE_OneParam(FHeistEscapePhaseStateChanged, bool);
DECLARE_MULTICAST_DELEGATE(FHeistPlayerResultsChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FHeistTeamResultChanged, const FHeistTeamResult&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FHeistSoundPingEventReported, const FHeistSoundPingEvent&, int32*);
DECLARE_MULTICAST_DELEGATE_OneParam(FHeistPlayerConnectionsChanged, int32);
DECLARE_MULTICAST_DELEGATE_TwoParams(FHeistMatchPhaseChanged, EHeistMatchPhase, EHeistMatchPhase);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FHeistLobbyMapSelectionChanged, FName, bool, int32);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FHeistSurfaceTemplateSelectionChanged, FName, FName, int32);
DECLARE_MULTICAST_DELEGATE_OneParam(FHeistContractSnapshotChanged, const FHeistContractSnapshot&);
DECLARE_MULTICAST_DELEGATE_FourParams(FHeistObjectiveStateChanged, FName, FName, EHeistObjectiveState, AHeistPlayerState*);
DECLARE_MULTICAST_DELEGATE_FourParams(FHeistAlertStateChanged, EHeistAlertLevel, EHeistAlertLevel, int32, FName);

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistGameState : public AGameStateBase
{
	GENERATED_BODY()

#pragma region Construction

  public:
	AHeistGameState();

#pragma endregion

#pragma region PlayerConnections

  public:
	virtual void AddPlayerState(APlayerState* PlayerState) override;
	virtual void RemovePlayerState(APlayerState* PlayerState) override;
	int32 GetConnectedPlayerCount() const;
	FHeistPlayerConnectionsChanged& GetPlayerConnectionsChangedDelegate();

  private:
	FHeistPlayerConnectionsChanged PlayerConnectionsChangedDelegate;

#pragma endregion

#pragma region MatchPhase

  public:
	UFUNCTION(BlueprintPure, Category = "Heist|Match")
	EHeistMatchPhase GetMatchPhase() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Heist|Match")
	bool SetMatchPhase(EHeistMatchPhase NewMatchPhase);

	FHeistMatchPhaseChanged& GetMatchPhaseChangedDelegate();

  private:
	UPROPERTY(ReplicatedUsing = OnRep_MatchPhase, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Match", meta = (AllowPrivateAccess = "true"))
	EHeistMatchPhase MatchPhase = EHeistMatchPhase::None;

	UFUNCTION()
	void OnRep_MatchPhase(EHeistMatchPhase PreviousMatchPhase);

	void BroadcastMatchPhaseChanged(EHeistMatchPhase PreviousMatchPhase, const TCHAR* ChangeSource);

	FHeistMatchPhaseChanged MatchPhaseChangedDelegate;

#pragma endregion

#pragma region LobbyMapSelection

  public:
	UFUNCTION(BlueprintPure, Category = "Heist|Lobby")
	FName GetSelectedLobbyMapId() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Lobby")
	bool IsRandomLobbyMapSelection() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Lobby")
	int32 GetLobbyMapSelectionRevision() const;

	bool SetLobbyMapSelection(FName NewSelectedMapId, bool bNewRandomSelection);
	bool InitializeSessionMapSelection(FName NewSelectedMapId, bool bNewRandomSelection);
	FHeistLobbyMapSelectionChanged& GetLobbyMapSelectionChangedDelegate();

  private:
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	FName SelectedLobbyMapId = FName(TEXT("M01"));

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	bool bRandomLobbyMapSelection = false;

	UPROPERTY(ReplicatedUsing = OnRep_LobbyMapSelectionRevision, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	int32 LobbyMapSelectionRevision = 0;

	UFUNCTION()
	void OnRep_LobbyMapSelectionRevision();

	void BroadcastLobbyMapSelection(const TCHAR* ChangeSource);
	FHeistLobbyMapSelectionChanged LobbyMapSelectionChangedDelegate;

#pragma endregion

#pragma region SurfaceTemplateSelection

  public:
	UFUNCTION(BlueprintPure, Category = "Heist|Forgery")
	FName GetSurfaceTemplatePoolId() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Forgery")
	FName GetSelectedSurfaceTemplateId() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Forgery")
	int32 GetSurfaceTemplatePoolSize() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Forgery")
	int32 GetSurfaceTemplateBagCycle() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Forgery")
	int32 GetSurfaceTemplateRemainingCount() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Forgery")
	int32 GetSurfaceTemplateSelectionRevision() const;

	bool InitializeSurfaceTemplateSelection(FName PoolId, FName TemplateId, int32 PoolSize, int32 BagCycle, int32 RemainingCount, int32 SelectionRevision);
	FHeistSurfaceTemplateSelectionChanged& GetSurfaceTemplateSelectionChangedDelegate();

  private:
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	FName SurfaceTemplatePoolId = NAME_None;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	FName SelectedSurfaceTemplateId = NAME_None;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	int32 SurfaceTemplatePoolSize = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	int32 SurfaceTemplateBagCycle = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	int32 SurfaceTemplateRemainingCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_SurfaceTemplateSelectionRevision, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Forgery", meta = (AllowPrivateAccess = "true"))
	int32 SurfaceTemplateSelectionRevision = 0;

	UFUNCTION()
	void OnRep_SurfaceTemplateSelectionRevision();

	void BroadcastSurfaceTemplateSelection(const TCHAR* ChangeSource, bool bAccepted);
	FHeistSurfaceTemplateSelectionChanged SurfaceTemplateSelectionChangedDelegate;

#pragma endregion

#pragma region Contract

  public:
	UFUNCTION(BlueprintPure, Category = "Heist|Contract")
	FHeistContractSnapshot GetContractSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Contract")
	bool IsContractInitialized() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Contract")
	bool IsContractSuccessConditionMet() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Contract")
	FText GetContractOutcomeReasonText() const;

	bool InitializeContractSnapshot(FName ContractId, FName MapId, int32 AssignmentSeed, FName RequiredTargetArtifactId, FName RequiredTargetCaseId, int32 LootValueQuota);
	bool SetContractProgress(int32 CarriedValue, int32 SecuredValue, bool bRequiredTargetSecured);
	bool RefreshContractCarriedValue();
	bool CanCommitPlayerDeposit(const AHeistPlayerState* DepositingPlayerState, int32 DepositValue, bool bRequiredTargetDeposited, const TCHAR*& OutRejectReason) const;
	bool CommitPlayerDeposit(AHeistPlayerState* DepositingPlayerState, int32 DepositValue, bool bRequiredTargetDeposited);
	bool CommitContractOutcome(EHeistContractOutcome Outcome, FName OutcomeReasonId);
	FHeistContractSnapshotChanged& GetContractSnapshotChangedDelegate();

  private:
	UPROPERTY(ReplicatedUsing = OnRep_ContractSnapshot, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Contract", meta = (AllowPrivateAccess = "true"))
	FHeistContractSnapshot ContractSnapshot;

	UFUNCTION()
	void OnRep_ContractSnapshot();

	void BroadcastContractSnapshot(const TCHAR* ChangeSource);
	void ClearContractSnapshot();

	FHeistContractSnapshotChanged ContractSnapshotChangedDelegate;

#pragma endregion

#pragma region Alert

  public:
	UFUNCTION(BlueprintPure, Category = "Heist|Alert")
	EHeistAlertLevel GetAlertLevel() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Alert")
	float GetAlertNextTransitionServerTime() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Alert")
	float GetAlertTransitionRemainingSeconds() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Alert")
	bool IsLockdownCountdownActive() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Alert")
	float GetLockdownCountdownRemainingSeconds() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Alert")
	bool IsLockdownActive() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Alert")
	bool AreWorldInteractionsRestricted() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Alert")
	int32 GetAlertRevision() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Alert")
	FName GetLastAlertTriggerId() const;

	bool SetAlertSnapshot(EHeistAlertLevel NewAlertLevel, float NewNextTransitionServerTime, FName TriggerId);
	FHeistAlertStateChanged& GetAlertStateChangedDelegate();

  private:
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Alert", meta = (AllowPrivateAccess = "true"))
	EHeistAlertLevel AlertLevel = EHeistAlertLevel::Quiet;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Alert", meta = (AllowPrivateAccess = "true"))
	float AlertNextTransitionServerTime = 0.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Alert", meta = (AllowPrivateAccess = "true"))
	FName LastAlertTriggerId = NAME_None;

	UPROPERTY(ReplicatedUsing = OnRep_AlertRevision, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Alert", meta = (AllowPrivateAccess = "true"))
	int32 AlertRevision = 0;

	UFUNCTION()
	void OnRep_AlertRevision();

	void BroadcastAlertState(EHeistAlertLevel PreviousAlertLevel, const TCHAR* ChangeSource);

	FHeistAlertStateChanged AlertStateChangedDelegate;
	EHeistAlertLevel LastBroadcastAlertLevel = EHeistAlertLevel::Quiet;

#pragma endregion

#pragma region Objective

  public:
	FName GetActiveTargetArtifactId() const;
	FName GetActiveTargetCaseId() const;
	EHeistObjectiveState GetObjectiveState() const;
	AHeistPlayerState* GetOriginalCarrierCandidate() const;
	int32 GetObjectiveRevision() const;
	bool SetObjectiveSnapshot(FName InActiveTargetArtifactId, FName InActiveTargetCaseId, EHeistObjectiveState InObjectiveState, AHeistPlayerState* InOriginalCarrierCandidate);
	FHeistObjectiveStateChanged& GetObjectiveStateChangedDelegate();

  private:
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Objective", meta = (AllowPrivateAccess = "true"))
	FName ActiveTargetArtifactId = NAME_None;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Objective", meta = (AllowPrivateAccess = "true"))
	FName ActiveTargetCaseId = NAME_None;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Objective", meta = (AllowPrivateAccess = "true"))
	EHeistObjectiveState ObjectiveState = EHeistObjectiveState::Inactive;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Objective", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AHeistPlayerState> OriginalCarrierCandidate;

	UPROPERTY(ReplicatedUsing = OnRep_ObjectiveRevision, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Objective", meta = (AllowPrivateAccess = "true"))
	int32 ObjectiveRevision = 0;

	UFUNCTION()
	void OnRep_ObjectiveRevision();

	void BroadcastObjectiveState(const TCHAR* ChangeSource);

	FHeistObjectiveStateChanged ObjectiveStateChangedDelegate;

#pragma endregion

#pragma region EscapePhase

  public:
	bool IsEscapePhaseOpen() const;
	float GetEscapePhaseDelaySeconds() const;
	float GetEscapePhaseOpenTimeSeconds() const;
	void InitializeEscapePhase(float InDelaySeconds);
	void OpenEscapePhase();
	FHeistEscapePhaseStateChanged& GetEscapePhaseStateChangedDelegate();

  private:
	UPROPERTY(ReplicatedUsing = OnRep_EscapePhaseOpen, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Escape", meta = (AllowPrivateAccess = "true"))
	bool bEscapePhaseOpen = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Escape", meta = (AllowPrivateAccess = "true"))
	float EscapePhaseDelaySeconds = 0.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Escape", meta = (AllowPrivateAccess = "true"))
	float EscapePhaseOpenTimeSeconds = -1.0f;

	UFUNCTION()
	void OnRep_EscapePhaseOpen();

	FHeistEscapePhaseStateChanged EscapePhaseStateChangedDelegate;

#pragma endregion

#pragma region SoundPing

  public:
	int32 ReportSoundPing(const FHeistSoundPingEvent& SoundPingEvent);
	FHeistSoundPingEventReported& GetSoundPingEventReportedDelegate();

  private:
	UPROPERTY(Transient)
	int32 NextSoundPingSequenceId = 1;

	FHeistSoundPingEventReported SoundPingEventReportedDelegate;

#pragma endregion

#pragma region ResultData

  public:
	bool CommitTeamResult(FHeistTeamResult NewTeamResult);
	const FHeistTeamResult& GetTeamResult() const;
	FHeistTeamResultChanged& GetTeamResultChangedDelegate();
	void RebuildPlayerResults();
	const TArray<FHeistPlayerResult>& GetPlayerResults() const;
	FHeistPlayerResultsChanged& GetPlayerResultsChangedDelegate();
	int32 GetActiveCrewCount() const;
	int32 GetEscapedCrewCount() const;
	int32 GetArrestedCrewCount() const;
	bool AreAllCrewMembersResolved() const;
	bool AreAllRemainingCrewMembersArrested() const;

  private:
	void GetPlayerLifecycleCounts(int32& OutTotalPlayers, int32& OutActivePlayers, int32& OutEscapedPlayers, int32& OutArrestedPlayers) const;

	UPROPERTY(ReplicatedUsing = OnRep_TeamResult, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	FHeistTeamResult TeamResult;

	UFUNCTION()
	void OnRep_TeamResult();

	FHeistTeamResultChanged TeamResultChangedDelegate;

	UPROPERTY(ReplicatedUsing = OnRep_PlayerResults, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	TArray<FHeistPlayerResult> PlayerResults;

	UFUNCTION()
	void OnRep_PlayerResults();

	FHeistPlayerResultsChanged PlayerResultsChangedDelegate;

#pragma endregion

#pragma region Replication

  public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion
};
