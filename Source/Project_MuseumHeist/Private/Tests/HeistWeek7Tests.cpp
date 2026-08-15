#if WITH_DEV_AUTOMATION_TESTS

#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameInstance.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistTypes.h"
#include "Data/HeistArtifactDataTypes.h"
#include "Data/HeistGameBalanceDataAsset.h"
#include "Engine/DataTable.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistWeek7ReadabilityContractTest, "ProjectMuseumHeist.W7.ReadabilityContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistWeek7ReadabilityContractTest::RunTest(const FString& Parameters)
{
	const AHeistPlayerCharacter* CharacterCDO = GetDefault<AHeistPlayerCharacter>();
	TestNotNull(TEXT("Player character CDO exists"), CharacterCDO);
	if (CharacterCDO)
	{
		TestTrue(TEXT("Walk baseline is 300 cm/s"), FMath::IsNearlyEqual(CharacterCDO->CalculateMovementSpeedForPace(0.0f, false), 300.0f));
		TestTrue(TEXT("Sprint baseline is 600 cm/s"), FMath::IsNearlyEqual(CharacterCDO->CalculateMovementSpeedForPace(0.0f, true), 600.0f));
		TestTrue(TEXT("Walk weight penalty is 7.5 cm/s per kg"), FMath::IsNearlyEqual(CharacterCDO->CalculateMovementSpeedForPace(10.0f, false), 225.0f));
		TestTrue(TEXT("Sprint weight penalty is 15 cm/s per kg"), FMath::IsNearlyEqual(CharacterCDO->CalculateMovementSpeedForPace(10.0f, true), 450.0f));
		TestTrue(TEXT("Walk minimum is 150 cm/s"), FMath::IsNearlyEqual(CharacterCDO->CalculateMovementSpeedForPace(999.0f, false), 150.0f));
		TestTrue(TEXT("Sprint minimum is 250 cm/s"), FMath::IsNearlyEqual(CharacterCDO->CalculateMovementSpeedForPace(999.0f, true), 250.0f));
	}

	for (const EHeistCrewStatus Status : {EHeistCrewStatus::Active, EHeistCrewStatus::Forging, EHeistCrewStatus::Assembling, EHeistCrewStatus::CarryingOriginal,
			 EHeistCrewStatus::Heavy, EHeistCrewStatus::Stunned, EHeistCrewStatus::Arrested, EHeistCrewStatus::Escaped})
	{
		TestFalse(TEXT("Crew status full label is present"), HeistCrewStatus::ToDisplayText(Status).IsEmpty());
		TestFalse(TEXT("Crew status compact label is present"), HeistCrewStatus::ToCompactText(Status).IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistWeek7InputAssetContractTest, "ProjectMuseumHeist.W7.InputAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistWeek7InputAssetContractTest::RunTest(const FString& Parameters)
{
	const UInputAction* SprintAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Blueprints/Player/Input/IA_Sprint.IA_Sprint"));
	const UInputAction* MapAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Blueprints/Player/Input/IA_Map.IA_Map"));
	const UInputMappingContext* GameplayContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Blueprints/Player/Input/IMC_Default.IMC_Default"));
	const UInputMappingContext* MapContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Blueprints/Player/Input/IMC_Map.IMC_Map"));
	TestNotNull(TEXT("IA_Sprint exists"), SprintAction);
	TestNotNull(TEXT("IA_Map exists"), MapAction);
	TestNotNull(TEXT("IMC_Default exists"), GameplayContext);
	TestNotNull(TEXT("IMC_Map exists"), MapContext);

	auto HasMapping = [](const UInputMappingContext* Context, const UInputAction* Action, const FKey Key)
	{
		return IsValid(Context) && Context->GetMappings().ContainsByPredicate(
			[Action, Key](const FEnhancedActionKeyMapping& Mapping) { return Mapping.Action == Action && Mapping.Key == Key; });
	};
	TestTrue(TEXT("Gameplay maps Left Shift to sprint"), HasMapping(GameplayContext, SprintAction, EKeys::LeftShift));
	TestTrue(TEXT("Gameplay maps M to map"), HasMapping(GameplayContext, MapAction, EKeys::M));
	TestTrue(TEXT("Map mode maps M to close"), HasMapping(MapContext, MapAction, EKeys::M));

	const UClass* ControllerClass = LoadClass<AHeistPlayerController>(nullptr, TEXT("/Game/Blueprints/Player/BP_HeistPlayerController.BP_HeistPlayerController_C"));
	const AHeistPlayerController* ControllerCDO = IsValid(ControllerClass) ? Cast<AHeistPlayerController>(ControllerClass->GetDefaultObject()) : nullptr;
	TestNotNull(TEXT("BP_HeistPlayerController CDO exists"), ControllerCDO);
	TestTrue(TEXT("Controller W7 input defaults are assigned"), IsValid(ControllerCDO) && ControllerCDO->AreW7InputAssetsConfigured());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistWeek7VariationContractTest, "ProjectMuseumHeist.W7.Variation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistWeek7VariationContractTest::RunTest(const FString& Parameters)
{
	const UHeistGameInstance* GameInstanceCDO = GetDefault<UHeistGameInstance>();
	const UDataTable* TemplateTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DataTable/DT_ForgeryTemplate.DT_ForgeryTemplate"));
	TestNotNull(TEXT("DT_ForgeryTemplate exists"), TemplateTable);
	TestTrue(TEXT("DT_ForgeryTemplate uses FHeistForgeryTemplateRow"), IsValid(TemplateTable) && TemplateTable->GetRowStruct() == FHeistForgeryTemplateRow::StaticStruct());
	TMap<FName, int32> TemplateCountByPool;
	if (IsValid(TemplateTable) && TemplateTable->GetRowStruct() == FHeistForgeryTemplateRow::StaticStruct())
	{
		for (const FName RowName : TemplateTable->GetRowNames())
		{
			const FHeistForgeryTemplateRow* Row = TemplateTable->FindRow<FHeistForgeryTemplateRow>(RowName, TEXT("FHeistWeek7VariationContractTest"), false);
			if (Row != nullptr && Row->TemplateId == RowName)
			{
				++TemplateCountByPool.FindOrAdd(Row->SurfacePoolId);
			}
		}
	}
	for (const FName PoolId : {FName(TEXT("M01")), FName(TEXT("M02")), FName(TEXT("M03"))})
	{
		TestEqual(FString::Printf(TEXT("%s Surface pool has exactly 12 valid templates"), *PoolId.ToString()), TemplateCountByPool.FindRef(PoolId), 12);
	}

	int32 DrawCount = 0;
	int32 FirstCycleUnique = 0;
	int32 SecondCycleUnique = 0;
	TestTrue(TEXT("Random map bag exhausts all maps before reuse"),
		GameInstanceCDO->RunRandomMapShuffleBagSelfTestForDebug(DrawCount, FirstCycleUnique, SecondCycleUnique));
	TestEqual(TEXT("Random map test draws two cycles"), DrawCount, 6);
	TestEqual(TEXT("First random map cycle has 3 unique maps"), FirstCycleUnique, 3);
	TestEqual(TEXT("Second random map cycle has 3 unique maps"), SecondCycleUnique, 3);

	int32 SurfaceDrawCount = 0;
	int32 SurfaceFirstUnique = 0;
	int32 SurfaceSecondUnique = 0;
	int32 RecentChecks = 0;
	int32 RecentPasses = 0;
	TestTrue(TEXT("Fixed-seed Surface template shuffle-bag is deterministic and protects recent history"), GameInstanceCDO->RunSurfaceTemplateShuffleBagSelfTestForDebug(
		12, SurfaceDrawCount, SurfaceFirstUnique, SurfaceSecondUnique, RecentChecks, RecentPasses));
	TestEqual(TEXT("Surface test draws two 12-template cycles"), SurfaceDrawCount, 24);
	TestEqual(TEXT("Surface first cycle unique count"), SurfaceFirstUnique, 12);
	TestEqual(TEXT("Surface second cycle unique count"), SurfaceSecondUnique, 12);
	TestEqual(TEXT("All recent-history checks pass"), RecentPasses, RecentChecks);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistWeek7BalanceContractTest, "ProjectMuseumHeist.W7.Balance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistWeek7BalanceContractTest::RunTest(const FString& Parameters)
{
	const UHeistGameBalanceDataAsset* Balance = LoadObject<UHeistGameBalanceDataAsset>(nullptr, TEXT("/Game/Data/DataAsset/DA_GameBalance.DA_GameBalance"));
	TestNotNull(TEXT("DA_GameBalance exists"), Balance);
	if (!Balance)
	{
		return false;
	}
	const float ExpectedGuardMultipliers[] = {0.75f, 1.00f, 1.25f, 1.50f};
	const float ExpectedDetectionMultipliers[] = {0.85f, 1.00f, 1.10f, 1.20f};
	const float ExpectedInspectionMultipliers[] = {1.20f, 1.00f, 0.90f, 0.80f};
	const int32 ExpectedGuardsForFourAuthored[] = {3, 4, 5, 6};
	for (int32 PlayerCount = 1; PlayerCount <= 4; ++PlayerCount)
	{
		FHeistPlayerCountDifficultyBaseline Baseline;
		TestTrue(FString::Printf(TEXT("Difficulty baseline exists for %d players"), PlayerCount), Balance->TryGetPlayerCountDifficultyBaseline(PlayerCount, Baseline));
		TestTrue(TEXT("Guard multiplier is positive"), Baseline.GuardCountMultiplier > 0.0f);
		TestTrue(TEXT("Detection multiplier is positive"), Baseline.DetectionMultiplier > 0.0f);
		TestTrue(TEXT("Inspection multiplier is positive"), Baseline.InspectionDurationMultiplier > 0.0f);
		TestTrue(FString::Printf(TEXT("Guard multiplier matches %d-player balance table"), PlayerCount),
			FMath::IsNearlyEqual(Baseline.GuardCountMultiplier, ExpectedGuardMultipliers[PlayerCount - 1]));
		TestTrue(FString::Printf(TEXT("Detection multiplier matches %d-player balance table"), PlayerCount),
			FMath::IsNearlyEqual(Baseline.DetectionMultiplier, ExpectedDetectionMultipliers[PlayerCount - 1]));
		TestTrue(FString::Printf(TEXT("Inspection multiplier matches %d-player balance table"), PlayerCount),
			FMath::IsNearlyEqual(Baseline.InspectionDurationMultiplier, ExpectedInspectionMultipliers[PlayerCount - 1]));
		TestEqual(FString::Printf(TEXT("Guard count rounding matches %d-player balance table"), PlayerCount),
			AHeistGameMode::CalculateDifficultyGuardCount(4, Baseline.GuardCountMultiplier), ExpectedGuardsForFourAuthored[PlayerCount - 1]);
	}
	TestTrue(TEXT("Reward multiplier bounds are ordered"), Balance->MinimumForgeryRewardMultiplier <= Balance->MaximumForgeryRewardMultiplier);
	TestTrue(TEXT("Alert reward penalty does not mutate quota"), Balance->AlertLevelRewardPenalty >= 0.0f && Balance->AlertLevelRewardPenalty <= 0.25f);
	TestTrue(TEXT("Arrest reward penalty is bounded"), Balance->ArrestRewardPenaltyPerPlayer >= 0.0f && Balance->ArrestRewardPenaltyPerPlayer <= 1.0f);
	TestTrue(TEXT("Minimum forgery reward matches balance table"), FMath::IsNearlyEqual(Balance->MinimumForgeryRewardMultiplier, 0.75f));
	TestTrue(TEXT("Maximum forgery reward matches balance table"), FMath::IsNearlyEqual(Balance->MaximumForgeryRewardMultiplier, 1.25f));
	TestTrue(TEXT("Alert reward penalty matches balance table"), FMath::IsNearlyEqual(Balance->AlertLevelRewardPenalty, 0.05f));
	TestTrue(TEXT("Minimum stealth reward matches balance table"), FMath::IsNearlyEqual(Balance->MinimumStealthRewardMultiplier, 0.75f));
	TestTrue(TEXT("Arrest reward penalty matches balance table"), FMath::IsNearlyEqual(Balance->ArrestRewardPenaltyPerPlayer, 0.10f));
	return true;
}

#endif
