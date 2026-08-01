#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "World/Interaction/HeistInteractable.h"

#include "HeistPlayerCharacter.generated.h"

class UHeistActionComponent;
class UHeistCustomizationComponent;
class UHeistForgeryComponent;
class UHeistInteractionComponent;
class UHeistInventoryComponent;
class UHeistNoiseEmitterComponent;
class UHeistObjectAssemblyComponent;
class UHeistStatusComponent;
class UHeistTagComponent;
class UHeistVisionComponent;
class UCameraComponent;
class USphereComponent;

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

  protected:
	virtual void PossessedBy(AController* NewController) override;

  private:
	float CalculateMoveSpeedFromWeight(float InTotalWeight) const;
	void ApplyCurrentMoveSpeed();

	UFUNCTION()
	void OnRep_CurrentMoveSpeed();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Movement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float BaseMoveSpeed = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Movement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float WeightSpeedPenalty = 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Movement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float MinimumMoveSpeed = 200.0f;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentMoveSpeed, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Movement", meta = (AllowPrivateAccess = "true"))
	float CurrentMoveSpeed = 600.0f;

#pragma endregion

#pragma region EscapeState

  public:
	bool CanPerformGameplayActions() const;
	void ApplyPlayerStateGameplayRestrictions();
	void ApplyEscapedGameplayRestrictions();
	void HandleInventoryOpenStateChanged(bool bInventoryOpen);

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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistTagComponent> TagComponent;

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
	TObjectPtr<UHeistCustomizationComponent> CustomizationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistNoiseEmitterComponent> NoiseEmitterComponent;

  public:
	UHeistTagComponent* GetTagComponent() const;
	UHeistStatusComponent* GetStatusComponent() const;
	UHeistInventoryComponent* GetInventoryComponent() const;
	UHeistInteractionComponent* GetInteractionComponent() const;
	UHeistActionComponent* GetActionComponent() const;
	UHeistForgeryComponent* GetForgeryComponent() const;
	UHeistObjectAssemblyComponent* GetObjectAssemblyComponent() const;
	UHeistVisionComponent* GetVisionComponent() const;
	UHeistCustomizationComponent* GetCustomizationComponent() const;
	UHeistNoiseEmitterComponent* GetNoiseEmitterComponent() const;

#pragma endregion
};
