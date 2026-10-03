#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "Engine/DataTable.h"
#include "World/Interaction/HeistInteractableActor.h"

#include "HeistLootActor.generated.h"

class AHeistLootActor;
class AHeistPlayerCharacter;
class AHeistPlayerState;
class AHeistGameState;
class USoundBase;
class USoundAttenuation;

DECLARE_MULTICAST_DELEGATE_TwoParams(FHeistLootPickupCommitted, AHeistLootActor*, AActor*);

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistLootActor : public AHeistInteractableActor
{
	GENERATED_BODY()

#pragma region Construction

  public:
	AHeistLootActor();

#pragma endregion

#pragma region Lifecycle

  protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#pragma endregion

#pragma region LootData

  public:
	FName GetLootRowId() const;
	void InitializeLootData(UDataTable* InLootDataTable, FName InLootRowId);
	int32 GetScoreValue() const;
	float GetWeightValue() const;
	EHeistLootGrade GetLootGrade() const;
	bool IsLootAvailable() const;
	bool IsPickupReady() const;

  private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Loot", meta = (AllowPrivateAccess = "true"))
	FDataTableRowHandle LootDataRow;

	UPROPERTY(ReplicatedUsing = OnRep_LootRowId, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Loot", meta = (AllowPrivateAccess = "true"))
	FName LootRowId = NAME_None;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Loot", meta = (AllowPrivateAccess = "true"))
	EHeistLootGrade LootGrade = EHeistLootGrade::OneStar;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Loot", meta = (AllowPrivateAccess = "true"))
	int32 ScoreValue = 0;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Loot", meta = (AllowPrivateAccess = "true"))
	float WeightValue = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_IsAvailable, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Loot", meta = (AllowPrivateAccess = "true"))
	bool bIsAvailable = true;

#pragma endregion

#pragma region ExhibitionCase

  public:
	void InitializeExhibitionLoot(bool bInActive, bool bInLocked = true);
	bool IsExhibitionPresentation() const { return bExhibitionPresentation; }
	bool IsExhibitionLootActive() const { return bExhibitionLootActive; }
	bool IsExhibitionCaseOpen() const { return bCaseOpen; }
	int32 GetCaseRevision() const { return CaseRevision; }
	int32 GetCompletedCaseLatches() const { return CompletedCaseLatches; }
	AHeistPlayerState* GetCaseOperator() const { return CaseOperator; }
	float GetCaseTimingProgress() const;
	float GetCaseSuccessWindowWidth() const;
	float GetCaseSuccessWindowCenter() const { return CaseSuccessWindowCenter; }
	FText GetCasePrompt(const AHeistPlayerCharacter* Character) const;
	bool TryUseExhibitionCase(AHeistPlayerCharacter* Character, int32 ExpectedRevision);
	void CancelExhibitionCaseForPlayer(AHeistPlayerCharacter* Character);

  protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Loot|Case")
	TObjectPtr<UStaticMeshComponent> CaseShell;

	UPROPERTY(EditDefaultsOnly, Category = "Heist|Loot|Case")
	TObjectPtr<USoundBase> CaseLatchSound;

	UPROPERTY(EditDefaultsOnly, Category = "Heist|Loot|Case")
	TObjectPtr<USoundBase> CaseFailureSound;

	UPROPERTY(EditDefaultsOnly, Category = "Heist|Loot|Case")
	TObjectPtr<USoundBase> CaseOpenSound;

	UPROPERTY(EditDefaultsOnly, Category = "Heist|Loot|Case")
	TObjectPtr<USoundAttenuation> CaseSoundAttenuation;

  private:
	UPROPERTY(ReplicatedUsing = OnRep_CaseState)
	bool bExhibitionPresentation = false;

	UPROPERTY(ReplicatedUsing = OnRep_CaseState)
	bool bExhibitionLootActive = true;

	UPROPERTY(ReplicatedUsing = OnRep_CaseState)
	bool bCaseOpen = true;

	UPROPERTY(Replicated)
	int32 CompletedCaseLatches = 0;

	UPROPERTY(Replicated)
	int32 CaseRevision = 0;

	UPROPERTY(Replicated)
	TObjectPtr<AHeistPlayerState> CaseOperator;

	UPROPERTY(Replicated)
	float CaseRoundStartServerTime = 0.0f;

	UPROPERTY(Replicated)
	float CaseSuccessWindowCenter = 0.70f;

	FVector CaseOperationOrigin;
	float LastCaseAttemptServerTime = -1.0f;
	TWeakObjectPtr<AHeistPlayerState> LastCaseAttemptPlayer;
	TWeakObjectPtr<AHeistGameState> CaseGameState;
	FTimerHandle CaseValidationTimer;
	float GetCaseServerTime() const;
	bool IsCaseOperatorValid() const;
	void ValidateCaseOperation();
	void CancelCaseOperation();
	void StartCaseRound(float Now);
	void HandleCaseMatchPhaseChanged(EHeistMatchPhase Previous, EHeistMatchPhase Current);

	UFUNCTION()
	void OnRep_CaseState();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastCaseSound(uint8 Event);

#pragma endregion

#pragma region Interaction

  public:
	virtual bool CanInteract(const AActor* Interactor) const override;

#pragma endregion

#pragma region Replication

  public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

#pragma region LootPickup

  public:
	bool TryReserveForPickup(AActor* Requester);
	bool CommitPickupReservation(AActor* Requester);
	void ReleasePickupReservation(AActor* Requester);
	FHeistLootPickupCommitted& GetLootPickupCommittedDelegate();

  private:
	TWeakObjectPtr<AActor> PickupReservationOwner;
	FHeistLootPickupCommitted LootPickupCommittedDelegate;

#pragma endregion

#pragma region InternalHelpers

  private:
	void ResolveLootData();
	void ResolveLootVisualFromRowId();
	void ApplyLootVisual(const struct FHeistLootDataRow& LootDefinition);
	void ApplyFallbackLootData();
	void RefreshAvailabilityPresentation();

	UFUNCTION()
	void OnRep_LootRowId();

	UFUNCTION()
	void OnRep_IsAvailable();

#pragma endregion
};
