#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Core/HeistGameState.h"
#include "Core/HeistTypes.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UI/ViewModels/HeistResultViewModel.h"
#include "UI/Result/Widgets/HeistResultPlayerRowWidget.h"
#include "UI/Widgets/HeistResultWidget.h"

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
	Result.RequiredTargetArtifactId = FName(TEXT("Artifact_Painting_M01"));
	Result.RequiredTargetDisplayName = FText::FromString(TEXT("별이 빛나는 위작"));
	Result.LootValueQuota = 4000;
	Result.SecuredValue = 4500;
	Result.TeamReward = 4200;
	Result.Revision = 1;
	TestTrue(TEXT("Complete replicated team result is valid"), Result.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistResultScreenPresentationTest, "ProjectMuseumHeist.Result.ScreenPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistResultScreenPresentationTest::RunTest(const FString& Parameters)
{
	FHeistTeamResult TeamResult;
	TeamResult.Outcome = EHeistContractOutcome::Success;
	TeamResult.OutcomeReasonId = HeistContractOutcomeReasons::ContractComplete();
	TeamResult.RequiredTargetArtifactId = FName(TEXT("Artifact_Painting_M01"));
	TeamResult.RequiredTargetDisplayName = FText::FromString(TEXT("별이 빛나는 위작"));
	TeamResult.bRequiredTargetSecured = true;
	TeamResult.LootValueQuota = 4000;
	TeamResult.SecuredValue = 5200;
	TeamResult.ExtraValue = 1200;
	TeamResult.RequiredTargetValue = 2500;
	TeamResult.SecuredLooseLootValue = 2700;
	TeamResult.ForgeryRewardMultiplier = 1.15f;
	TeamResult.StealthRewardMultiplier = 0.90f;
	TeamResult.ArrestPenalty = 500;
	TeamResult.TeamReward = 4788;
	TeamResult.Revision = 1;

	FHeistReplicaRecapEntry& Recap = TeamResult.ReplicaRecap.AddDefaulted_GetRef();
	Recap.CaseId = FName(TEXT("Case_M01_Target"));
	Recap.ArtifactId = TeamResult.RequiredTargetArtifactId;
	Recap.ArtifactDisplayName = TeamResult.RequiredTargetDisplayName;
	Recap.TemplateId = FName(TEXT("Template_M01_Portrait_05"));
	Recap.ForgeryType = EHeistForgeryType::Drawing;
	Recap.QualityScore = 84.0f;
	Recap.bRequiredTarget = true;

	TestEqual(TEXT("Success outcome is player-facing Korean"), UHeistResultViewModel::BuildOutcomeDisplayText(EHeistContractOutcome::Success).ToString(), FString(TEXT("임무 성공")));
	TestEqual(TEXT("Partial haul is distinct from full success"), UHeistResultViewModel::BuildOutcomeDisplayText(EHeistContractOutcome::PartialHaul).ToString(), FString(TEXT("부분 성공")));
	TestEqual(TEXT("Failure outcome is player-facing Korean"), UHeistResultViewModel::BuildOutcomeDisplayText(EHeistContractOutcome::Failed).ToString(), FString(TEXT("임무 실패")));

	const FString RecapSummary = UHeistResultViewModel::BuildReplicaRecapSummaryText(TeamResult).ToString();
	TestTrue(TEXT("Replica recap identifies required target and actual type/quality"), RecapSummary.Contains(TEXT("필수 목표")) &&
		RecapSummary.Contains(TEXT("별이 빛나는 위작")) && RecapSummary.Contains(TEXT("그림")) && RecapSummary.Contains(TEXT("84")));
	const FString ReplicaCardTitle = UHeistResultWidget::BuildReplicaCardTitle(Recap).ToString();
	TestTrue(TEXT("Surface-only replica card keeps target, name, and quality"), ReplicaCardTitle.Contains(TEXT("필수 목표")) &&
		ReplicaCardTitle.Contains(TEXT("별이 빛나는 위작")) && ReplicaCardTitle.Contains(TEXT("84")));
	TestFalse(TEXT("Surface-only replica card omits redundant type labels"), ReplicaCardTitle.Contains(TEXT("그림")) || ReplicaCardTitle.Contains(TEXT("조립")));
	TestFalse(TEXT("Team result presentation does not create Winner or Rank"), RecapSummary.Contains(TEXT("Winner")) || RecapSummary.Contains(TEXT("Rank")) ||
		RecapSummary.Contains(TEXT("우승")) || RecapSummary.Contains(TEXT("순위")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistReplicaRecapPayloadTest, "ProjectMuseumHeist.Result.ReplicaRecapPayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistReplicaRecapPayloadTest::RunTest(const FString& Parameters)
{
	FHeistReplicaRecapEntry PaintingRecap;
	PaintingRecap.CaseId = FName(TEXT("Case_M01_Target"));
	PaintingRecap.ArtifactId = FName(TEXT("Artifact_Painting_M01"));
	PaintingRecap.ArtifactDisplayName = FText::FromString(TEXT("별이 빛나는 위작"));
	PaintingRecap.TemplateId = FName(TEXT("Template_M01_Portrait_05"));
	PaintingRecap.ForgeryType = EHeistForgeryType::Drawing;
	PaintingRecap.QualityScore = 84.0f;
	PaintingRecap.bRequiredTarget = true;
	PaintingRecap.PaintingResolution = FHeistReplicaRecapEntry::PaintingThumbnailResolution;
	PaintingRecap.PaintingPalette = {FColor::Red, FColor::Blue};
	PaintingRecap.PaintingPackedPaletteIndices.Init(0x21, FMath::DivideAndRoundUp(
		PaintingRecap.PaintingResolution * PaintingRecap.PaintingResolution, 2));

	TArray64<uint8> PaintingPixels;
	TestTrue(TEXT("Painting recap decodes the actual submitted palette payload"),
		UHeistResultWidget::DecodePaintingRecapPixels(PaintingRecap, FColor::Black, PaintingPixels));
	TestEqual(TEXT("Painting recap produces BGRA pixels"), PaintingPixels.Num(), static_cast<int64>(PaintingRecap.PaintingResolution * PaintingRecap.PaintingResolution * 4));
	TestEqual(TEXT("First packed nibble resolves palette color one"), PaintingPixels[2], static_cast<uint8>(255));
	TestEqual(TEXT("Second packed nibble resolves palette color two"), PaintingPixels[4], static_cast<uint8>(255));

	TArray<uint8> PaintingBytes;
	FMemoryWriter PaintingWriter(PaintingBytes);
	bool bPaintingSaved = false;
	PaintingRecap.NetSerialize(PaintingWriter, nullptr, bPaintingSaved);
	TestTrue(TEXT("Painting recap network payload saves"), bPaintingSaved && !PaintingWriter.IsError());
	FHeistReplicaRecapEntry PaintingCopy;
	FMemoryReader PaintingReader(PaintingBytes);
	bool bPaintingLoaded = false;
	PaintingCopy.NetSerialize(PaintingReader, nullptr, bPaintingLoaded);
	TestTrue(TEXT("Painting recap network payload round-trips"), bPaintingLoaded && !PaintingReader.IsError() && PaintingCopy == PaintingRecap);

	FHeistReplicaRecapEntry AssemblyRecap;
	AssemblyRecap.CaseId = FName(TEXT("Case_M01_Object"));
	AssemblyRecap.ArtifactId = FName(TEXT("Artifact_Sculpture_M01"));
	AssemblyRecap.ArtifactDisplayName = FText::FromString(TEXT("날개 달린 큐레이터"));
	AssemblyRecap.TemplateId = FName(TEXT("Template_Sculpture_Gallery_03"));
	AssemblyRecap.ForgeryType = EHeistForgeryType::Assembly;
	AssemblyRecap.QualityScore = 91.0f;
	FHeistObjectAssemblyEntry& AssemblyEntry = AssemblyRecap.AssemblyEntries.AddDefaulted_GetRef();
	AssemblyEntry.PartId = FName(TEXT("Part_Sculpture_Gallery_Head"));
	AssemblyEntry.SocketId = FName(TEXT("Head"));
	AssemblyEntry.QuantizedOrientation = 4;

	TArray<uint8> AssemblyBytes;
	FMemoryWriter AssemblyWriter(AssemblyBytes);
	bool bAssemblySaved = false;
	AssemblyRecap.NetSerialize(AssemblyWriter, nullptr, bAssemblySaved);
	TestTrue(TEXT("Assembly recap network payload saves"), bAssemblySaved && !AssemblyWriter.IsError());
	FHeistReplicaRecapEntry AssemblyCopy;
	FMemoryReader AssemblyReader(AssemblyBytes);
	bool bAssemblyLoaded = false;
	AssemblyCopy.NetSerialize(AssemblyReader, nullptr, bAssemblyLoaded);
	TestTrue(TEXT("Assembly recap network payload round-trips"), bAssemblyLoaded && !AssemblyReader.IsError() && AssemblyCopy == AssemblyRecap);
	TestTrue(TEXT("Assembly recap uses the same head socket anchor as the minigame"),
		UHeistResultWidget::ResolveAssemblyRecapSocketAnchor(AssemblyEntry.SocketId).Equals(FVector2D(0.50, 0.23)));
	TestTrue(TEXT("Assembly recap preserves submitted orientation"), FMath::IsNearlyEqual(UHeistResultWidget::ResolveAssemblyRecapPartAngle(4), 90.0f));
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
	PlayerResult.PlayerDisplayName = TEXT("STEAM TESTER");
	PlayerResult.PlatformUserId = TEXT("76561198000000001");
	PlayerResult.bEscaped = true;
	PlayerResult.Contribution = Contribution;
	FHeistPlayerResult ReplicatedCopy = PlayerResult;
	TestTrue(TEXT("Contribution survives the player result snapshot contract"), ReplicatedCopy == PlayerResult);
	TestEqual(TEXT("Surface count remains descriptive data"), ReplicatedCopy.Contribution.SurfaceForgeries, 2);
	TestEqual(TEXT("Secured loot remains descriptive data"), ReplicatedCopy.Contribution.SecuredLootValue, 1500);
	TestEqual(TEXT("Result snapshot preserves the platform display name"), ReplicatedCopy.PlayerDisplayName, FString(TEXT("STEAM TESTER")));
	TestEqual(TEXT("Result snapshot preserves the platform user id for the avatar"), ReplicatedCopy.PlatformUserId, FString(TEXT("76561198000000001")));
	TestEqual(TEXT("Escaped player state is explicit"), UHeistResultPlayerRowWidget::BuildPlayerStateText(ReplicatedCopy).ToString(), FString(TEXT("탈출")));
	ReplicatedCopy.bEscaped = false;
	ReplicatedCopy.bArrested = true;
	TestEqual(TEXT("Arrested player state is not collapsed into unresolved"), UHeistResultPlayerRowWidget::BuildPlayerStateText(ReplicatedCopy).ToString(), FString(TEXT("체포")));

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistSoundPingAcceptanceAggregationTest, "ProjectMuseumHeist.Result.SoundPingAcceptanceAggregation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistSoundPingAcceptanceAggregationTest::RunTest(const FString& Parameters)
{
	FHeistSoundPingEventReported Delegate;
	Delegate.AddLambda([](const FHeistSoundPingEvent&, int32* InOutAcceptedGuardCount) { ++*InOutAcceptedGuardCount; });
	Delegate.AddLambda([](const FHeistSoundPingEvent&, int32* InOutAcceptedGuardCount) { ++*InOutAcceptedGuardCount; });

	int32 AcceptedGuardCount = 0;
	Delegate.Broadcast(FHeistSoundPingEvent(), &AcceptedGuardCount);
	TestEqual(TEXT("Synchronous sound-ping listeners aggregate accepted guard reactions"), AcceptedGuardCount, 2);
	return true;
}

#endif
