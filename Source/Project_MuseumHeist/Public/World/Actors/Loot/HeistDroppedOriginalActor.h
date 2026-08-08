#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "World/Interaction/HeistInteractableActor.h"

#include "HeistDroppedOriginalActor.generated.h"

class UMaterialInterface;
class UStaticMesh;

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
	FText GetArtifactDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Original Drop")
	EHeistLootGrade GetItemGrade() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Original Drop")
	EHeistForgeryType GetForgeryType() const;

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
	bool ResolveArtifactPresentationData();
	void ResolveDroppedOriginalVisual();
	UMaterialInterface* ResolveGradeMaterial() const;

	UFUNCTION()
	void OnRep_DropRevision();

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Original Drop", meta = (AllowPrivateAccess = "true"))
	FName ArtifactId = NAME_None;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Original Drop", meta = (AllowPrivateAccess = "true"))
	FText ArtifactDisplayName;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Original Drop", meta = (AllowPrivateAccess = "true"))
	EHeistLootGrade ItemGrade = EHeistLootGrade::OneStar;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Original Drop", meta = (AllowPrivateAccess = "true"))
	EHeistForgeryType ForgeryType = EHeistForgeryType::None;

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

	/** Category-only visual assigned once on BP_DroppedOriginal. Artifact rows never select actor classes or individual dropped meshes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Original Drop|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMesh> PaintingDropMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Original Drop|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMesh> ObjectDropMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Original Drop|Visual", meta = (AllowPrivateAccess = "true"))
	FTransform PaintingDropVisualRelativeTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Original Drop|Visual", meta = (AllowPrivateAccess = "true"))
	FTransform ObjectDropVisualRelativeTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Original Drop|Grade", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> OneStarGradeMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Original Drop|Grade", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> TwoStarGradeMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Original Drop|Grade", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> ThreeStarGradeMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Original Drop|Grade", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> FourStarGradeMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Original Drop|Grade", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 GradeMaterialSlot = 0;

	TWeakObjectPtr<AActor> PickupReservationOwner;
};
