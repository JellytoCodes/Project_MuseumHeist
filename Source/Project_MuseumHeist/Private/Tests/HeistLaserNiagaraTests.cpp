#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "World/Actors/Security/HeistLaserBarrierActor.h"
#include "Character/HeistPlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HeistCollisionChannels.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerState.h"
#include "Data/HeistArtifactDataTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "World/Actors/Loot/HeistPaintingDisplayCaseActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistLaserNiagaraTest, "ProjectMuseumHeist.Security.LaserNiagara",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistLaserNiagaraTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	UClass* Class = LoadClass<AHeistLaserBarrierActor>(nullptr, TEXT("/Game/Blueprints/World/Actors/Security/BP_LaserBarrier.BP_LaserBarrier_C"));
	if (!TestNotNull(TEXT("Laser Blueprint exists"), Class)) return false;
	AHeistLaserBarrierActor* Laser = World->SpawnActor<AHeistLaserBarrierActor>(Class);
	if (!TestNotNull(TEXT("Existing Niagara asset is assigned"), Laser->BeamEffect.Get())) return false;
	TestEqual(TEXT("Uses requested existing system"), Laser->BeamEffect->GetFName(), FName(TEXT("NS_HeistSecurityLaser")));
	TestEqual(TEXT("Three visible rows share one system asset"), Laser->BeamEffectComponents.Num(), 3);
	TestEqual(TEXT("Query volume lower face starts at floor"), Laser->BeamTriggerComponent->GetRelativeLocation().Z, 120.0);
	Laser->SetActorTransform(FTransform(FRotator(0, 37, 0), FVector(900, -400, 50), FVector(1, 1.7, 1.2)));
	Laser->BeamTriggerComponent->SetBoxExtent(FVector(10, 215, 120));
	Laser->ConfigureBeamEffects();
	for (UNiagaraComponent* Beam : Laser->BeamEffectComponents)
	{
		bool bStartValid = false, bEndValid = false;
		const FVector Start = Beam->GetVariableVec3(TEXT("User.BeamStart"), bStartValid);
		const FVector End = Beam->GetVariableVec3(TEXT("User.BeamEnd"), bEndValid);
		TestTrue(TEXT("Typed beam parameters are present"), bStartValid && bEndValid);
		TestEqual(TEXT("Start follows edited box width"), Start, FVector(0, -215, 0));
		TestEqual(TEXT("End follows edited box width"), End, FVector(0, 215, 0));
		const double WorldLength = FVector::Distance(Beam->GetComponentTransform().TransformPosition(Start), Beam->GetComponentTransform().TransformPosition(End));
		TestTrue(TEXT("Rotated and scaled actor applies width scale exactly once"), FMath::IsNearlyEqual(WorldLength, 430.0 * 1.7, .01));
		TestEqual(TEXT("Niagara never provides gameplay collision"), Beam->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	}
	TestEqual(TEXT("Box remains query only"), Laser->BeamTriggerComponent->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
	Laser->bBarrierEnabled = true;
	Laser->bBeamActive = true;
	Laser->OnRep_LaserState();
	TestFalse(TEXT("Old cube is hidden when Niagara is assigned"), Laser->BeamVisualComponent->IsVisible());
	for (UNiagaraComponent* Beam : Laser->BeamEffectComponents) TestTrue(TEXT("Active replicated state shows all rows"), Beam->IsActive() && Beam->IsVisible());
	Laser->bBeamActive = false;
	++Laser->SecurityRevision;
	Laser->OnRep_LaserState();
	for (UNiagaraComponent* Beam : Laser->BeamEffectComponents) TestFalse(TEXT("Bypass immediately removes persistent particles"), Beam->IsActive() || Beam->IsVisible());
	Laser->bRearmGraceActive = true;
	++Laser->SecurityRevision;
	Laser->OnRep_LaserState();
	for (UNiagaraComponent* Beam : Laser->BeamEffectComponents) TestFalse(TEXT("Rearm grace keeps rows off"), Beam->IsActive() || Beam->IsVisible());
	Laser->bRearmGraceActive = false;
	Laser->bBeamActive = true;
	++Laser->SecurityRevision;
	Laser->OnRep_LaserState();
	for (UNiagaraComponent* Beam : Laser->BeamEffectComponents) TestTrue(TEXT("Rearm restarts the beam"), Beam->IsActive() && Beam->IsVisible());
	Laser->bBarrierEnabled = false;
	++Laser->SecurityRevision;
	Laser->OnRep_LaserState();
	for (UNiagaraComponent* Beam : Laser->BeamEffectComponents) TestFalse(TEXT("Inactive contract hides beam even with stale active bit"), Beam->IsActive() || Beam->IsVisible());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistLaserContactTest, "ProjectMuseumHeist.Security.LaserContact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistLaserContactTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	if (!TestNotNull(TEXT("Contact fixture creates a game world"), World)) return false;
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Context.OwningGameInstance = Instance;
	World->SetGameInstance(Instance);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	FURL URL;
	URL.AddOption(TEXT("game=/Script/Project_MuseumHeist.HeistGameMode"));
	if (!TestTrue(TEXT("Contact fixture uses the production authority GameMode"), World->SetGameMode(URL))) return false;
	World->InitializeActorsForPlay(URL);
	AHeistGameMode* Mode = World->GetAuthGameMode<AHeistGameMode>();
	AHeistGameState* State = World->GetGameState<AHeistGameState>();
	if (!TestNotNull(TEXT("Heist GameMode is present"), Mode) || !TestNotNull(TEXT("Heist GameState is present"), State)) return false;
	State->SetMatchPhase(EHeistMatchPhase::InGame);
	if (!TestTrue(TEXT("Optional painting uses an initialized two-player contract"), State->InitializeContractSnapshot(
		TEXT("Contract_LaserContact"), TEXT("M01"), 600.f, 123, 2, TEXT("Artifact_Painting_M01"),
		FText::FromString(TEXT("Laser contact required target")), TEXT("Case_LaserContactRequired"), 100))) return false;

	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AHeistPaintingDisplayCaseActor* Painting = World->SpawnActor<AHeistPaintingDisplayCaseActor>(FVector(1000, 0, 0), FRotator::ZeroRotator, Spawn);
	FNameProperty* ArtifactProperty = FindFProperty<FNameProperty>(Painting->GetClass(), TEXT("TargetArtifactId"));
	if (!TestNotNull(TEXT("Painting exposes its existing authored artifact property"), ArtifactProperty)) return false;
	ArtifactProperty->SetPropertyValue_InContainer(Painting, FName(TEXT("Artifact_Painting_SecurityTest")));
	FHeistArtifactDataRow Definition;
	if (!TestTrue(TEXT("Existing security artifact resolves through the production DataTable"), Mode->TryGetArtifactDefinition(Painting->GetTargetArtifactId(), Definition))) return false;
	TestEqual(TEXT("Protected optional painting is FourStar"), Definition.ItemGrade, EHeistLootGrade::FourStar);
	TestEqual(TEXT("Protected optional painting is Drawing"), Definition.ForgeryType, EHeistForgeryType::Drawing);
	Painting->SetContractExhibitActive(true);
	AHeistLaserBarrierActor* Laser = World->SpawnActor<AHeistLaserBarrierActor>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
	Laser->ProtectedPaintingCase = Painting;
	Laser->BeamTriggerComponent->SetRelativeLocation(FVector(0, 0, 120));
	AHeistPlayerCharacter* Player = World->SpawnActor<AHeistPlayerCharacter>(FVector(300, 0, 100), FRotator::ZeroRotator, Spawn);
	AHeistPlayerState* PlayerState = World->SpawnActor<AHeistPlayerState>();
	AHeistPlayerState* HolderState = World->SpawnActor<AHeistPlayerState>();
	Player->SetPlayerState(PlayerState);
	State->AddPlayerState(PlayerState);
	State->AddPlayerState(HolderState);
	Player->DispatchBeginPlay();
	Laser->DispatchBeginPlay();
	World->SetBegunPlay(true);
	if (!TestTrue(TEXT("The real optional contract enables the beam"), Laser->IsBeamActive())) return false;
	TestEqual(TEXT("Beam remains query-only and cannot trap the player"), Laser->BeamTriggerComponent->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);

	const auto MovePlayer = [&](const FVector& Location)
	{
		Player->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
		Player->GetCapsuleComponent()->UpdateOverlaps();
		Laser->BeamTriggerComponent->UpdateOverlaps();
		World->GetPhysicsScene()->Flush();
	};
	const auto Advance = [&](const float Seconds)
	{
		for (int32 Step = 0; Step < FMath::CeilToInt(Seconds / .05f); ++Step)
		{
			World->TimeSeconds += .05f;
			++GFrameCounter;
			World->GetPhysicsScene()->Flush();
			World->GetTimerManager().Tick(.05f);
		}
	};
	const auto ExpectTrip = [&](const TCHAR* Scenario, const int32 Sequence, const float Alert)
	{
		TestEqual(FString::Printf(TEXT("%s: one trip per contact"), Scenario), Laser->TripSequence, Sequence);
		TestTrue(FString::Printf(TEXT("%s: each trip adds exactly 0.5 alert"), Scenario), FMath::IsNearlyEqual(State->GetAlertMeterValue(), Alert));
	};
	const FVector Outside(300, 0, 100);
	const FVector Inside(0, 0, 100);
	MovePlayer(Inside);
	TestTrue(TEXT("Active entry produces a real capsule/beam overlap"), Laser->BeamTriggerComponent->IsOverlappingComponent(Player->GetCapsuleComponent()));
	ExpectTrip(TEXT("Active entry"), 1, .5f);
	TestEqual(TEXT("Trip attributes the connected entrant"), Laser->LastTrippedPlayerState.Get(), PlayerState);
	MovePlayer(FVector(0, 30, 100));
	ExpectTrip(TEXT("Movement within the same contact"), 1, .5f);
	MovePlayer(Outside);
	TestFalse(TEXT("Leaving the beam ends the real overlap"), Laser->BeamTriggerComponent->IsOverlappingActor(Player));
	MovePlayer(Inside);
	ExpectTrip(TEXT("Exit then active re-entry"), 2, 1.f);

	// Additional overlap components must not turn one actor contact into several incidents.
	UBoxComponent* ExtraContact = NewObject<UBoxComponent>(Player);
	ExtraContact->SetupAttachment(Player->GetCapsuleComponent());
	ExtraContact->SetBoxExtent(FVector(10));
	ExtraContact->SetCollisionObjectType(HeistCollisionChannels::Player);
	ExtraContact->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ExtraContact->SetCollisionResponseToAllChannels(ECR_Ignore);
	ExtraContact->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	ExtraContact->SetGenerateOverlapEvents(true);
	ExtraContact->RegisterComponent();
	ExtraContact->UpdateOverlaps();
	TestTrue(TEXT("A second component really touches the beam"), Laser->BeamTriggerComponent->IsOverlappingComponent(ExtraContact));
	ExpectTrip(TEXT("Duplicate component begin"), 2, 1.f);
	ExtraContact->SetRelativeLocation(FVector(300, 0, 0));
	ExtraContact->UpdateOverlaps();
	TestTrue(TEXT("Capsule remains inside after the extra component leaves"), Laser->BeamTriggerComponent->IsOverlappingComponent(Player->GetCapsuleComponent()));
	ExtraContact->SetRelativeLocation(FVector::ZeroVector);
	ExtraContact->UpdateOverlaps();
	ExpectTrip(TEXT("Component re-entry during the same actor contact"), 2, 1.f);
	ExtraContact->DestroyComponent();
	MovePlayer(Outside);

	const int32 BeforeBypassTrips = Laser->TripSequence;
	const float BeforeBypassAlert = State->GetAlertMeterValue();
	if (!TestTrue(TEXT("Separate holder starts the production bypass"), Laser->TryActivateBypass(HolderState))) return false;
	MovePlayer(Inside);
	TestTrue(TEXT("Bypass entrant really overlaps the beam"), Laser->BeamTriggerComponent->IsOverlappingActor(Player));
	ExpectTrip(TEXT("Non-holder entry during bypass"), BeforeBypassTrips, BeforeBypassAlert);
	if (!TestTrue(TEXT("Holder release starts production rearm"), Laser->BeginRearm(HolderState))) return false;
	const float Grace = Mode->GetSecurityLaserRearmGraceSeconds();
	if (!TestTrue(TEXT("Authored rearm grace gives the entrant time to leave"), Grace > .1f)) return false;
	Advance(Grace * .5f);
	TestTrue(TEXT("Grace keeps beam inactive while the entrant remains inside"), Laser->IsRearming() && !Laser->IsBeamActive());
	ExpectTrip(TEXT("Still inside during grace"), BeforeBypassTrips, BeforeBypassAlert);
	Advance(Grace + .1f);
	TestTrue(TEXT("Actual rearm timer reactivates the beam"), Laser->IsBeamActive() && !Laser->IsRearming());
	TestTrue(TEXT("Entrant remains in the same real overlap at rearm"), Laser->BeamTriggerComponent->IsOverlappingActor(Player));
	ExpectTrip(TEXT("Contact at reactivation"), BeforeBypassTrips + 1, BeforeBypassAlert + .5f);

	const int32 AfterRearmTrips = Laser->TripSequence;
	const float AfterRearmAlert = State->GetAlertMeterValue();
	Laser->ForceRestoreDefaultState();
	MovePlayer(FVector(0, 30, 100));
	ExpectTrip(TEXT("Already-active restore and continued contact"), AfterRearmTrips, AfterRearmAlert);
	MovePlayer(Outside);
	MovePlayer(Inside);
	ExpectTrip(TEXT("Exit then re-entry after rearm"), AfterRearmTrips + 1, AfterRearmAlert + .5f);

	MovePlayer(Outside);
	Painting->SetContractExhibitActive(false);
	Laser->RefreshRuntimeConfiguration();
	TestFalse(TEXT("Inactive protected exhibit disables the barrier"), Laser->IsBarrierEnabled());
	const int32 BeforeEnableTrips = Laser->TripSequence;
	const float BeforeEnableAlert = State->GetAlertMeterValue();
	MovePlayer(Inside);
	TestTrue(TEXT("Disabled barrier still has a real query overlap"), Laser->BeamTriggerComponent->IsOverlappingActor(Player));
	ExpectTrip(TEXT("Entry while exhibit is disabled"), BeforeEnableTrips, BeforeEnableAlert);
	Painting->SetContractExhibitActive(true);
	Laser->RefreshRuntimeConfiguration();
	TestTrue(TEXT("Production configuration refresh enables the beam"), Laser->IsBeamActive());
	ExpectTrip(TEXT("Contact at configuration activation"), BeforeEnableTrips + 1, BeforeEnableAlert + .5f);
	Laser->RefreshRuntimeConfiguration();
	ExpectTrip(TEXT("Unchanged configuration does not duplicate contact"), BeforeEnableTrips + 1, BeforeEnableAlert + .5f);
	if (!TestTrue(TEXT("Holder can bypass while an already-tripped entrant remains inside"), Laser->TryActivateBypass(HolderState)) ||
		!TestTrue(TEXT("Holder can release that bypass"), Laser->BeginRearm(HolderState))) return false;
	Advance(Grace + .1f);
	TestTrue(TEXT("Second rearm completes during the same actor contact"), Laser->IsBeamActive());
	ExpectTrip(TEXT("Second activation without exiting does not duplicate contact"), BeforeEnableTrips + 1, BeforeEnableAlert + .5f);
	return true;
}

#endif
