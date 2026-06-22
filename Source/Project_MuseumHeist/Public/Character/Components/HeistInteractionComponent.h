#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "HeistInteractionComponent.generated.h"

class AActor;

UCLASS(ClassGroup = (Heist), meta = (BlueprintSpawnableComponent))
class PROJECT_MUSEUMHEIST_API UHeistInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

#pragma region Construction

public:
	UHeistInteractionComponent();

#pragma endregion

#pragma region Interaction

public:
	bool RefreshInteractionTarget();
	AActor* GetCurrentInteractionTarget() const;
	bool HasValidInteractionTarget() const;
	float GetInteractionRange() const;
	bool IsActorWithinInteractionRange(const AActor* TargetActor) const;

private:
	bool CanOwnerInteract() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Interaction", meta = (AllowPrivateAccess = "true"))
	float InteractionRange = 300.0f;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CurrentInteractionTarget;

#pragma endregion
};
