#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "MVVMViewModelBase.h"

#include "HeistHUDViewModel.generated.h"

DECLARE_MULTICAST_DELEGATE(FHeistRareLootPresentationChanged);
DECLARE_MULTICAST_DELEGATE(FHeistHUDPresentationChanged);

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
	void RefreshRareLootState();
	FHeistHUDPresentationChanged& GetPresentationChangedDelegate();
	FHeistRareLootPresentationChanged& GetRareLootPresentationChangedDelegate();

  private:
	void HandleRareLootEventStateChanged(const FHeistRareLootEventState& EventState);
	void HandlePlayerConnectionsChanged(int32 ConnectedPlayers);
	void HandlePlayerIdentityChanged(int32 PlayerId);
	void HandleObjectiveStateChanged(FName ArtifactId, FName CaseId, EHeistObjectiveState ObjectiveState, class AHeistPlayerState* CarrierCandidate);
	void HandleEscapePhaseStateChanged(bool bEscapePhaseOpen);
	void HandleLootTotalsChanged(int32 TotalLootScore, float TotalLootWeight);
	void HandleEscapeStateChanged(bool bEscaped);
	void HandleActionStateChanged();

	UPROPERTY(Transient)
	TObjectPtr<AHeistGameState> GameState;

	UPROPERTY(Transient)
	TObjectPtr<AHeistPlayerState> LocalPlayerState;

	UPROPERTY(Transient)
	TObjectPtr<UHeistActionComponent> ActionComponent;

	FHeistHUDPresentationChanged PresentationChangedDelegate;
	FHeistRareLootPresentationChanged RareLootPresentationChangedDelegate;

#pragma endregion

#pragma region GeneralPresentation

  public:
	int32 GetLocalLootScore() const;
	float GetLocalLootWeight() const;
	int32 GetLocalPlayerId() const;
	int32 GetConnectedPlayerCount() const;
	bool IsLocalPlayerEscaped() const;
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

#pragma endregion

#pragma region RareLootPresentation

  public:
	bool IsRareLootIncoming() const;
	bool IsRareLootDirectionMarkerVisible() const;
	int32 GetRareLootEventIndex() const;
	FName GetRareLootItemId() const;
	FVector GetRareLootWorldLocation() const;
	float GetRareLootSpawnServerTime() const;

  private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|RareLoot", meta = (AllowPrivateAccess = "true"))
	bool bRareLootIncoming = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|RareLoot", meta = (AllowPrivateAccess = "true"))
	bool bRareLootDirectionMarkerVisible = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|RareLoot", meta = (AllowPrivateAccess = "true"))
	int32 RareLootEventIndex = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|RareLoot", meta = (AllowPrivateAccess = "true"))
	FName RareLootItemId = NAME_None;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|RareLoot", meta = (AllowPrivateAccess = "true"))
	FVector RareLootWorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|RareLoot", meta = (AllowPrivateAccess = "true"))
	float RareLootSpawnServerTime = -1.0f;

#pragma endregion
};
