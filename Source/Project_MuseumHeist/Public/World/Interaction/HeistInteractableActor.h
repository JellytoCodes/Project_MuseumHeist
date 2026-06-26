#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/Interaction/HeistInteractable.h"

#include "HeistInteractableActor.generated.h"

class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistInteractableActor : public AActor, public IHeistInteractable
{
	GENERATED_BODY()

#pragma region Construction

public:
	AHeistInteractableActor();

#pragma endregion

#pragma region Components

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> InteractionCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> VisualMeshComponent;

#pragma endregion

#pragma region Interaction

public:
	virtual bool CanInteract(const AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;

#pragma endregion
};
