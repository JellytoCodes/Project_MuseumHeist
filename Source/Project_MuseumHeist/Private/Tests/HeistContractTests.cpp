#if WITH_DEV_AUTOMATION_TESTS

#include "Core/HeistTypes.h"
#include "Data/HeistContractDataTypes.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistContractDefinitionTest, "ProjectMuseumHeist.Contract.Definition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistContractDefinitionTest::RunTest(const FString& Parameters)
{
	const FHeistContractDataRow Definition;
	FString FailureReason;

	TestTrue(TEXT("Default contract definition is valid"), Definition.IsRuntimeDefinitionValid(&FailureReason));
	TestTrue(TEXT("Valid definition has no failure reason"), FailureReason.IsEmpty());
	TestEqual(TEXT("One-player quota"), Definition.ResolveLootValueQuota(1), 4000);
	TestEqual(TEXT("Two-player quota"), Definition.ResolveLootValueQuota(2), 6400);
	TestEqual(TEXT("Three-player quota"), Definition.ResolveLootValueQuota(3), 8800);
	TestEqual(TEXT("Four-player quota"), Definition.ResolveLootValueQuota(4), 11200);
	TestEqual(TEXT("Player count below range clamps to one"), Definition.ResolveLootValueQuota(0), 4000);
	TestEqual(TEXT("Player count above range clamps to four"), Definition.ResolveLootValueQuota(5), 11200);
	TestEqual(TEXT("Three-player minimum optional exhibits"), Definition.ResolveMinimumOptionalExhibitCount(3), 4);
	TestEqual(TEXT("Three-player maximum optional exhibits"), Definition.ResolveMaximumOptionalExhibitCount(3), 6);

	FHeistContractDataRow InvalidDefinition = Definition;
	InvalidDefinition.PlayerCountQuotaMultipliers = {1.0f, 1.6f, 1.5f, 2.8f};
	TestFalse(TEXT("Decreasing player quota is rejected"), InvalidDefinition.IsRuntimeDefinitionValid(&FailureReason));
	TestEqual(TEXT("Decreasing quota failure reason"), FailureReason, FString(TEXT("NonMonotonicResolvedQuota")));

	InvalidDefinition = Definition;
	InvalidDefinition.MaximumOptionalExhibits.SetNum(3);
	TestFalse(TEXT("Incomplete player count arrays are rejected"), InvalidDefinition.IsRuntimeDefinitionValid(&FailureReason));
	TestEqual(TEXT("Array count failure reason"), FailureReason, FString(TEXT("PlayerCountArraySizeMismatch")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistContractSnapshotTest, "ProjectMuseumHeist.Contract.Snapshot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistContractSnapshotTest::RunTest(const FString& Parameters)
{
	FHeistContractSnapshot Snapshot;
	Snapshot.ContractId = FName(TEXT("Contract_MuseumSwap_01"));
	Snapshot.MapId = FName(TEXT("M01"));
	Snapshot.AssignmentSeed = 12345;
	Snapshot.RequiredTargetArtifactId = FName(TEXT("Artifact_Painting_M01"));
	Snapshot.RequiredTargetCaseId = FName(TEXT("Case_M01_Target"));
	Snapshot.LootValueQuota = 4000;
	Snapshot.Revision = 1;

	TestTrue(TEXT("Populated snapshot is initialized"), Snapshot.IsInitialized());
	TestTrue(TEXT("Initial progress is valid"), Snapshot.IsProgressValid());
	TestFalse(TEXT("Required target and quota are both needed"), Snapshot.IsSuccessConditionMet());
	TestEqual(TEXT("Missing required target resolves to failure"), static_cast<uint8>(Snapshot.ResolveTerminalOutcome(true)), static_cast<uint8>(EHeistContractOutcome::Failed));
	TestEqual(TEXT("No escaped crew resolves to failure even with future secured progress"), static_cast<uint8>(Snapshot.ResolveTerminalOutcome(false)),
		static_cast<uint8>(EHeistContractOutcome::Failed));

	Snapshot.bRequiredTargetSecured = true;
	Snapshot.SecuredValue = 3999;
	TestFalse(TEXT("Below-quota secured value is not success"), Snapshot.IsSuccessConditionMet());
	TestEqual(TEXT("Required target below quota resolves to partial haul"), static_cast<uint8>(Snapshot.ResolveTerminalOutcome(true)), static_cast<uint8>(EHeistContractOutcome::PartialHaul));
	Snapshot.Outcome = EHeistContractOutcome::PartialHaul;
	Snapshot.OutcomeReasonId = HeistContractOutcomeReasons::RequiredTargetSecuredQuotaShort();
	TestTrue(TEXT("Partial-haul outcome is consistent"), Snapshot.IsOutcomeConsistent());

	Snapshot.SecuredValue = 4000;
	Snapshot.Outcome = EHeistContractOutcome::None;
	Snapshot.OutcomeReasonId = NAME_None;
	TestTrue(TEXT("Target plus quota is success"), Snapshot.IsSuccessConditionMet());
	TestEqual(TEXT("Target plus quota resolves to success"), static_cast<uint8>(Snapshot.ResolveTerminalOutcome(true)), static_cast<uint8>(EHeistContractOutcome::Success));
	Snapshot.Outcome = EHeistContractOutcome::Success;
	Snapshot.OutcomeReasonId = HeistContractOutcomeReasons::ContractComplete();
	TestTrue(TEXT("Success outcome is consistent"), Snapshot.IsOutcomeConsistent());

	Snapshot.bRequiredTargetSecured = false;
	TestFalse(TEXT("Success outcome without required target is inconsistent"), Snapshot.IsOutcomeConsistent());
	Snapshot.Outcome = EHeistContractOutcome::Failed;
	Snapshot.OutcomeReasonId = HeistContractOutcomeReasons::RequiredTargetMissing();
	TestTrue(TEXT("Failure outcome without required target is consistent"), Snapshot.IsOutcomeConsistent());

	TestEqual(TEXT("Lockdown has highest failure reason priority"),
		HeistContractOutcomeReasons::Resolve(EHeistContractOutcome::Failed, false, false, true, true, FName(TEXT("Lockdown"))),
		HeistContractOutcomeReasons::LockdownBeforeContractComplete());
	TestEqual(TEXT("Match timer has priority over lifecycle failure reasons"),
		HeistContractOutcomeReasons::Resolve(EHeistContractOutcome::Failed, false, false, true, true, FName(TEXT("MatchTimerExpired"))),
		HeistContractOutcomeReasons::MatchTimerExpired());
	TestEqual(TEXT("Arrest reason has priority over disconnect"),
		HeistContractOutcomeReasons::Resolve(EHeistContractOutcome::Failed, false, false, true, true, FName(TEXT("PlayerArrested"))),
		HeistContractOutcomeReasons::AllRemainingCrewArrested());
	TestEqual(TEXT("Disconnect reason is selected when the crew is gone"),
		HeistContractOutcomeReasons::Resolve(EHeistContractOutcome::Failed, false, false, false, true, FName(TEXT("PlayerDisconnected"))),
		HeistContractOutcomeReasons::AllCrewDisconnected());
	TestEqual(TEXT("Missing target reason is selected after a crew member escaped"),
		HeistContractOutcomeReasons::Resolve(EHeistContractOutcome::Failed, false, true, false, false, FName(TEXT("PlayerEscaped"))),
		HeistContractOutcomeReasons::RequiredTargetMissing());
	TestFalse(TEXT("Every terminal reason maps to non-empty player-facing text"),
		HeistContractOutcomeReasons::ToDisplayText(HeistContractOutcomeReasons::MatchTimerExpired()).IsEmpty());

	return true;
}

#endif
