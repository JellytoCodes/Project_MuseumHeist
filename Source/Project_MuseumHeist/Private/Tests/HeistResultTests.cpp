#if WITH_DEV_AUTOMATION_TESTS

#include "Core/HeistTypes.h"
#include "Misc/AutomationTest.h"

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

	TestFalse(TEXT("Negative balance input is rejected"), HeistTeamReward::Calculate(3000, 1000, 50.0f, -0.1f, 1.25f, 0, 0.05f, 0.75f, 0, 0.10f,
		ForgeryMultiplier, StealthMultiplier, ArrestPenalty, TeamReward));
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

#endif
