#if WITH_DEV_AUTOMATION_TESTS

#include "AI/HeistGuardAIController.h"
#include "AI/HeistGuardStateComponent.h"
#include "Core/HeistGameMode.h"
#include "Data/HeistGameBalanceDataAsset.h"
#include "Inventory/HeistItemDataTypes.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistSecurityIncidentPolicyTest, "ProjectMuseumHeist.W8.SecurityIncidentPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistSecurityIncidentPolicyTest::RunTest(const FString& Parameters)
{
	TSet<FName> ProcessedIncidentIds;
	TSet<FName> ProcessedInvestigationIds;
	const FName CameraIncidentId(TEXT("CCTV_Camera01_Revision7"));
	const FName LaserIncidentId(TEXT("Laser_Barrier02_Revision3"));

	TestFalse(TEXT("None cannot become a one-shot security id"), AHeistGameMode::TryConsumeOneShotSecurityId(ProcessedIncidentIds, NAME_None));
	TestTrue(TEXT("First security incident is consumed"), AHeistGameMode::TryConsumeOneShotSecurityId(ProcessedIncidentIds, CameraIncidentId));
	TestFalse(TEXT("Duplicate security incident is blocked"), AHeistGameMode::TryConsumeOneShotSecurityId(ProcessedIncidentIds, CameraIncidentId));
	TestTrue(TEXT("A distinct security incident remains independent"), AHeistGameMode::TryConsumeOneShotSecurityId(ProcessedIncidentIds, LaserIncidentId));
	TestEqual(TEXT("Only unique security incidents remain recorded"), ProcessedIncidentIds.Num(), 2);

	TestTrue(TEXT("The same incident can independently consume its one guard investigation"),
		AHeistGameMode::TryConsumeOneShotSecurityId(ProcessedInvestigationIds, CameraIncidentId));
	TestFalse(TEXT("A duplicate guard investigation for one incident is blocked"),
		AHeistGameMode::TryConsumeOneShotSecurityId(ProcessedInvestigationIds, CameraIncidentId));

	TestTrue(TEXT("Patrol guards can accept a security investigation"),
		AHeistGuardAIController::IsSecurityInvestigationStateEligible(EHeistGuardState::Patrol));
	TestTrue(TEXT("Returning guards can accept a security investigation"),
		AHeistGuardAIController::IsSecurityInvestigationStateEligible(EHeistGuardState::ReturnToPatrol));
	for (const EHeistGuardState GuardState : {EHeistGuardState::Disabled, EHeistGuardState::Stunned, EHeistGuardState::InvestigateNoise,
			 EHeistGuardState::ChasePlayer, EHeistGuardState::SearchLastKnownLocation, EHeistGuardState::InspectExhibit})
	{
		TestFalse(FString::Printf(TEXT("Busy guard state %s cannot be reassigned"), *UEnum::GetValueAsString(GuardState)),
			AHeistGuardAIController::IsSecurityInvestigationStateEligible(GuardState));
	}

	UHeistGuardStateComponent* GuardStateComponent = NewObject<UHeistGuardStateComponent>();
	TestNotNull(TEXT("Guard state component can be created for policy validation"), GuardStateComponent);
	if (GuardStateComponent)
	{
		FHeistGuardDataRow GuardProfile;
		GuardProfile.InvestigateDuration = 4.25f;
		GuardStateComponent->ConfigureGuardProfile(GuardProfile);
		TestTrue(TEXT("A new investigation uses the configured profile duration instead of an empty pending duration"),
			FMath::IsNearlyEqual(GuardStateComponent->GetConfiguredInvestigateDuration(), 4.25f));
		TestTrue(TEXT("Pending confirmation remains empty before an investigation begins"),
			FMath::IsNearlyZero(GuardStateComponent->GetInvestigateConfirmationDuration()));
	}

	const UHeistGameBalanceDataAsset* BalanceDefaults = GetDefault<UHeistGameBalanceDataAsset>();
	TestNotNull(TEXT("Game balance defaults exist"), BalanceDefaults);
	if (BalanceDefaults)
	{
		TestTrue(TEXT("Security incidents own a positive nearby-guard radius"), BalanceDefaults->SecurityIncidentInvestigationRadius > 0.0f);
		TestTrue(TEXT("Forgery timeout keeps its independent positive nearby-guard radius"), BalanceDefaults->ForgeryTimeoutInvestigationRadius > 0.0f);
		TestTrue(TEXT("CCTV evaluation interval is positive"), BalanceDefaults->SecurityCameraEvaluationIntervalSeconds > 0.0f);
		TestTrue(TEXT("CCTV build-up stays inside the Rev14 range"),
			FMath::IsWithinInclusive(BalanceDefaults->SecurityCameraDetectionBuildUpSeconds, 1.2f, 1.5f));
		TestTrue(TEXT("Laser hold duration stays inside the Rev14 range"),
			FMath::IsWithinInclusive(BalanceDefaults->SecurityLaserHoldDurationSeconds, 2.0f, 5.0f));
		TestTrue(TEXT("Laser rearm grace is non-negative"), BalanceDefaults->SecurityLaserRearmGraceSeconds >= 0.0f);
	}

	return true;
}

#endif
