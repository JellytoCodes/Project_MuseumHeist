#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "HeistGameBalanceDataAsset.generated.h"

class UDataTable;

UCLASS(BlueprintType)
class PROJECT_MUSEUMHEIST_API UHeistGameBalanceDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UHeistGameBalanceDataAsset();

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
};
