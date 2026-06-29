#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "HeistGameMode.generated.h"

class UHeistGameBalanceDataAsset;
class UDataTable;
class AHeistLootActor;
class AHeistLootSpawnPoint;
class AHeistPlayerState;
struct FHeistItemDataRow;
struct FHeistLootDataRow;
struct FHeistUsableItemDataRow;
struct FHeistGuardDataRow;
struct FHeistSoundPingDataRow;
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
	bool TryGetUsableItemDefinition(FName ItemId, FHeistUsableItemDataRow& OutUsableItemDefinition) const;
	bool TryGetGuardDefinition(FName GuardProfileId, FHeistGuardDataRow& OutGuardDefinition) const;
	bool TryGetSoundPingDefinition(FName SoundPingId, FHeistSoundPingDataRow& OutSoundPingDefinition) const;
	bool TrySpawnDroppedLoot(const FHeistLootDropRequest& DropRequest, AHeistLootActor*& OutDroppedLootActor) const;

private:
	void ValidateItemDataTables() const;

#pragma endregion

#pragma region RareLootEvent

public:
	void ForceRareLootEvent(float WarningDelaySeconds = 5.0f);

private:
	void StartRareLootEventTimers();
	void BeginRareLootWarning(int32 EventIndex, float ScheduledSpawnTime);
	void TriggerRareLootEvent(int32 EventIndex);
	bool TrySpawnRareLoot(int32 EventIndex, AHeistLootActor*& OutRareLootActor, AHeistLootSpawnPoint*& OutSpawnPoint);
	void HandleRareLootPickedUp(AHeistLootActor* LootActor, AActor* Requester);
	const UHeistGameBalanceDataAsset* ResolveGameBalanceData() const;

	TArray<FTimerHandle> RareLootWarningTimerHandles;
	TArray<FTimerHandle> RareLootSpawnTimerHandles;
	TSet<int32> TriggeredRareLootEventIndices;
	TMap<TWeakObjectPtr<AHeistLootActor>, int32> ActiveRareLootEventIndices;
	int32 NextForcedRareLootEventIndex = 1;

#pragma endregion

#pragma region GapTracker

public:
	void RefreshGapTrackerState();
	void DebugForceGapTracker(bool bActive);
	void DebugClearGapTrackerOverride();

private:
	void StartGapTrackerTimer();
	void UpdateGapTrackerDirections(AHeistPlayerState* LeaderPlayerState, const TArray<AHeistPlayerState*>& RankedPlayerStates);

	FTimerHandle GapTrackerUpdateTimerHandle;
	bool bGapTrackerDebugOverride = false;
	bool bGapTrackerDebugForcedActive = false;

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
