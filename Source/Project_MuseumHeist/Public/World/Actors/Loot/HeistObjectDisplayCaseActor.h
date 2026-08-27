#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "World/Interaction/HeistInteractableActor.h"

#include "HeistObjectDisplayCaseActor.generated.h"

class AHeistGameState;
class AHeistPlayerState;
class AHeistDroppedOriginalActor;
class USceneComponent;
class USoundBase;
class UStaticMeshComponent;
struct FHeistInventoryItem;
struct FHeistObjectAssemblyPartRow;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FHeistObjectAssemblySessionChangedSignature, AHeistPlayerState*, SessionOwner, bool, bLocked, int32, Revision);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FHeistObjectOriginalCarryChangedSignature, AHeistPlayerState*, Carrier, FName, ArtifactId, int32, Revision);

/**
 * Generic display-case contract for Sculpture and Ceramic Object Assembly.
 */
UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistObjectDisplayCaseActor : public AHeistInteractableActor
{
	GENERATED_BODY()

  public:
	AHeistObjectDisplayCaseActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

  protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

  public:
	UFUNCTION(BlueprintPure, Category = "Heist|Object Assembly|Contract")
	bool IsContractExhibitActive() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Heist|Object Assembly|Contract")
	bool SetContractExhibitActive(bool bActive);

	UFUNCTION(BlueprintPure, Category = "Heist|Object Assembly")
	FName GetObjectCaseId() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Object Assembly")
	FName GetTargetArtifactId() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Object Assembly")
	FName GetObjectFamilyId() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Object Assembly")
	EHeistObjectAssemblyState GetAssemblyState() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Object Assembly")
	int32 GetAssemblyRevision() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Object Assembly")
	FHeistObjectAssemblyReplicaData GetAssemblyReplicaData() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Object Assembly|Replica")
	bool HasCommittedAssemblyResult() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Object Assembly|Replica")
	bool HasReplicaPreview() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Object Assembly|Replica|Feedback")
	bool IsReplicaReviewReadyFor(const AActor* Interactor) const;

	UFUNCTION(BlueprintPure, Category = "Heist|Object Assembly|Replica")
	FHeistObjectAssemblyResult GetCommittedAssemblyResult() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Heist|Object Assembly|Replica")
	bool TryCommitAssemblyReplica(AHeistPlayerState* RequestingPlayerState, const FHeistObjectAssemblyResult& AssemblyResult, const TArray<FHeistObjectAssemblyEntry>& Entries);

	bool TryRestartAssemblyFromPreview(AHeistPlayerState* RequestingPlayerState, FName& OutRejectReason);
	bool TryCommitReplicaSwapAndTakeOriginal(AHeistPlayerState* RequestingPlayerState, int32& OutAddedInstanceId, FName& OutRejectReason);

	void GetReplicaComponentDebugState(int32& OutReplicaRevision, int32& OutExpectedEntryCount, int32& OutBuiltPartCount, int32& OutUnresolvedSocketCount, bool& OutCoreReady,
									   bool& OutContractPassed) const;
	bool ForceReplicaRebuildForDebug();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Heist|Object Assembly")
	bool InitializeObjectIdentity(FName InObjectCaseId, FName InTargetArtifactId, FName InObjectFamilyId);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Heist|Object Assembly")
	bool TryTransitionToAssemblyState(EHeistObjectAssemblyState NewState);

	UFUNCTION(BlueprintPure, Category = "Heist|Object Assembly|Session")
	AHeistPlayerState* GetSessionOwner() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Object Assembly|Session")
	bool IsSessionLocked() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Object Assembly|Session")
	float GetMaximumSessionDistance() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Heist|Object Assembly|Session")
	bool TryBeginSession(AHeistPlayerState* RequestingPlayerState);

	bool CancelSessionForOwner(AHeistPlayerState* ExpectedOwner, FName Reason);

	UPROPERTY(BlueprintAssignable, Category = "Heist|Object Assembly|Session")
	FHeistObjectAssemblySessionChangedSignature OnObjectAssemblySessionChanged;

	UFUNCTION(BlueprintPure, Category = "Heist|Object Assembly|Original")
	AHeistPlayerState* GetOriginalCarrier() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Object Assembly|Original")
	int32 GetOriginalCarryRevision() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Object Assembly|Original")
	bool IsOriginalSecuredAtExit() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Heist|Object Assembly|Original")
	bool TryTakeOriginal(AHeistPlayerState* RequestingPlayerState);

	bool ReleaseOriginalForCarrier(AHeistPlayerState* ExpectedCarrier, FName Reason);
	bool DropOriginalForCarrier(AHeistPlayerState* ExpectedCarrier, FName Reason);
	bool CanStageOriginalForArrest(const AHeistPlayerState* ExpectedCarrier, const FHeistInventoryItem& OriginalItem) const;
	bool TryStageOriginalForArrest(AHeistPlayerState* ExpectedCarrier, const FHeistInventoryItem& OriginalItem, const FTransform& EvidenceTransform,
		AHeistDroppedOriginalActor*& OutDroppedOriginal);
	void CommitStagedOriginalForArrest(AHeistPlayerState* ExpectedCarrier, const FHeistInventoryItem& OriginalItem);
	bool TryClaimDroppedOriginal(AHeistPlayerState* RequestingPlayerState, AHeistDroppedOriginalActor* DroppedOriginal);
	bool CanCommitOriginalDepositForCarrier(const AHeistPlayerState* ExpectedCarrier, const FHeistInventoryItem& OriginalItem) const;
	bool CommitOriginalDepositForCarrier(AHeistPlayerState* ExpectedCarrier, const FHeistInventoryItem& OriginalItem);

	UPROPERTY(BlueprintAssignable, Category = "Heist|Object Assembly|Original")
	FHeistObjectOriginalCarryChangedSignature OnObjectOriginalCarryChanged;

	UFUNCTION(BlueprintPure, Category = "Heist|Object Assembly|Inspection")
	bool IsRegisteredForInspection() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Object Assembly|Inspection")
	bool IsValidInspectionCandidate() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Object Assembly|Inspection")
	float GetInspectionDelayRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Object Assembly|Inspection")
	float GetResolvedInspectionDelay() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Object Assembly|Inspection")
	int32 GetInspectionScheduleRevision() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Object Assembly|Inspection")
	int32 GetInspectionRegistrationRevision() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Object Assembly|Inspection")
	int32 GetInspectionResultApplicationCount() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Object Assembly|Inspection")
	int32 GetInspectionDuplicateBlockCount() const;

	bool TryBeginInspection(AActor* InspectingGuard);
	bool InterruptInspection(AActor* InspectingGuard, FName Reason);
	bool ApplyInspectionResult(AActor* InspectingGuard);
	bool IsInspectionOwnedBy(const AActor* InspectingGuard) const;
	bool IsInspectionClaimActive() const;
	bool IsInspectionDelayTimerActive() const;
	AActor* GetInspectingGuard() const;
	bool ForceInspectionReadyForDebug();

	static bool CalculateInspectionSchedule(float BaseInspectionDelay, float& OutDelay);

  protected:
	virtual bool CanInteract(const AActor* Interactor) const override;

	UFUNCTION()
	void OnRep_AssemblyState();

	UFUNCTION()
	void OnRep_ObjectIdentity();

	UFUNCTION()
	void OnRep_AssemblyRevision();

	UFUNCTION()
	void OnRep_AssemblyReplicaData();

	UFUNCTION()
	void OnRep_SessionSnapshot();

	UFUNCTION()
	void OnRep_OriginalCarryRevision();

	UFUNCTION()
	void OnRep_InspectionScheduleRevision();

	UFUNCTION()
	void OnRep_ContractExhibitActive();

	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|Object Assembly", meta = (DisplayName = "Object Assembly Snapshot Changed"))
	void BP_ObjectAssemblySnapshotChanged();

	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|Object Assembly|Replica|Feedback", meta = (DisplayName = "Replica Swap Committed"))
	void BP_OnReplicaSwapCommitted();

  private:
	void ApplyContractExhibitActiveState();
	bool ValidateSessionRequest(AHeistPlayerState* RequestingPlayerState, FName& OutRejectReason) const;
	bool ValidateReplicaCommit(AHeistPlayerState* RequestingPlayerState, const FHeistObjectAssemblyResult& AssemblyResult, const TArray<FHeistObjectAssemblyEntry>& Entries,
							   FName& OutRejectReason) const;
	bool ValidateReplicaReviewOwner(AHeistPlayerState* RequestingPlayerState, FName& OutRejectReason) const;
	void ResetAssemblyPreviewData();
	void EmitReplicaSwapFeedback();
	void RefreshObjectVisualState();
	void RebuildOriginalComponents();
	void DestroyOriginalComponents();
	void RebuildReplicaComponents();
	void DestroyReplicaComponents();
	void ApplyPartMaterial(UStaticMeshComponent* PartComponent, const FHeistObjectAssemblyPartRow& PartDefinition, FName MaterialId) const;
	FTransform ResolveFallbackPartTransform(FName SocketId, int32 PlacementIndex, uint8 QuantizedOrientation) const;
	bool ShouldDisplayOriginalVisual() const;
	bool ShouldDisplayReplicaVisual() const;
	bool IsAssemblyStateTransitionAllowed(EHeistObjectAssemblyState NewState) const;
	void ClearSession(FName Reason);
	void UnbindSessionOwnerDelegate();
	void BroadcastAssemblySnapshot(FName EventName, FName Reason, bool bResult);
	bool ValidateOriginalTakeRequest(AHeistPlayerState* RequestingPlayerState, int32& OutArtifactValue, float& OutArtifactWeight, bool& bOutRequiredTarget, FName& OutRejectReason,
								 bool bAllowLockedReplicaReady = false) const;
	void SyncObjectiveCarrierCandidate(AHeistPlayerState* Carrier);
	void UnbindOriginalCarrierDelegate();
	void BroadcastOriginalCarrySnapshot(FName EventName, FName Reason, bool bResult);
	bool ResolveInspectionSchedule(FName& OutRejectReason);
	void StartInspectionDelayTimer();
	void ClearInspectionDelayTimer();
	void HandleInspectionDelayExpired(int32 ExpectedScheduleRevision, int32 ExpectedTimerRevision);
	bool HasInspectionDelayElapsed() const;
	void RefreshInspectionRegistration();
	void ClearInspectionStateForMatchEnd();
	void HandleSessionOwnerArrestStateChanged(bool bArrested);
	void HandleOriginalCarrierArrestStateChanged(bool bArrested);
	void HandleMatchPhaseChanged(EHeistMatchPhase PreviousMatchPhase, EHeistMatchPhase NewMatchPhase);

	UPROPERTY(ReplicatedUsing = OnRep_ContractExhibitActive, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Contract",
		meta = (AllowPrivateAccess = "true"))
	bool bContractExhibitActive = true;

	UPROPERTY(ReplicatedUsing = OnRep_ObjectIdentity, EditInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true"))
	FName ObjectCaseId = TEXT("ObjectCase_Unassigned");

	UPROPERTY(ReplicatedUsing = OnRep_ObjectIdentity, EditInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly",
			  meta = (AllowPrivateAccess = "true", DisplayName = "Target Artifact Id"))
	FName TargetObjectArtifactId = NAME_None;

	UPROPERTY(ReplicatedUsing = OnRep_ObjectIdentity, EditInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true"))
	FName ObjectFamilyId = NAME_None;

	UPROPERTY(ReplicatedUsing = OnRep_AssemblyState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true"))
	EHeistObjectAssemblyState AssemblyState = EHeistObjectAssemblyState::Secured;

	UPROPERTY(ReplicatedUsing = OnRep_AssemblyRevision, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true"))
	int32 AssemblyRevision = 0;

	UPROPERTY(ReplicatedUsing = OnRep_AssemblyReplicaData, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true"))
	FHeistObjectAssemblyReplicaData AssemblyReplicaData;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Replica", meta = (AllowPrivateAccess = "true"))
	bool bHasCommittedAssemblyResult = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Replica", meta = (AllowPrivateAccess = "true"))
	FHeistObjectAssemblyResult CommittedAssemblyResult;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Object Assembly|Replica", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> ReplicaRootComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> OriginalPartComponents;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ReplicaCoreComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ReplicaPartComponents;

	int32 AppliedReplicaRevision = 0;
	int32 UnresolvedReplicaSocketCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_SessionSnapshot, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Session", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AHeistPlayerState> SessionOwner;

	UPROPERTY(ReplicatedUsing = OnRep_SessionSnapshot, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Session", meta = (AllowPrivateAccess = "true"))
	bool bSessionLocked = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Session", meta = (AllowPrivateAccess = "true", ClampMin = "100.0"))
	float MaximumSessionDistance = 300.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Original", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AHeistPlayerState> OriginalCarrier;

	UPROPERTY(ReplicatedUsing = OnRep_OriginalCarryRevision, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Original", meta = (AllowPrivateAccess = "true"))
	int32 OriginalCarryRevision = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Original", meta = (AllowPrivateAccess = "true"))
	bool bOriginalSecuredAtExit = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Original", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AHeistDroppedOriginalActor> DroppedOriginalActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Replica|Feedback", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> ReplicaSwapSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Replica|Feedback", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float ReplicaSwapNoiseRadius = 1000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Replica|Feedback", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float ReplicaSwapNoiseDuration = 1.5f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Inspection", meta = (AllowPrivateAccess = "true"))
	bool bRegisteredForInspection = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Inspection", meta = (AllowPrivateAccess = "true"))
	int32 InspectionRegistrationRevision = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Inspection", meta = (AllowPrivateAccess = "true"))
	float ResolvedInspectionDelay = 0.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Inspection", meta = (AllowPrivateAccess = "true"))
	float InspectionReadyServerTime = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_InspectionScheduleRevision, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Inspection", meta = (AllowPrivateAccess = "true"))
	int32 InspectionScheduleRevision = 0;

	TWeakObjectPtr<AHeistGameState> BoundGameState;
	TWeakObjectPtr<AActor> InspectingGuardActor;
	FDelegateHandle MatchPhaseChangedHandle;
	FDelegateHandle SessionOwnerArrestChangedHandle;
	FDelegateHandle OriginalCarrierArrestChangedHandle;
	FTimerHandle InspectionDelayTimerHandle;
	int32 InspectionDelayTimerRevision = 0;
	int32 ActiveInspectionScheduleRevision = INDEX_NONE;
	int32 LastAppliedInspectionScheduleRevision = INDEX_NONE;
	int32 InspectionResultApplicationCount = 0;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayReplicaSwapFeedback();
	int32 InspectionDuplicateBlockCount = 0;
	EHeistObjectAssemblyState PreInspectionState = EHeistObjectAssemblyState::OriginalAvailable;
};
