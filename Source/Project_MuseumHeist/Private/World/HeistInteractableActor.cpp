#include "World/HeistInteractableActor.h"

#include "Components/SphereComponent.h"

#pragma region Construction

AHeistInteractableActor::AHeistInteractableActor()
{
	PrimaryActorTick.bCanEverTick = false;

	InteractionCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionCollision"));
	SetRootComponent(InteractionCollision);
	InteractionCollision->InitSphereRadius(50.0f);
	InteractionCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionCollision->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionCollision->SetCollisionResponseToAllChannels(ECR_Overlap);
	InteractionCollision->SetGenerateOverlapEvents(false);
}

#pragma endregion

#pragma region Interaction

bool AHeistInteractableActor::CanInteract(const AActor* Interactor) const
{
	return IsValid(Interactor);
}

void AHeistInteractableActor::Interact(AActor* Interactor)
{
}

#pragma endregion
