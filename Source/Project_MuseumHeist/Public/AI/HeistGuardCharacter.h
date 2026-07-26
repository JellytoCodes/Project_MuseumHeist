#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/HeistTypes.h"
#include "Inventory/HeistItemDataTypes.h"

#include "HeistGuardCharacter.generated.h"

class UHeistGuardNoiseReactionComponent;
class UHeistGuardStateComponent;
class UHeistPatrolPathComponent;

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistGuardCharacter : public ACharacter
{
	GENERATED_BODY()

#pragma region Construction

  public:
	AHeistGuardCharacter();

#pragma endregion

#pragma region Lifecycle

  protected:
	virtual void BeginPlay() override;

  public:
	virtual void GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const override;

#pragma endregion

#pragma region GameplayComponents

  public:
	UHeistGuardStateComponent* GetGuardStateComponent() const;
	UHeistPatrolPathComponent* GetPatrolPathComponent() const;
	UHeistGuardNoiseReactionComponent* GetNoiseReactionComponent() const;

  private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Guard", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistGuardStateComponent> GuardStateComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Guard", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistPatrolPathComponent> PatrolPathComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Guard", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistGuardNoiseReactionComponent> NoiseReactionComponent;

#pragma endregion

#pragma region GuardProfile

  public:
	FName GetGuardProfileId() const;
	bool HasResolvedGuardProfile() const;
	const FHeistGuardDataRow& GetGuardProfile() const;
	void SetAlertPatrolSpeedMultiplier(float Multiplier);
	float GetAlertPatrolSpeedMultiplier() const;
	float GetEffectivePatrolSpeed() const;

  private:
	void ResolveGuardProfile();
	void HandleGuardStateChanged(EHeistGuardState PreviousState, EHeistGuardState NewState);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Guard", meta = (AllowPrivateAccess = "true"))
	FName GuardProfileId = FName(TEXT("Guard_Alert_Medium"));

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, AdvancedDisplay, Category = "Heist|Guard", meta = (AllowPrivateAccess = "true"))
	FHeistGuardDataRow GuardProfile;

	bool bHasResolvedGuardProfile = false;
	float AlertPatrolSpeedMultiplier = 1.0f;

#pragma endregion
};
