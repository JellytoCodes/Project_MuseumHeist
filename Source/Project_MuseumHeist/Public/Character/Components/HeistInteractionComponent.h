#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "HeistInteractionComponent.generated.h"

class AActor;
class AHeistPlayerCharacter;

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
	bool RefreshInteractionTarget(bool bForceRefresh = false);
	AActor* GetCurrentInteractionTarget() const;
	bool HasValidInteractionTarget() const;
	float GetInteractionRange() const;
	bool IsActorWithinInteractionRange(const AActor* TargetActor) const;
	FHeistInteractionTargetChanged& GetInteractionTargetChangedDelegate();

private:
	bool CanOwnerInteract() const;
	bool ResolveCenterScreenTrace(FVector& OutTraceStart, FVector& OutTraceEnd) const;
	void ClearInteractionTarget(const TCHAR* Reason);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Interaction", meta = (AllowPrivateAccess = "true"))
	float InteractionRange = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Interaction", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", Units = "s"))
	float InteractionScanInterval = 0.05f;

	UPROPERTY(Transient)
	TObjectPtr<AHeistPlayerCharacter> OwnerCharacter;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CurrentInteractionTarget;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CurrentTraceHitActor;

	bool bCurrentTargetAvailable = false;
	float LastInteractionTraceTime = -BIG_NUMBER;

	FHeistInteractionTargetChanged InteractionTargetChangedDelegate;

#pragma endregion
};
