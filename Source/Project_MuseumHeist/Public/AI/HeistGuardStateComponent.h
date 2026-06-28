#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/HeistTypes.h"

#include "HeistGuardStateComponent.generated.h"

struct FHeistGuardDataRow;

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FHeistGuardStateChanged,
	EHeistGuardState,
	EHeistGuardState);

UCLASS(ClassGroup = (Heist), meta = (BlueprintSpawnableComponent))
class PROJECT_MUSEUMHEIST_API UHeistGuardStateComponent : public UActorComponent
{
	GENERATED_BODY()

#pragma region Construction

public:
	UHeistGuardStateComponent();

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#pragma endregion

#pragma region State

public:
	bool EnterPatrol();
	bool EnterChasePlayer(AActor* TargetActor);
	bool RefreshChaseTargetLocation();
	bool EnterInvestigateNoise(const FVector& InvestigateLocation, float DurationSeconds);
	bool EnterSearchLastKnownLocation(const FVector& SearchLocation);
	bool EnterReturnToPatrol();
	bool SetDisabled(bool bDisabled);
	bool ApplyStun(float DurationSeconds);

	UFUNCTION(BlueprintPure, Category = "Heist|AI")
	EHeistGuardState GetGuardState() const;

	UFUNCTION(BlueprintPure, Category = "Heist|AI")
	float GetStateEndServerTime() const;

	UFUNCTION(BlueprintPure, Category = "Heist|AI")
	FVector GetStateFocusLocation() const;

	UFUNCTION(BlueprintPure, Category = "Heist|AI")
	AActor* GetChaseTarget() const;

	FHeistGuardStateChanged& GetGuardStateChangedDelegate();
	void ConfigureGuardProfile(const FHeistGuardDataRow& GuardData);

private:
	bool CommitState(
		EHeistGuardState NewState,
		float DurationSeconds = 0.0f,
		bool bBypassPriority = false);
	bool CanEnterState(EHeistGuardState NewState) const;
	void HandleTimedStateExpired();
	void ClearStateTimer();

	UFUNCTION()
	void OnRep_GuardState(EHeistGuardState PreviousState);

	UPROPERTY(ReplicatedUsing = OnRep_GuardState, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|AI", meta = (AllowPrivateAccess = "true"))
	EHeistGuardState GuardState = EHeistGuardState::Patrol;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|AI", meta = (AllowPrivateAccess = "true"))
	float StateEndServerTime = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|AI", meta = (AllowPrivateAccess = "true"))
	FVector StateFocusLocation = FVector::ZeroVector;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|AI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> ChaseTarget;

	float InvestigateDuration = 0.0f;
	FTimerHandle StateTimerHandle;
	FHeistGuardStateChanged GuardStateChangedDelegate;

#pragma endregion

#pragma region Replication

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion
};
