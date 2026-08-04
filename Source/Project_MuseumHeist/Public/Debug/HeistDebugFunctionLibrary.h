#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "Data/HeistArtifactDataTypes.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "HeistDebugFunctionLibrary.generated.h"

class AHeistPaintingDisplayCaseActor;
class AHeistObjectDisplayCaseActor;
class AHeistPlayerController;
class AHeistPlayerState;
class APlayerController;
class UHeistForgeryComponent;
class UHeistGameInstance;
class UHeistObjectAssemblyComponent;

UENUM(BlueprintType)
enum class EHeistDebugLevel : uint8
{
	Info,
	Warning,
	Error,
	Verbose
};

enum class EHeistDebugChannel : uint8
{
	Core,
	Inventory,
	Network,
	UI,
	AI
};

UCLASS()
class PROJECT_MUSEUMHEIST_API UHeistDebugFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

#pragma region Logging

  public:
	UFUNCTION(BlueprintCallable, Category = "Heist|Debug", meta = (DevelopmentOnly, WorldContext = "WorldContextObject", AdvancedDisplay = "bPrintToScreen,Duration"))
	static void Message(const UObject* WorldContextObject, const FString& Message, EHeistDebugLevel Level = EHeistDebugLevel::Info, bool bPrintToScreen = false, float Duration = 3.0f);

  private:
	static void LogMessage(EHeistDebugChannel Channel, EHeistDebugLevel Level, const FString& Message);

#pragma endregion

#pragma region GameplayDebug

  public:
	static void DebugMissingInputAsset(const UObject* WorldContextObject, const TCHAR* AssetName);
	static void DebugInventoryOpenSkipped(const UObject* WorldContextObject);
	static void DebugInventoryRequestRejected(const UObject* WorldContextObject, const TCHAR* RequestName, int32 InstanceId, const TCHAR* Reason);
	static void DebugInventoryDropAccepted(const UObject* WorldContextObject, const UObject* Character, FName ItemId, int32 InstanceId, const UObject* DroppedLootActor, const FVector& DropOrigin);
	static void DebugInventoryItemDefinitionLookupRejected(FName ItemId, const TCHAR* Reason);
	static void DebugInventoryAddRejected(const UObject* OwnerActor, FName ItemId, const TCHAR* Reason, int32 GridColumnCount = INDEX_NONE, int32 GridRowCount = INDEX_NONE);
	static void DebugInventoryItemAdded(const UObject* OwnerActor, FName ItemId, int32 InstanceId, const FIntPoint& GridPosition, const FIntPoint& PlacedSize, bool bRotated, int32 ItemCount);
	static void DebugInventoryItemMoved(const UObject* OwnerActor, int32 InstanceId, const FIntPoint& GridPosition);
	static void DebugInventoryItemRotated(const UObject* OwnerActor, int32 InstanceId, bool bRotated);
	static void DebugInventoryItemRemoved(const UObject* OwnerActor, FName ItemId, int32 InstanceId, int32 ItemCount);
	static void DebugInventoryOccupancyInvalid(int32 InstanceId, FName ItemId, const TCHAR* Reason, const FIntPoint& GridPosition = FIntPoint::ZeroValue,
											   const FIntPoint& ItemSize = FIntPoint::ZeroValue);
	static void DebugQuickSlotAssigned(const UObject* OwnerActor, int32 SlotTypeValue, int32 InstanceId, FName ItemId);

	static void DebugLootPickupRequestReceived(const UObject* WorldContextObject, const UObject* Character, const UObject* TargetLootActor);
	static void DebugLootPickupRequestRejected(const UObject* WorldContextObject, const UObject* TargetLootActor, const TCHAR* Reason, float Distance = -1.0f);
	static void DebugLootPickupRequestAccepted(const UObject* WorldContextObject, const UObject* TargetLootActor, FName ItemId, int32 InstanceId, float Distance);

	static void DebugEscapeRequestRejected(const UObject* WorldContextObject, const UObject* TargetVentActor, const TCHAR* Reason, float Distance = -1.0f);
	static void DebugEscapeRequestAccepted(const UObject* WorldContextObject, const UObject* Character, const UObject* TargetVentActor, float Distance);
	static void DebugEscapeCastStarted(const UObject* WorldContextObject, const UObject* Character, const UObject* TargetVentActor, float DurationSeconds, float EndServerTime);
	static void DebugEscapeCastStateReplicated(const UObject* WorldContextObject, const UObject* Character, bool bIsActive, float EndServerTime);
	static void DebugEscapeCastCompleted(const UObject* WorldContextObject, const UObject* Character, const UObject* TargetVentActor);
	static void DebugEscapeCastCancelled(const UObject* WorldContextObject, const FString& CharacterName, const FString& VentName, const TCHAR* Reason);
	static void DebugObservationRequestRejected(const UObject* WorldContextObject, const UObject* TargetDisplayCase, const TCHAR* Reason, float Distance = -1.0f);
	static void DebugObservationCastStarted(const UObject* WorldContextObject, const UObject* Character, const UObject* TargetDisplayCase, float DurationSeconds, float EndServerTime);
	static void DebugObservationCastStateReplicated(const UObject* WorldContextObject, const UObject* Character, bool bIsActive, float EndServerTime);
	static void DebugObservationCastCompleted(const UObject* WorldContextObject, const UObject* Character, const UObject* TargetDisplayCase);
	static void DebugObservationCastCancelled(const UObject* WorldContextObject, const FString& CharacterName, const FString& DisplayCaseName, const TCHAR* Reason);

	static void DebugForgerySessionBeginRejected(const UHeistForgeryComponent* ForgeryComponent, const AHeistPaintingDisplayCaseActor* TargetDisplayCase, FName Reason);
	static void DebugForgeryTemplatePreparationRejected(const UHeistForgeryComponent* ForgeryComponent, const AHeistPaintingDisplayCaseActor* TargetDisplayCase, FName ArtifactId,
														const FHeistForgeryTemplateRow* TemplateDefinition, FName Reason);
	static void DebugForgeryTemplatePrepared(const UHeistForgeryComponent* ForgeryComponent, const AHeistPaintingDisplayCaseActor* TargetDisplayCase,
											 const FHeistForgeryTemplateRow& TemplateDefinition);
	static void DebugSurfaceTemplateSelectionState(const UObject* WorldContextObject, const TCHAR* ChangeSource, FName PoolId, FName TemplateId, int32 PoolSize,
												   int32 BagCycle, int32 RemainingCount, int32 SelectionRevision, bool bAccepted);
	static void DebugForgerySubmitRejected(const UHeistForgeryComponent* ForgeryComponent, FName Reason);
	static void DebugForgeryStrokePayloadRejected(const UHeistForgeryComponent* ForgeryComponent, int32 StrokeCount, int32 PointCount, int32 PayloadBytes, int32 BrushPresetCount,
												  int32 ClientSessionRevision, FName Reason);
	static void DebugForgeryStrokePayloadAccepted(const UHeistForgeryComponent* ForgeryComponent, const AHeistPaintingDisplayCaseActor* SubmittedDisplayCase,
												  int32 ValidatedSessionRevision);
	static void DebugForgeryStrokeValidationResult(const UHeistForgeryComponent* ForgeryComponent, bool bAccepted, FName Reason);
	static void DebugForgeryScoreCalculationRejected(const UHeistForgeryComponent* ForgeryComponent, FName Reason);
	static void DebugForgeryPaintingDataBuildRejected(const UHeistForgeryComponent* ForgeryComponent, FName Reason);
	static void DebugForgeryScoreCommitRejected(const UHeistForgeryComponent* ForgeryComponent, const AHeistPaintingDisplayCaseActor* TargetDisplayCase, FName Reason);
	static void DebugForgeryReferenceDecodeFailed(const UHeistForgeryComponent* ForgeryComponent, const UObject* Texture, FName SourceKind, int32 PixelFormat, int32 SourceWidth,
												  int32 SourceHeight);
	static void DebugForgeryOpenCVMetrics(const UHeistForgeryComponent* ForgeryComponent, float DistanceRecall, float DistancePrecision, float BidirectionalDistance,
										 float MaskPrecision, float MaskRecall, float MaskIoU, float MaskDice, float LabSSIM, float PaletteHistogram, float ColorGeometric,
										 float PaletteBonus);
	static void DebugForgeryReferenceMaskRejected(const UHeistForgeryComponent* ForgeryComponent, bool bUseReferenceMask, int32 ReferencePixels, int32 TotalPixels);
	static void DebugForgeryScoreCommitted(const UHeistForgeryComponent* ForgeryComponent, const AHeistPaintingDisplayCaseActor* TargetDisplayCase, int32 TotalScorePixels,
										   float PaintCompletenessExponent, bool bReferenceCacheHit, double ReferenceMilliseconds, double OpenCVMilliseconds,
										   double TotalMilliseconds);
	static void DebugForgerySessionCompleted(const UHeistForgeryComponent* ForgeryComponent, const AHeistPaintingDisplayCaseActor* PreviousDisplayCase);
	static void DebugForgerySessionCleared(const UHeistForgeryComponent* ForgeryComponent, const AHeistPaintingDisplayCaseActor* PreviousDisplayCase, FName Reason);
	static void DebugForgerySessionSnapshot(const UHeistForgeryComponent* ForgeryComponent, const TCHAR* ChangeSource, FName Reason);
	static void DebugForgeryStrokeValidationReplicated(const UHeistForgeryComponent* ForgeryComponent);
	static void DebugForgeryScoreReplicated(const UHeistForgeryComponent* ForgeryComponent);

	static void DebugObjectAssemblyCaseSnapshot(const AHeistObjectDisplayCaseActor* DisplayCase, FName EventName, FName Reason, bool bResult);
	static void DebugObjectAssemblyCaseSessionCleared(const AHeistObjectDisplayCaseActor* DisplayCase, const AHeistPlayerState* PreviousOwner, FName Reason);
	static void DebugObjectAssemblySessionSnapshot(const UHeistObjectAssemblyComponent* ObjectAssemblyComponent, FName EventName, FName Reason, bool bResult);
	static void DebugObjectAssemblyPayloadValidation(const UHeistObjectAssemblyComponent* ObjectAssemblyComponent, bool bAccepted, FName Reason, int32 EntryCount,
													 int32 PayloadBytes);
	static void DebugObjectAssemblyScoreCommitted(const UHeistObjectAssemblyComponent* ObjectAssemblyComponent, const FHeistObjectAssemblyResult& Result);
	static void DebugObjectAssemblySessionCleared(const UHeistObjectAssemblyComponent* ObjectAssemblyComponent, const AHeistObjectDisplayCaseActor* PreviousDisplayCase,
												  FName Reason, bool bPreservedResult);
	static void DebugObjectAssemblyReplicaCommit(const AHeistObjectDisplayCaseActor* DisplayCase, const AHeistPlayerState* RequestingPlayerState,
												 const FHeistObjectAssemblyResult& Result, int32 EntryCount, FName Reason, bool bResult);
	static void DebugObjectAssemblyReplicaRebuildEvent(const AHeistObjectDisplayCaseActor* DisplayCase, int32 ExpectedEntryCount, int32 BuiltPartCount,
													   int32 UnresolvedSocketCount, bool bCoreReady, int32 ReplicaRevision, bool bResult);
	static void DebugObjectAssemblyOriginalCarry(const AHeistObjectDisplayCaseActor* DisplayCase, const AHeistPlayerState* Carrier, FName EventName, FName Reason,
												 bool bResult);
	static void DebugObjectAssemblyInspection(const AHeistObjectDisplayCaseActor* DisplayCase, const AActor* InspectingGuard, FName EventName, FName Reason, bool bResult);

	static void DebugLootScoreWeightRejected(const UObject* WorldContextObject, const TCHAR* Reason, int32 ScoreDelta = INDEX_NONE, float WeightDelta = -1.0f);
	static void DebugLootScoreWeightApplied(const UObject* WorldContextObject, int32 ScoreDelta, float WeightDelta, int32 TotalScore, float TotalWeight);
	static void DebugLootScoreWeightRemoved(const UObject* WorldContextObject, int32 ScoreDelta, float WeightDelta, int32 TotalScore, float TotalWeight);
	static void DebugPlayerEscapeStateRejected(const UObject* WorldContextObject, const TCHAR* Reason);
	static void DebugPlayerEscapeStateCommitted(const UObject* WorldContextObject, int32 HeistPlayerId, int32 FinalScore, float EscapeTimeSeconds);
	static void DebugPlayerEscapeStateReplicated(const UObject* WorldContextObject, int32 HeistPlayerId, bool bEscaped);
	static void DebugPlayerStateScoreReplicated(const UObject* WorldContextObject, int32 TotalLootScore);
	static void DebugPlayerStateWeightReplicated(const UObject* WorldContextObject, float TotalLootWeight);

	static void DebugWeightMovementSkipped(const UObject* WorldContextObject, const TCHAR* Reason);
	static void DebugWeightMovementSpeedApplied(const UObject* WorldContextObject, float TotalWeight, float BaseSpeed, float FinalSpeed);
	static void DebugThrowableUseRejected(const UObject* WorldContextObject, EHeistQuickSlotType SlotType, FName ItemId, const TCHAR* Reason);
	static void DebugThrowableProjectileSpawned(const UObject* WorldContextObject, const UObject* Character, const UObject* Projectile, FName ItemId, const FVector& TargetWorldLocation,
												const FVector& LaunchDirection, float ProjectileSpeed, bool bDebugBypassInventory);
	static void DebugThrowableProjectileImpact(const UObject* WorldContextObject, const UObject* Projectile, const UObject* OtherActor, FName ItemId, const FVector& ImpactLocation);

	static void DebugRareLootTimersStarted(const UObject* WorldContextObject, const TArray<float>& EventTimes, float WarningLeadTime);
	static void DebugRareLootWarningStarted(const UObject* WorldContextObject, int32 EventIndex, FName ItemId, float SpawnServerTime);
	static void DebugRareLootSpawned(const UObject* WorldContextObject, int32 EventIndex, const UObject* LootActor, const UObject* SpawnPoint, FName ItemId, const FVector& WorldLocation);
	static void DebugRareLootEventFailed(const UObject* WorldContextObject, int32 EventIndex, const TCHAR* Reason);
	static void DebugRareLootPickedUp(const UObject* WorldContextObject, int32 EventIndex, const UObject* LootActor, const UObject* Requester, FName ItemId);

	static void DebugGuardStunApplied(const UObject* WorldContextObject, const UObject* GuardActor, float DurationSeconds);
	static void DebugGuardStunCleared(const UObject* WorldContextObject, const UObject* GuardActor, EHeistGuardState NewState);
	static void DebugGuardStateChanged(const UObject* WorldContextObject, const UObject* GuardActor, EHeistGuardState PreviousState, EHeistGuardState NewState, float StateEndServerTime);
	static void DebugGuardStateRequestRejected(const UObject* WorldContextObject, const UObject* GuardActor, EHeistGuardState RequestedState, const TCHAR* Reason);
	static void DebugGuardStateReplicated(const UObject* WorldContextObject, const UObject* GuardActor, EHeistGuardState NewState);
	static void DebugDrawGuardSpawnMarker(const UObject* WorldContextObject, UObject* GuardActor);
	static void DebugGuardStateTreeEvent(const UObject* WorldContextObject, const UObject* GuardActor, const FGameplayTag& StateEventTag);
	static void DebugGuardPerceptionConfigured(const UObject* WorldContextObject, const UObject* GuardActor, float SightRadius, float AggroResetDistance, float SightAngle, float InvestigateSightAngle,
											   float EyeHeight, float DetectionGrace, bool bDoorsBlockSight, bool bDisplayCasesBlockSight, FName DoorOccluderTag, float UpdateInterval);
	static void DebugGuardSightEvaluated(const UObject* WorldContextObject, const UObject* GuardActor, const UObject* TargetActor, bool bCanSeeTarget, const TCHAR* Reason,
										 const UObject* BlockingActor);
	static void DebugGuardDetectionGraceStarted(const UObject* WorldContextObject, const UObject* GuardActor, const UObject* TargetActor, float DurationSeconds);
	static void DebugGuardDetectionGraceCancelled(const UObject* WorldContextObject, const UObject* GuardActor, const UObject* TargetActor, const TCHAR* Reason);
	static void DebugGuardSightTargetAcquired(const UObject* WorldContextObject, const UObject* GuardActor, const UObject* TargetActor);
	static void DebugGuardSightTargetLost(const UObject* WorldContextObject, const UObject* GuardActor, const UObject* TargetActor, const FVector& LastKnownLocation, const TCHAR* Reason);
	static void DebugGuardNoiseReactionAccepted(const UObject* WorldContextObject, const UObject* GuardActor, const FHeistSoundPingEvent& SoundPingEvent, float Distance, float InvestigateDuration);
	static void DebugGuardNoiseReactionRejected(const UObject* WorldContextObject, const UObject* GuardActor, const FHeistSoundPingEvent& SoundPingEvent, const TCHAR* Reason, float Distance = -1.0f);
	static void DebugCoinDistractionDecision(const UObject* WorldContextObject, const UObject* GuardActor, const FHeistSoundPingEvent& SoundPingEvent, EHeistGuardState GuardState,
											 const TCHAR* Decision, const TCHAR* Rule, int32 CoinPriority, EHeistSoundPingType PreviousCandidateType, int32 PreviousCandidatePriority);
	static void DebugGuardPatrolPathResolved(const UObject* WorldContextObject, const UObject* GuardActor, FName RouteId, int32 WaypointCount);
	static void DebugGuardMoveStalled(const UObject* WorldContextObject, const UObject* GuardActor, EHeistGuardState GuardState, const FVector& Destination, float NoProgressSeconds,
									  uint8 RetryCount);
	static void DebugGuardInvestigateConfirmationStarted(const UObject* WorldContextObject, const UObject* GuardActor, const FVector& InvestigateLocation, float DurationSeconds);
	static void DebugGuardSearchTimerStarted(const UObject* WorldContextObject, const UObject* GuardActor, const FVector& SearchLocation, float DurationSeconds);
	static void DebugSoundPingDefinitionRejected(const UObject* WorldContextObject, FName SoundPingId, const TCHAR* Reason);
	static void DebugEscapedPlayerRestrictionsApplied(const UObject* WorldContextObject);

#pragma endregion

#pragma region LobbyDebug

  public:
	static void DebugLobbyHelp(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Lobby", meta = (DevelopmentOnly))
	static void DebugLobbyShow(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Lobby", meta = (DevelopmentOnly))
	static void DebugLobbyHide(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Lobby", meta = (DevelopmentOnly))
	static void DebugLobbyDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Lobby", meta = (DevelopmentOnly))
	static void DebugOnlineSessionHost(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Lobby", meta = (DevelopmentOnly))
	static void DebugOnlineSessionJoin(APlayerController* PlayerController, const FString& JoinCode);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Lobby", meta = (DevelopmentOnly))
	static void DebugOnlineSessionLeave(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Lobby", meta = (DevelopmentOnly))
	static void DebugOnlineSessionCancel(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Lobby", meta = (DevelopmentOnly))
	static void DebugOnlineSessionCancelTest(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Lobby", meta = (DevelopmentOnly))
	static void DebugOnlineSessionRetry(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Lobby", meta = (DevelopmentOnly))
	static void DebugOnlineSessionFailure(APlayerController* PlayerController, FName FailureReason);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Lobby", meta = (DevelopmentOnly))
	static void DebugOnlineSessionMap(APlayerController* PlayerController, const FString& MapId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Lobby", meta = (DevelopmentOnly))
	static void DebugOnlineSessionStart(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Lobby", meta = (DevelopmentOnly))
	static void DebugOnlineSessionComplete(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Lobby", meta = (DevelopmentOnly))
	static void DebugOnlineSessionReturn(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Lobby", meta = (DevelopmentOnly))
	static void DebugOnlineSessionDump(APlayerController* PlayerController);

	static void DebugOnlineSessionControllerRequest(const UObject* WorldContextObject, const TCHAR* Request, bool bAccepted, FName FailureReason);
	static void DebugOnlineSessionRequest(const UHeistGameInstance* GameInstance, const TCHAR* Request, FName SubsystemName, const FString& JoinCode, FName State, bool bAccepted,
										  const TCHAR* Reason);
	static void DebugOnlineSessionCreateComplete(const UHeistGameInstance* GameInstance, FName SessionName, FName SubsystemName, const FString& JoinCode, bool bCreated,
												 bool bTravelAccepted, FName FailureReason);
	static void DebugOnlineSessionFindComplete(const UHeistGameInstance* GameInstance, const FString& JoinCode, int32 ResultCount, int32 MatchingCodeCount, int32 FullMatchCount,
											   int32 VersionMismatchCount, FName SelectedSessionId, bool bJoinRequestAccepted, FName FailureReason);
	static void DebugOnlineSessionJoinComplete(const UHeistGameInstance* GameInstance, FName SessionName, const FString& JoinCode, int32 JoinResult, bool bAddressResolved,
											   bool bTravelRequested, FName FailureReason);
	static void DebugOnlineSessionLeaveRequest(const UHeistGameInstance* GameInstance, bool bWasHosting, FName State, bool bAccepted, FName FailureReason);
	static void DebugOnlineSessionDestroyComplete(const UHeistGameInstance* GameInstance, FName SessionName, bool bWasHosting, bool bDestroyed, bool bReturnedToTitleMenu,
												  FName LeaveReason, FName FailureReason);
	static void DebugOnlineSessionRemoteEnded(const UHeistGameInstance* GameInstance, FName Reason, bool bLeaveStarted);
	static void DebugOnlineSessionMapSelection(const UHeistGameInstance* GameInstance, FName RequestedMapId, FName ResolvedMapId, bool bRandomSelection, bool bOnlineUpdateRequested,
											   bool bAccepted, FName FailureReason);
	static void DebugLobbyMapSelectionState(const UObject* WorldContextObject, const TCHAR* ChangeSource, FName SelectedMapId, bool bRandomSelection, int32 Revision, bool bAccepted);
	static void DebugOnlineSessionShutdownCleanup(const UObject* WorldContextObject, FName Reason, int32 CancelledActionCount, int32 CancelledForgeryCount, int32 ClosedInventoryCount,
												  int32 ReleasedOriginalCount, int32 ClearedCaseLockCount, int32 ClearedTimerCount, bool bAuthority);

#pragma endregion

#pragma region ResultDebug

  public:
	static void DebugResultHelp(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Result", meta = (DevelopmentOnly))
	static void DebugResultShow(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Result", meta = (DevelopmentOnly))
	static void DebugResultHide(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Result", meta = (DevelopmentOnly))
	static void DebugResultDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Result", meta = (DevelopmentOnly))
	static void DebugResultRebuild(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Result", meta = (DevelopmentOnly))
	static void DebugResultSeed(APlayerController* PlayerController, int32 Score, bool bEscaped, float EscapeTimeSeconds);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Result", meta = (DevelopmentOnly))
	static void DebugContributionSeed(APlayerController* PlayerController, int32 SurfaceForgeries, float BestSurfaceQuality, int32 Assemblies,
		float BestAssemblyQuality, int32 ArtifactsRecovered, float CarryTimeSeconds, int32 SecuredLootValue, int32 GuardsDistracted, int32 TeammatesRescued,
		int32 AlarmsTriggered);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Gate", meta = (DevelopmentOnly))
	static void DebugExitPlacementDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Gate", meta = (DevelopmentOnly))
	static void DebugMissionGateDump(APlayerController* PlayerController);

#pragma endregion

#pragma region ObjectiveDebug

  public:
	static void DebugObjectiveHelp(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Objective", meta = (DevelopmentOnly))
	static void DebugObjectiveDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Objective", meta = (DevelopmentOnly))
	static void DebugM01ObjectivePlacementDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Objective", meta = (DevelopmentOnly))
	static void DebugCoreGrayboxDump(APlayerController* PlayerController, const FString& MapId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Objective", meta = (DevelopmentOnly))
	static void DebugObjectiveSet(APlayerController* PlayerController, FName ArtifactId, FName CaseId, const FString& StateName, bool bUseLocalPlayerAsCarrier);

#pragma endregion

#pragma region DisplayCaseDebug

  public:
	static void DebugDisplayCaseHelp(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|DisplayCase", meta = (DevelopmentOnly))
	static void DebugDisplayCaseSpawn(APlayerController* PlayerController, float Distance);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|DisplayCase", meta = (DevelopmentOnly))
	static void DebugDisplayCaseSpawnFor(APlayerController* PlayerController, int32 PlayerId, float Distance);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|DisplayCase", meta = (DevelopmentOnly))
	static void DebugDisplayCaseDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|DisplayCase", meta = (DevelopmentOnly))
	static void DebugDisplayCaseAdvance(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|DisplayCase", meta = (DevelopmentOnly))
	static void DebugDisplayCaseSet(APlayerController* PlayerController, const FString& StateName);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|DisplayCase", meta = (DevelopmentOnly))
	static void DebugDisplayCaseBegin(APlayerController* PlayerController, int32 PlayerId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|DisplayCase", meta = (DevelopmentOnly))
	static void DebugDisplayCaseCancel(APlayerController* PlayerController, int32 PlayerId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|DisplayCase", meta = (DevelopmentOnly))
	static void DebugDisplayCasePhase(APlayerController* PlayerController, const FString& PhaseName);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Original", meta = (DevelopmentOnly))
	static void DebugOriginalHelp(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Original", meta = (DevelopmentOnly))
	static void DebugOriginalDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Original", meta = (DevelopmentOnly))
	static void DebugOriginalTake(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Original", meta = (DevelopmentOnly))
	static void DebugOriginalDrop(APlayerController* PlayerController);

#pragma endregion

#pragma region DepositDebug

  public:
	static void DebugDepositHelp(APlayerController* PlayerController);
	static void DebugDepositOpen(APlayerController* PlayerController);
	static void DebugDepositSpawnFor(APlayerController* PlayerController, int32 PlayerId, float Distance);
	static void DebugDepositDump(APlayerController* PlayerController, int32 PlayerId);

#pragma endregion

#pragma region OutcomeDebug

  public:
	static void DebugOutcomeHelp(APlayerController* PlayerController);
	static void DebugOutcomeSeed(APlayerController* PlayerController, int32 SecuredValue, bool bRequiredTargetSecured);
	static void DebugOutcomeResolve(APlayerController* PlayerController, const FString& TerminalTrigger);
	static void DebugOutcomeDump(APlayerController* PlayerController);

#pragma endregion

#pragma region ObjectAssemblyDebug

  public:
	static void DebugObjectAssemblyHelp(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|ObjectAssembly", meta = (DevelopmentOnly))
	static void DebugObjectAssemblySpawn(APlayerController* PlayerController, float Distance);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|ObjectAssembly", meta = (DevelopmentOnly))
	static void DebugObjectAssemblySpawnFor(APlayerController* PlayerController, int32 PlayerId, float Distance);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|ObjectAssembly", meta = (DevelopmentOnly))
	static void DebugObjectAssemblyContentSpawn(APlayerController* PlayerController, const FString& Family, int32 Variant, float Distance);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|ObjectAssembly", meta = (DevelopmentOnly))
	static void DebugObjectAssemblyContentSpawnFor(APlayerController* PlayerController, int32 PlayerId, const FString& Family, int32 Variant, float Distance);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|ObjectAssembly", meta = (DevelopmentOnly))
	static void DebugObjectAssemblyKickPlayer(APlayerController* PlayerController, int32 PlayerId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|ObjectAssembly", meta = (DevelopmentOnly))
	static void DebugObjectAssemblyBegin(APlayerController* PlayerController, float DurationSeconds);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|ObjectAssembly", meta = (DevelopmentOnly))
	static void DebugObjectAssemblyTest(APlayerController* PlayerController, const FString& Scenario);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|ObjectAssembly", meta = (DevelopmentOnly))
	static void DebugObjectAssemblyDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|ObjectAssembly", meta = (DevelopmentOnly))
	static void DebugObjectAssemblyUIDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|ObjectAssembly", meta = (DevelopmentOnly))
	static void DebugObjectAssemblyCancel(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|ObjectAssembly", meta = (DevelopmentOnly))
	static void DebugObjectAssemblyTimeout(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|ObjectAssembly", meta = (DevelopmentOnly))
	static void DebugObjectAssemblyReplicaDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|ObjectAssembly", meta = (DevelopmentOnly))
	static void DebugObjectAssemblyPrototypeGate(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|ObjectAssembly", meta = (DevelopmentOnly))
	static void DebugObjectAssemblyContentValidate(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|ObjectAssembly", meta = (DevelopmentOnly))
	static void DebugObjectAssemblyReplicaRebuild(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|ObjectAssembly", meta = (DevelopmentOnly))
	static void DebugObjectAssemblyTakeOriginal(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|ObjectAssembly", meta = (DevelopmentOnly))
	static void DebugObjectAssemblyInspectionReady(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|ObjectAssembly", meta = (DevelopmentOnly))
	static void DebugObjectAssemblyInspectionGate(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|ObjectAssembly", meta = (DevelopmentOnly))
	static void DebugObjectAssemblyTestIsolation(APlayerController* PlayerController, bool bEnabled);

#pragma endregion

#pragma region ForgeryDebug

  public:
	static void DebugForgeryHelp(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Forgery", meta = (DevelopmentOnly))
	static void DebugForgeryDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Forgery", meta = (DevelopmentOnly))
	static void DebugForgeryInputDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Forgery", meta = (DevelopmentOnly))
	static void DebugForgeryTemplateDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Forgery", meta = (DevelopmentOnly))
	static void DebugSurfaceTemplateDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Forgery", meta = (DevelopmentOnly))
	static void DebugSurfaceTemplatePoolTest(APlayerController* PlayerController, int32 PoolSize = 12);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Forgery", meta = (DevelopmentOnly))
	static void DebugSurfaceTemplateContentValidate(APlayerController* PlayerController, const FString& PoolId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Forgery", meta = (DevelopmentOnly))
	static void DebugForgeryStrokeDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Forgery", meta = (DevelopmentOnly))
	static void DebugForgeryTransportDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Forgery", meta = (DevelopmentOnly))
	static void DebugForgeryTransportTest(APlayerController* PlayerController, const FString& Scenario);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Forgery", meta = (DevelopmentOnly))
	static void DebugForgeryScoreDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Forgery", meta = (DevelopmentOnly))
	static void DebugForgeryScoreTest(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Forgery", meta = (DevelopmentOnly))
	static void DebugForgerySwapDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Forgery", meta = (DevelopmentOnly))
	static void DebugForgeryVisualDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Forgery", meta = (DevelopmentOnly))
	static void DebugForgeryPaintingDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Forgery", meta = (DevelopmentOnly))
	static void DebugForgeryBegin(APlayerController* PlayerController, float DurationSeconds);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Forgery", meta = (DevelopmentOnly))
	static void DebugForgerySubmit(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Forgery", meta = (DevelopmentOnly))
	static void DebugForgeryCancel(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Forgery", meta = (DevelopmentOnly))
	static void DebugForgeryTimeout(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Forgery", meta = (DevelopmentOnly))
	static void DebugForgeryRecoveryDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Forgery", meta = (DevelopmentOnly))
	static void DebugForgeryRecoveryRace(APlayerController* PlayerController, const FString& Order);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Forgery", meta = (DevelopmentOnly))
	static void DebugForgeryUIDump(APlayerController* PlayerController);

#pragma endregion

#pragma region BuildDebug

  public:
	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Build", meta = (DevelopmentOnly))
	static void DebugBuildDump(APlayerController* PlayerController);

#pragma endregion

#pragma region SettingsDebug

  public:
	static void DebugSettingsHelp(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Settings", meta = (DevelopmentOnly))
	static void DebugSettingsDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Settings", meta = (DevelopmentOnly))
	static void DebugSettingsApply(APlayerController* PlayerController, float FieldOfView, float MouseSensitivity, float MasterVolume, int32 ResolutionWidth,
								   int32 ResolutionHeight, const FString& WindowMode);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Settings", meta = (DevelopmentOnly))
	static void DebugSettingsReset(APlayerController* PlayerController);

#pragma endregion

#pragma region InventoryDebug

  public:
	static void DebugInventoryHelp(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Inventory", meta = (DevelopmentOnly))
	static void DebugInventoryDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Inventory", meta = (DevelopmentOnly))
	static void DebugInventoryOpen(APlayerController* PlayerController, bool bOpen);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Inventory", meta = (DevelopmentOnly))
	static void DebugInventoryAdd(APlayerController* PlayerController, FName ItemId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Inventory", meta = (DevelopmentOnly))
	static void DebugInventoryMove(APlayerController* PlayerController, int32 InstanceId, int32 GridX, int32 GridY);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Inventory", meta = (DevelopmentOnly))
	static void DebugInventoryRotate(APlayerController* PlayerController, int32 InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Inventory", meta = (DevelopmentOnly))
	static void DebugInventoryDrop(APlayerController* PlayerController, int32 InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Inventory", meta = (DevelopmentOnly))
	static void DebugInventoryAssignQuickSlot(APlayerController* PlayerController, const FString& SlotName, int32 InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Inventory", meta = (DevelopmentOnly))
	static void DebugInventoryClearQuickSlot(APlayerController* PlayerController, const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Inventory", meta = (DevelopmentOnly))
	static void DebugInventoryInvalidMove(APlayerController* PlayerController, int32 InstanceId);

#pragma endregion

#pragma region StatusDebug

  public:
	static void DebugStatusHelp(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Status", meta = (DevelopmentOnly))
	static void DebugStatusDump(APlayerController* PlayerController);

#pragma endregion

#pragma region FeedbackDebug

  public:
	static void DebugFeedbackHelp(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Feedback", meta = (DevelopmentOnly))
	static void DebugFeedbackTest(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Feedback", meta = (DevelopmentOnly))
	static void DebugFeedbackBagFull(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Feedback", meta = (DevelopmentOnly))
	static void DebugFeedbackDump(APlayerController* PlayerController);

#pragma endregion

#pragma region ThrowableDebug

  public:
	static void DebugThrowableHelp(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Throwable", meta = (DevelopmentOnly))
	static void DebugCoinThrow(APlayerController* PlayerController, float Distance);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Throwable", meta = (DevelopmentOnly))
	static void DebugCoinThrowAt(APlayerController* PlayerController, float TargetX, float TargetY, float TargetZ);

#pragma endregion

#pragma region SoundPingDebug

  public:
	static void DebugSoundPingHelp(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|SoundPing", meta = (DevelopmentOnly, ClampMin = "0.0"))
	static void DebugFootstepWeight(APlayerController* PlayerController, float TotalLootWeight);

#pragma endregion

#pragma region HUDDebug

  public:
	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|HUD", meta = (DevelopmentOnly))
	static void DebugFirstPersonHUDDump(APlayerController* PlayerController);

#pragma endregion

#pragma region TutorialDebug

  public:
	static void DebugTutorialHelp(APlayerController* PlayerController);
	static void DebugTutorialDump(APlayerController* PlayerController);
	static void DebugTutorialReset(APlayerController* PlayerController);
	static void DebugTutorialAdvance(APlayerController* PlayerController);
	static void DebugTutorialSkip(APlayerController* PlayerController);
	static void DebugTutorialTransition(APlayerController* PlayerController, FName EventId, FName StepId, int32 StepIndex, int32 StepCount, bool bActive, bool bCompleted, bool bResult);

#pragma endregion

#pragma region FirstPersonScaleDebug

  public:
	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|FirstPerson", meta = (DevelopmentOnly, ClampMin = "0.0", Units = "cm"))
	static void DebugFirstPersonScaleCheck(APlayerController* PlayerController, float ForwardDistance = 200.0f);

#pragma endregion

#pragma region GuardDebug

  public:
	static void DebugGuardHelp(APlayerController* PlayerController);
	static void DebugDifficultyDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Guard", meta = (DevelopmentOnly))
	static void DebugGuardSpawn(APlayerController* PlayerController, float Distance);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Guard", meta = (DevelopmentOnly))
	static void DebugGuardDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Guard", meta = (DevelopmentOnly))
	static void DebugInspectionTargetSelect(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Guard", meta = (DevelopmentOnly))
	static void DebugInspectionTargetDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Guard", meta = (DevelopmentOnly))
	static void DebugInspectionBegin(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Guard", meta = (DevelopmentOnly))
	static void DebugInspectionStateDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Guard", meta = (DevelopmentOnly))
	static void DebugInspectionProtectionDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Guard", meta = (DevelopmentOnly))
	static void DebugInspectionTimerProtectionTest(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Alert", meta = (DevelopmentOnly))
	static void DebugAlertDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Alert", meta = (DevelopmentOnly))
	static void DebugLockdownDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Alert", meta = (DevelopmentOnly))
	static void DebugGuardAlertModifiersDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Alert", meta = (DevelopmentOnly))
	static void DebugAlertRequest(APlayerController* PlayerController, const FString& LevelName);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Guard", meta = (DevelopmentOnly))
	static void DebugGuardSetState(APlayerController* PlayerController, const FString& StateName, float DurationSeconds);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Guard", meta = (DevelopmentOnly))
	static void DebugGuardSightCheck(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Guard", meta = (DevelopmentOnly))
	static void DebugGuardAutomaticSight(APlayerController* PlayerController, bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Guard", meta = (DevelopmentOnly))
	static void DebugGuardNoise(APlayerController* PlayerController, float Distance);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Guard", meta = (DevelopmentOnly))
	static void DebugSetPlayerArrested(APlayerController* PlayerController, bool bArrested);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Guard", meta = (DevelopmentOnly))
	static void DebugArrestDump(APlayerController* PlayerController);

#pragma endregion
};
