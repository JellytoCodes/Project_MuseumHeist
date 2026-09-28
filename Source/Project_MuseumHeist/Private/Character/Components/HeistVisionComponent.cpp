#include "Character/Components/HeistVisionComponent.h"

#include "Net/UnrealNetwork.h"
#include "Components/SpotLightComponent.h"
#include "GameFramework/Actor.h"

UHeistVisionComponent::UHeistVisionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UHeistVisionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UHeistVisionComponent, FlashlightAimDirection, COND_SkipOwner);
	DOREPLIFETIME(UHeistVisionComponent, bFlashlightEnabled);
}

void UHeistVisionComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshFlashlightPresentation();
}

#pragma region Flashlight

void UHeistVisionComponent::UpdateFlashlightAimDirection(const FVector& InWorldDirection)
{
	if (InWorldDirection.ContainsNaN()) return;
	const FVector NewAimDirection = InWorldDirection.GetSafeNormal();
	if (NewAimDirection.IsNearlyZero() || FlashlightAimDirection.Equals(NewAimDirection, 0.001f))
	{
		return;
	}

	FlashlightAimDirection = NewAimDirection;
	RefreshFlashlightPresentation();
	FlashlightAimDirectionChanged.Broadcast(FlashlightAimDirection, GetFlashlightAimYawDegrees());
}

void UHeistVisionComponent::OnRep_FlashlightAimDirection()
{
	RefreshFlashlightPresentation();
	FlashlightAimDirectionChanged.Broadcast(FlashlightAimDirection, GetFlashlightAimYawDegrees());
}

void UHeistVisionComponent::SetFlashlightEnabled(const bool bEnabled)
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority() || bFlashlightEnabled == bEnabled) return;
	bFlashlightEnabled = bEnabled;
	OnRep_FlashlightEnabled();
	GetOwner()->ForceNetUpdate();
}

void UHeistVisionComponent::OnRep_FlashlightEnabled()
{
	RefreshFlashlightPresentation();
	FlashlightStateChanged.Broadcast();
}

void UHeistVisionComponent::RefreshFlashlightPresentation()
{
	if (!IsValid(Flashlight) && IsValid(GetOwner()))
	{
		TInlineComponentArray<USpotLightComponent*> Lights(GetOwner());
		for (USpotLightComponent* Light : Lights)
		{
			if (Light->ComponentHasTag(TEXT("Flashlight")))
			{
				Flashlight = Light;
				break;
			}
		}
	}
	if (IsValid(Flashlight))
	{
		// Remote cameras do not consume the local controller's pitch.
		Flashlight->SetAbsolute(false, true, false);
		Flashlight->SetWorldRotation(FlashlightAimDirection.Rotation());
		Flashlight->SetVisibility(bFlashlightEnabled);
	}
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
