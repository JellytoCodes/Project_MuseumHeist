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
class USphereComponent;
class UWidgetComponent;

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

  protected:
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> NameplateWidgetComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Presentation", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class UHeistNameplateWidget> NameplateWidgetClass;

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
