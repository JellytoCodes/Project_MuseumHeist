#if WITH_DEV_AUTOMATION_TESTS

#include "Core/HeistTypes.h"
#include "Misc/AutomationTest.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistTeamRewardDeterminismTest, "ProjectMuseumHeist.Result.TeamRewardDeterminism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistTeamRewardDeterminismTest::RunTest(const FString& Parameters)
{
	float ForgeryMultiplier = 0.0f;
	float StealthMultiplier = 0.0f;
	int32 ArrestPenalty = 0;
	int32 TeamReward = 0;
	TestTrue(TEXT("Valid reward inputs calculate"), HeistTeamReward::Calculate(3000, 1000, 50.0f, 0.75f, 1.25f, 2, 0.05f, 0.75f, 1, 0.10f,
		ForgeryMultiplier, StealthMultiplier, ArrestPenalty, TeamReward));
	TestTrue(TEXT("Fifty quality is neutral"), FMath::IsNearlyEqual(ForgeryMultiplier, 1.0f));
	TestTrue(TEXT("Searching alert applies ten percent stealth penalty"), FMath::IsNearlyEqual(StealthMultiplier, 0.90f));
	TestEqual(TEXT("One arrest removes ten percent of subtotal"), ArrestPenalty, 370);
	TestEqual(TEXT("Reward remains deterministic and separate from quota"), TeamReward, 3330);

	const float FirstForgeryMultiplier = ForgeryMultiplier;
	const float FirstStealthMultiplier = StealthMultiplier;
	const int32 FirstArrestPenalty = ArrestPenalty;
	const int32 FirstTeamReward = TeamReward;
	for (int32 Iteration = 0; Iteration < 8; ++Iteration)
	{
		TestTrue(FString::Printf(TEXT("Repeated reward calculation %d succeeds"), Iteration + 1),
			HeistTeamReward::Calculate(3000, 1000, 50.0f, 0.75f, 1.25f, 2, 0.05f, 0.75f, 1, 0.10f,
				ForgeryMultiplier, StealthMultiplier, ArrestPenalty, TeamReward));
		TestTrue(FString::Printf(TEXT("Repeated forgery multiplier %d is identical"), Iteration + 1),
			FMath::IsNearlyEqual(ForgeryMultiplier, FirstForgeryMultiplier));
		TestTrue(FString::Printf(TEXT("Repeated stealth multiplier %d is identical"), Iteration + 1),
			FMath::IsNearlyEqual(StealthMultiplier, FirstStealthMultiplier));
		TestEqual(FString::Printf(TEXT("Repeated arrest penalty %d is identical"), Iteration + 1), ArrestPenalty, FirstArrestPenalty);
		TestEqual(FString::Printf(TEXT("Repeated team reward %d is identical"), Iteration + 1), TeamReward, FirstTeamReward);
	}

	TestFalse(TEXT("Negative balance input is rejected"), HeistTeamReward::Calculate(3000, 1000, 50.0f, -0.1f, 1.25f, 0, 0.05f, 0.75f, 0, 0.10f,
		ForgeryMultiplier, StealthMultiplier, ArrestPenalty, TeamReward));
	TestEqual(TEXT("Rejected calculation resets reward"), TeamReward, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistTeamRewardBoundariesTest, "ProjectMuseumHeist.Result.TeamRewardBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistTeamRewardBoundariesTest::RunTest(const FString& Parameters)
{
	float ForgeryMultiplier = 0.0f;
	float StealthMultiplier = 0.0f;
	int32 ArrestPenalty = 0;
	int32 TeamReward = 0;

	TestTrue(TEXT("Ninety-two quality reward calculates"), HeistTeamReward::Calculate(2500, 1500, 92.0f, 0.75f, 1.25f, 0, 0.05f, 0.75f, 0, 0.10f,
		ForgeryMultiplier, StealthMultiplier, ArrestPenalty, TeamReward));
	TestTrue(TEXT("Ninety-two quality produces the expected multiplier"), FMath::IsNearlyEqual(ForgeryMultiplier, 1.21f));
	TestTrue(TEXT("Quiet alert preserves the full stealth multiplier"), FMath::IsNearlyEqual(StealthMultiplier, 1.0f));
	TestEqual(TEXT("Ninety-two quality affects only target value and leaves loose loot intact"), TeamReward, 4525);

	TestTrue(TEXT("Zero quality reward calculates"), HeistTeamReward::Calculate(2500, 1500, 0.0f, 0.75f, 1.25f, 0, 0.05f, 0.75f, 0, 0.10f,
		ForgeryMultiplier, StealthMultiplier, ArrestPenalty, TeamReward));
	TestTrue(TEXT("Zero quality uses the minimum forgery multiplier"), FMath::IsNearlyEqual(ForgeryMultiplier, 0.75f));
	TestEqual(TEXT("Zero quality still preserves secured loose loot"), TeamReward, 3375);

	TestTrue(TEXT("Extreme alert reward calculates"), HeistTeamReward::Calculate(3000, 1000, 50.0f, 0.75f, 1.25f, 10, 0.05f, 0.75f, 0, 0.10f,
		ForgeryMultiplier, StealthMultiplier, ArrestPenalty, TeamReward));
	TestTrue(TEXT("Extreme alert clamps to the minimum stealth multiplier"), FMath::IsNearlyEqual(StealthMultiplier, 0.75f));
	TestEqual(TEXT("Alert penalty affects target value and not secured loose loot"), TeamReward, 3250);

	TestTrue(TEXT("Multiple arrest reward calculates"), HeistTeamReward::Calculate(3000, 1000, 50.0f, 0.75f, 1.25f, 0, 0.05f, 0.75f, 2, 0.10f,
		ForgeryMultiplier, StealthMultiplier, ArrestPenalty, TeamReward));
	TestEqual(TEXT("Two arrests remove twenty percent of the subtotal"), ArrestPenalty, 800);
	TestEqual(TEXT("Two-arrest reward is deterministic"), TeamReward, 3200);

	TestTrue(TEXT("Excessive arrest count reward calculates"), HeistTeamReward::Calculate(3000, 1000, 50.0f, 0.75f, 1.25f, 0, 0.05f, 0.75f, 20, 0.10f,
		ForgeryMultiplier, StealthMultiplier, ArrestPenalty, TeamReward));
	TestEqual(TEXT("Arrest penalty cannot exceed the subtotal"), ArrestPenalty, 4000);
	TestEqual(TEXT("Reward cannot become negative"), TeamReward, 0);

	TestTrue(TEXT("Large reward inputs calculate without overflow"), HeistTeamReward::Calculate(MAX_int32, MAX_int32, 100.0f, 0.75f, 1.25f, 0, 0.05f, 0.75f, 0, 0.10f,
		ForgeryMultiplier, StealthMultiplier, ArrestPenalty, TeamReward));
	TestEqual(TEXT("Large reward subtotal clamps to int32 range"), TeamReward, MAX_int32);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistTeamResultSnapshotTest, "ProjectMuseumHeist.Result.TeamSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistTeamResultSnapshotTest::RunTest(const FString& Parameters)
{
	FHeistTeamResult Result;
	TestFalse(TEXT("Default team result is not terminal"), Result.IsValid());
	Result.Outcome = EHeistContractOutcome::Success;
	Result.OutcomeReasonId = HeistContractOutcomeReasons::ContractComplete();
	Result.LootValueQuota = 4000;
	Result.SecuredValue = 4500;
	Result.TeamReward = 4200;
	Result.Revision = 1;
	TestTrue(TEXT("Complete replicated team result is valid"), Result.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistPlayerContributionDataContractTest, "ProjectMuseumHeist.Result.PlayerContribution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistPlayerContributionDataContractTest::RunTest(const FString& Parameters)
{
	FHeistPlayerContribution Contribution;
	TestTrue(TEXT("Default contribution is a valid empty noncompetitive record"), Contribution.IsValid());

	Contribution.SurfaceForgeries = 2;
	Contribution.BestSurfaceQuality = 73.1f;
	Contribution.Assemblies = 1;
	Contribution.BestAssemblyQuality = 82.5f;
	Contribution.ArtifactsRecovered = 1;
	Contribution.CarryTimeSeconds = 12.5f;
	Contribution.SecuredLootValue = 1500;
	Contribution.GuardsDistracted = 2;
	Contribution.TeammatesRescued = 1;
	Contribution.AlarmsTriggered = 1;
	Contribution.bEscaped = true;
	TestTrue(TEXT("Complete contribution record is valid"), Contribution.IsValid());

	FHeistPlayerResult PlayerResult;
	PlayerResult.PlayerId = 1;
	PlayerResult.bEscaped = true;
	PlayerResult.Contribution = Contribution;
	const FHeistPlayerResult ReplicatedCopy = PlayerResult;
	TestTrue(TEXT("Contribution survives the player result snapshot contract"), ReplicatedCopy == PlayerResult);
	TestEqual(TEXT("Surface count remains descriptive data"), ReplicatedCopy.Contribution.SurfaceForgeries, 2);
	TestEqual(TEXT("Secured loot remains descriptive data"), ReplicatedCopy.Contribution.SecuredLootValue, 1500);

	FHeistPlayerContribution InvalidContribution = Contribution;
	InvalidContribution.BestSurfaceQuality = 100.1f;
	TestFalse(TEXT("Out-of-range surface quality is rejected"), InvalidContribution.IsValid());
	InvalidContribution = Contribution;
	InvalidContribution.CarryTimeSeconds = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Non-finite carry time is rejected"), InvalidContribution.IsValid());
	InvalidContribution = Contribution;
	InvalidContribution.TeammatesRescued = -1;
	TestFalse(TEXT("Negative counters are rejected"), InvalidContribution.IsValid());
	InvalidContribution = Contribution;
	InvalidContribution.bArrested = true;
	TestFalse(TEXT("Escaped and arrested cannot both be recorded"), InvalidContribution.IsValid());
	return true;
}

#endif
