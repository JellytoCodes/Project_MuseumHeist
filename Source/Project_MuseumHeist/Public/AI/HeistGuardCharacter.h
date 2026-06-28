#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/HeistTypes.h"
#include "Inventory/HeistItemDataTypes.h"

#include "HeistGuardCharacter.generated.h"

class UHeistGuardNoiseReactionComponent;
class UHeistGuardStateComponent;
class UHeistPatrolPathComponent;

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistGuardCharacter : public ACharacter
{
	GENERATED_BODY()

#pragma region Construction

public:
	AHeistGuardCharacter();

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void BeginPlay() override;

#pragma endregion

#pragma region GameplayComponents

public:
	UHeistGuardStateComponent* GetGuardStateComponent() const;
	UHeistPatrolPathComponent* GetPatrolPathComponent() const;
	UHeistGuardNoiseReactionComponent* GetNoiseReactionComponent() const;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|AI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistGuardStateComponent> GuardStateComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|AI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistPatrolPathComponent> PatrolPathComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|AI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistGuardNoiseReactionComponent> NoiseReactionComponent;

#pragma endregion

#pragma region GuardProfile

public:
	FName GetGuardProfileId() const;
	bool HasResolvedGuardProfile() const;
	const FHeistGuardDataRow& GetGuardProfile() const;

private:
	void ResolveGuardProfile();
	void HandleGuardStateChanged(EHeistGuardState PreviousState, EHeistGuardState NewState);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|AI|Data", meta = (AllowPrivateAccess = "true"))
	FName GuardProfileId = FName(TEXT("Guard_Default"));

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|AI|Data", meta = (AllowPrivateAccess = "true"))
	FHeistGuardDataRow GuardProfile;

	bool bHasResolvedGuardProfile = false;

#pragma endregion
};
