#include "AI/HeistGuardCharacter.h"

#include "AI/HeistGuardNoiseReactionComponent.h"
#include "AI/HeistGuardStateComponent.h"
#include "AI/HeistPatrolPathComponent.h"

AHeistGuardCharacter::AHeistGuardCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	GuardStateComponent = CreateDefaultSubobject<UHeistGuardStateComponent>(TEXT("GuardStateComponent"));
	PatrolPathComponent = CreateDefaultSubobject<UHeistPatrolPathComponent>(TEXT("PatrolPathComponent"));
	NoiseReactionComponent = CreateDefaultSubobject<UHeistGuardNoiseReactionComponent>(TEXT("NoiseReactionComponent"));
}

UHeistGuardStateComponent* AHeistGuardCharacter::GetGuardStateComponent() const
{
	return GuardStateComponent.Get();
}
