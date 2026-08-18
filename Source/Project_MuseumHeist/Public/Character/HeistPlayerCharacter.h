#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "GameFramework/Character.h"
#include "World/Interaction/HeistInteractable.h"

#include "HeistPlayerCharacter.generated.h"

class UHeistActionComponent;
class UHeistForgeryComponent;
class UHeistInteractionComponent;
class UHeistInventoryComponent;
class UHeistNoiseEmitterComponent;
class UHeistObjectAssemblyComponent;
class UHeistStatusComponent;
class UHeistVisionComponent;
class UCameraComponent;
class UAudioComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class USphereComponent;
class USoundBase;
class USoundMix;
class UWidgetComponent;
class AHeistGameState;
class AHeistPlayerState;

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistPlayerCharacter : public ACharacter, public IHeistInteractable
{
	GENERATED_BODY()

#pragma region Construction

  public:
	AHeistPlayerCharacter();

#pragma endregion

#pragma region Lifecycle

  protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PawnClientRestart() override;
	virtual void UnPossessed() override;

#pragma endregion

#pragma region Movement

  public:
	void MoveOnGameplayPlane(const FVector2D& MovementInput);
	void RefreshMovementSpeedFromWeight();
	void SetSprintRequested(bool bRequested);
	bool IsSprinting() const;
	float CalculateMovementSpeedForPace(float TotalWeight, bool bSprintPace) const;

  protected:
	virtual void PossessedBy(AController* NewController) override;

  private:
	float CalculateMoveSpeedFromWeight(float InTotalWeight, bool bUseSprintPace) const;
	void ApplyCurrentMoveSpeed();
	void HandleInventoryStateForCrewStatus();
	void HandleForgeryStateForCrewStatus();
	void HandleAssemblyStateForCrewStatus();
	void HandleStatusStateForCrewStatus(const TArray<FHeistTimedTagState>& StatusTags);
	void RefreshAuthoritativeCrewStatus();

	UFUNCTION()
	void OnRep_CurrentMoveSpeed();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Movement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float WalkMoveSpeed = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Movement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float SprintMoveSpeed = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Movement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float WalkWeightSpeedPenalty = 7.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Movement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float SprintWeightSpeedPenalty = 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Movement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float MinimumWalkMoveSpeed = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Movement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float MinimumSprintMoveSpeed = 250.0f;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentMoveSpeed, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Movement", meta = (AllowPrivateAccess = "true"))
	float CurrentMoveSpeed = 300.0f;

	UPROPERTY(ReplicatedUsing = OnRep_Sprinting, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Movement", meta = (AllowPrivateAccess = "true"))
	bool bSprinting = false;

	bool bSprintRequested = false;

	UFUNCTION()
	void OnRep_Sprinting();

#pragma endregion

#pragma region EscapeState

  public:
	bool CanPerformGameplayActions() const;
	void ApplyPlayerStateGameplayRestrictions();
	void ApplyEscapedGameplayRestrictions();
	void HandleInventoryOpenStateChanged(bool bInventoryOpen);

	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|Presentation")
	void BP_ApplyCrewStatusPresentation(EHeistCrewStatus CrewStatus);

	void NotifyAuthoritativeCrewStatusFootstep(bool bSprintingPace);

	EHeistCrewStatus GetAppliedCrewStatusForDebug() const { return AppliedCrewStatusPresentation; }
	bool IsLocalStunPostProcessEnabledForDebug() const { return bLocalStunPostProcessEnabled; }
	bool IsStunSoundMixPushedForDebug() const { return bStunSoundMixPushed; }
	bool AreCrewStatusAudioAssetsAssignedForDebug() const;
	bool IsCrewStatusAudioPlayingForDebug() const;
	bool AreCrewStatusEffectComponentsReadyForDebug() const;
	bool IsCrewStatusEffectPresentationCleanForDebug() const;
	int32 GetCrewStatusFootstepPlayCountForDebug() const { return CrewStatusFootstepPlayCount; }

  protected:
	virtual void OnPlayerStateChanged(APlayerState* NewPlayerState, APlayerState* OldPlayerState) override;
	virtual void OnRep_Controller() override;
	virtual void OnRep_PlayerState() override;

#pragma endregion

#pragma region RescueInteraction

  public:
	virtual bool CanInteract(const AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;
	bool IsRescueInteractionAvailable() const;

  private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Rescue", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> RescueInteractionTarget;

#pragma endregion

#pragma region Replication

  public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

#pragma region Camera

  public:
	void SetFirstPersonFieldOfView(float NewFieldOfView);
	float GetFirstPersonFieldOfView() const;

  private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Camera", meta = (AllowPrivateAccess = "true"))
	FName FirstPersonCameraSocketName = TEXT("FirstPersonCameraSocket");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Camera", meta = (AllowPrivateAccess = "true"))
	FVector FirstPersonCameraSocketOffset = FVector::ZeroVector;

#pragma endregion

#pragma region GameplayComponents

  private:
	void RefreshNameplatePresentation();
	void HandleReplicatedCrewStatus(EHeistCrewStatus CrewStatus);
	void BindPresentationGameState();
	void HandlePresentationMatchPhaseChanged(EHeistMatchPhase PreviousMatchPhase, EHeistMatchPhase NewMatchPhase);
	void ApplyCrewStatusPresentation(EHeistCrewStatus CrewStatus);
	void ResetCrewStatusPresentation();
	void SetLocalStunPostProcessEnabled(bool bEnabled);
	UNiagaraSystem* ResolveCrewStatusVFX(EHeistCrewStatus CrewStatus) const;
	USoundBase* ResolveCrewStatusTransitionSound(EHeistCrewStatus CrewStatus) const;
	void ApplyCrewStatusVFX(EHeistCrewStatus CrewStatus, bool bForceRestart);
	void PlayCrewStatusTransitionSound(EHeistCrewStatus CrewStatus);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayCrewStatusFootstep(EHeistCrewStatus CrewStatus, bool bSprintingPace);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> NameplateWidgetComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Presentation", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class UHeistNameplateWidget> NameplateWidgetClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAudioComponent> CrewStatusFootstepAudioComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Presentation|Status Effects", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraComponent> CrewStatusVFXComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Presentation|Status Effects", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAudioComponent> CrewStatusTransitionAudioComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Presentation|Audio", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundMix> StunSoundMix;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Presentation|Audio", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> CarryingOriginalFootstepSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Presentation|Audio", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> HeavyFootstepSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Presentation|Status Effects|VFX", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> ForgingStatusVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Presentation|Status Effects|VFX", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> AssemblingStatusVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Presentation|Status Effects|VFX", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> CarryingOriginalStatusVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Presentation|Status Effects|VFX", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> HeavyStatusVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Presentation|Status Effects|VFX", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> StunnedStatusVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Presentation|Status Effects|VFX", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> ArrestedStatusVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Presentation|Status Effects|VFX",
		meta = (AllowPrivateAccess = "true", ToolTip = "One-shot Niagara System spawned at the character location before the escaped character is hidden. Use a non-looping system."))
	TObjectPtr<UNiagaraSystem> EscapedStatusBurstVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Presentation|Status Effects|Audio", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> ForgingStatusSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Presentation|Status Effects|Audio", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> AssemblingStatusSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Presentation|Status Effects|Audio", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> CarryingOriginalStatusSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Presentation|Status Effects|Audio", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> HeavyStatusSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Presentation|Status Effects|Audio", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> StunnedStatusSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Presentation|Status Effects|Audio", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> ArrestedStatusSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Presentation|Status Effects|Audio", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> EscapedStatusSound;

	TWeakObjectPtr<AHeistGameState> BoundPresentationGameState;
	TWeakObjectPtr<AHeistPlayerState> BoundPresentationPlayerState;
	EHeistCrewStatus AppliedCrewStatusPresentation = EHeistCrewStatus::Active;
	bool bCrewStatusPresentationInitialized = false;
	bool bLocalStunPostProcessEnabled = false;
	bool bStunSoundMixPushed = false;
	bool bStunPostProcessSnapshotValid = false;
	bool bSavedOverrideColorSaturation = false;
	bool bSavedOverrideVignetteIntensity = false;
	FVector4 SavedColorSaturation = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	float SavedVignetteIntensity = 0.0f;
	float SavedPostProcessBlendWeight = 0.0f;
	int32 CrewStatusFootstepPlayCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistStatusComponent> StatusComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistInventoryComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistActionComponent> ActionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistForgeryComponent> ForgeryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistObjectAssemblyComponent> ObjectAssemblyComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistVisionComponent> VisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistNoiseEmitterComponent> NoiseEmitterComponent;

  public:
	UHeistStatusComponent* GetStatusComponent() const;
	UHeistInventoryComponent* GetInventoryComponent() const;
	UHeistInteractionComponent* GetInteractionComponent() const;
	UHeistActionComponent* GetActionComponent() const;
	UHeistForgeryComponent* GetForgeryComponent() const;
	UHeistObjectAssemblyComponent* GetObjectAssemblyComponent() const;
	UHeistVisionComponent* GetVisionComponent() const;
	UHeistNoiseEmitterComponent* GetNoiseEmitterComponent() const;

#pragma endregion
};
