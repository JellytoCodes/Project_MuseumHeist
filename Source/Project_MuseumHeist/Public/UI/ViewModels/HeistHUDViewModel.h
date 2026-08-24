#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "MVVMViewModelBase.h"

#include "HeistHUDViewModel.generated.h"

DECLARE_MULTICAST_DELEGATE(FHeistHUDPresentationChanged);

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistCrewStatusEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Crew")
	int32 PlayerId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Crew")
	FText PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Crew")
	FLinearColor PlayerColor = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Crew")
	EHeistCrewStatus Status = EHeistCrewStatus::Active;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Crew")
	FString PlatformUserId;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Crew")
	TObjectPtr<class AHeistPlayerState> PlayerState = nullptr;
};

UCLASS(BlueprintType)
class PROJECT_MUSEUMHEIST_API UHeistHUDViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

#pragma region Construction

  public:
	UHeistHUDViewModel(const FObjectInitializer& ObjectInitializer);

#pragma endregion

#pragma region Lifecycle

  protected:
	virtual void BeginDestroy() override;

#pragma endregion

#pragma region Setup

  public:
	void SetupViewModel(class AHeistGameState* InGameState, class AHeistPlayerState* InLocalPlayerState, class UHeistActionComponent* InActionComponent);
	void RefreshPresentationState();
	FHeistHUDPresentationChanged& GetPresentationChangedDelegate();

  private:
	void HandlePlayerConnectionsChanged(int32 ConnectedPlayers);
	void HandlePlayerIdentityChanged(int32 PlayerId);
	void HandleCrewStatusChanged(EHeistCrewStatus CrewStatus);
	void HandleContractSnapshotChanged(const FHeistContractSnapshot& ContractSnapshot);
	void RefreshCrewStatusEntries();
	void UnbindCrewPlayerStates();
	void HandleAlertStateChanged(EHeistAlertLevel PreviousAlertLevel, EHeistAlertLevel NewAlertLevel, int32 AlertRevision, FName TriggerId);
	void HandleObjectiveStateChanged(FName ArtifactId, FName CaseId, EHeistObjectiveState ObjectiveState, class AHeistPlayerState* CarrierCandidate);
	void HandleEscapePhaseStateChanged(bool bEscapePhaseOpen);
	void HandleLootTotalsChanged(int32 TotalLootScore, float TotalLootWeight);
	void HandleEscapeStateChanged(bool bEscaped);
	void HandleArrestStateChanged(bool bArrested);
	void HandleActionStateChanged();

	UPROPERTY(Transient)
	TObjectPtr<AHeistGameState> GameState;

	UPROPERTY(Transient)
	TObjectPtr<AHeistPlayerState> LocalPlayerState;

	UPROPERTY(Transient)
	TObjectPtr<UHeistActionComponent> ActionComponent;

	FHeistHUDPresentationChanged PresentationChangedDelegate;

#pragma endregion

#pragma region GeneralPresentation

  public:
	int32 GetLocalLootScore() const;
	float GetLocalLootWeight() const;
	int32 GetLocalPlayerId() const;
	int32 GetConnectedPlayerCount() const;
	bool IsLocalPlayerEscaped() const;
	bool IsLocalPlayerArrested() const;
	bool IsEscapePhaseOpen() const;
	bool IsEscapeCastActive() const;
	float GetEscapeCastEndServerTime() const;
	bool IsObservationCastActive() const;
	float GetObservationCastEndServerTime() const;
	bool IsObservationReferenceVisible() const;
	FName GetObservationReferenceArtifactId() const;
	FName GetObjectiveArtifactId() const;
	FName GetObjectiveCaseId() const;
	EHeistObjectiveState GetObjectiveState() const;
	const FText& GetObservationReferenceText() const;
	const FText& GetObjectiveStateText() const;
	EHeistAlertLevel GetAlertLevel() const;
	float GetAlertMeterValue() const;
	float GetMissionEndServerTime() const;
	const FText& GetRequiredTargetDisplayName() const;
	bool IsRequiredTargetAcquired() const;
	FName GetLastAlertTriggerId() const;
	int32 GetSecurityLevel() const;
	const FText& GetAlertBannerText() const;
	FLinearColor GetAlertColor() const;
	bool IsLockdownCountdownVisible() const;
	float GetLockdownCountdownEndServerTime() const;
	bool IsSuspenseMusicActive() const;
	bool IsAlarmMusicActive() const;
	const TArray<FHeistCrewStatusEntry>& GetCrewStatusEntries() const;

  private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|HUD", meta = (AllowPrivateAccess = "true"))
	int32 LocalLootScore = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|HUD", meta = (AllowPrivateAccess = "true"))
	float LocalLootWeight = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|HUD", meta = (AllowPrivateAccess = "true"))
	int32 LocalPlayerId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|HUD", meta = (AllowPrivateAccess = "true"))
	int32 ConnectedPlayerCount = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|HUD", meta = (AllowPrivateAccess = "true"))
	bool bLocalPlayerEscaped = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|HUD", meta = (AllowPrivateAccess = "true"))
	bool bLocalPlayerArrested = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|HUD", meta = (AllowPrivateAccess = "true"))
	bool bEscapePhaseOpen = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Action", meta = (AllowPrivateAccess = "true"))
	bool bEscapeCastActive = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Action", meta = (AllowPrivateAccess = "true"))
	float EscapeCastEndServerTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Observation", meta = (AllowPrivateAccess = "true"))
	bool bObservationCastActive = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Observation", meta = (AllowPrivateAccess = "true"))
	float ObservationCastEndServerTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Observation", meta = (AllowPrivateAccess = "true"))
	bool bObservationReferenceVisible = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Observation", meta = (AllowPrivateAccess = "true"))
	FName ObservationReferenceArtifactId = NAME_None;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Objective", meta = (AllowPrivateAccess = "true"))
	FName ObjectiveArtifactId = NAME_None;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Objective", meta = (AllowPrivateAccess = "true"))
	FName ObjectiveCaseId = NAME_None;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Objective", meta = (AllowPrivateAccess = "true"))
	EHeistObjectiveState ObjectiveState = EHeistObjectiveState::Inactive;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Observation", meta = (AllowPrivateAccess = "true"))
	FText ObservationReferenceText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Objective", meta = (AllowPrivateAccess = "true"))
	FText ObjectiveStateText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Alert", meta = (AllowPrivateAccess = "true"))
	EHeistAlertLevel AlertLevel = EHeistAlertLevel::Quiet;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Alert", meta = (AllowPrivateAccess = "true"))
	float AlertMeterValue = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Mission", meta = (AllowPrivateAccess = "true"))
	float MissionEndServerTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Mission", meta = (AllowPrivateAccess = "true"))
	FText RequiredTargetDisplayName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Mission", meta = (AllowPrivateAccess = "true"))
	bool bRequiredTargetAcquired = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Alert", meta = (AllowPrivateAccess = "true"))
	FName LastAlertTriggerId = NAME_None;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Alert", meta = (AllowPrivateAccess = "true"))
	int32 SecurityLevel = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Alert", meta = (AllowPrivateAccess = "true"))
	FText AlertBannerText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Alert", meta = (AllowPrivateAccess = "true"))
	FLinearColor AlertColor = FLinearColor(0.45f, 0.58f, 0.70f);

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Alert", meta = (AllowPrivateAccess = "true"))
	bool bLockdownCountdownVisible = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Alert", meta = (AllowPrivateAccess = "true"))
	float LockdownCountdownEndServerTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Audio", meta = (AllowPrivateAccess = "true"))
	bool bSuspenseMusicActive = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Audio", meta = (AllowPrivateAccess = "true"))
	bool bAlarmMusicActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Crew", meta = (AllowPrivateAccess = "true"))
	TArray<FHeistCrewStatusEntry> CrewStatusEntries;

	TArray<TWeakObjectPtr<AHeistPlayerState>> BoundCrewPlayerStates;

#pragma endregion

};
