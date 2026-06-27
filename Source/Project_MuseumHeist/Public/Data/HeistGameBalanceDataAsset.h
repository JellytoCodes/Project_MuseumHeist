#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "HeistGameBalanceDataAsset.generated.h"

class UDataTable;

UCLASS(BlueprintType)
class PROJECT_MUSEUMHEIST_API UHeistGameBalanceDataAsset : public UDataAsset
{
	GENERATED_BODY()

#pragma region Construction

public:
	UHeistGameBalanceDataAsset();

#pragma endregion

#pragma region Config

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Match", meta = (ClampMin = "0.0", Units = "s"))
	float VentUnlockTime = 180.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Rare Loot")
	TArray<float> RareLootEventTimes = { 90.0f, 225.0f };

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Rare Loot", meta = (ClampMin = "0.0", Units = "s"))
	float RareLootWarningLeadTime = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Rare Loot")
	FName RareLootItemId = FName(TEXT("Loot_RareArtifact"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Gap Tracker", meta = (ClampMin = "0"))
	int32 GapTrackerScoreThreshold = 1000;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Gap Tracker", meta = (ClampMin = "0.01", Units = "s"))
	float GapTrackerUpdateInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Interaction", meta = (ClampMin = "0.0", Units = "s"))
	float EscapeCastTime = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Data Tables")
	TSoftObjectPtr<UDataTable> ItemDataTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Data Tables")
	TSoftObjectPtr<UDataTable> LootDataTable;

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
	TSoftObjectPtr<UDataTable> CustomizationDataTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Data Tables")
	TSoftObjectPtr<UDataTable> UITextDataTable;

#pragma endregion
};
