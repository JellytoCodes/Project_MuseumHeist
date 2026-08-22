#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "HeistSecurityCameraActor.generated.h"

class AHeistGameState;
class AHeistPlayerCharacter;
class AHeistPlayerState;
class UBoxComponent;
class UNiagaraSystem;
class USceneComponent;
class USoundBase;
class UStaticMeshComponent;
class UPrimitiveComponent;
struct FHitResult;

enum class EHeistMatchPhase : uint8;

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistSecurityCameraActor : public AActor
{
	GENERATED_BODY()

#pragma region Construction

  public:
	AHeistSecurityCameraActor();

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

  private:
	void StartAuthorityEvaluation();
	void StopAuthorityEvaluation(bool bResetReplicatedState);
	void EvaluateDetectionCandidates();
	bool IsEligibleTarget(const AHeistPlayerCharacter* PlayerCharacter) const;
	bool HasDetectionLineOfSight(const AHeistPlayerCharacter* PlayerCharacter) const;
	FVector ResolveSensorForward() const;
	float ResolveServerWorldTimeSeconds() const;
	float ResolveEvaluationIntervalSeconds() const;
	float ResolveDetectionBuildUpSeconds() const;
	float ResolveDetectionCooldownSeconds() const;
	void CommitDetection(AHeistPlayerCharacter* PlayerCharacter);
	void RefreshReplicatedDetectionProgress();
	void ApplyPresentation();
	void HandleMatchPhaseChanged(EHeistMatchPhase PreviousMatchPhase, EHeistMatchPhase NewMatchPhase);

	UFUNCTION()
	void HandleDetectionVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex,
									   bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleDetectionVolumeEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);

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
	TObjectPtr<UBoxComponent> DetectionVolumeComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Security|Camera")
	TObjectPtr<UStaticMeshComponent> VisualMeshComponent;

#pragma endregion

#pragma region Configuration

  protected:
	/** Broad-phase overlap volume remains map-authored; the server still validates range, angle, and LOS. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Camera|Coverage", meta = (ClampMin = "100.0", Units = "cm"))
	float DetectionRange = 1800.0f;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Camera|Coverage", meta = (ClampMin = "1.0", ClampMax = "89.0", Units = "deg"))
	float DetectionHalfAngleDegrees = 35.0f;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Camera|Sweep", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
	float SweepHalfAngleDegrees = 35.0f;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Camera|Sweep", meta = (ClampMin = "0.1", Units = "s"))
	float SweepPeriodSeconds = 6.0f;

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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

#pragma region Runtime

  private:
	TSet<TWeakObjectPtr<AHeistPlayerCharacter>> OverlappingPlayers;
	TMap<TWeakObjectPtr<AHeistPlayerCharacter>, float> DetectionBuildUpByPlayer;
	TWeakObjectPtr<AHeistGameState> BoundGameState;
	FTimerHandle DetectionEvaluationTimerHandle;
	float DetectionCooldownEndServerTime = 0.0f;
	bool bAppliedCameraEnabled = false;
	bool bAppliedTrackingAnyTarget = false;
	uint8 AppliedDetectionProgressByte = 0;
	int32 AppliedDetectionRevision = INDEX_NONE;
	TWeakObjectPtr<AHeistPlayerState> AppliedDetectedPlayerState;
	float AppliedSweepEpochServerTime = 0.0f;

#pragma endregion
};
