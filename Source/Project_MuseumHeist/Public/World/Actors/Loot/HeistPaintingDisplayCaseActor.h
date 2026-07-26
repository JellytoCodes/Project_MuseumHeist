#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "World/Interaction/HeistInteractableActor.h"

#include "HeistPaintingDisplayCaseActor.generated.h"

class AHeistGameState;
class AHeistPlayerState;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;
class UTexture2D;

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistReplicaPaintingData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Heist|DisplayCase|Replica|Painting")
	int32 Resolution = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|DisplayCase|Replica|Painting")
	TArray<FColor> Palette;

	/**
	 * Two 4-bit indices per byte. Index 0 is the canvas background and
	 * indices 1~8 address Palette[0~7].
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Heist|DisplayCase|Replica|Painting")
	TArray<uint8> PackedPaletteIndices;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|DisplayCase|Replica|Painting")
	int32 Revision = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHeistDisplayCaseStateChangedSignature, EHeistDisplayCaseState, PreviousState, EHeistDisplayCaseState, NewState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FHeistDisplayCaseSessionChangedSignature, AHeistPlayerState*, SessionOwner, bool, bLocked, int32, Revision);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FHeistOriginalCarryChangedSignature, AHeistPlayerState*, Carrier, FName, ArtifactId, int32, Revision);

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistPaintingDisplayCaseActor : public AHeistInteractableActor
{
	GENERATED_BODY()

  public:
	AHeistPaintingDisplayCaseActor();

  protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool CanInteract(const AActor* Interactor) const override;

#pragma region StateMachine

  public:
	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase")
	EHeistDisplayCaseState GetDisplayCaseState() const;

	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Visual")
	bool ShouldDisplayOriginalPlaceholder() const;

	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Visual")
	bool ShouldDisplayReplicaPlaceholder() const;

	void GetPlaceholderVisualDebugState(bool& OutExpectedOriginalVisible, bool& OutExpectedReplicaVisible, int32& OutOriginalComponentCount, int32& OutReplicaComponentCount,
										bool& OutComponentsMatchExpectedState) const;

	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase")
	bool CanTransitionToDisplayCaseState(EHeistDisplayCaseState NewState) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Heist|DisplayCase")
	bool TryTransitionToDisplayCaseState(EHeistDisplayCaseState NewState);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Heist|DisplayCase")
	bool TryAdvanceDisplayCaseState();

	bool ResetForgerySessionState(FName Reason);

	UPROPERTY(BlueprintAssignable, Category = "Heist|DisplayCase")
	FHeistDisplayCaseStateChangedSignature OnDisplayCaseStateChanged;

  protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|DisplayCase|Visual", meta = (DisplayName = "Apply Placeholder Visual State"))
	void BP_ApplyPlaceholderVisualState(EHeistDisplayCaseState NewState, bool bOriginalVisible, bool bReplicaVisible);

  private:
	UPROPERTY(ReplicatedUsing = OnRep_DisplayCaseState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|DisplayCase", meta = (AllowPrivateAccess = "true"))
	EHeistDisplayCaseState DisplayCaseState = EHeistDisplayCaseState::Secured;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|DisplayCase|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> OriginalVisualComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|DisplayCase|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ReplicaVisualComponent;

	static const FName OriginalVisualComponentTag;
	static const FName ReplicaVisualComponentTag;

	UFUNCTION()
	void OnRep_DisplayCaseState(EHeistDisplayCaseState PreviousState);

	static bool TryGetNextDisplayCaseState(EHeistDisplayCaseState CurrentState, EHeistDisplayCaseState& OutNextState);
	static bool ShouldDisplayOriginalPlaceholderForState(EHeistDisplayCaseState State);
	static bool ShouldDisplayReplicaPlaceholderForState(EHeistDisplayCaseState State);

	void HandleDisplayCaseStateChanged(EHeistDisplayCaseState PreviousState);
	void RefreshPlaceholderVisualState();

#pragma endregion

#pragma region ReplicaPlacement

  public:
	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Replica")
	bool HasCommittedForgeryResult() const;

	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Replica")
	FHeistForgeryResult GetCommittedForgeryResult() const;

	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Replica")
	int32 GetCommittedForgeryRevision() const;

	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Replica|Visual")
	int32 GetReplicaVisualTier() const;

	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Replica|Visual")
	FName GetReplicaVisualTierName() const;

	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Replica|Visual")
	bool IsReplicaWorldVisualReady() const;

	void GetReplicaWorldVisualDebugState(bool& OutReplicaExpectedVisible, bool& OutHasReplicaMesh, int32& OutExpectedTier, int32& OutAppliedTier, FName& OutTierName, bool& OutUsingTierMaterial,
										 bool& OutUsingTransformFallback, bool& OutCustomPrimitiveDataApplied, bool& OutContractPassed) const;

	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Replica|Painting")
	bool HasReplicaPaintingData() const;

	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Replica|Painting")
	int32 GetReplicaPaintingRevision() const;

	void GetReplicaPaintingDebugState(int32& OutResolution, int32& OutPaletteColorCount, int32& OutPackedByteCount, int32& OutPaintingRevision, bool& OutTextureBuilt, bool& OutDynamicMaterialBuilt,
									  bool& OutTextureParameterApplied, bool& OutContractPassed) const;

	bool TryCommitReplicaPlacement(AHeistPlayerState* RequestingPlayerState, const FHeistForgeryResult& ForgeryResult, const FHeistReplicaPaintingData& PaintingData);

  protected:
	/**
	 * Optional Blueprint presentation hook. Gameplay state and tier selection
	 * remain C++; Blueprint may only add visual polish.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|DisplayCase|Replica|Visual", meta = (DisplayName = "Apply Replica World Visual"))
	void BP_ApplyReplicaWorldVisual(int32 VisualTier, FName VisualTierName, float SimilarityScore, float CoverageScore, float ColorAccuracyScore, FName TemplateId, bool bUsingAssignedTierMaterial);

  private:
	bool ValidateReplicaPlacementRequest(AHeistPlayerState* RequestingPlayerState, const FHeistForgeryResult& ForgeryResult, FName& OutRejectReason) const;
	bool ValidateReplicaPaintingData(const FHeistReplicaPaintingData& PaintingData, FName& OutRejectReason) const;
	void CaptureReplicaVisualBaseline();
	void RefreshReplicaWorldVisual();
	void RefreshReplicaPaintingTexture();
	UTexture2D* BuildReplicaPaintingTexture() const;
	void ResetReplicaPaintingResources();
	UMaterialInterface* ResolveReplicaTierMaterial(int32 VisualTier) const;
	static int32 ResolveReplicaVisualTier(float SimilarityScore);
	static FName ResolveReplicaVisualTierName(int32 VisualTier);

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Replica", meta = (AllowPrivateAccess = "true"))
	bool bHasCommittedForgeryResult = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Replica", meta = (AllowPrivateAccess = "true"))
	FHeistForgeryResult CommittedForgeryResult;

	UPROPERTY(ReplicatedUsing = OnRep_CommittedForgeryRevision, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Replica", meta = (AllowPrivateAccess = "true"))
	int32 CommittedForgeryRevision = 0;

	UPROPERTY(ReplicatedUsing = OnRep_ReplicaPaintingData, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Replica|Painting", meta = (AllowPrivateAccess = "true"))
	FHeistReplicaPaintingData ReplicaPaintingData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Replica|Painting", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> ReplicaPaintingMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Replica|Painting", meta = (AllowPrivateAccess = "true"))
	FName ReplicaPaintingTextureParameter = TEXT("PaintingTexture");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Replica|Painting", meta = (AllowPrivateAccess = "true"))
	FLinearColor ReplicaPaintingBackgroundColor = FLinearColor(0.94f, 0.92f, 0.84f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Replica|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> ReplicaPoorMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Replica|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> ReplicaFairMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Replica|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> ReplicaGoodMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Replica|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> ReplicaExcellentMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ReplicaBaselineMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ReplicaPaintingTexture;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ReplicaPaintingDynamicMaterial;

	FTransform ReplicaBaselineRelativeTransform = FTransform::Identity;
	int32 AppliedReplicaVisualTier = INDEX_NONE;
	int32 AppliedReplicaPaintingRevision = 0;
	bool bReplicaVisualBaselineCaptured = false;
	bool bUsingReplicaTierMaterial = false;
	bool bUsingReplicaTransformFallback = false;
	bool bReplicaVisualCustomPrimitiveDataApplied = false;
	bool bReplicaPaintingTextureParameterApplied = false;

	UFUNCTION()
	void OnRep_CommittedForgeryRevision();

	UFUNCTION()
	void OnRep_ReplicaPaintingData();

#pragma endregion

#pragma region InspectionTarget

  public:
	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Inspection")
	bool IsRegisteredForInspection() const;

	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Inspection")
	bool IsValidInspectionCandidate() const;

	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Inspection")
	int32 GetInspectionRegistrationRevision() const;

	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Inspection")
	float GetResolvedInspectionDelay() const;

	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Inspection")
	float GetInspectionReadyServerTime() const;

	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Inspection")
	float GetInspectionDelayRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Inspection")
	FName GetInspectionScoreBand() const;

	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Inspection")
	EHeistAlertLevel GetResolvedInspectionAlertOutcome() const;

	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Inspection")
	EHeistDisplayCaseState GetResolvedInspectionCaseOutcome() const;

	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Inspection")
	int32 GetInspectionScheduleRevision() const;

	static bool CalculateInspectionSchedule(float SimilarityScore, float BaseInspectionDelay, float& OutDelay, FName& OutScoreBand, EHeistAlertLevel& OutAlertOutcome,
											EHeistDisplayCaseState& OutCaseOutcome);

	bool TryBeginInspection(AActor* InspectingGuard);
	bool InterruptInspection(AActor* InspectingGuard, FName Reason);
	bool ApplyInspectionResult(AActor* InspectingGuard);
	bool IsInspectionOwnedBy(const AActor* InspectingGuard) const;
	bool IsInspectionClaimActive() const;
	AActor* GetInspectingGuard() const;
	bool IsInspectionDelayTimerActive() const;
	int32 GetInspectionResultApplicationCount() const;
	int32 GetInspectionDuplicateBlockCount() const;

  private:
	bool ResolveInspectionSchedule(const FHeistForgeryResult& ForgeryResult, FName& OutRejectReason);
	void StartInspectionDelayTimer();
	void ClearInspectionDelayTimer();
	void HandleInspectionDelayExpired(int32 ExpectedScheduleRevision, int32 ExpectedTimerRevision);
	bool HasInspectionDelayElapsed() const;
	void RefreshInspectionRegistration();

	UFUNCTION()
	void OnRep_InspectionScheduleRevision();

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Inspection", meta = (AllowPrivateAccess = "true"))
	bool bRegisteredForInspection = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Inspection", meta = (AllowPrivateAccess = "true"))
	int32 InspectionRegistrationRevision = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Inspection", meta = (AllowPrivateAccess = "true"))
	float ResolvedInspectionDelay = 0.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Inspection", meta = (AllowPrivateAccess = "true"))
	float InspectionReadyServerTime = 0.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Inspection", meta = (AllowPrivateAccess = "true"))
	FName InspectionScoreBand = NAME_None;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Inspection", meta = (AllowPrivateAccess = "true"))
	EHeistAlertLevel ResolvedInspectionAlertOutcome = EHeistAlertLevel::Quiet;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Inspection", meta = (AllowPrivateAccess = "true"))
	EHeistDisplayCaseState ResolvedInspectionCaseOutcome = EHeistDisplayCaseState::Suspected;

	UPROPERTY(ReplicatedUsing = OnRep_InspectionScheduleRevision, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Inspection", meta = (AllowPrivateAccess = "true"))
	int32 InspectionScheduleRevision = 0;

	TWeakObjectPtr<AActor> InspectingGuardActor;
	EHeistDisplayCaseState PreInspectionState = EHeistDisplayCaseState::OriginalAvailable;
	FTimerHandle InspectionDelayTimerHandle;
	int32 InspectionDelayTimerRevision = 0;
	int32 ActiveInspectionScheduleRevision = INDEX_NONE;
	int32 LastAppliedInspectionScheduleRevision = INDEX_NONE;
	int32 InspectionResultApplicationCount = 0;
	int32 InspectionDuplicateBlockCount = 0;

#pragma endregion

#pragma region OriginalCarry

  public:
	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Original")
	FName GetTargetArtifactId() const;

	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Original")
	FName GetDisplayCaseId() const;

	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Original")
	AHeistPlayerState* GetOriginalCarrier() const;

	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Original")
	int32 GetOriginalCarryRevision() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Heist|DisplayCase|Original")
	bool TryTakeOriginal(AHeistPlayerState* RequestingPlayerState);

	bool ReleaseOriginalForCarrier(AHeistPlayerState* ExpectedCarrier, FName Reason);

	UPROPERTY(BlueprintAssignable, Category = "Heist|DisplayCase|Original")
	FHeistOriginalCarryChangedSignature OnOriginalCarryChanged;

  private:
	bool ValidateOriginalTakeRequest(AHeistPlayerState* RequestingPlayerState, float& OutArtifactWeight, FName& OutRejectReason) const;
	void SyncObjectiveCarrierCandidate(AHeistPlayerState* Carrier);
	void UnbindOriginalCarrierDelegate();
	void BroadcastOriginalCarrySnapshot(const TCHAR* ChangeSource, FName Reason);
	void HandleOriginalCarrierArrestStateChanged(bool bArrested);

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Original", meta = (AllowPrivateAccess = "true"))
	FName TargetArtifactId = TEXT("Artifact_Painting_M01");

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Original", meta = (AllowPrivateAccess = "true"))
	FName DisplayCaseId = TEXT("Case_M01_Target");

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Original", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AHeistPlayerState> OriginalCarrier;

	UPROPERTY(ReplicatedUsing = OnRep_OriginalCarryRevision, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Original", meta = (AllowPrivateAccess = "true"))
	int32 OriginalCarryRevision = 0;

	UFUNCTION()
	void OnRep_OriginalCarryRevision();

	FDelegateHandle OriginalCarrierArrestChangedHandle;

#pragma endregion

#pragma region Session

  public:
	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Session")
	AHeistPlayerState* GetSessionOwner() const;

	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Session")
	bool IsSessionLocked() const;

	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Session")
	int32 GetSessionRevision() const;

	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase|Session")
	float GetMaximumSessionDistance() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Heist|DisplayCase|Session")
	bool TryBeginSession(AHeistPlayerState* RequestingPlayerState);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Heist|DisplayCase|Session")
	bool TryCancelSession(AHeistPlayerState* RequestingPlayerState);

	bool CancelSessionForOwner(AHeistPlayerState* ExpectedOwner, FName Reason);

	UPROPERTY(BlueprintAssignable, Category = "Heist|DisplayCase|Session")
	FHeistDisplayCaseSessionChangedSignature OnDisplayCaseSessionChanged;

  private:
	bool ValidateSessionRequest(AHeistPlayerState* RequestingPlayerState, FName& OutRejectReason) const;
	void ClearSession(FName Reason);
	void UnbindSessionOwnerDelegate();
	void BroadcastSessionSnapshot(const TCHAR* ChangeSource, FName Reason);

	void HandleSessionOwnerArrestStateChanged(bool bArrested);
	void HandleMatchPhaseChanged(EHeistMatchPhase PreviousMatchPhase, EHeistMatchPhase NewMatchPhase);

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Session", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AHeistPlayerState> SessionOwner;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Session", meta = (AllowPrivateAccess = "true"))
	bool bSessionLocked = false;

	UPROPERTY(ReplicatedUsing = OnRep_SessionRevision, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Session", meta = (AllowPrivateAccess = "true"))
	int32 SessionRevision = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|DisplayCase|Session", meta = (AllowPrivateAccess = "true", ClampMin = "100.0"))
	float MaximumSessionDistance = 300.0f;

	UFUNCTION()
	void OnRep_SessionRevision();

	TWeakObjectPtr<AHeistGameState> BoundGameState;
	FDelegateHandle MatchPhaseChangedHandle;
	FDelegateHandle SessionOwnerArrestChangedHandle;

#pragma endregion

#pragma region Replication

  public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion
};
