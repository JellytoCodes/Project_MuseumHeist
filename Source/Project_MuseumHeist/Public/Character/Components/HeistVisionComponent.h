#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "HeistVisionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHeistFlashlightAimDirectionChanged, FVector, AimDirection, float, AimYawDegrees);
DECLARE_MULTICAST_DELEGATE(FHeistFlashlightStateChanged);

UCLASS(ClassGroup = (Heist), meta = (BlueprintSpawnableComponent))
class PROJECT_MUSEUMHEIST_API UHeistVisionComponent : public UActorComponent
{
	GENERATED_BODY()

#pragma region Construction

  public:
	UHeistVisionComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

#pragma endregion

#pragma region Flashlight

  public:
	void UpdateFlashlightAimDirection(const FVector& InWorldDirection);
	void SetFlashlightEnabled(bool bEnabled);
	void RefreshFlashlightPresentation();
	FHeistFlashlightStateChanged& GetFlashlightStateChangedDelegate() { return FlashlightStateChanged; }

	UFUNCTION(BlueprintPure, Category = "Heist|Vision")
	bool IsFlashlightEnabled() const { return bFlashlightEnabled; }

	UFUNCTION(BlueprintPure, Category = "Heist|Vision")
	FVector GetFlashlightAimDirection() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Vision")
	float GetFlashlightAimYawDegrees() const;

	UPROPERTY(BlueprintAssignable, Category = "Heist|Vision")
	FHeistFlashlightAimDirectionChanged FlashlightAimDirectionChanged;

  private:
	UFUNCTION()
	void OnRep_FlashlightAimDirection();

	UFUNCTION()
	void OnRep_FlashlightEnabled();
	friend class FHeistFlashlightLifecycleTest;

	UPROPERTY(ReplicatedUsing = OnRep_FlashlightEnabled)
	bool bFlashlightEnabled = false;

	UPROPERTY(Transient)
	TObjectPtr<class USpotLightComponent> Flashlight;

	FHeistFlashlightStateChanged FlashlightStateChanged;

	UPROPERTY(ReplicatedUsing = OnRep_FlashlightAimDirection, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Vision", meta = (AllowPrivateAccess = "true"))
	FVector FlashlightAimDirection = FVector::ForwardVector;

#pragma endregion
};
