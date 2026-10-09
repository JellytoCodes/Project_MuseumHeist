#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "AI/HeistGuardAIController.h"
#include "AI/HeistGuardCharacter.h"
#include "AI/HeistGuardStateComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/HeistCollisionChannels.h"
#include "Core/HeistGameMode.h"
#include "Data/HeistGameBalanceDataAsset.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Inventory/HeistItemDataTypes.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "NavigationSystem.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Navigation/CrowdManager.h"
#include "Tests/AutomationEditorCommon.h"

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
		TestTrue(TEXT("Alert meter advances in half-step increments"), FMath::IsNearlyEqual(BalanceDefaults->AlertMeterStep, 0.5f));
		TestTrue(TEXT("Alert meter owns ten full stages"), FMath::IsNearlyEqual(BalanceDefaults->AlertMeterMaximum, 10.0f));
		TestTrue(TEXT("A successful guard capture adds one full Alert stage"),
			FMath::IsNearlyEqual(BalanceDefaults->GuardCaptureAlertIncrease, 1.0f));
		TestTrue(TEXT("CCTV and Laser incidents add one half Alert stage"),
			FMath::IsNearlyEqual(BalanceDefaults->SecurityIncidentAlertIncrease, 0.5f));
		TestTrue(TEXT("Searching behavior starts at Alert 4"), FMath::IsNearlyEqual(BalanceDefaults->SearchingAlertMeterThreshold, 4.0f));
		TestTrue(TEXT("Alarmed behavior starts at Alert 7"), FMath::IsNearlyEqual(BalanceDefaults->AlarmedAlertMeterThreshold, 7.0f));
		TestTrue(TEXT("Lockdown starts immediately at Alert 10"), FMath::IsNearlyEqual(BalanceDefaults->LockdownAlertMeterThreshold, 10.0f));
		TestTrue(TEXT("Security incidents own a positive nearby-guard radius"), BalanceDefaults->SecurityIncidentInvestigationRadius > 0.0f);
		TestTrue(TEXT("Forgery timeout keeps its independent positive nearby-guard radius"), BalanceDefaults->ForgeryTimeoutInvestigationRadius > 0.0f);
		TestTrue(TEXT("CCTV evaluation interval is positive"), BalanceDefaults->SecurityCameraEvaluationIntervalSeconds > 0.0f);
		TestTrue(TEXT("CCTV build-up stays inside the Rev14 range"),
			FMath::IsWithinInclusive(BalanceDefaults->SecurityCameraDetectionBuildUpSeconds, 1.8f, 2.25f));
		TestTrue(TEXT("Laser hold duration stays inside the Rev14 range"),
			FMath::IsWithinInclusive(BalanceDefaults->SecurityLaserHoldDurationSeconds, 2.0f, 5.0f));
		TestTrue(TEXT("Laser rearm grace is non-negative"), BalanceDefaults->SecurityLaserRearmGraceSeconds >= 0.0f);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistGuardAvoidanceLifecycleTest, "ProjectMuseumHeist.Security.GuardAvoidanceLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistGuardAvoidanceLifecycleTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	UClass* GuardClass = LoadClass<AHeistGuardCharacter>(nullptr, TEXT("/Game/Blueprints/Guard/BP_Guard.BP_Guard_C"));
	if (!TestNotNull(TEXT("The shared authored guard Blueprint exists"), GuardClass)) return false;
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Spawn.bDeferConstruction = true;
	AHeistGuardCharacter* Left = World->SpawnActor<AHeistGuardCharacter>(GuardClass, FVector(-100.f, 0.f, 100.f), FRotator::ZeroRotator, Spawn);
	AHeistGuardCharacter* Right = World->SpawnActor<AHeistGuardCharacter>(GuardClass, FVector(100.f, 0.f, 100.f), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Left guard spawns"), Left) || !TestNotNull(TEXT("Right guard spawns"), Right)) return false;
	for (AHeistGuardCharacter* Guard : {Left, Right})
	{
		// This solver fixture has no patrol navigation or controller. Finish normal
		// Game-world component initialization before dispatching actor BeginPlay.
		Guard->AutoPossessAI = EAutoPossessAI::Disabled;
		Guard->FinishSpawning(Guard->GetActorTransform());
		if (!TestTrue(TEXT("Character movement has its initialized guard owner"), Guard->GetCharacterMovement()->GetCharacterOwner() == Guard)) return false;
		Guard->DispatchBeginPlay();
	}
	UCharacterMovementComponent* LeftMove = Left->GetCharacterMovement();
	UCharacterMovementComponent* RightMove = Right->GetCharacterMovement();
	TestFalse(TEXT("Character RVO does not compete with crowd steering"), LeftMove->bUseRVOAvoidance || RightMove->bUseRVOAvoidance);
	TestTrue(TEXT("Avoidance keeps the guards' solid collision"), Left->GetActorEnableCollision() && Right->GetActorEnableCollision());
	TestEqual(TEXT("Guard-to-guard collision stays blocking"), Left->GetCapsuleComponent()->GetCollisionResponseToChannel(HeistCollisionChannels::Guard), ECR_Block);

	Left->SetDifficultyActive(false);
	TestFalse(TEXT("An inactive difficulty slot disables avoidance"), LeftMove->bUseRVOAvoidance);
	TestFalse(TEXT("The existing inactive presentation still disables collision"), Left->GetActorEnableCollision());
	Left->SetDifficultyActive(true);
	TestFalse(TEXT("Reactivation keeps the second RVO solver disabled"), LeftMove->bUseRVOAvoidance);
	TestTrue(TEXT("Reactivation restores the existing collision"), Left->GetActorEnableCollision());
	TestTrue(TEXT("A stunned guard enters the existing server state"), Left->GetGuardStateComponent()->ApplyStun(1.f));
	TestTrue(TEXT("A visible stunned guard keeps solid collision"), Left->GetActorEnableCollision());
	TestEqual(TEXT("Stun still stops movement without a collision bypass"), LeftMove->MaxWalkSpeed, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistGuardCrowdNavigationTest, "ProjectMuseumHeist.Security.GuardCrowdNavigation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistGuardCrowdNavigationTest::RunTest(const FString& Parameters)
{
	FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Maps/M03_GlasshousePrototype"));
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Saved M03 opens for native crowd testing"), World)) return false;
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	UCrowdManager* CrowdManager = UCrowdManager::GetCurrent(World);
	if (!TestNotNull(TEXT("The authored navigation system exists"), Navigation) ||
		!TestNotNull(TEXT("The existing world owns the real engine crowd manager"), CrowdManager)) return false;
	UClass* GuardClass = LoadClass<AHeistGuardCharacter>(nullptr, TEXT("/Game/Blueprints/Guard/BP_Guard.BP_Guard_C"));
	UClass* ControllerClass = LoadClass<AHeistGuardAIController>(nullptr, TEXT("/Game/Blueprints/Guard/AIC_Guard.AIC_Guard_C"));
	if (!TestNotNull(TEXT("The canonical guard exists"), GuardClass) || !TestNotNull(TEXT("The canonical controller exists"), ControllerClass)) return false;
	TestNotNull(TEXT("The canonical controller replaces its existing path-following slot"),
		Cast<UCrowdFollowingComponent>(ControllerClass->GetDefaultObject<AHeistGuardAIController>()->GetPathFollowingComponent()));
	FActorSpawnParameters Spawn;
	Spawn.ObjectFlags = RF_Transient;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Spawn.bDeferConstruction = true;
	AHeistGuardCharacter* Left = World->SpawnActor<AHeistGuardCharacter>(GuardClass, FVector(2670.f, 1976.f, 90.f), FRotator::ZeroRotator, Spawn);
	AHeistGuardCharacter* Right = World->SpawnActor<AHeistGuardCharacter>(GuardClass, FVector(2870.f, 1976.f, 90.f), FRotator::ZeroRotator, Spawn);
	TArray<AHeistGuardAIController*> Controllers;
	ON_SCOPE_EXIT
	{
		for (AHeistGuardAIController* Controller : Controllers) { Controller->UnPossess(); Controller->Destroy(); }
		if (Left) Left->Destroy();
		if (Right) Right->Destroy();
	};
	if (!TestNotNull(TEXT("Left transient guard spawns"), Left) || !TestNotNull(TEXT("Right transient guard spawns"), Right)) return false;
	for (AHeistGuardCharacter* Guard : {Left, Right})
	{
		Guard->AutoPossessAI = EAutoPossessAI::Disabled;
		Guard->FinishSpawning(Guard->GetActorTransform());
		UCharacterMovementComponent* Move = Guard->GetCharacterMovement();
		Move->SetUpdatedComponent(nullptr);
		Move->SetUpdatedComponent(Guard->GetCapsuleComponent());
		if (!TestTrue(TEXT("Movement owns the actual guard before BeginPlay"), Move->GetCharacterOwner() == Guard)) return false;
		Guard->DispatchBeginPlay();
		Move->MaxWalkSpeed = 250.f; // explicit native solver input, not an authored profile change
		FActorSpawnParameters ControllerSpawn;
		ControllerSpawn.ObjectFlags = RF_Transient;
		AHeistGuardAIController* Controller = World->SpawnActor<AHeistGuardAIController>(ControllerClass, ControllerSpawn);
		if (!TestNotNull(TEXT("The canonical controller spawns"), Controller)) return false;
		Controllers.Add(Controller);
		Controller->Possess(Guard);
		UCrowdFollowingComponent* Crowd = Cast<UCrowdFollowingComponent>(Controller->GetPathFollowingComponent());
		if (!TestNotNull(TEXT("Possession initializes engine crowd following"), Crowd)) return false;
		TestTrue(TEXT("An active solid guard has a valid actual Detour agent"), CrowdManager->IsAgentValid(Crowd));
		TestFalse(TEXT("Crowd steering does not enable Character RVO"), Move->bUseRVOAvoidance);
		TestEqual(TEXT("The actual solid guard channel remains blocking"), Guard->GetCapsuleComponent()->GetCollisionResponseToChannel(HeistCollisionChannels::Guard), ECR_Block);
	}
	AHeistGuardAIController* LeftController = Controllers[0];
	UCrowdFollowingComponent* LeftCrowd = CastChecked<UCrowdFollowingComponent>(LeftController->GetPathFollowingComponent());
	TestEqual(TEXT("Left corridor move is accepted"), LeftController->MoveToLocation(FVector(3070.f, 1976.f, 10.f), 20.f, true, true, true, false, nullptr, false), EPathFollowingRequestResult::RequestSuccessful);
	TestEqual(TEXT("Right corridor move is accepted"), Controllers[1]->MoveToLocation(FVector(2470.f, 1976.f, 10.f), 20.f, true, true, true, false, nullptr, false), EPathFollowingRequestResult::RequestSuccessful);
	CrowdManager->Tick(.1f);
	const FVector LeftRequested = Left->GetCharacterMovement()->RequestedVelocity;
	const FVector RightRequested = Right->GetCharacterMovement()->RequestedVelocity;
	TestTrue(TEXT("The real crowd solver changes at least one opposing direct request"),
		!LeftRequested.Equals(FVector(250.f, 0.f, 0.f), .1f) || !RightRequested.Equals(FVector(-250.f, 0.f, 0.f), .1f));
	TestTrue(TEXT("Crowd requests remain within the configured speed"), LeftRequested.Size2D() <= 250.1f && RightRequested.Size2D() <= 250.1f);
	TestTrue(TEXT("Steering supplies a nonempty movement request"), !LeftRequested.IsNearlyZero() || !RightRequested.IsNearlyZero());
	Left->SetDifficultyActive(false);
	TestTrue(TEXT("A pooled guard unregisters after its move becomes idle"), LeftCrowd->GetCrowdSimulationState() == ECrowdSimulationState::Disabled && !CrowdManager->IsAgentValid(LeftCrowd));
	Left->SetDifficultyActive(true);
	TestTrue(TEXT("A reused active guard restores actual registration"), LeftCrowd->IsCrowdSimulationEnabled() && CrowdManager->IsAgentValid(LeftCrowd));
	TestTrue(TEXT("The active idle guard remains solid"), Left->GetActorEnableCollision());
	// The editor solver fixture has no authoritative profile owner. Restore its
	// explicit solver input and verify the engine reads live speed after reuse.
	Left->GetCharacterMovement()->MaxWalkSpeed = 250.f;
	TestEqual(TEXT("A reused crowd agent reads the live restored speed"), LeftCrowd->GetCrowdAgentMaxSpeed(), 250.f);
	TestEqual(TEXT("A reused guard accepts a fresh corridor request"), LeftController->MoveToLocation(FVector(2470.f, 1976.f, 10.f), 20.f, true, true, true, false, nullptr, false), EPathFollowingRequestResult::RequestSuccessful);
	CrowdManager->Tick(.1f);
	TestTrue(TEXT("A reused crowd agent supplies motion within its live speed"),
		!Left->GetCharacterMovement()->RequestedVelocity.IsNearlyZero() && Left->GetCharacterMovement()->RequestedVelocity.Size2D() <= 250.1f);
	TestTrue(TEXT("The server applies the existing stun"), Left->GetGuardStateComponent()->ApplyStun(1.f));
	TestTrue(TEXT("A visible stunned guard is a registered obstacle rather than a moving crowd agent"),
		LeftCrowd->GetCrowdSimulationState() == ECrowdSimulationState::ObstacleOnly && CrowdManager->IsAgentValid(LeftCrowd) && Left->GetActorEnableCollision());
	TestEqual(TEXT("Stun retains its existing zero movement speed"), Left->GetCharacterMovement()->MaxWalkSpeed, 0.f);
	LeftController->UnPossess();
	TestFalse(TEXT("Unpossessed guards leave no stale crowd agent"), CrowdManager->IsAgentValid(LeftCrowd));
	return true;
}

#endif
