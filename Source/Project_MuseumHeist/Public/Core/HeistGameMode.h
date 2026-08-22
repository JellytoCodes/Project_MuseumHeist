#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "GameFramework/GameModeBase.h"

#include "HeistGameMode.generated.h"

class UHeistGameBalanceDataAsset;
class UHeistInventoryComponent;
class UDataTable;
class AHeistGuardCharacter;
class AHeistLootActor;
class AHeistPlayerCharacter;
class AHeistPlayerController;
class AHeistPlayerState;
struct FHeistItemDataRow;
struct FHeistContractDataRow;
struct FHeistArtifactDataRow;
struct FHeistForgeryTemplateRow;
struct FHeistObjectAssemblyPartRow;
struct FHeistObjectAssemblyTemplateRow;
struct FHeistLootDataRow;
struct FHeistUsableItemDataRow;
struct FHeistGuardDataRow;
struct FHeistSoundPingDataRow;
struct FHeistLootDropRequest;
struct FHeistPlayerCountDifficultyBaseline;

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistGameMode : public AGameModeBase
{
	GENERATED_BODY()

#pragma region Construction

  public:
	AHeistGameMode();

#pragma endregion

#pragma region Lifecycle

  protected:
	virtual void StartPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void Logout(AController* Exiting) override;

  private:
	void HandleMatchPhaseChanged(EHeistMatchPhase PreviousMatchPhase, EHeistMatchPhase NewMatchPhase);
	void HandlePlayerConnectionsChanged(int32 ConnectedPlayerCount);
	int32 DropDisconnectedPlayerLooseLoot(AHeistPlayerCharacter* ExitingCharacter, AHeistPlayerState* ExitingPlayerState,
		UHeistInventoryComponent* InventoryComponent, int32& OutFailureCount);
	int32 ClearMatchScopedTimers();

  public:
	void PrepareForOnlineSessionShutdown(FName Reason);
	void HandlePlayerPawnLeavingGame(AHeistPlayerController* ExitingController);
	void NotifyPlayerTerminalStateChanged(AHeistPlayerState* PlayerState, FName TerminalTrigger);

#pragma endregion

#pragma region ContractOutcome

  public:
#if !UE_BUILD_SHIPPING
	bool ForceContractOutcomeForDebug(FName TerminalTrigger, bool bTreatAsCrewEscaped, bool bTreatAsAllRemainingCrewArrested, bool bTreatAsAllCrewDisconnected);
#endif

  private:
	void StartContractDurationTimer();
	void HandleContractDurationTimerElapsed();
	bool TryResolveContractOutcome(FName TerminalTrigger, bool bForceTerminal = false, bool bTreatAsCrewEscaped = false,
		bool bTreatAsAllRemainingCrewArrested = false, bool bTreatAsAllCrewDisconnected = false);
	bool BuildTeamResultSnapshot(EHeistContractOutcome Outcome, FName OutcomeReasonId, FHeistTeamResult& OutTeamResult) const;
	bool FinalizeContractOutcome(EHeistContractOutcome Outcome, FName OutcomeReasonId, FName TerminalTrigger);

	FTimerHandle ContractDurationTimerHandle;
	bool bAnyPlayerEscapedThisMatch = false;
	bool bMatchHadPlayer = false;

#pragma endregion

#pragma region Alert

  public:
	bool RequestAlertEscalation(EHeistAlertLevel RequestedAlertLevel, FName TriggerId, bool* bOutLevelChanged = nullptr);
	bool RequestSecurityIncident(const FVector& WorldLocation, FName IncidentId);
	bool RequestForgeryTimeoutInvestigation(const FVector& WorldLocation, FName SourceId);
	static bool TryConsumeOneShotSecurityId(TSet<FName>& InOutProcessedIds, FName SourceId);
	bool IsAlertTransitionTimerActive() const;
	int32 GetProcessedAlertTriggerCount() const;
	int32 GetProcessedSecurityIncidentCount() const;
	int32 GetProcessedGuardInvestigationCount() const;
	int32 GetActiveMatchTimerCount() const;

  private:
	bool RequestNearestGuardInvestigation(const FVector& WorldLocation, FName SourceId, float SearchRadius, AHeistGuardCharacter*& OutAssignedGuard, float& OutDistance,
		bool& bOutDuplicate, FName& OutReason);
	void InitializeAlertState();
	bool ApplyAlertLevel(EHeistAlertLevel NewAlertLevel, FName TriggerId);
	bool ApplyLockdownWorldRestrictions(FName TriggerId);
	void HandleAlertTransitionTimerElapsed();
	EHeistAlertLevel GetNextAlertLevel(EHeistAlertLevel CurrentAlertLevel) const;
	float ResolveAlertTransitionDelay(EHeistAlertLevel CurrentAlertLevel) const;

	FTimerHandle AlertTransitionTimerHandle;
	TSet<FName> ProcessedAlertTriggerIds;
	TSet<FName> ProcessedSecurityIncidentIds;
	TSet<FName> ProcessedGuardInvestigationSourceIds;
	EHeistAlertLevel ScheduledAlertSourceLevel = EHeistAlertLevel::Quiet;
	int32 ScheduledAlertRevision = 0;
	bool bLockdownWorldRestrictionsApplied = false;

#pragma endregion

#pragma region Balance

  public:
	UDataTable* GetItemDataTable() const;
	UDataTable* GetContractDataTable() const;
	UDataTable* GetArtifactDataTable() const;
	UDataTable* GetForgeryTemplateDataTable() const;
	UDataTable* GetObjectAssemblyPartDataTable() const;
	UDataTable* GetObjectAssemblyTemplateDataTable() const;
	bool TryGetItemDefinition(FName ItemId, FHeistItemDataRow& OutItemDefinition) const;
	bool TryGetContractDefinition(FName ContractId, FHeistContractDataRow& OutContractDefinition) const;
	bool TryGetArtifactDefinition(FName ArtifactId, FHeistArtifactDataRow& OutArtifactDefinition) const;
	bool TryGetForgeryTemplateDefinition(FName TemplateId, FHeistForgeryTemplateRow& OutTemplateDefinition) const;
	bool TryGetObjectAssemblyPartDefinition(FName PartId, FHeistObjectAssemblyPartRow& OutPartDefinition) const;
	bool TryGetObjectAssemblyTemplateDefinition(FName TemplateId, FHeistObjectAssemblyTemplateRow& OutTemplateDefinition) const;
	bool TryGetLootDefinition(FName ItemId, FHeistLootDataRow& OutLootDefinition) const;
	bool TryGetUsableItemDefinition(FName ItemId, FHeistUsableItemDataRow& OutUsableItemDefinition) const;
	bool TryGetGuardDefinition(FName GuardProfileId, FHeistGuardDataRow& OutGuardDefinition) const;
	bool TryGetSoundPingDefinition(FName SoundPingId, FHeistSoundPingDataRow& OutSoundPingDefinition) const;
	bool TryGetPlayerCountDifficultyBaseline(int32 PlayerCount, FHeistPlayerCountDifficultyBaseline& OutBaseline) const;
	static int32 CalculateDifficultyGuardCount(int32 AuthoredGuardCount, float GuardCountMultiplier);
	bool IsPlayerCountGuardScalingApplied() const;
	int32 GetDifficultyAuthoredGuardCount() const;
	int32 GetDifficultyExpectedGuardCount() const;
	int32 GetDifficultyActiveGuardCount() const;
	int32 GetDifficultyAppliedPlayerCount() const;
	float GetDifficultyAppliedGuardCountMultiplier() const;
	float GetDifficultyAppliedDetectionMultiplier() const;
	float GetDifficultyAppliedInspectionDurationMultiplier() const;
	float GetGuardPerceptionRangeMultiplier() const;
	float GetSecurityCameraEvaluationIntervalSeconds() const;
	float GetSecurityCameraDetectionBuildUpSeconds() const;
	float GetSecurityCameraDetectionCooldownSeconds() const;
	float GetSecurityLaserHoldDurationSeconds() const;
	float GetSecurityLaserRearmGraceSeconds() const;
	void DebugDumpPlayerCountDifficultyBaseline() const;
	bool TrySpawnDroppedLoot(const FHeistLootDropRequest& DropRequest, AHeistLootActor*& OutDroppedLootActor) const;

  private:
	void ValidateItemDataTables() const;
	void InitializeSurfaceTemplateSelection();
	bool GatherSurfaceTemplatePool(FName PoolId, TArray<FName>& OutTemplateIds) const;
	void LockGuardsForPlayerCountResolution();
	void SchedulePlayerCountGuardScaling();
	void ApplyPlayerCountGuardScaling();
	const UHeistGameBalanceDataAsset* ResolveGameBalanceData() const;

	bool bPlayerCountGuardScalingApplied = false;
	int32 DifficultyAuthoredGuardCount = 0;
	int32 DifficultyExpectedGuardCount = 0;
	int32 DifficultyActiveGuardCount = 0;
	int32 DifficultyAppliedPlayerCount = 0;
	float DifficultyAppliedGuardCountMultiplier = 1.0f;
	float DifficultyAppliedDetectionMultiplier = 1.0f;
	float DifficultyAppliedInspectionDurationMultiplier = 1.0f;
	FTimerHandle GuardScalingTimerHandle;

#pragma endregion

#pragma region EscapePhase

  public:
	float GetEscapeCastTimeSeconds() const;

  private:
	void StartEscapePhaseTimer();
	void HandleEscapePhaseTimerElapsed();
	float ResolveEscapePhaseDelaySeconds() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Balance", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistGameBalanceDataAsset> GameBalanceDataAsset;

	FTimerHandle EscapePhaseTimerHandle;

#pragma endregion

#pragma region RuntimeState

  private:
	void InitializeContractFromPlacedTargetCase();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Objective",
			  meta = (AllowPrivateAccess = "true", ToolTip = "Optional explicit target case id. When None, the map's single DisplayCaseId ending in _Target is selected."))
	FName ObjectiveTargetCaseId = NAME_None;

#pragma endregion
};
