#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"

#include "HeistContractDataTypes.generated.h"

/** Static definition for the single v1.0 Contract archetype. */
USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistContractDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Contract")
	FName ContractId = FName(TEXT("Contract_MuseumSwap_01"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Contract")
	FName RequiredTargetPoolId = FName(TEXT("Pool_AllExhibits"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Contract", meta = (ClampMin = "1"))
	int32 BaseLootValueQuota = 4000;

	/** Index 0~3 maps to 1~4 connected players. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Contract", meta = (EditFixedOrder))
	TArray<float> PlayerCountQuotaMultipliers = {1.0f, 1.6f, 2.2f, 2.8f};

	/** Minimum optional Exhibit count by player count. Consumed by W6-002 assignment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Contract", meta = (EditFixedOrder))
	TArray<int32> MinimumOptionalExhibits = {2, 3, 4, 5};

	/** Maximum optional Exhibit count by player count. Consumed by W6-002 assignment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Contract", meta = (EditFixedOrder))
	TArray<int32> MaximumOptionalExhibits = {4, 5, 6, 8};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Contract", meta = (ClampMin = "900.0", ClampMax = "1500.0", Units = "s"))
	float MatchDurationSeconds = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Contract")
	FName ExtractionRuleId = FName(TEXT("IndividualDeposit_SharedExit"));

	int32 ResolveLootValueQuota(int32 PlayerCount) const;
	int32 ResolveMinimumOptionalExhibitCount(int32 PlayerCount) const;
	int32 ResolveMaximumOptionalExhibitCount(int32 PlayerCount) const;
	bool IsRuntimeDefinitionValid(FString* OutFailureReason = nullptr) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
