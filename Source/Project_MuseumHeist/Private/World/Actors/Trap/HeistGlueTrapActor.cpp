#include "World/Actors/Trap/HeistGlueTrapActor.h"

#include "AI/HeistGuardCharacter.h"
#include "AI/HeistGuardStateComponent.h"
#include "Character/Components/HeistStatusComponent.h"
#include "Character/HeistPlayerCharacter.h"
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

	if (AHeistPlayerCharacter* TriggeringCharacter = Cast<AHeistPlayerCharacter>(TriggeringActor))
	{
		UHeistStatusComponent* StatusComponent = TriggeringCharacter->GetStatusComponent();
		checkf(IsValid(StatusComponent), TEXT("HeistPlayerCharacter requires HeistStatusComponent"));
		if (StatusComponent->ApplyStun(GetEffectDurationSeconds(), this))
		{
			UHeistDebugFunctionLibrary::DebugTrapTriggered(
				this,
				this,
				TriggeringCharacter,
				GetSourceItemId(),
				GetEffectDurationSeconds());
			return true;
		}

		UHeistDebugFunctionLibrary::DebugTrapTriggerRejected(this, this, TriggeringCharacter, TEXT("StatusRejected"));
		return false;
	}

	if (AHeistGuardCharacter* TriggeringGuard = Cast<AHeistGuardCharacter>(TriggeringActor))
	{
		UHeistGuardStateComponent* GuardStateComponent = TriggeringGuard->GetGuardStateComponent();
		if (IsValid(GuardStateComponent) && GuardStateComponent->ApplyStun(GetEffectDurationSeconds()))
		{
			UHeistDebugFunctionLibrary::DebugTrapTriggered(
				this,
				this,
				TriggeringGuard,
				GetSourceItemId(),
				GetEffectDurationSeconds());
			return true;
		}

		UHeistDebugFunctionLibrary::DebugTrapTriggerRejected(this, this, TriggeringGuard, TEXT("GuardStateRejected"));
		return false;
	}

	UHeistDebugFunctionLibrary::DebugTrapTriggerRejected(this, this, TriggeringActor, TEXT("UnsupportedActor"));
	return false;
}

#pragma endregion
