#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "World/Interaction/HeistInteractableActor.h"

#include "HeistObjectDisplayCaseActor.generated.h"

/**
 * Generic display-case contract for Sculpture and Ceramic Object Assembly.
 *
 * TASK-W5-017 intentionally keeps interaction disabled. Session ownership,
 * submission, scoring, and replica placement are implemented by later tasks.
 */
UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistObjectDisplayCaseActor : public AHeistInteractableActor
{
	GENERATED_BODY()

  public:
	AHeistObjectDisplayCaseActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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

	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|Object Assembly", meta = (DisplayName = "Object Assembly Snapshot Changed"))
	void BP_ObjectAssemblySnapshotChanged();

  private:
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
};
