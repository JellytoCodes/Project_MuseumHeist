#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "HeistGameMode.generated.h"

class UHeistGameBalanceDataAsset;
class UDataTable;
class AHeistLootActor;
struct FHeistItemDataRow;
struct FHeistLootDataRow;
struct FHeistLootDropRequest;

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistGameMode : public AGameModeBase
{
	GENERATED_BODY()

#pragma region Construction

public:
	AHeistGameMode();

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void StartPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;

#pragma endregion

#pragma region Balance

public:
	UDataTable* GetItemDataTable() const;
	bool TryGetItemDefinition(FName ItemId, FHeistItemDataRow& OutItemDefinition) const;
	bool TryGetLootDefinition(FName ItemId, FHeistLootDataRow& OutLootDefinition) const;
	bool TrySpawnDroppedLoot(const FHeistLootDropRequest& DropRequest, AHeistLootActor*& OutDroppedLootActor) const;

private:
	void ValidateItemDataTable() const;

#pragma endregion

#pragma region EscapePhase

public:
	float GetEscapeCastTimeSeconds() const;

private:
	void StartEscapePhaseTimer();
	void HandleEscapePhaseTimerElapsed();
	float ResolveEscapePhaseDelaySeconds() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Balance", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistGameBalanceDataAsset> GameBalanceDataAsset;

	FTimerHandle EscapePhaseTimerHandle;

#pragma endregion

#pragma region RuntimeState

private:
	int32 NextHeistPlayerId = 1;

#pragma endregion
};
