#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "World/Interaction/HeistInteractableActor.h"

#include "HeistDisplayCaseActor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FHeistDisplayCaseStateChangedSignature,
	EHeistDisplayCaseState,
	PreviousState,
	EHeistDisplayCaseState,
	NewState);

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistDisplayCaseActor : public AHeistInteractableActor
{
	GENERATED_BODY()

public:
	AHeistDisplayCaseActor();

#pragma region StateMachine

public:
	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase")
	EHeistDisplayCaseState GetDisplayCaseState() const;

	UFUNCTION(BlueprintPure, Category = "Heist|DisplayCase")
	bool CanTransitionToDisplayCaseState(EHeistDisplayCaseState NewState) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Heist|DisplayCase")
	bool TryTransitionToDisplayCaseState(EHeistDisplayCaseState NewState);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Heist|DisplayCase")
	bool TryAdvanceDisplayCaseState();

	UPROPERTY(BlueprintAssignable, Category = "Heist|DisplayCase")
	FHeistDisplayCaseStateChangedSignature OnDisplayCaseStateChanged;

private:
	UPROPERTY(ReplicatedUsing = OnRep_DisplayCaseState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|DisplayCase", meta = (AllowPrivateAccess = "true"))
	EHeistDisplayCaseState DisplayCaseState = EHeistDisplayCaseState::Secured;

	UFUNCTION()
	void OnRep_DisplayCaseState(EHeistDisplayCaseState PreviousState);

	static bool TryGetNextDisplayCaseState(EHeistDisplayCaseState CurrentState, EHeistDisplayCaseState& OutNextState);

	void HandleDisplayCaseStateChanged(EHeistDisplayCaseState PreviousState);

#pragma endregion

#pragma region Replication

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion
};
