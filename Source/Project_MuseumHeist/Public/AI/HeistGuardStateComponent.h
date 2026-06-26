#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/HeistTypes.h"

#include "HeistGuardStateComponent.generated.h"

UCLASS(ClassGroup = (Heist), meta = (BlueprintSpawnableComponent))
class PROJECT_MUSEUMHEIST_API UHeistGuardStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeistGuardStateComponent();

	bool ApplyStun(float DurationSeconds);
	EHeistGuardState GetGuardState() const;

private:
	void ClearStun();

	UFUNCTION()
	void OnRep_GuardState();

	UPROPERTY(ReplicatedUsing = OnRep_GuardState, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|AI", meta = (AllowPrivateAccess = "true"))
	EHeistGuardState GuardState = EHeistGuardState::Patrol;

	FTimerHandle StunTimerHandle;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
