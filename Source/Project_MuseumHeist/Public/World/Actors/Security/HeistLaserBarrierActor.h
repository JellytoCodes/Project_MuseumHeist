#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "HeistLaserBarrierActor.generated.h"

class AHeistGameState;
class AHeistPaintingDisplayCaseActor;
class AHeistPlayerCharacter;
class AHeistPlayerState;
class UBoxComponent;
class UNiagaraSystem;
class USceneComponent;
class USoundBase;
class UStaticMeshComponent;
class UPrimitiveComponent;
struct FHitResult;
struct FHeistContractSnapshot;

enum class EHeistMatchPhase : uint8;

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistLaserBarrierActor : public AActor
{
	GENERATED_BODY()

#pragma region Construction

  public:
	AHeistLaserBarrierActor();

#pragma endregion

#pragma region Lifecycle

  protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#pragma endregion

#pragma region Security

  public:
	UFUNCTION(BlueprintPure, Category = "Heist|Security|Laser")
	bool IsBarrierEnabled() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Security|Laser")
	bool IsBeamActive() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Security|Laser")
	bool IsRearming() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Security|Laser")
	int32 GetSecurityRevision() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Security|Laser")
	AHeistPlayerState* GetBypassHolderPlayerState() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Security|Laser")
	AHeistPaintingDisplayCaseActor* GetProtectedPaintingCase() const;

	bool TryActivateBypass(AHeistPlayerState* RequestingHolder);
	bool BeginRearm(AHeistPlayerState* ReleasingHolder);
	void ForceRestoreDefaultState();

  private:
	bool IsRuntimeConfigurationValid() const;
	bool IsEligibleEntrant(const AHeistPlayerCharacter* PlayerCharacter) const;
	void CompleteRearm();
	void CommitTrip(AHeistPlayerCharacter* PlayerCharacter);
	void ApplyPresentation();
	void ScheduleConfigurationRefresh();
	void RefreshRuntimeConfiguration();
	void HandleMatchPhaseChanged(EHeistMatchPhase PreviousMatchPhase, EHeistMatchPhase NewMatchPhase);
	void HandleContractSnapshotChanged(const FHeistContractSnapshot& ContractSnapshot);

	UFUNCTION()
	void HandleBeamBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex,
							bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleBeamEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);

	UFUNCTION()
	void OnRep_LaserState();

  protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|Security|Laser", meta = (DisplayName = "Apply Laser Barrier Presentation"))
	void BP_ApplyLaserBarrierPresentation(bool bEnabled, bool bActive, bool bInRearmGrace, int32 Revision, AHeistPlayerState* BypassHolder);

	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|Security|Laser", meta = (DisplayName = "Play Laser Trip Presentation"))
	void BP_PlayLaserTripPresentation(AHeistPlayerState* TrippedPlayer, int32 TripRevision);

#pragma endregion

#pragma region Components

  protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Security|Laser")
	TObjectPtr<USceneComponent> SceneRootComponent;

	/** Query-only overlap. It must never physically block player movement. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Security|Laser")
	TObjectPtr<UBoxComponent> BeamTriggerComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Security|Laser")
	TObjectPtr<UStaticMeshComponent> BeamVisualComponent;

#pragma endregion

#pragma region Configuration

  protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Laser|Link")
	TObjectPtr<AHeistPaintingDisplayCaseActor> ProtectedPaintingCase;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Security|Laser|Assets")
	TObjectPtr<USoundBase> BypassActivatedSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Security|Laser|Assets")
	TObjectPtr<USoundBase> RearmSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Security|Laser|Assets")
	TObjectPtr<USoundBase> TripSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Security|Laser|Assets")
	TObjectPtr<UNiagaraSystem> TripEffect;

#pragma endregion

#pragma region Replication

  private:
	UPROPERTY(ReplicatedUsing = OnRep_LaserState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Laser", meta = (AllowPrivateAccess = "true"))
	bool bBarrierEnabled = false;

	UPROPERTY(ReplicatedUsing = OnRep_LaserState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Laser", meta = (AllowPrivateAccess = "true"))
	bool bBeamActive = false;

	UPROPERTY(ReplicatedUsing = OnRep_LaserState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Laser", meta = (AllowPrivateAccess = "true"))
	bool bRearmGraceActive = false;

	UPROPERTY(ReplicatedUsing = OnRep_LaserState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Laser", meta = (AllowPrivateAccess = "true"))
	int32 SecurityRevision = 0;

	UPROPERTY(ReplicatedUsing = OnRep_LaserState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Laser", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AHeistPlayerState> BypassHolderPlayerState;

	UPROPERTY(ReplicatedUsing = OnRep_LaserState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Laser", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AHeistPlayerState> LastTrippedPlayerState;

	UPROPERTY(ReplicatedUsing = OnRep_LaserState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Security|Laser", meta = (AllowPrivateAccess = "true"))
	int32 TripSequence = 0;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

#pragma region Runtime

  private:
	TSet<TWeakObjectPtr<AHeistPlayerCharacter>> PlayersInsideBeam;
	TWeakObjectPtr<AHeistGameState> BoundGameState;
	FTimerHandle RearmTimerHandle;
	FTimerHandle ConfigurationRefreshTimerHandle;
	int32 AppliedTripPresentationSequence = 0;
	bool bAppliedBarrierEnabled = false;
	bool bAppliedBeamActive = false;
	bool bAppliedRearmGraceActive = false;
	int32 AppliedSecurityRevision = INDEX_NONE;
	TWeakObjectPtr<AHeistPlayerState> AppliedBypassHolderPlayerState;

#pragma endregion
};
