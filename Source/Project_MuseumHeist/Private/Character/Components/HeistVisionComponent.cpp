#include "Character/Components/HeistVisionComponent.h"

#include "Net/UnrealNetwork.h"

UHeistVisionComponent::UHeistVisionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UHeistVisionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHeistVisionComponent, FlashlightAimDirection);
}

#pragma region Flashlight

void UHeistVisionComponent::UpdateFlashlightAimDirection(const FVector& InWorldDirection)
{
	const FVector NewAimDirection = InWorldDirection.GetSafeNormal();
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

void UHeistVisionComponent::OnRep_FlashlightAimDirection()
{
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
