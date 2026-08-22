#pragma once

#include "CoreMinimal.h"
#include "World/Interaction/HeistInteractableActor.h"

#include "HeistSecurityHoldButtonActor.generated.h"

class AHeistGameState;
class AHeistLaserBarrierActor;
class AHeistPlayerCharacter;
class AHeistPlayerState;
class UNiagaraSystem;
class USoundBase;
class UPrimitiveComponent;

enum class EHeistCrewStatus : uint8;
enum class EHeistMatchPhase : uint8;

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistSecurityHoldButtonActor : public AHeistInteractableActor
{
	GENERATED_BODY()

#pragma region Construction

  public:
	AHeistSecurityHoldButtonActor();

#pragma endregion

#pragma region Lifecycle

  protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#pragma endregion

#pragma region Interaction

  public:
	virtual bool CanInteract(const AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;

	bool TryBeginHold(AHeistPlayerCharacter* RequestingCharacter);
	bool TryEndHold(AHeistPlayerState* RequestingPlayerState, FName Reason);
	bool ForceReleaseForPlayer(AHeistPlayerState* PlayerState, FName Reason);
	void ForceReleaseHold(FName Reason);

	UFUNCTION(BlueprintPure, Category = "Heist|Security|Hold Button")
	bool IsHoldActive() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Security|Hold Button")
	bool IsBypassActive() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Security|Hold Button")
	float GetHoldProgress() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Security|Hold Button")
	AHeistPlayerState* GetHolderPlayerState() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Security|Hold Button")
	AHeistLaserBarrierActor* GetLinkedLaserBarrier() const;

  private:
	bool IsHolderContextValid() const;
	float ResolveServerWorldTimeSeconds() const;
	void CompleteHold();
	void ValidateHolder();
	void ReleaseHoldInternal(FName Reason, bool bImmediateBarrierRestore);
	void ApplyPresentation();
	void HandleMatchPhaseChanged(EHeistMatchPhase PreviousMatchPhase, EHeistMatchPhase NewMatchPhase);
	void HandleHolderCrewStatusChanged(EHeistCrewStatus NewStatus);

	UFUNCTION()
	void HandleInteractionAreaEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);

	UFUNCTION()
	void OnRep_HoldState();

  protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|Security|Hold Button", meta = (DisplayName = "Apply Security Hold Button Presentation"))
	void BP_ApplySecurityHoldButtonPresentation(bool bHolding, bool bLaserBypassed, float HoldProgress, int32 Revision, AHeistPlayerState* Holder);

#pragma endregion

#pragma region Configuration

  protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Hold Button|Link")
	TObjectPtr<AHeistLaserBarrierActor> LinkedLaserBarrier;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Security|Hold Button|Assets")
	TObjectPtr<USoundBase> HoldStartedSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Security|Hold Button|Assets")
	TObjectPtr<USoundBase> HoldCompletedSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Security|Hold Button|Assets")
	TObjectPtr<USoundBase> HoldReleasedSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Security|Hold Button|Assets")
	TObjectPtr<UNiagaraSystem> HoldCompletedEffect;

	/** Multiplies the Blueprint-authored resting mesh scale on Z while a player owns the hold. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Security|Hold Button|Presentation", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float HeldVisualScaleZMultiplier = 0.72f;

#pragma endregion

#pragma region Replication

  private:
	UPROPERTY(ReplicatedUsing = OnRep_HoldState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Hold Button", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AHeistPlayerState> HolderPlayerState;

	UPROPERTY(ReplicatedUsing = OnRep_HoldState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Hold Button", meta = (AllowPrivateAccess = "true"))
	bool bBypassActive = false;

	UPROPERTY(ReplicatedUsing = OnRep_HoldState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Hold Button", meta = (AllowPrivateAccess = "true"))
	float HoldStartServerTime = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_HoldState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Hold Button", meta = (AllowPrivateAccess = "true"))
	float HoldEndServerTime = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_HoldState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Hold Button", meta = (AllowPrivateAccess = "true"))
	int32 HoldRevision = 0;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

#pragma region Runtime

  private:
	TWeakObjectPtr<AHeistPlayerCharacter> HolderCharacter;
	TWeakObjectPtr<AHeistGameState> BoundGameState;
	FTimerHandle HoldCompletionTimerHandle;
	FTimerHandle HolderValidationTimerHandle;
	FVector ReleasedVisualScale = FVector::OneVector;
	bool bAppliedBypassActive = false;
	int32 AppliedHoldRevision = INDEX_NONE;
	TWeakObjectPtr<AHeistPlayerState> AppliedHolderPlayerState;

#pragma endregion
};
