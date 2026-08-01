#pragma once

#include "CoreMinimal.h"
#include "World/Interaction/HeistInteractableActor.h"

#include "HeistDroppedOriginalActor.generated.h"

/**
 * Neutral, server-authoritative world representation of an Original that is
 * no longer inside its source display case and is not currently carried.
 */
UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API AHeistDroppedOriginalActor : public AHeistInteractableActor
{
	GENERATED_BODY()

  public:
	AHeistDroppedOriginalActor();

	void InitializeDroppedOriginal(FName InArtifactId, int32 InArtifactValue, float InWeight, bool bInRequiredTarget, AActor* InSourceDisplayCase);

	UFUNCTION(BlueprintPure, Category = "Heist|Original Drop")
	FName GetArtifactId() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Original Drop")
	int32 GetArtifactValue() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Original Drop")
	float GetWeight() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Original Drop")
	bool IsRequiredTarget() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Original Drop")
	AActor* GetSourceDisplayCase() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Original Drop")
	bool IsDropAvailable() const;

	bool TryReserveForPickup(AActor* Requester);
	bool IsReservedBy(const AActor* Requester) const;
	bool CommitPickupReservation(AActor* Requester);
	void ReleasePickupReservation(AActor* Requester);

	virtual bool CanInteract(const AActor* Interactor) const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

  protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|Original Drop", meta = (DisplayName = "Dropped Original Snapshot Changed"))
	void BP_DroppedOriginalSnapshotChanged();

  private:
	bool HasValidDropData() const;

	UFUNCTION()
	void OnRep_DropRevision();

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Original Drop", meta = (AllowPrivateAccess = "true"))
	FName ArtifactId = NAME_None;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Original Drop", meta = (AllowPrivateAccess = "true"))
	int32 ArtifactValue = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Original Drop", meta = (AllowPrivateAccess = "true"))
	float Weight = 0.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Original Drop", meta = (AllowPrivateAccess = "true"))
	bool bRequiredTarget = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Original Drop", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> SourceDisplayCase;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Original Drop", meta = (AllowPrivateAccess = "true"))
	bool bAvailable = false;

	UPROPERTY(ReplicatedUsing = OnRep_DropRevision, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Original Drop", meta = (AllowPrivateAccess = "true"))
	int32 DropRevision = 0;

	TWeakObjectPtr<AActor> PickupReservationOwner;
};
