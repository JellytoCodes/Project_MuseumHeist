#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "World/Interaction/HeistInteractableActor.h"

#include "HeistDisplayCaseActor.generated.h"

class AHeistGameState;
class AHeistPlayerState;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FHeistDisplayCaseStateChangedSignature,
	EHeistDisplayCaseState,
	PreviousState,
	EHeistDisplayCaseState,
	NewState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FHeistDisplayCaseSessionChangedSignature,
	AHeistPlayerState*,
	SessionOwner,
	bool,
	bLocked,
	int32,
	Revision);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FHeistOriginalCarryChangedSignature,
	AHeistPlayerState*,
	Carrier,
	FName,
	ArtifactId,
	int32,
	Revision);

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistDisplayCaseActor : public AHeistInteractableActor
{
	GENERATED_BODY()

public:
	AHeistDisplayCaseActor();

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

	void GetPlaceholderVisualDebugState(
		bool& OutExpectedOriginalVisible,
		bool& OutExpectedReplicaVisible,
		int32& OutOriginalComponentCount,
		int32& OutReplicaComponentCount,
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
	void BP_ApplyPlaceholderVisualState(
		EHeistDisplayCaseState NewState,
		bool bOriginalVisible,
		bool bReplicaVisible);

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

	bool TryCommitReplicaPlacement(
		AHeistPlayerState* RequestingPlayerState,
		const FHeistForgeryResult& ForgeryResult);

private:
	bool ValidateReplicaPlacementRequest(
		AHeistPlayerState* RequestingPlayerState,
		const FHeistForgeryResult& ForgeryResult,
		FName& OutRejectReason) const;

	UPROPERTY(
		Replicated,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "Heist|DisplayCase|Replica",
		meta = (AllowPrivateAccess = "true"))
	bool bHasCommittedForgeryResult = false;

	UPROPERTY(
		Replicated,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "Heist|DisplayCase|Replica",
		meta = (AllowPrivateAccess = "true"))
	FHeistForgeryResult CommittedForgeryResult;

	UPROPERTY(
		ReplicatedUsing = OnRep_CommittedForgeryRevision,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "Heist|DisplayCase|Replica",
		meta = (AllowPrivateAccess = "true"))
	int32 CommittedForgeryRevision = 0;

	UFUNCTION()
	void OnRep_CommittedForgeryRevision();

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
	bool ValidateOriginalTakeRequest(
		AHeistPlayerState* RequestingPlayerState,
		float& OutArtifactWeight,
		FName& OutRejectReason) const;
	void SyncObjectiveCarrierCandidate(AHeistPlayerState* Carrier);
	void UnbindOriginalCarrierDelegate();
	void BroadcastOriginalCarrySnapshot(const TCHAR* ChangeSource, FName Reason);
	void HandleOriginalCarrierArrestStateChanged(bool bArrested);

	UPROPERTY(
		EditInstanceOnly,
		BlueprintReadOnly,
		Category = "Heist|DisplayCase|Original",
		meta = (AllowPrivateAccess = "true"))
	FName TargetArtifactId = TEXT("Artifact_Painting_M01");

	UPROPERTY(
		EditInstanceOnly,
		BlueprintReadOnly,
		Category = "Heist|DisplayCase|Original",
		meta = (AllowPrivateAccess = "true"))
	FName DisplayCaseId = TEXT("Case_M01_Target");

	UPROPERTY(
		Replicated,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "Heist|DisplayCase|Original",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AHeistPlayerState> OriginalCarrier;

	UPROPERTY(
		ReplicatedUsing = OnRep_OriginalCarryRevision,
		VisibleInstanceOnly,
		BlueprintReadOnly,
		Category = "Heist|DisplayCase|Original",
		meta = (AllowPrivateAccess = "true"))
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
