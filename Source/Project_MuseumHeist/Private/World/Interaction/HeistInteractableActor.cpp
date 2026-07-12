#include "World/Interaction/HeistInteractableActor.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HeistCollisionChannels.h"

#pragma region Construction

AHeistInteractableActor::AHeistInteractableActor()
{
	PrimaryActorTick.bCanEverTick = false;

	InteractionCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionCollision"));
	SetRootComponent(InteractionCollision);
	InteractionCollision->InitSphereRadius(50.0f);
	InteractionCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionCollision->SetGenerateOverlapEvents(false);

	VisualMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMeshComponent"));
	VisualMeshComponent->SetupAttachment(InteractionCollision);
	VisualMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMeshComponent->SetGenerateOverlapEvents(false);
}

#pragma endregion

#pragma region Lifecycle

void AHeistInteractableActor::BeginPlay()
{
	Super::BeginPlay();

	checkf(IsValid(InteractionCollision), TEXT("HeistInteractableActor requires InteractionCollision."));
	InteractionCollision->SetCollisionObjectType(HeistCollisionChannels::Interactable);
	InteractionCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionCollision->SetCollisionResponseToChannel(HeistCollisionChannels::Player, ECR_Overlap);
	InteractionCollision->SetCollisionResponseToChannel(HeistCollisionChannels::InteractionTrace, ECR_Block);
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
