#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "HeistInteractable.generated.h"

class AActor;

UINTERFACE()
class PROJECT_MUSEUMHEIST_API UHeistInteractable : public UInterface
{
	GENERATED_BODY()
};

class PROJECT_MUSEUMHEIST_API IHeistInteractable
{
	GENERATED_BODY()

public:
	virtual bool CanInteract(const AActor* Interactor) const = 0;
	virtual void Interact(AActor* Interactor) = 0;
};
