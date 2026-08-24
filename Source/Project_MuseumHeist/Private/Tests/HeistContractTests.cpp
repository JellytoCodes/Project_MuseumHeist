#if WITH_DEV_AUTOMATION_TESTS

#include "Core/HeistTypes.h"
#include "Data/HeistArtifactDataTypes.h"
#include "Data/HeistContractDataTypes.h"
#include "Misc/AutomationTest.h"
#include "World/Actors/Loot/HeistObjectDisplayCaseActor.h"
#include "World/Actors/Loot/HeistPaintingDisplayCaseActor.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistReplicaAcceptanceContractTest, "ProjectMuseumHeist.Forgery.ReplicaAcceptanceContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistReplicaAcceptanceContractTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Configured values below the v1 floor resolve to 70"), FMath::IsNearlyEqual(HeistReplicaAcceptance::ResolveMinimumQualityScore(0.60f), 70.0f));
	TestTrue(TEXT("Exactly 70 is accepted"), HeistReplicaAcceptance::MeetsMinimumQuality(70.0f, 0.60f));
	TestFalse(TEXT("Below 70 is rejected"), HeistReplicaAcceptance::MeetsMinimumQuality(69.99f, 0.60f));

	float SurfaceDelay = 0.0f;
	TestTrue(TEXT("Surface ambient inspection uses the configured delay"), AHeistPaintingDisplayCaseActor::CalculateInspectionSchedule(8.0f, SurfaceDelay));
	TestTrue(TEXT("Surface inspection delay is preserved"), FMath::IsNearlyEqual(SurfaceDelay, 8.0f));

	float ObjectDelay = 0.0f;
	TestTrue(TEXT("Object ambient inspection uses the configured delay"), AHeistObjectDisplayCaseActor::CalculateInspectionSchedule(8.0f, ObjectDelay));
	TestTrue(TEXT("Object inspection delay is preserved"), FMath::IsNearlyEqual(ObjectDelay, 8.0f));

	float RejectedDelay = 0.0f;
	TestFalse(TEXT("A negative surface inspection delay is rejected"), AHeistPaintingDisplayCaseActor::CalculateInspectionSchedule(-1.0f, RejectedDelay));
	TestFalse(TEXT("A non-finite object inspection delay is rejected"),
		AHeistObjectDisplayCaseActor::CalculateInspectionSchedule(std::numeric_limits<float>::quiet_NaN(), RejectedDelay));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistContractDefinitionTest, "ProjectMuseumHeist.Contract.Definition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistContractDefinitionTest::RunTest(const FString& Parameters)
{
	const FHeistContractDataRow Definition;
	FString FailureReason;

	TestFalse(TEXT("Public one-player start is rejected"), HeistSessionContract::IsPublicStartPlayerCountSupported(1));
	TestTrue(TEXT("Public two-player start is supported"), HeistSessionContract::IsPublicStartPlayerCountSupported(2));
	TestTrue(TEXT("Public four-player start is supported"), HeistSessionContract::IsPublicStartPlayerCountSupported(4));
	TestFalse(TEXT("Public five-player start is rejected"), HeistSessionContract::IsPublicStartPlayerCountSupported(5));
	TestTrue(TEXT("One-player snapshot remains available for direct PIE and automation"), HeistSessionContract::IsSnapshotStartPlayerCountSupported(1));
	TestFalse(TEXT("Object Assembly runtime is disabled for the v1 release"), HeistReleaseFeatures::IsObjectAssemblyRuntimeEnabled());

	TestTrue(TEXT("Default contract definition is valid"), Definition.IsRuntimeDefinitionValid(&FailureReason));
	TestTrue(TEXT("Valid definition has no failure reason"), FailureReason.IsEmpty());
	TestEqual(TEXT("One-player quota"), Definition.ResolveLootValueQuota(1), 4000);
	TestEqual(TEXT("Two-player quota"), Definition.ResolveLootValueQuota(2), 6400);
	TestEqual(TEXT("Three-player quota"), Definition.ResolveLootValueQuota(3), 8800);
	TestEqual(TEXT("Four-player quota"), Definition.ResolveLootValueQuota(4), 11200);
	TestEqual(TEXT("Player count below range clamps to one"), Definition.ResolveLootValueQuota(0), 4000);
	TestEqual(TEXT("Player count above range clamps to four"), Definition.ResolveLootValueQuota(5), 11200);
	TestEqual(TEXT("Three-player minimum optional exhibits"), Definition.ResolveMinimumOptionalExhibitCount(3), 4);
	TestEqual(TEXT("Three-player maximum optional exhibits"), Definition.ResolveMaximumOptionalExhibitCount(3), 19);
	TestEqual(TEXT("Per-map Surface template catalog target"), Definition.SurfaceTemplateCatalogSize, 40);
	TestEqual(TEXT("Per-match Painting exhibit target"), Definition.MatchPaintingExhibitCount, 20);

	FHeistContractDataRow InvalidDefinition = Definition;
	InvalidDefinition.PlayerCountQuotaMultipliers = {1.0f, 1.6f, 1.5f, 2.8f};
	TestFalse(TEXT("Decreasing player quota is rejected"), InvalidDefinition.IsRuntimeDefinitionValid(&FailureReason));
	TestEqual(TEXT("Decreasing quota failure reason"), FailureReason, FString(TEXT("NonMonotonicResolvedQuota")));

	InvalidDefinition = Definition;
	InvalidDefinition.MaximumOptionalExhibits.SetNum(3);
	TestFalse(TEXT("Incomplete player count arrays are rejected"), InvalidDefinition.IsRuntimeDefinitionValid(&FailureReason));
	TestEqual(TEXT("Array count failure reason"), FailureReason, FString(TEXT("PlayerCountArraySizeMismatch")));

	InvalidDefinition = Definition;
	InvalidDefinition.MatchPaintingExhibitCount = InvalidDefinition.SurfaceTemplateCatalogSize + 1;
	TestFalse(TEXT("A match cannot assign more Painting exhibits than the map catalog"), InvalidDefinition.IsRuntimeDefinitionValid(&FailureReason));
	TestEqual(TEXT("Surface catalog range failure reason"), FailureReason, FString(TEXT("InvalidSurfaceExhibitCatalogRange")));

	InvalidDefinition = Definition;
	InvalidDefinition.MaximumOptionalExhibits[0] = InvalidDefinition.MatchPaintingExhibitCount;
	TestFalse(TEXT("Optional exhibit capacity reserves one slot for the required target"), InvalidDefinition.IsRuntimeDefinitionValid(&FailureReason));
	TestEqual(TEXT("Optional exhibit capacity failure reason"), FailureReason, FString(TEXT("OptionalExhibitRangeExceedsMatchPaintingCount")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistContractSnapshotTest, "ProjectMuseumHeist.Contract.Snapshot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistContractSnapshotTest::RunTest(const FString& Parameters)
{
	FHeistContractSnapshot Snapshot;
	Snapshot.ContractId = FName(TEXT("Contract_MuseumSwap_01"));
	Snapshot.MapId = FName(TEXT("M01"));
	Snapshot.ContractEndServerTime = 1200.0f;
	Snapshot.AssignmentSeed = 12345;
	Snapshot.ContractStartPlayerCount = 2;
	Snapshot.RequiredTargetArtifactId = FName(TEXT("Artifact_Painting_M01"));
	Snapshot.RequiredTargetDisplayName = FText::FromString(TEXT("테스트 필수 작품"));
	Snapshot.RequiredTargetCaseId = FName(TEXT("Case_M01_Target"));
	Snapshot.LootValueQuota = 4000;
	Snapshot.Revision = 1;

	TestTrue(TEXT("Populated snapshot is initialized"), Snapshot.IsInitialized());
	FHeistContractSnapshot MissingStartPlayerCount = Snapshot;
	MissingStartPlayerCount.ContractStartPlayerCount = 0;
	TestFalse(TEXT("Contract start player count is required"), MissingStartPlayerCount.IsInitialized());
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistContractOutcomeMatrixTest, "ProjectMuseumHeist.Contract.OutcomeMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistContractOutcomeMatrixTest::RunTest(const FString& Parameters)
{
	auto MakeSnapshot = []
	{
		FHeistContractSnapshot Snapshot;
		Snapshot.ContractId = FName(TEXT("Contract_MuseumSwap_01"));
		Snapshot.MapId = FName(TEXT("M01"));
		Snapshot.ContractEndServerTime = 1200.0f;
		Snapshot.AssignmentSeed = 12345;
		Snapshot.ContractStartPlayerCount = 2;
		Snapshot.RequiredTargetArtifactId = FName(TEXT("Artifact_Painting_M01"));
		Snapshot.RequiredTargetDisplayName = FText::FromString(TEXT("테스트 필수 작품"));
		Snapshot.RequiredTargetCaseId = FName(TEXT("Case_M01_Target"));
		Snapshot.LootValueQuota = 4000;
		Snapshot.Revision = 1;
		return Snapshot;
	};

	auto ResolveAndValidate = [this](FHeistContractSnapshot Snapshot, const bool bAtLeastOneCrewEscaped,
		const bool bAllRemainingCrewArrested, const bool bAllCrewDisconnected, const FName TerminalTrigger,
		const EHeistContractOutcome ExpectedOutcome, const FName ExpectedReason, const TCHAR* Scenario)
	{
		Snapshot.Outcome = Snapshot.ResolveTerminalOutcome(bAtLeastOneCrewEscaped);
		Snapshot.OutcomeReasonId = HeistContractOutcomeReasons::Resolve(Snapshot.Outcome, Snapshot.bRequiredTargetSecured,
			bAtLeastOneCrewEscaped, bAllRemainingCrewArrested, bAllCrewDisconnected, TerminalTrigger);

		TestEqual(FString::Printf(TEXT("%s resolves the expected outcome"), Scenario), static_cast<uint8>(Snapshot.Outcome),
			static_cast<uint8>(ExpectedOutcome));
		TestEqual(FString::Printf(TEXT("%s resolves the expected reason"), Scenario), Snapshot.OutcomeReasonId, ExpectedReason);
		TestTrue(FString::Printf(TEXT("%s produces a consistent replicated snapshot"), Scenario), Snapshot.IsOutcomeConsistent());
		TestFalse(FString::Printf(TEXT("%s exposes a player-facing natural-language reason"), Scenario), Snapshot.GetOutcomeReasonText().IsEmpty());
	};

	FHeistContractSnapshot Snapshot = MakeSnapshot();
	Snapshot.bRequiredTargetSecured = true;
	Snapshot.SecuredValue = Snapshot.LootValueQuota;
	ResolveAndValidate(Snapshot, true, false, false, FName(TEXT("Lockdown")), EHeistContractOutcome::Success,
		HeistContractOutcomeReasons::ContractComplete(), TEXT("Committed target and quota before same-frame lockdown"));

	Snapshot = MakeSnapshot();
	Snapshot.bRequiredTargetSecured = true;
	Snapshot.SecuredValue = Snapshot.LootValueQuota - 1;
	ResolveAndValidate(Snapshot, true, false, false, FName(TEXT("MatchTimerExpired")), EHeistContractOutcome::PartialHaul,
		HeistContractOutcomeReasons::RequiredTargetSecuredQuotaShort(), TEXT("Committed target below quota before same-frame timer"));

	Snapshot = MakeSnapshot();
	Snapshot.CarriedValue = Snapshot.LootValueQuota * 2;
	Snapshot.SecuredValue = Snapshot.LootValueQuota;
	ResolveAndValidate(Snapshot, true, false, false, FName(TEXT("PlayerEscaped")), EHeistContractOutcome::Failed,
		HeistContractOutcomeReasons::RequiredTargetMissing(), TEXT("Quota without required target"));

	Snapshot = MakeSnapshot();
	Snapshot.bRequiredTargetSecured = true;
	Snapshot.CarriedValue = Snapshot.LootValueQuota * 2;
	Snapshot.SecuredValue = Snapshot.LootValueQuota - 1;
	ResolveAndValidate(Snapshot, true, false, false, FName(TEXT("PlayerEscaped")), EHeistContractOutcome::PartialHaul,
		HeistContractOutcomeReasons::RequiredTargetSecuredQuotaShort(), TEXT("Carried value excluded from quota"));

	Snapshot = MakeSnapshot();
	Snapshot.bRequiredTargetSecured = true;
	Snapshot.SecuredValue = Snapshot.LootValueQuota;
	ResolveAndValidate(Snapshot, false, false, false, FName(TEXT("PlayerEscaped")), EHeistContractOutcome::Failed,
		HeistContractOutcomeReasons::NoCrewEscaped(), TEXT("Contract progress without an escaped crew member"));

	Snapshot = MakeSnapshot();
	ResolveAndValidate(Snapshot, false, true, true, FName(TEXT("Lockdown")), EHeistContractOutcome::Failed,
		HeistContractOutcomeReasons::LockdownBeforeContractComplete(), TEXT("Lockdown priority over lifecycle failures"));
	ResolveAndValidate(Snapshot, false, true, true, FName(TEXT("MatchTimerExpired")), EHeistContractOutcome::Failed,
		HeistContractOutcomeReasons::MatchTimerExpired(), TEXT("Match timer priority over lifecycle failures"));
	ResolveAndValidate(Snapshot, false, true, true, FName(TEXT("PlayerArrested")), EHeistContractOutcome::Failed,
		HeistContractOutcomeReasons::AllRemainingCrewArrested(), TEXT("All remaining crew arrested priority over disconnect"));
	ResolveAndValidate(Snapshot, false, false, true, FName(TEXT("PlayerDisconnected")), EHeistContractOutcome::Failed,
		HeistContractOutcomeReasons::AllCrewDisconnected(), TEXT("All crew disconnected"));

	const FName AllReasonIds[] = {
		HeistContractOutcomeReasons::ContractComplete(),
		HeistContractOutcomeReasons::RequiredTargetSecuredQuotaShort(),
		HeistContractOutcomeReasons::LockdownBeforeContractComplete(),
		HeistContractOutcomeReasons::MatchTimerExpired(),
		HeistContractOutcomeReasons::AllRemainingCrewArrested(),
		HeistContractOutcomeReasons::AllCrewDisconnected(),
		HeistContractOutcomeReasons::NoCrewEscaped(),
		HeistContractOutcomeReasons::RequiredTargetMissing()
	};
	for (const FName ReasonId : AllReasonIds)
	{
		TestFalse(FString::Printf(TEXT("Reason %s has player-facing text"), *ReasonId.ToString()), HeistContractOutcomeReasons::ToDisplayText(ReasonId).IsEmpty());
	}

	Snapshot = MakeSnapshot();
	Snapshot.Outcome = EHeistContractOutcome::Failed;
	Snapshot.OutcomeReasonId = FName(TEXT("UnknownOutcomeReason"));
	TestFalse(TEXT("Unknown failure reason cannot form a valid replicated outcome"), Snapshot.IsOutcomeConsistent());
	TestTrue(TEXT("Unknown failure reason has no player-facing text"), Snapshot.GetOutcomeReasonText().IsEmpty());

	return true;
}

#endif
