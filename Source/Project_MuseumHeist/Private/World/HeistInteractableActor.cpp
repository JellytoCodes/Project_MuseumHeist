#include "World/HeistInteractableActor.h"

AHeistInteractableActor::AHeistInteractableActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

bool AHeistInteractableActor::CanInteract(const AActor* Interactor) const
{
	return false;
}

void AHeistInteractableActor::Interact(AActor* Interactor)
{
}
