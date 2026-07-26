#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Core/HeistTypes.h"
#include "Perception/AIPerceptionTypes.h"

#include "HeistGuardAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UStateTreeAIComponent;
class AHeistPaintingDisplayCaseActor;
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
	bool TryGetAlertExitSurveillanceTarget(AActor*& OutTargetActor, float& OutAcceptanceRadius) const;
	bool IsAlertExitSurveillanceActive() const;
	EHeistAlertLevel GetAppliedAlertLevel() const;
	float GetActiveSightRadius() const;
	float GetAlertSightRadiusMultiplier() const;

  private:
	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* TargetActor, FAIStimulus Stimulus);

	bool CanInitiallySeeTarget(const AActor* TargetActor, const TCHAR*& OutRejectReason, AActor*& OutBlockingActor) const;
	bool IsChaseTargetOccluded(const AActor* TargetActor, const TCHAR*& OutRejectReason, AActor*& OutBlockingActor) const;
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
	void HandleAlertStateChanged(EHeistAlertLevel PreviousLevel, EHeistAlertLevel NewLevel, int32 Revision, FName TriggerId);
	void HandleMatchPhaseChanged(EHeistMatchPhase PreviousMatchPhase, EHeistMatchPhase NewMatchPhase);
	void ApplyAlertModifiers(EHeistAlertLevel NewLevel);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|AI|Perception", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIPerceptionComponent> GuardPerceptionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|AI|Perception", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAISenseConfig_Sight> GuardSightConfig;

	float SightRadius = 0.0f;
	float AggroResetDistance = 0.0f;
	float ActiveSightRadius = 0.0f;
	float ActiveAggroResetDistance = 0.0f;
	float DefaultSightHalfAngle = 0.0f;
	float InvestigateSightHalfAngle = 0.0f;
	float EyeHeight = 0.0f;
	float DetectionGrace = 0.0f;
	float SightUpdateInterval = 0.0f;
	bool bDoorsBlockSight = true;
	bool bDisplayCasesBlockSight = true;
	bool bPerceptionConfigured = false;
	bool bAutomaticSightEnabled = true;
	bool bAlertExitSurveillanceActive = false;
	EHeistAlertLevel AppliedAlertLevel = EHeistAlertLevel::Quiet;
	float AlertSightRadiusMultiplier = 1.0f;
	float AlertExitSurveillanceAcceptanceRadius = 175.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|AI|Arrest", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float ArrestDistance = 125.0f;
	FName DoorOccluderTag = FName(TEXT("HeistDoorOccluder"));
	TWeakObjectPtr<AActor> PendingSightTarget;
	FTimerHandle DetectionGraceTimerHandle;
	FTimerHandle SightValidationTimerHandle;

#pragma endregion

#pragma region InspectionTarget

  public:
	bool TrySelectInspectionTarget();
	bool TryBeginInspection();
	bool StartInspectionCast();
	void AbortInspection(FName Reason);
	AHeistPaintingDisplayCaseActor* GetInspectionTarget() const;
	bool IsInspectionTargetValid() const;
	int32 GetInspectionTargetSelectionRevision() const;
	float GetInspectionAcceptanceRadius() const;

  private:
	AHeistPaintingDisplayCaseActor* FindBestInspectionTarget() const;
	void HandleInspectionCastExpired();

	TWeakObjectPtr<AHeistPaintingDisplayCaseActor> InspectionTarget;
	int32 InspectionTargetSelectionRevision = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|AI|Inspection", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", Units = "s"))
	float InspectionCastDuration = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|AI|Inspection", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float InspectionAcceptanceRadius = 100.0f;

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
