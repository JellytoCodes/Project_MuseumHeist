#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/HeistInteractable.h"

#include "HeistInteractableActor.generated.h"

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistInteractableActor : public AActor, public IHeistInteractable
{
	GENERATED_BODY()

public:
	AHeistInteractableActor();

	virtual bool CanInteract(const AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;
};
