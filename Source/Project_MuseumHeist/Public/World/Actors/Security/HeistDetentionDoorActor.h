#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "World/Interaction/HeistInteractableActor.h"
#include "HeistDetentionDoorActor.generated.h"

class UBoxComponent;
class USceneComponent;
class USoundBase;
class USoundAttenuation;
class AHeistPlayerCharacter;
class AHeistPlayerState;
class AHeistGameState;

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistDetentionDoorActor : public AHeistInteractableActor
{
	GENERATED_BODY()
public:
	AHeistDetentionDoorActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual bool CanInteract(const AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;
	bool TryUse(AHeistPlayerCharacter* Character, int32 ExpectedRevision);
	void ReleaseRescue(AHeistPlayerCharacter* Character);
	void CancelForPlayer(AHeistPlayerCharacter* Character);
	bool ContainsLocation(const FVector& Location) const;
	bool IsPlayerContained(const AHeistPlayerState* Player) const;
	bool CanSecureCell() const;
	void SecureForArrest(AHeistPlayerState* Player);
	bool IsOpen() const { return bOpen; }
	int32 GetRevision() const { return Revision; }
	int32 GetCompletedLatches() const { return CompletedLatches; }
	AHeistPlayerState* GetOperator() const { return Operator; }
	bool IsRescueOperation() const { return bOutsideRescue; }
	float GetTimingProgress() const;
	float GetSuccessWindowWidth() const;
	float GetSuccessWindowCenter() const { return 0.70f; }
	FText GetPrompt(const AHeistPlayerCharacter* Character) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Heist|Detention")
	TObjectPtr<UBoxComponent> CellBounds;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Heist|Detention")
	TObjectPtr<UBoxComponent> DoorBlocker;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Heist|Detention")
	TObjectPtr<USceneComponent> DoorHinge;
	UPROPERTY(EditDefaultsOnly, Category="Heist|Detention|Audio")
	TObjectPtr<USoundBase> LatchSound;
	UPROPERTY(EditDefaultsOnly, Category="Heist|Detention|Audio")
	TObjectPtr<USoundBase> FailureSound;
	UPROPERTY(EditDefaultsOnly, Category="Heist|Detention|Audio")
	TObjectPtr<USoundBase> OpenSound;
	UPROPERTY(EditDefaultsOnly, Category="Heist|Detention|Audio")
	TObjectPtr<USoundAttenuation> SoundAttenuation;
	UPROPERTY(EditDefaultsOnly, Category="Heist|Detention", meta=(ClampMin="3.0"))
	float LatchPeriodSeconds = 6.0f;
	UPROPERTY(EditDefaultsOnly, Category="Heist|Detention", meta=(ClampMin="0.5"))
	float RescueDurationSeconds = 2.0f;
	UPROPERTY(EditDefaultsOnly, Category="Heist|Detention", meta=(ClampMin="0.0"))
	float FailureNoiseRadius = 2400.0f;

private:
	bool IsOperatorValid() const;
	void ValidateOperation();
	void CancelOperation();
	void OpenCell();
	void ApplyPresentation();
	void HandlePhaseChanged(EHeistMatchPhase Previous, EHeistMatchPhase Current);
	float ServerTime() const;
	UFUNCTION() void OnRep_State();
	UFUNCTION(NetMulticast, Unreliable) void MulticastSound(uint8 Event);

	UPROPERTY(ReplicatedUsing=OnRep_State) bool bOpen = false;
	UPROPERTY(ReplicatedUsing=OnRep_State) int32 CompletedLatches = 0;
	UPROPERTY(ReplicatedUsing=OnRep_State) int32 Revision = 0;
	UPROPERTY(ReplicatedUsing=OnRep_State) TObjectPtr<AHeistPlayerState> Operator;
	UPROPERTY(Replicated) bool bOutsideRescue = false;
	UPROPERTY(Replicated) float RoundStartServerTime = 0.0f;
	FVector OperationOrigin = FVector::ZeroVector;
	float LastAttemptServerTime = -1.0f;
	FTimerHandle ValidationTimer;
	TWeakObjectPtr<AHeistGameState> BoundGameState;
};
