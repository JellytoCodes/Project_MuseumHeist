#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "GameFramework/PlayerState.h"

#include "HeistPlayerState.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FHeistPlayerEscapeStateChanged, bool);
DECLARE_MULTICAST_DELEGATE_OneParam(FHeistPlayerArrestStateChanged, bool);
DECLARE_MULTICAST_DELEGATE_TwoParams(FHeistLootTotalsChanged, int32, float);
DECLARE_MULTICAST_DELEGATE_OneParam(FHeistPlayerIdentityChanged, int32);
DECLARE_MULTICAST_DELEGATE_OneParam(FHeistCrewStatusChanged, EHeistCrewStatus);
DECLARE_MULTICAST_DELEGATE_OneParam(FHeistLobbyReadyChanged, bool);

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistPlayerState : public APlayerState
{
	GENERATED_BODY()

#pragma region ScoreAndWeight

  public:
	int32 GetTotalLootScore() const;
	float GetTotalLootWeight() const;
	FHeistLootTotalsChanged& GetLootTotalsChangedDelegate();
	bool CanAddLootScoreAndWeight(int32 ScoreDelta, float WeightDelta) const;
	bool AddLootScoreAndWeight(int32 ScoreDelta, float WeightDelta);
	bool CanRemoveLootScoreAndWeight(int32 ScoreDelta, float WeightDelta) const;
	bool RemoveLootScoreAndWeight(int32 ScoreDelta, float WeightDelta);
	bool RemoveLootScoreAndWeightForDisconnect(int32 ScoreDelta, float WeightDelta);
	bool RemoveCarriedOriginalWeight(float WeightDelta);

  private:
	void CommitLootScoreAndWeightRemoval(int32 ScoreDelta, float WeightDelta, bool bRefreshMovement);
	void BroadcastLootTotalsChanged();

	UPROPERTY(ReplicatedUsing = OnRep_TotalLootScore, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Score", meta = (AllowPrivateAccess = "true"))
	int32 TotalLootScore = 0;

	UPROPERTY(ReplicatedUsing = OnRep_TotalLootWeight, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Weight", meta = (AllowPrivateAccess = "true"))
	float TotalLootWeight = 0.0f;

	FHeistLootTotalsChanged LootTotalsChangedDelegate;

#pragma endregion

#pragma region CrewStatus

  public:
	EHeistCrewStatus GetCrewStatus() const;
	FText GetHeistDisplayName() const;
	bool RefreshCrewStatus();
	FHeistCrewStatusChanged& GetCrewStatusChangedDelegate();

  private:
	EHeistCrewStatus ResolveCrewStatusFromPawn() const;

	UPROPERTY(ReplicatedUsing = OnRep_CrewStatus, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Crew", meta = (AllowPrivateAccess = "true"))
	EHeistCrewStatus CrewStatus = EHeistCrewStatus::Active;

	FHeistCrewStatusChanged CrewStatusChangedDelegate;

	UFUNCTION()
	void OnRep_CrewStatus();

#pragma endregion

#pragma region Contribution

  public:
	const FHeistPlayerContribution& GetContribution() const;
	void RecordSurfaceForgeryContribution(float QualityScore);
	void RecordAssemblyContribution(float QualityScore);
	void BeginOriginalCarryContribution();
	void EndOriginalCarryContribution(int32 RecoveredArtifactCount);
	void RecordSecuredLootContribution(int32 SecuredLootValue);
	void RecordGuardDistractionContribution(int32 DistractedGuardCount = 1);
	void RecordTeammateRescueContribution();
	void RecordAlarmContribution();
	void DebugSetContributionState(const FHeistPlayerContribution& NewContribution);

  private:
	void CommitContributionMutation();

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Contribution", meta = (AllowPrivateAccess = "true"))
	FHeistPlayerContribution Contribution;

	float OriginalCarryContributionStartServerTime = -1.0f;

#pragma endregion

#pragma region ArrestState

  public:
	bool IsArrested() const;
	bool MarkArrested(AActor* ArrestingGuard);
	bool ClearArrested();
	FHeistPlayerArrestStateChanged& GetArrestStateChangedDelegate();

  private:
	bool SetArrestedInternal(bool bNewArrested, AActor* ArrestingGuard);

	UPROPERTY(ReplicatedUsing = OnRep_Arrested, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Arrest", meta = (AllowPrivateAccess = "true"))
	bool bArrested = false;

	FHeistPlayerArrestStateChanged ArrestStateChangedDelegate;

	UFUNCTION()
	void OnRep_Arrested();

#pragma endregion

#pragma region EscapeState

  public:
	bool IsEscaped() const;
	bool MarkEscaped();
	FHeistPlayerEscapeStateChanged& GetEscapeStateChangedDelegate();

  private:
	UPROPERTY(ReplicatedUsing = OnRep_Escaped, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Escape", meta = (AllowPrivateAccess = "true"))
	bool bEscaped = false;

	FHeistPlayerEscapeStateChanged EscapeStateChangedDelegate;

	UFUNCTION()
	void OnRep_Escaped();

#pragma endregion

#pragma region LobbyReady

  public:
	UFUNCTION(BlueprintPure, Category = "Heist|Lobby")
	bool IsLobbyReady() const;

	bool SetLobbyReady(bool bNewLobbyReady);
	FHeistLobbyReadyChanged& GetLobbyReadyChangedDelegate();

  private:
	UPROPERTY(ReplicatedUsing = OnRep_LobbyReady, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	bool bLobbyReady = false;

	UFUNCTION()
	void OnRep_LobbyReady();

	FHeistLobbyReadyChanged LobbyReadyChangedDelegate;

#pragma endregion

#pragma region Replication

  public:
	virtual void OnRep_PlayerName() override;
	virtual void CopyProperties(APlayerState* PlayerState) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

  private:
	UFUNCTION()
	void OnRep_TotalLootScore();

	UFUNCTION()
	void OnRep_TotalLootWeight();

#pragma endregion

#pragma region Debug

  public:
	void DebugSetTotalLootWeight(float InWeight);
	void DebugSetResultState(bool bInEscaped);

#pragma endregion

#pragma region PlayerIdentity

  public:
	void InitializePlayerIdentity(int32 InHeistPlayerId, const FLinearColor& InPlayerColor);
	FHeistPlayerIdentityChanged& GetPlayerIdentityChangedDelegate();

	UPROPERTY(ReplicatedUsing = OnRep_HeistPlayerId, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Identity")
	int32 HeistPlayerId = INDEX_NONE;

	UPROPERTY(ReplicatedUsing = OnRep_PlayerColor, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Identity")
	FLinearColor PlayerColor = FLinearColor::White;

  private:
	UFUNCTION()
	void OnRep_HeistPlayerId();

	UFUNCTION()
	void OnRep_PlayerColor();

	FHeistPlayerIdentityChanged PlayerIdentityChangedDelegate;

#pragma endregion
};
