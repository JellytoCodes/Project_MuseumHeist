#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Core/HeistTypes.h"
#include "Perception/AIPerceptionTypes.h"

#include "HeistGuardAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UStateTreeAIComponent;
struct FHeistGuardDataRow;

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistGuardAIController : public AAIController
{
	GENERATED_BODY()

#pragma region Construction

public:
	AHeistGuardAIController();

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

#pragma endregion

#pragma region Perception

public:
	void ConfigurePerceptionFromGuardProfile(const FHeistGuardDataRow& GuardData);
	bool DebugEvaluateSightTarget(AActor* TargetActor);
	void SetAutomaticSightEnabled(bool bEnabled);
	bool IsAutomaticSightEnabled() const;
	bool TryArrestChaseTarget();

private:
	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* TargetActor, FAIStimulus Stimulus);

	bool CanInitiallySeeTarget(
		const AActor* TargetActor,
		const TCHAR*& OutRejectReason,
		AActor*& OutBlockingActor) const;
	bool IsChaseTargetOccluded(
		const AActor* TargetActor,
		const TCHAR*& OutRejectReason,
		AActor*& OutBlockingActor) const;
	FVector GetTargetSightLocation(const AActor* TargetActor) const;
	bool IsDoorOccluder(const FHitResult& HitResult) const;
	void TryAcquireSightTarget();
	void BeginDetectionGrace(AActor* TargetActor);
	void CompleteDetectionGrace();
	void ClearDetectionGrace(const TCHAR* Reason);
	void ValidateCurrentChaseTarget();
	void UpdateSightForGuardState(EHeistGuardState NewState);
	void StartSightValidationTimer();
	void ClearSightValidationTimer();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|AI|Perception", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIPerceptionComponent> GuardPerceptionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|AI|Perception", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAISenseConfig_Sight> GuardSightConfig;

	float SightRadius = 0.0f;
	float AggroResetDistance = 0.0f;
	float DefaultSightHalfAngle = 0.0f;
	float InvestigateSightHalfAngle = 0.0f;
	float EyeHeight = 0.0f;
	float DetectionGrace = 0.0f;
	float SightUpdateInterval = 0.0f;
	bool bDoorsBlockSight = true;
	bool bDisplayCasesBlockSight = true;
	bool bPerceptionConfigured = false;
	bool bAutomaticSightEnabled = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|AI|Arrest", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float ArrestDistance = 125.0f;
	FName DoorOccluderTag = FName(TEXT("HeistDoorOccluder"));
	TWeakObjectPtr<AActor> PendingSightTarget;
	FTimerHandle DetectionGraceTimerHandle;
	FTimerHandle SightValidationTimerHandle;

#pragma endregion

#pragma region StateTree

public:
	UStateTreeAIComponent* GetGuardStateTreeComponent() const;

private:
	void HandleGuardStateChanged(EHeistGuardState PreviousState, EHeistGuardState NewState);
	void SendGuardStateTreeEvent(EHeistGuardState NewState);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|AI|StateTree", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStateTreeAIComponent> GuardStateTreeComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|AI|StateTree", meta = (AllowPrivateAccess = "true"))
	bool bStartStateTreeAutomatically = true;

#pragma endregion
};
