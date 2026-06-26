#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "HeistActionComponent.generated.h"

class AHeistPlayerCharacter;
class AHeistTrapActor;
class AHeistVentActor;
class UDamageType;

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FHeistEscapeCastCompleted,
	AHeistPlayerCharacter*,
	AHeistVentActor*);

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FHeistTrapPlacementCastCompleted,
	AHeistPlayerCharacter*,
	AHeistTrapActor*);

UCLASS(ClassGroup = (Heist), meta = (BlueprintSpawnableComponent))
class PROJECT_MUSEUMHEIST_API UHeistActionComponent : public UActorComponent
{
	GENERATED_BODY()

#pragma region Construction

public:
	UHeistActionComponent();

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

#pragma endregion

#pragma region EscapeCast

public:
	bool TryBeginEscapeRequest(AHeistVentActor* TargetVentActor);
	bool IsGameplayCastActive() const;
	bool HasPendingEscapeRequest() const;
	AHeistVentActor* GetPendingEscapeVent() const;
	void ClearPendingEscapeRequest();
	bool IsEscapeCastActive() const;
	float GetEscapeCastEndServerTime() const;
	FHeistEscapeCastCompleted& GetEscapeCastCompletedDelegate();

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

	FTimerHandle EscapeCastTimerHandle;
	FHeistEscapeCastCompleted EscapeCastCompletedDelegate;

	UFUNCTION()
	void OnRep_EscapeCastActive();

	UFUNCTION()
	void HandleOwnerTakeAnyDamage(
		AActor* DamagedActor,
		float Damage,
		const UDamageType* DamageType,
		AController* InstigatedBy,
		AActor* DamageCauser);

#pragma endregion

#pragma region TrapPlacementCast

public:
	bool TryBeginTrapPlacementRequest(
		FName SourceItemId,
		int32 SourceInstanceId,
		const FVector& TargetWorldLocation,
		float CastDurationSeconds,
		float EffectDurationSeconds,
		TSubclassOf<AHeistTrapActor> TrapActorClass,
		bool bConsumeSourceItem);
	bool IsTrapPlacementCastActive() const;
	FHeistTrapPlacementCastCompleted& GetTrapPlacementCastCompletedDelegate();

private:
	UPROPERTY(ReplicatedUsing = OnRep_TrapPlacementCastActive, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Trap", meta = (AllowPrivateAccess = "true"))
	bool bTrapPlacementCastActive = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Trap", meta = (AllowPrivateAccess = "true"))
	float TrapPlacementCastEndServerTime = 0.0f;

	UPROPERTY(Transient)
	FName PendingTrapItemId = NAME_None;

	UPROPERTY(Transient)
	int32 PendingTrapSourceInstanceId = INDEX_NONE;

	UPROPERTY(Transient)
	FVector PendingTrapTargetWorldLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	float PendingTrapEffectDurationSeconds = 0.0f;

	UPROPERTY(Transient)
	TSubclassOf<AHeistTrapActor> PendingTrapActorClass;

	UPROPERTY(Transient)
	bool bPendingTrapConsumesSourceItem = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Trap", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float TrapPlacementMovementCancelDistance = 5.0f;

	UPROPERTY(Transient)
	FVector TrapPlacementCastStartLocation = FVector::ZeroVector;

	FTimerHandle TrapPlacementCastTimerHandle;
	FHeistTrapPlacementCastCompleted TrapPlacementCastCompletedDelegate;

	UFUNCTION()
	void OnRep_TrapPlacementCastActive();

#pragma endregion

#pragma region Replication

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

#pragma region InternalHelpers

private:
	float ResolveEscapeCastDurationSeconds() const;
	bool HasMovedBeyondEscapeCastTolerance() const;
	bool HasMovedBeyondTrapPlacementCastTolerance() const;
	void HandleEscapeCastTimerElapsed();
	void HandleTrapPlacementCastTimerElapsed();
	void CancelEscapeCast(const TCHAR* Reason);
	void CancelTrapPlacementCast(const TCHAR* Reason);
	void ClearEscapeCastState();
	void ClearTrapPlacementCastState();

#pragma endregion
};
