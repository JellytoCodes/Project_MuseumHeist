#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "HeistInteractionComponent.generated.h"

class AActor;
class AHeistPlayerCharacter;
class UPrimitiveComponent;
struct FHitResult;

DECLARE_MULTICAST_DELEGATE_TwoParams(FHeistInteractionTargetChanged, AActor*, bool);

UCLASS(ClassGroup = (Heist), meta = (BlueprintSpawnableComponent))
class PROJECT_MUSEUMHEIST_API UHeistInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

#pragma region Construction

  public:
	UHeistInteractionComponent();

#pragma endregion

#pragma region Lifecycle

  protected:
	virtual void BeginPlay() override;

#pragma endregion

#pragma region Interaction

  public:
	bool RefreshInteractionTarget();
	AActor* GetCurrentInteractionTarget() const;
	bool HasValidInteractionTarget() const;
	bool IsActorOverlappingInteractionArea(const AActor* TargetActor) const;
	FHeistInteractionTargetChanged& GetInteractionTargetChangedDelegate();

  private:
	bool CanOwnerInteract() const;
	bool IsInteractionCollisionComponent(const UPrimitiveComponent* Component, const AActor* ExpectedOwner) const;
	void ClearInteractionTarget(const TCHAR* Reason);

	UFUNCTION()
	void HandleInteractionOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex,
								   bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleInteractionOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);

	UPROPERTY(Transient)
	TObjectPtr<AHeistPlayerCharacter> OwnerCharacter;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CurrentInteractionTarget;

	TSet<TWeakObjectPtr<AActor>> OverlappingInteractionActors;

	bool bCurrentTargetAvailable = false;

	FHeistInteractionTargetChanged InteractionTargetChangedDelegate;

#pragma endregion
};
