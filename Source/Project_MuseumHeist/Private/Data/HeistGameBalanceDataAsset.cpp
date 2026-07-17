#include "Data/HeistGameBalanceDataAsset.h"

#include "Engine/DataTable.h"

UHeistGameBalanceDataAsset::UHeistGameBalanceDataAsset()
{
	ArtifactDataTable = TSoftObjectPtr<UDataTable>(
		FSoftObjectPath(TEXT("/Game/Data/DataTable/DT_ArtifactData.DT_ArtifactData")));

	PlayerCountDifficultyBaselines =
	{
		FHeistPlayerCountDifficultyBaseline(1, 0.75f, 0.85f, 1.20f),
		FHeistPlayerCountDifficultyBaseline(2, 1.00f, 1.00f, 1.00f),
		FHeistPlayerCountDifficultyBaseline(3, 1.25f, 1.10f, 0.90f),
		FHeistPlayerCountDifficultyBaseline(4, 1.50f, 1.20f, 0.80f)
	};
}

bool UHeistGameBalanceDataAsset::TryGetPlayerCountDifficultyBaseline(
	const int32 PlayerCount,
	FHeistPlayerCountDifficultyBaseline& OutBaseline) const
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
