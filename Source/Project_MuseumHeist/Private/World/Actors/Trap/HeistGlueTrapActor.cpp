#include "World/Actors/Trap/HeistGlueTrapActor.h"

#include "AI/HeistGuardCharacter.h"
#include "AI/HeistGuardStateComponent.h"
#include "Debug/HeistDebugFunctionLibrary.h"

#pragma region Construction

AHeistGlueTrapActor::AHeistGlueTrapActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

#pragma endregion

#pragma region Trap

bool AHeistGlueTrapActor::HandleAuthorityTrigger(AActor* TriggeringActor)
{
	if (!HasAuthority() || !IsValid(TriggeringActor))
	{
		return false;
	}

	if (AHeistGuardCharacter* TriggeringGuard = Cast<AHeistGuardCharacter>(TriggeringActor))
	{
		UHeistGuardStateComponent* GuardStateComponent = TriggeringGuard->GetGuardStateComponent();
		if (IsValid(GuardStateComponent) && GuardStateComponent->ApplyStun(GetEffectDurationSeconds()))
		{
			UHeistDebugFunctionLibrary::DebugTrapTriggered(this, this, TriggeringGuard, GetSourceItemId(), GetEffectDurationSeconds());
			return true;
		}

		UHeistDebugFunctionLibrary::DebugTrapTriggerRejected(this, this, TriggeringGuard, TEXT("GuardStateRejected"));
		return false;
	}

	UHeistDebugFunctionLibrary::DebugTrapTriggerRejected(this, this, TriggeringActor, TEXT("UnsupportedActor"));
	return false;
}

#pragma endregion
