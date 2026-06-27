#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "GameFramework/GameStateBase.h"

#include "HeistGameState.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FHeistEscapePhaseStateChanged, bool);
DECLARE_MULTICAST_DELEGATE(FHeistPlayerResultsChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FHeistSoundPingEventReported, const FHeistSoundPingEvent&);
DECLARE_MULTICAST_DELEGATE_OneParam(FHeistRareLootEventStateChanged, const FHeistRareLootEventState&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FHeistGapTrackerStateChanged, bool, int32);

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistGameState : public AGameStateBase
{
	GENERATED_BODY()

#pragma region Construction

public:
	AHeistGameState();


#pragma endregion

#pragma region PlayerConnections

public:
	int32 GetConnectedPlayerCount() const;

#pragma endregion

#pragma region EscapePhase

public:
	bool IsEscapePhaseOpen() const;
	float GetEscapePhaseDelaySeconds() const;
	float GetEscapePhaseOpenTimeSeconds() const;
	void InitializeEscapePhase(float InDelaySeconds);
	void OpenEscapePhase();
	FHeistEscapePhaseStateChanged& GetEscapePhaseStateChangedDelegate();

private:
	UPROPERTY(ReplicatedUsing = OnRep_EscapePhaseOpen, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Escape", meta = (AllowPrivateAccess = "true"))
	bool bEscapePhaseOpen = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Escape", meta = (AllowPrivateAccess = "true"))
	float EscapePhaseDelaySeconds = 0.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Escape", meta = (AllowPrivateAccess = "true"))
	float EscapePhaseOpenTimeSeconds = -1.0f;

	UFUNCTION()
	void OnRep_EscapePhaseOpen();

	FHeistEscapePhaseStateChanged EscapePhaseStateChangedDelegate;

#pragma endregion

#pragma region RareLootEvent

public:
	const FHeistRareLootEventState& GetRareLootEventState() const;
	void BeginRareLootWarning(int32 EventIndex, FName ItemId, float SpawnServerTime);
	void ActivateRareLootMarker(int32 EventIndex, FName ItemId, const FVector& WorldLocation);
	void DeactivateRareLootMarker(int32 EventIndex);
	FHeistRareLootEventStateChanged& GetRareLootEventStateChangedDelegate();

private:
	UPROPERTY(ReplicatedUsing = OnRep_RareLootEventState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|RareLoot", meta = (AllowPrivateAccess = "true"))
	FHeistRareLootEventState RareLootEventState;

	UFUNCTION()
	void OnRep_RareLootEventState();

	void BroadcastRareLootEventState();

	FHeistRareLootEventStateChanged RareLootEventStateChangedDelegate;

#pragma endregion

#pragma region GapTracker

public:
	bool IsGapTrackerActive() const;
	int32 GetGapTrackerLeaderPlayerId() const;
	void SetGapTrackerState(bool bInActive, int32 InLeaderPlayerId);
	FHeistGapTrackerStateChanged& GetGapTrackerStateChangedDelegate();

private:
	UPROPERTY(ReplicatedUsing = OnRep_GapTrackerState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|GapTracker", meta = (AllowPrivateAccess = "true"))
	bool bGapTrackerActive = false;

	UPROPERTY(ReplicatedUsing = OnRep_GapTrackerState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|GapTracker", meta = (AllowPrivateAccess = "true"))
	int32 GapTrackerLeaderPlayerId = INDEX_NONE;

	UFUNCTION()
	void OnRep_GapTrackerState();

	void BroadcastGapTrackerState();

	FHeistGapTrackerStateChanged GapTrackerStateChangedDelegate;

#pragma endregion

#pragma region SoundPing

public:
	void ReportSoundPing(const FHeistSoundPingEvent& SoundPingEvent);
	const FHeistSoundPingEvent& GetLastSoundPingEvent() const;
	FHeistSoundPingEventReported& GetSoundPingEventReportedDelegate();

private:
	UPROPERTY(ReplicatedUsing = OnRep_LastSoundPingEvent, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|SoundPing", meta = (AllowPrivateAccess = "true"))
	FHeistSoundPingEvent LastSoundPingEvent;

	UPROPERTY(Transient)
	int32 NextSoundPingSequenceId = 1;

	UFUNCTION()
	void OnRep_LastSoundPingEvent();

	FHeistSoundPingEventReported SoundPingEventReportedDelegate;

#pragma endregion

#pragma region ResultData

public:
	void RebuildPlayerResults();
	const TArray<FHeistPlayerResult>& GetPlayerResults() const;
	int32 GetWinnerPlayerId() const;
	FHeistPlayerResultsChanged& GetPlayerResultsChangedDelegate();

private:
	UPROPERTY(ReplicatedUsing = OnRep_PlayerResults, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	TArray<FHeistPlayerResult> PlayerResults;

	UFUNCTION()
	void OnRep_PlayerResults();

	FHeistPlayerResultsChanged PlayerResultsChangedDelegate;

#pragma endregion

#pragma region Replication

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion
};
