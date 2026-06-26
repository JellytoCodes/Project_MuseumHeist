#pragma once

#include "CoreMinimal.h"
#include "World/Interaction/HeistInteractableActor.h"

#include "HeistVentActor.generated.h"

class AHeistGameState;
class AHeistPlayerCharacter;

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistVentActor : public AHeistInteractableActor
{
	GENERATED_BODY()

#pragma region Construction

public:
	AHeistVentActor();

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#pragma endregion

#pragma region VentState

public:
	bool IsVentActive() const;
	void RefreshVentActiveState();

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Vent", meta = (AllowPrivateAccess = "true"))
	bool bRequiresEscapePhase = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Vent", meta = (AllowPrivateAccess = "true"))
	bool bVentManuallyEnabled = true;

	UPROPERTY(ReplicatedUsing = OnRep_VentActive, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Vent", meta = (AllowPrivateAccess = "true"))
	bool bVentActive = false;

	UFUNCTION()
	void OnRep_VentActive();

#pragma endregion

#pragma region Interaction

public:
	virtual bool CanInteract(const AActor* Interactor) const override;
	bool CanUseVent(const AHeistPlayerCharacter* RequestingCharacter) const;

#pragma endregion

#pragma region Replication

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

#pragma region InternalHelpers

private:
	void BindToEscapePhaseState();
	void HandleEscapePhaseStateChanged(bool bIsEscapePhaseOpen);

	TWeakObjectPtr<AHeistGameState> BoundGameState;
	FDelegateHandle EscapePhaseStateChangedHandle;

#pragma endregion
};
