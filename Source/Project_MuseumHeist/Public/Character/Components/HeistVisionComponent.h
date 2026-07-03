#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "HeistVisionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FHeistFlashlightAimDirectionChanged,
	FVector,
	AimDirection,
	float,
	AimYawDegrees);

UCLASS(ClassGroup = (Heist), meta = (BlueprintSpawnableComponent))
class PROJECT_MUSEUMHEIST_API UHeistVisionComponent : public UActorComponent
{
	GENERATED_BODY()

#pragma region Construction

public:
	UHeistVisionComponent();

#pragma endregion

#pragma region Flashlight

public:
	void UpdateFlashlightAimDirection(const FVector& InWorldDirection);

	UFUNCTION(BlueprintPure, Category = "Heist|Vision")
	FVector GetFlashlightAimDirection() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Vision")
	float GetFlashlightAimYawDegrees() const;

	UPROPERTY(BlueprintAssignable, Category = "Heist|Vision")
	FHeistFlashlightAimDirectionChanged FlashlightAimDirectionChanged;

private:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Vision", meta = (AllowPrivateAccess = "true"))
	FVector FlashlightAimDirection = FVector::ForwardVector;

#pragma endregion
};
