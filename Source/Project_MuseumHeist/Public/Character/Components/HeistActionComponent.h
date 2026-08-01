#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "HeistActionComponent.generated.h"

class AController;
class AHeistPlayerCharacter;
class AHeistPaintingDisplayCaseActor;
class AHeistVentActor;
class UDamageType;

DECLARE_MULTICAST_DELEGATE_TwoParams(FHeistEscapeCastCompleted, AHeistPlayerCharacter*, AHeistVentActor*);

DECLARE_MULTICAST_DELEGATE_TwoParams(FHeistObservationCastCompleted, AHeistPlayerCharacter*, AHeistPaintingDisplayCaseActor*);

DECLARE_MULTICAST_DELEGATE(FHeistActionStateChanged);

UCLASS(ClassGroup = (Heist), meta = (BlueprintSpawnableComponent))
class PROJECT_MUSEUMHEIST_API UHeistActionComponent : public UActorComponent
{
	GENERATED_BODY()

#pragma region Construction

  public:
	UHeistActionComponent();

#pragma endregion

#pragma region ObservationCast

  public:
	bool TryBeginObservationRequest(AHeistPaintingDisplayCaseActor* TargetDisplayCase);
	void CancelObservationRequest(const TCHAR* Reason);
	bool IsObservationCastActive() const;
	float GetObservationCastEndServerTime() const;
	AHeistPaintingDisplayCaseActor* GetPendingObservationDisplayCase() const;
	FHeistObservationCastCompleted& GetObservationCastCompletedDelegate();

  private:
	UPROPERTY(Transient)
	TWeakObjectPtr<AHeistPaintingDisplayCaseActor> PendingObservationDisplayCase;

	UPROPERTY(ReplicatedUsing = OnRep_ObservationCastActive, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Observation", meta = (AllowPrivateAccess = "true"))
	bool bObservationCastActive = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Observation", meta = (AllowPrivateAccess = "true"))
	float ObservationCastEndServerTime = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Observation", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float ObservationCastDurationSeconds = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Observation", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float ObservationCastMovementCancelDistance = 5.0f;

	UPROPERTY(Transient)
	FVector ObservationCastStartLocation = FVector::ZeroVector;

	FTimerHandle ObservationCastTimerHandle;
	FHeistObservationCastCompleted ObservationCastCompletedDelegate;

	UFUNCTION()
	void OnRep_ObservationCastActive();

#pragma endregion

#pragma region Lifecycle

  protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#pragma endregion

#pragma region EscapeCast

  public:
	bool TryBeginEscapeRequest(AHeistVentActor* TargetVentActor);
	bool IsGameplayCastActive() const;
	void CancelGameplayActions(const TCHAR* Reason);
	bool HasPendingEscapeRequest() const;
	AHeistVentActor* GetPendingEscapeVent() const;
	void ClearPendingEscapeRequest();
	bool IsEscapeCastActive() const;
	float GetEscapeCastEndServerTime() const;
	FHeistEscapeCastCompleted& GetEscapeCastCompletedDelegate();
	FHeistActionStateChanged& GetActionStateChangedDelegate();

  private:
	UPROPERTY(Transient)
	TWeakObjectPtr<AHeistVentActor> PendingEscapeVent;

	UPROPERTY(ReplicatedUsing = OnRep_EscapeCastActive, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Escape", meta = (AllowPrivateAccess = "true"))
	bool bEscapeCastActive = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Escape", meta = (AllowPrivateAccess = "true"))
	float EscapeCastEndServerTime = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Escape", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float EscapeCastMovementCancelDistance = 5.0f;

	UPROPERTY(Transient)
	FVector EscapeCastStartLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	int32 EscapeCastStartAlertRevision = INDEX_NONE;

	FTimerHandle EscapeCastTimerHandle;
	FHeistEscapeCastCompleted EscapeCastCompletedDelegate;
	FHeistActionStateChanged ActionStateChangedDelegate;

	UFUNCTION()
	void OnRep_EscapeCastActive();

	UFUNCTION()
	void HandleOwnerTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

#pragma endregion

#pragma region Replication

  public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

#pragma region InternalHelpers

  private:
	float ResolveEscapeCastDurationSeconds() const;
	bool HasMovedBeyondEscapeCastTolerance() const;
	bool HasMovedBeyondObservationCastTolerance() const;
	void HandleEscapeCastTimerElapsed();
	void HandleObservationCastTimerElapsed();
	void CancelEscapeCast(const TCHAR* Reason);
	void CancelObservationCast(const TCHAR* Reason);
	void ClearEscapeCastState();
	void ClearObservationCastState();

#pragma endregion
};
