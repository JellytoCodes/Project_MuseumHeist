#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "GameFramework/GameModeBase.h"

#include "HeistGameMode.generated.h"

class UHeistGameBalanceDataAsset;
class UHeistInventoryComponent;
class UDataTable;
class AHeistGuardCharacter;
class AHeistGameState;
class AHeistLootActor;
class AHeistLootSpawnPoint;
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
struct FHeistArrestConfiscationPayload;
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
	struct FPlacedTargetCase;
	struct FMatchLooseLootDefinition;
	struct FPlannedMatchLoot;

	void HandleMatchPhaseChanged(EHeistMatchPhase PreviousMatchPhase, EHeistMatchPhase NewMatchPhase);
	void HandlePlayerConnectionsChanged(int32 ConnectedPlayerCount);
	int32 DropDisconnectedPlayerLooseLoot(AHeistPlayerCharacter* ExitingCharacter, AHeistPlayerState* ExitingPlayerState, UHeistInventoryComponent* InventoryComponent, int32& OutFailureCount);
	int32 ClearMatchScopedTimers();

  public:
	void PrepareForOnlineSessionShutdown(FName Reason);
	void HandlePlayerPawnLeavingGame(AHeistPlayerController* ExitingController);
	void NotifyPlayerTerminalStateChanged(AHeistPlayerState* PlayerState, FName TerminalTrigger);

#pragma endregion

#pragma region Arrest

  public:
	bool TryCompletePlayerArrest(AHeistPlayerCharacter* ArrestedCharacter, AActor* ArrestingGuard, FName& OutRejectReason);

  private:
	int32 NextArrestEvidenceSlotIndex = 0;

#pragma endregion

#pragma region ContractOutcome

  public:
#if !UE_BUILD_SHIPPING
	bool ForceContractOutcomeForDebug(FName TerminalTrigger, bool bTreatAsCrewEscaped, bool bTreatAsAllRemainingCrewArrested, bool bTreatAsAllCrewDisconnected);
#endif

  private:
	void StartContractDurationTimer();
	void HandleContractDurationTimerElapsed();
	bool TryResolveContractOutcome(FName TerminalTrigger, bool bForceTerminal = false, bool bTreatAsCrewEscaped = false, bool bTreatAsAllRemainingCrewArrested = false,
								   bool bTreatAsAllCrewDisconnected = false);
	bool BuildTeamResultSnapshot(EHeistContractOutcome Outcome, FName OutcomeReasonId, FHeistTeamResult& OutTeamResult) const;
	bool FinalizeContractOutcome(EHeistContractOutcome Outcome, FName OutcomeReasonId, FName TerminalTrigger);

	FTimerHandle ContractDurationTimerHandle;
	bool bAnyPlayerEscapedThisMatch = false;
	bool bMatchHadPlayer = false;

#pragma endregion

#pragma region Alert

  public:
	bool RequestAlertEscalation(EHeistAlertLevel RequestedAlertLevel, FName TriggerId, bool* bOutLevelChanged = nullptr);
	bool RequestAlertIncrease(float IncreaseAmount, FName TriggerId, bool* bOutMeterChanged = nullptr);
	bool RequestSecurityIncident(const FVector& WorldLocation, FName IncidentId);
	bool RequestForgeryTimeoutInvestigation(const FVector& WorldLocation, FName SourceId);
	static bool TryConsumeOneShotSecurityId(TSet<FName>& InOutProcessedIds, FName SourceId);
	int32 GetProcessedAlertTriggerCount() const;
	int32 GetProcessedSecurityIncidentCount() const;
	int32 GetProcessedGuardInvestigationCount() const;
	int32 GetActiveMatchTimerCount() const;

  private:
	bool RequestNearestGuardInvestigation(const FVector& WorldLocation, FName SourceId, float SearchRadius, AHeistGuardCharacter*& OutAssignedGuard, float& OutDistance, bool& bOutDuplicate,
										  FName& OutReason);
	void InitializeAlertState();
	bool ApplyAlertLevel(EHeistAlertLevel NewAlertLevel, FName TriggerId);
	bool ApplyAlertMeterValue(float NewAlertMeterValue, FName TriggerId, bool* bOutLevelChanged = nullptr);
	bool ApplyLockdownWorldRestrictions(FName TriggerId);
	EHeistAlertLevel ResolveAlertLevelForMeter(float AlertMeterValue) const;
	float ResolveMinimumMeterForAlertLevel(EHeistAlertLevel AlertLevel) const;

	TSet<FName> ProcessedAlertTriggerIds;
	TSet<FName> ProcessedSecurityIncidentIds;
	TSet<FName> ProcessedGuardInvestigationSourceIds;
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
	float GetGuardCaptureAlertIncrease() const;
	float GetDetentionRestraintDurationSeconds() const;
	float GetSecurityCameraEvaluationIntervalSeconds() const;
	float GetSecurityCameraDetectionBuildUpSeconds() const;
	float GetSecurityCameraDetectionCooldownSeconds() const;
	float GetSecurityCameraResumeDelaySeconds() const;
	float GetSecurityLaserHoldDurationSeconds() const;
	float GetSecurityLaserRearmGraceSeconds() const;
	void GetMatchStartLooseLootCounts(int32& OutVaultLootCount, int32& OutExhibitionLootCount) const;
	void DebugDumpPlayerCountDifficultyBaseline() const;
	bool TrySpawnDroppedLoot(const FHeistLootDropRequest& DropRequest, AHeistLootActor*& OutDroppedLootActor) const;

  private:
	void ValidateItemDataTables() const;
	void InitializeSurfaceTemplateSelection();
	bool InitializeMatchLooseLoot(int32 AssignmentSeed, int32& OutSpawnPointCount, int32& OutExpectedLootCount, int32& OutSpawnedLootCount);
	bool GatherMatchLootSpawnPoints(TArray<AHeistLootSpawnPoint*>& VaultSpawnPoints, TArray<AHeistLootSpawnPoint*>& ExhibitionSpawnPoints,
		const TCHAR*& OutRejectReason) const;
	bool GatherMatchLootDefinitions(const UDataTable* LootDataTable, TArray<FMatchLooseLootDefinition>& VaultDefinitions,
		TArray<FMatchLooseLootDefinition>& ExhibitionDefinitions, int32& ReleaseLootRowCount, const TCHAR*& OutRejectReason) const;
	static bool AppendMatchLootCategoryPlan(int32 SpawnCount, TArray<AHeistLootSpawnPoint*>& SpawnPoints, const TArray<FMatchLooseLootDefinition>& Definitions,
		FRandomStream& Random, TArray<FPlannedMatchLoot>& PlannedLoot);
	static bool AppendMatchLootDecorations(int32 ActiveCount, const TArray<AHeistLootSpawnPoint*>& SpawnPoints,
		const TArray<FMatchLooseLootDefinition>& Definitions, FRandomStream& DecorationRandom, TArray<FPlannedMatchLoot>& PlannedLoot);
	static void DestroyStagedMatchLoot(TArray<AHeistLootActor*>& StagedLootActors);
	bool SpawnPlannedMatchLoot(const TArray<FPlannedMatchLoot>& PlannedLoot, UDataTable* LootDataTable, UClass* LootActorClass,
		TArray<AHeistLootActor*>& StagedLootActors, const TCHAR*& OutRejectReason);
	void RollbackMatchLooseLoot(FName Reason);
	bool GatherSurfaceTemplatePool(FName PoolId, TArray<FName>& OutTemplateIds) const;
	void LockGuardsForPlayerCountResolution();
	void SchedulePlayerCountGuardScaling();
	void ApplyPlayerCountGuardScaling();
	const UHeistGameBalanceDataAsset* ResolveGameBalanceData() const;

	bool bPlayerCountGuardScalingApplied = false;
	TArray<FName> SelectedSurfaceTemplateIdsForMatch;
	int32 DifficultyAuthoredGuardCount = 0;
	int32 DifficultyExpectedGuardCount = 0;
	int32 DifficultyActiveGuardCount = 0;
	int32 DifficultyAppliedPlayerCount = 0;
	float DifficultyAppliedGuardCountMultiplier = 1.0f;
	float DifficultyAppliedDetectionMultiplier = 1.0f;
	float DifficultyAppliedInspectionDurationMultiplier = 1.0f;
	FTimerHandle GuardScalingTimerHandle;
	bool bMatchLooseLootInitializationAttempted = false;
	bool bMatchLooseLootInitialized = false;
	int32 MatchLooseLootSpawnPointCount = 0;
	int32 MatchLooseLootExpectedCount = 0;
	int32 MatchLooseLootSpawnedCount = 0;
	int32 MatchLooseLootSeed = 0;
	TArray<TWeakObjectPtr<AHeistLootActor>> MatchLooseLootActors;

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
	void GatherPlacedContractCases(TArray<FPlacedTargetCase>& MatchingTargetCases, TArray<FPlacedTargetCase>& OptionalCases, int32& DeferredObjectCaseCount,
		int32& DeferredObjectDeactivationFailureCount);
	int32 BuildEligibleOptionalContractCases(TArray<FPlacedTargetCase>& OptionalCases, bool bUsesReleaseMatchLootSupply, int32 AssignmentSeed,
		TArray<FPlacedTargetCase>& EligibleOptionalCases, int32& InvalidOptionalCaseCount, int32& InvalidRegionCaseCount);
	static void SelectOptionalContractCases(const TArray<FPlacedTargetCase>& EligibleOptionalCases, int32 MaximumSelectableOptionalCount,
		int32 FourStarOptionalIndex, TArray<const FPlacedTargetCase*>& SelectedOptionalCases, TMap<FName, int32>& SelectedRegionCounts, int32& SelectedOptionalValue,
		FString& SelectedOptionalCaseIds);
	void ApplyContractExhibitAssignments(const FPlacedTargetCase& TargetDisplayCase, const TArray<FPlacedTargetCase>& OptionalCases,
		const TArray<const FPlacedTargetCase*>& SelectedOptionalCases, const TArray<FName>& DecorativeTemplateIds, AHeistGameState* HeistGameState,
		int32 AssignmentRevision, FRandomStream& DecorativeRandom, int32& DeactivatedOptionalCaseCount, int32& AssignedPaintingCaseCount,
		int32& AssignedDecorativeCaseCount);
	static int32 RollbackContractExhibits(const FPlacedTargetCase& TargetDisplayCase, const TArray<FPlacedTargetCase>& OptionalCases);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Objective",
			  meta = (AllowPrivateAccess = "true", ToolTip = "Optional explicit target case id. When None, the map's single DisplayCaseId ending in _Target is selected."))
	FName ObjectiveTargetCaseId = NAME_None;

#pragma endregion
};
