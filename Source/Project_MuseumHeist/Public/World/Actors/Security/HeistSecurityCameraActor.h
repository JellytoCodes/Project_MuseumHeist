#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "HeistSecurityCameraActor.generated.h"

class AHeistGameState;
class AHeistPlayerCharacter;
class AHeistPlayerState;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UNiagaraSystem;
class USceneComponent;
class USoundBase;
class USpotLightComponent;
class UStaticMeshComponent;
struct FAIStimulus;

enum class EHeistMatchPhase : uint8;

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistSecurityCameraActor : public AActor
{
	GENERATED_BODY()

#pragma region Construction

  public:
	AHeistSecurityCameraActor();
	virtual void GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

#pragma endregion

#pragma region Lifecycle

  protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#pragma endregion

#pragma region Security

  public:
	UFUNCTION(BlueprintPure, Category = "Heist|Security|Camera")
	bool IsCameraEnabled() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Security|Camera")
	float GetDetectionProgress() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Security|Camera")
	int32 GetDetectionRevision() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Security|Camera")
	AHeistPlayerState* GetLastDetectedPlayerState() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Security|Camera")
	float GetResolvedSweepYawDegrees() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Security|Camera")
	bool IsSweepPaused() const;

  private:
	friend class FHeistCCTVPerceptionTest;
	void StartAuthorityEvaluation();
	void StopAuthorityEvaluation(bool bResetReplicatedState);
	void EvaluateDetectionCandidates();
	void UpdateSweepPauseState(const TArray<AActor*>& VisibleActors);
	void ResumeSweepAfterSightLoss();
	bool IsEligibleTarget(const AHeistPlayerCharacter* PlayerCharacter) const;
	FVector ResolveSensorForward() const;
	float ResolveServerWorldTimeSeconds() const;
	float ResolveEvaluationIntervalSeconds() const;
	float ResolveDetectionBuildUpSeconds() const;
	float ResolveDetectionCooldownSeconds() const;
	void CommitDetection(AHeistPlayerCharacter* PlayerCharacter);
	void RefreshReplicatedDetectionProgress();
	void ApplyPresentation();
	void ConfigureSightLight();
	void HandleMatchPhaseChanged(EHeistMatchPhase PreviousMatchPhase, EHeistMatchPhase NewMatchPhase);

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* TargetActor, FAIStimulus Stimulus);

	UFUNCTION()
	void OnRep_SecurityCameraState();

  protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|Security|Camera", meta = (DisplayName = "Apply Security Camera Presentation"))
	void BP_ApplySecurityCameraPresentation(bool bEnabled, bool bTrackingTarget, float DetectionProgress, int32 InConfirmedDetectionRevision,
										 AHeistPlayerState* DetectedPlayerState);

#pragma endregion

#pragma region Components

  protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Security|Camera")
	TObjectPtr<USceneComponent> SceneRootComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Security|Camera")
	TObjectPtr<USceneComponent> SensorOriginComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Security|Camera")
	TObjectPtr<UAIPerceptionComponent> CameraPerceptionComponent;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> CameraSightConfig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Security|Camera")
	TObjectPtr<UStaticMeshComponent> VisualMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Security|Camera")
	TObjectPtr<USpotLightComponent> SightLightComponent;

#pragma endregion

#pragma region Configuration

  protected:
	/** Map-authored range/angle configure server Sight; no collision volume bounds the cone. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Camera|Coverage", meta = (ClampMin = "100.0", Units = "cm"))
	float DetectionRange = 1200.0f;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Camera|Coverage", meta = (ClampMin = "1.0", ClampMax = "89.0", Units = "deg"))
	float DetectionHalfAngleDegrees = 35.0f;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Camera|Sweep", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
	float SweepHalfAngleDegrees = 35.0f;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Camera|Sweep", meta = (ClampMin = "0.1", Units = "s"))
	float SweepPeriodSeconds = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Security|Camera|Presentation")
	FLinearColor IdleLightColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Security|Camera|Presentation")
	FLinearColor AlertLightColor = FLinearColor(1.0f, 0.02f, 0.01f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Security|Camera|Assets")
	TObjectPtr<USoundBase> TrackingLoopSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Security|Camera|Assets")
	TObjectPtr<USoundBase> DetectionConfirmedSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Security|Camera|Assets")
	TObjectPtr<UNiagaraSystem> DetectionConfirmedEffect;

#pragma endregion

#pragma region Replication

  private:
	UPROPERTY(ReplicatedUsing = OnRep_SecurityCameraState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Camera", meta = (AllowPrivateAccess = "true"))
	bool bCameraEnabled = false;

	UPROPERTY(ReplicatedUsing = OnRep_SecurityCameraState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Camera", meta = (AllowPrivateAccess = "true"))
	bool bTrackingAnyTarget = false;

	UPROPERTY(ReplicatedUsing = OnRep_SecurityCameraState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Camera", meta = (AllowPrivateAccess = "true"))
	uint8 DetectionProgressByte = 0;

	UPROPERTY(ReplicatedUsing = OnRep_SecurityCameraState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Camera", meta = (AllowPrivateAccess = "true"))
	int32 ConfirmedDetectionRevision = 0;

	UPROPERTY(ReplicatedUsing = OnRep_SecurityCameraState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AHeistPlayerState> LastDetectedPlayerState;

	UPROPERTY(ReplicatedUsing = OnRep_SecurityCameraState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Camera", meta = (AllowPrivateAccess = "true"))
	float SweepEpochServerTime = 0.0f;

	/** Negative while sweeping; otherwise freezes the shared sweep clock at first sight. */
	UPROPERTY(ReplicatedUsing = OnRep_SecurityCameraState)
	float SweepPausedServerTime = -1.0f;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

#pragma region Runtime

  private:
	TMap<TWeakObjectPtr<AHeistPlayerCharacter>, float> DetectionBuildUpByPlayer;
	FQuat InitialVisualRelativeRotation = FQuat::Identity;
	float DisplayedLightRisk = 0.0f;
	float ConfirmedLightHoldUntil = 0.0f;
	TWeakObjectPtr<AHeistGameState> BoundGameState;
	FTimerHandle DetectionEvaluationTimerHandle;
	FTimerHandle SweepResumeTimerHandle;
	float DetectionCooldownEndServerTime = 0.0f;
	bool bAppliedCameraEnabled = false;
	bool bAppliedTrackingAnyTarget = false;
	uint8 AppliedDetectionProgressByte = 0;
	int32 AppliedDetectionRevision = INDEX_NONE;
	TWeakObjectPtr<AHeistPlayerState> AppliedDetectedPlayerState;
	float AppliedSweepEpochServerTime = 0.0f;

#pragma endregion
};
