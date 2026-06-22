#include "Character/Components/HeistStatusComponent.h"

UHeistStatusComponent::UHeistStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UHeistStatusComponent::IsStunned() const
{
	return bStunned;
}
