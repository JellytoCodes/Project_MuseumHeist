#include "Character/Components/HeistVisionComponent.h"

UHeistVisionComponent::UHeistVisionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

#pragma region Flashlight

void UHeistVisionComponent::UpdateFlashlightAimDirection(const FVector& InWorldDirection)
{
	const FVector NewAimDirection =
		FVector(InWorldDirection.X, InWorldDirection.Y, 0.0f).GetSafeNormal();
	if (NewAimDirection.IsNearlyZero()
		|| FlashlightAimDirection.Equals(NewAimDirection, 0.001f))
	{
		return;
	}

	FlashlightAimDirection = NewAimDirection;
	FlashlightAimDirectionChanged.Broadcast(
		FlashlightAimDirection,
		GetFlashlightAimYawDegrees());
}

FVector UHeistVisionComponent::GetFlashlightAimDirection() const
{
	return FlashlightAimDirection;
}

float UHeistVisionComponent::GetFlashlightAimYawDegrees() const
{
	return FlashlightAimDirection.Rotation().Yaw;
}

#pragma endregion
