#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/HeistTypes.h"

#include "HeistGuardStateComponent.generated.h"

struct FHeistGuardDataRow;

DECLARE_MULTICAST_DELEGATE_TwoParams(FHeistGuardStateChanged, EHeistGuardState, EHeistGuardState);

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
	bool StartInvestigateConfirmationTimer();
	bool EnterInspectExhibit(const FVector& ExhibitLocation);
	bool StartInspectExhibitCast(float DurationSeconds);
	bool EnterSearchLastKnownLocation(const FVector& SearchLocation);
	bool StartSearchTimer();
	bool EnterReturnToPatrol();
	bool SetDisabled(bool bDisabled);
	bool ApplyStun(float DurationSeconds);

	UFUNCTION(BlueprintPure, Category = "Heist|Guard Runtime State")
	EHeistGuardState GetGuardState() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Guard Runtime State")
	float GetStateEndServerTime() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Guard Runtime State")
	FVector GetStateFocusLocation() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Guard Runtime State")
	AActor* GetChaseTarget() const;
	float GetInvestigateConfirmationDuration() const;
	float GetSearchDuration() const;
	void SetAlertSearchDurationMultiplier(float Multiplier);
	float GetAlertSearchDurationMultiplier() const;
	bool IsStateTimerActive() const;

	FHeistGuardStateChanged& GetGuardStateChangedDelegate();
	DECLARE_MULTICAST_DELEGATE(FHeistInspectExhibitCastExpired);
	FHeistInspectExhibitCastExpired& GetInspectExhibitCastExpiredDelegate();
	void ConfigureGuardProfile(const FHeistGuardDataRow& GuardData);

  private:
	bool CommitState(EHeistGuardState NewState, float DurationSeconds = 0.0f, bool bBypassPriority = false);
	bool CanEnterState(EHeistGuardState NewState) const;
	bool StartStateTimer(float DurationSeconds);
	void HandleTimedStateExpired(int32 ExpectedTimerRevision, EHeistGuardState ExpectedState);
	void ClearStateTimer();

	UFUNCTION()
	void OnRep_GuardState(EHeistGuardState PreviousState);

	UPROPERTY(ReplicatedUsing = OnRep_GuardState, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Guard Runtime State", meta = (AllowPrivateAccess = "true"))
	EHeistGuardState GuardState = EHeistGuardState::Patrol;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Guard Runtime State", meta = (AllowPrivateAccess = "true"))
	float StateEndServerTime = 0.0f;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Guard Runtime State", meta = (AllowPrivateAccess = "true"))
	FVector StateFocusLocation = FVector::ZeroVector;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Guard Runtime State", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> ChaseTarget;

	float InvestigateDuration = 0.0f;
	float SearchDuration = 0.0f;
	float BaseSearchDuration = 0.0f;
	float AlertSearchDurationMultiplier = 1.0f;
	float PendingInvestigateDuration = 0.0f;
	FTimerHandle StateTimerHandle;
	int32 StateTimerRevision = 0;
	FHeistGuardStateChanged GuardStateChangedDelegate;
	FHeistInspectExhibitCastExpired InspectExhibitCastExpiredDelegate;

#pragma endregion

#pragma region Replication

  public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion
};
