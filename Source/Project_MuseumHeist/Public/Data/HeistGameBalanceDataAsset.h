#pragma once

#include "CoreMinimal.h"
#include "Data/HeistContractDataTypes.h"
#include "Engine/DataAsset.h"

#include "HeistGameBalanceDataAsset.generated.h"

class UDataTable;
class AHeistLootActor;
class AHeistObjectDisplayCaseActor;

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistPlayerCountDifficultyBaseline
{
	GENERATED_BODY()

	FHeistPlayerCountDifficultyBaseline() = default;
	FHeistPlayerCountDifficultyBaseline(int32 InPlayerCount, float InGuardCountMultiplier, float InDetectionMultiplier, float InInspectionDurationMultiplier)
		: PlayerCount(InPlayerCount), GuardCountMultiplier(InGuardCountMultiplier), DetectionMultiplier(InDetectionMultiplier), InspectionDurationMultiplier(InInspectionDurationMultiplier)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1", ClampMax = "4"))
	int32 PlayerCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.01"))
	float GuardCountMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.01"))
	float DetectionMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.01"))
	float InspectionDurationMultiplier = 1.0f;
};

UCLASS(BlueprintType)
class PROJECT_MUSEUMHEIST_API UHeistGameBalanceDataAsset : public UDataAsset
{
	GENERATED_BODY()

#pragma region Construction

  public:
	UHeistGameBalanceDataAsset();
	bool TryGetPlayerCountDifficultyBaseline(int32 PlayerCount, FHeistPlayerCountDifficultyBaseline& OutBaseline) const;

#pragma endregion

#pragma region Config

  public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Difficulty")
	bool bAllowSoloProgression = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Difficulty")
	TArray<FHeistPlayerCountDifficultyBaseline> PlayerCountDifficultyBaselines;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Match", meta = (ClampMin = "0.0", Units = "s"))
	float VentUnlockTime = 180.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Loot|Match Start", meta = (ClampMin = "0"))
	int32 MatchStartVaultLootCount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Loot|Match Start", meta = (ClampMin = "0"))
	int32 MatchStartExhibitionLootCount = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Alert", meta = (ClampMin = "0.5", ClampMax = "0.5"))
	float AlertMeterStep = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Alert", meta = (ClampMin = "10.0", ClampMax = "10.0"))
	float AlertMeterMaximum = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Alert", meta = (ClampMin = "0.5", ClampMax = "10.0"))
	float GuardCaptureAlertIncrease = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Alert", meta = (ClampMin = "0.5", ClampMax = "10.0"))
	float SecurityIncidentAlertIncrease = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Alert", meta = (ClampMin = "0.5", ClampMax = "10.0"))
	float SearchingAlertMeterThreshold = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Alert", meta = (ClampMin = "0.5", ClampMax = "10.0"))
	float AlarmedAlertMeterThreshold = 7.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Alert", meta = (ClampMin = "10.0", ClampMax = "10.0"))
	float LockdownAlertMeterThreshold = 10.0f;

	/** Global range-only multiplier for guard sight, lose-sight, and SoundPing acceptance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|AI|Perception", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float GuardPerceptionRangeMultiplier = 0.5f;

	/** Event-only lookup radius used once when a forgery mode expires. This does not raise global Alert. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|AI|Forgery Timeout", meta = (ClampMin = "0.0", Units = "cm"))
	float ForgeryTimeoutInvestigationRadius = 1500.0f;

	/** Nearby-guard lookup radius for CCTV detection and Laser trip incidents. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|AI|Security Incident", meta = (ClampMin = "0.0", Units = "cm"))
	float SecurityIncidentInvestigationRadius = 1500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Security|CCTV", meta = (ClampMin = "0.05", Units = "s"))
	float SecurityCameraEvaluationIntervalSeconds = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Security|CCTV", meta = (ClampMin = "1.2", ClampMax = "1.5", Units = "s"))
	float SecurityCameraDetectionBuildUpSeconds = 1.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Security|CCTV", meta = (ClampMin = "0.0", Units = "s"))
	float SecurityCameraDetectionCooldownSeconds = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Security|Laser", meta = (ClampMin = "2.0", ClampMax = "5.0", Units = "s"))
	float SecurityLaserHoldDurationSeconds = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Security|Laser", meta = (ClampMin = "0.0", Units = "s"))
	float SecurityLaserRearmGraceSeconds = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Interaction", meta = (ClampMin = "0.0", Units = "s"))
	float EscapeCastTime = 2.0f;

	/** Reward-only tuning. These values never change SecuredValue or LootValueQuota. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Result|Reward", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MinimumForgeryRewardMultiplier = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Result|Reward", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MaximumForgeryRewardMultiplier = 1.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Result|Reward", meta = (ClampMin = "0.0", ClampMax = "0.25"))
	float AlertLevelRewardPenalty = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Result|Reward", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumStealthRewardMultiplier = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Result|Reward", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ArrestRewardPenaltyPerPlayer = 0.10f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Data Tables")
	TSoftObjectPtr<UDataTable> ItemDataTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Contract")
	FHeistContractDataRow DefaultContractDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Data Tables")
	TSoftObjectPtr<UDataTable> ContractDataTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Data Tables")
	TSoftObjectPtr<UDataTable> MapPresentationDataTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Data Tables")
	TSoftObjectPtr<UDataTable> ArtifactDataTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Data Tables")
	TSoftObjectPtr<UDataTable> ForgeryTemplateDataTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Data Tables")
	TSoftObjectPtr<UDataTable> ObjectAssemblyPartDataTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Data Tables")
	TSoftObjectPtr<UDataTable> ObjectAssemblyTemplateDataTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Data Tables")
	TSoftObjectPtr<UDataTable> LootDataTable;

	/** Shared presentation shell used for every loose-loot row. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Actor Shells")
	TSoftClassPtr<AHeistLootActor> WorldLootActorClass;

	/** Shared presentation shell used for every Sculpture/Ceramic display case. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Actor Shells")
	TSoftClassPtr<AHeistObjectDisplayCaseActor> ObjectDisplayCaseActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Data Tables")
	TSoftObjectPtr<UDataTable> UsableItemDataTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Data Tables")
	TSoftObjectPtr<UDataTable> SoundPingDataTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Data Tables")
	TSoftObjectPtr<UDataTable> GuardDataTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Data Tables")
	TSoftObjectPtr<UDataTable> LootSpawnDataTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Data Tables")
	TSoftObjectPtr<UDataTable> VentDataTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Data Tables")
	TSoftObjectPtr<UDataTable> UITextDataTable;

#pragma endregion
};
