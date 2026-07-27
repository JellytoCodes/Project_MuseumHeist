#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "World/Interaction/HeistInteractableActor.h"

#include "HeistObjectDisplayCaseActor.generated.h"

class AHeistGameState;
class AHeistPlayerState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FHeistObjectAssemblySessionChangedSignature, AHeistPlayerState*, SessionOwner, bool, bLocked, int32, Revision);

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

  protected:
	virtual bool CanInteract(const AActor* Interactor) const override;

	/** One-time identity bridge used only by deprecated display-case aliases. */
	void SetObjectIdentityForLegacyMigration(FName InObjectCaseId, FName InTargetArtifactId, FName InObjectFamilyId);

	UFUNCTION()
	void OnRep_AssemblyState();

	UFUNCTION()
	void OnRep_AssemblyRevision();

	UFUNCTION()
	void OnRep_AssemblyReplicaData();

	UFUNCTION()
	void OnRep_SessionSnapshot();

	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|Object Assembly", meta = (DisplayName = "Object Assembly Snapshot Changed"))
	void BP_ObjectAssemblySnapshotChanged();

  private:
	bool ValidateSessionRequest(AHeistPlayerState* RequestingPlayerState, FName& OutRejectReason) const;
	bool IsAssemblyStateTransitionAllowed(EHeistObjectAssemblyState NewState) const;
	void ClearSession(FName Reason);
	void UnbindSessionOwnerDelegate();
	void BroadcastAssemblySnapshot(FName EventName, FName Reason, bool bResult);
	void HandleSessionOwnerArrestStateChanged(bool bArrested);
	void HandleMatchPhaseChanged(EHeistMatchPhase PreviousMatchPhase, EHeistMatchPhase NewMatchPhase);

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true"))
	FName ObjectCaseId = TEXT("ObjectCase_Unassigned");

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true", DisplayName = "Target Artifact Id"))
	FName TargetObjectArtifactId = NAME_None;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true"))
	FName ObjectFamilyId = NAME_None;

	UPROPERTY(ReplicatedUsing = OnRep_AssemblyState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true"))
	EHeistObjectAssemblyState AssemblyState = EHeistObjectAssemblyState::Secured;

	UPROPERTY(ReplicatedUsing = OnRep_AssemblyRevision, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true"))
	int32 AssemblyRevision = 0;

	UPROPERTY(ReplicatedUsing = OnRep_AssemblyReplicaData, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true"))
	FHeistObjectAssemblyReplicaData AssemblyReplicaData;

	UPROPERTY(ReplicatedUsing = OnRep_SessionSnapshot, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Session", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AHeistPlayerState> SessionOwner;

	UPROPERTY(ReplicatedUsing = OnRep_SessionSnapshot, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Session", meta = (AllowPrivateAccess = "true"))
	bool bSessionLocked = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Object Assembly|Session", meta = (AllowPrivateAccess = "true", ClampMin = "100.0"))
	float MaximumSessionDistance = 300.0f;

	TWeakObjectPtr<AHeistGameState> BoundGameState;
	FDelegateHandle MatchPhaseChangedHandle;
	FDelegateHandle SessionOwnerArrestChangedHandle;
};
