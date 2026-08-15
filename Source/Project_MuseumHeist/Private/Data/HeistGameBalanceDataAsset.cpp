#include "Data/HeistGameBalanceDataAsset.h"

#include "Engine/DataTable.h"

UHeistGameBalanceDataAsset::UHeistGameBalanceDataAsset()
{
	WorldLootActorClass = TSoftClassPtr<AHeistLootActor>(FSoftObjectPath(TEXT("/Game/Blueprints/World/Actors/Loot/BP_Loot.BP_Loot_C")));
	ObjectDisplayCaseActorClass =
		TSoftClassPtr<AHeistObjectDisplayCaseActor>(FSoftObjectPath(TEXT("/Game/Blueprints/World/Actors/Loot/BP_ObjectDisplayCase.BP_ObjectDisplayCase_C")));
	ItemDataTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DataTable/DT_ItemData.DT_ItemData")));
	LootDataTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DataTable/DT_LootData.DT_LootData")));
	ContractDataTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DataTable/DT_ContractData.DT_ContractData")));
	MapPresentationDataTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DataTable/DT_MapPresentation.DT_MapPresentation")));
	ArtifactDataTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DataTable/DT_ArtifactData.DT_ArtifactData")));
	ForgeryTemplateDataTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DataTable/DT_ForgeryTemplate.DT_ForgeryTemplate")));
	ObjectAssemblyPartDataTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DataTable/DT_ObjectAssemblyPart.DT_ObjectAssemblyPart")));
	ObjectAssemblyTemplateDataTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DataTable/DT_ObjectAssemblyTemplate.DT_ObjectAssemblyTemplate")));

	PlayerCountDifficultyBaselines = {FHeistPlayerCountDifficultyBaseline(1, 0.75f, 0.85f, 1.20f), FHeistPlayerCountDifficultyBaseline(2, 1.00f, 1.00f, 1.00f),
									  FHeistPlayerCountDifficultyBaseline(3, 1.25f, 1.10f, 0.90f), FHeistPlayerCountDifficultyBaseline(4, 1.50f, 1.20f, 0.80f)};
}

bool UHeistGameBalanceDataAsset::TryGetPlayerCountDifficultyBaseline(const int32 PlayerCount, FHeistPlayerCountDifficultyBaseline& OutBaseline) const
{
	OutBaseline = FHeistPlayerCountDifficultyBaseline();
	const int32 SafePlayerCount = FMath::Clamp(PlayerCount, 1, 4);
	for (const FHeistPlayerCountDifficultyBaseline& Baseline : PlayerCountDifficultyBaselines)
	{
		if (Baseline.PlayerCount == SafePlayerCount)
		{
			OutBaseline = Baseline;
			return true;
		}
	}

	return false;
}
