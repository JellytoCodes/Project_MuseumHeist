#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "World/Actors/Security/HeistSecurityCameraActor.h"
#include "Character/HeistPlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AIPerceptionSystem.h"
#include "Perception/AISense_Sight.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "TimerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistCCTVPerceptionTest, "ProjectMuseumHeist.W8.CCTVPerception",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistCCTVPerceptionTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false)
		.CreateAISystem(true).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	AHeistGameState* State = World->SpawnActor<AHeistGameState>();
	World->SetGameState(State);
	State->SetMatchPhase(EHeistMatchPhase::InGame);
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AHeistPlayerCharacter* Player = World->SpawnActor<AHeistPlayerCharacter>(FVector(600, 0, 0), FRotator::ZeroRotator, Spawn);
	AHeistPlayerState* PS = World->SpawnActor<AHeistPlayerState>();
	Player->SetPlayerState(PS);
	State->AddPlayerState(PS);
	AHeistSecurityCameraActor* Camera = World->SpawnActor<AHeistSecurityCameraActor>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
	Camera->SweepHalfAngleDegrees = 0;
	Camera->DetectionHalfAngleDegrees = 70;
	Player->DispatchBeginPlay();
	Camera->DispatchBeginPlay();
	World->SetBegunPlay(true);
	UAIPerceptionSystem* Perception = UAIPerceptionSystem::GetCurrent(World);
	if (!TestNotNull(TEXT("World has real Perception system"), Perception)) return false;
	Perception->RegisterSource<UAISense_Sight>(*Player);
	const auto Advance = [&](float Seconds)
	{
		for (int32 Step = 0; Step < FMath::CeilToInt(Seconds / .05f); ++Step)
		{
			World->TimeSeconds += .05f;
			++GFrameCounter;
			World->GetPhysicsScene()->Flush();
			Perception->Tick(.05f);
			World->GetTimerManager().Tick(.05f);
			Camera->Tick(.05f);
		}
	};
	const auto SeesPlayer = [&]()
	{
		TArray<AActor*> Visible;
		Camera->CameraPerceptionComponent->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), Visible);
		return Visible.Contains(Player);
	};
	TestNull(TEXT("CCTV no longer owns a Box collision"), Camera->FindComponentByClass<UBoxComponent>());
	TestTrue(TEXT("Active CCTV shows its light"), Camera->SightLightComponent->IsVisible());
	TestTrue(TEXT("Light radius matches Sight range"), FMath::IsNearlyEqual(Camera->SightLightComponent->AttenuationRadius, 1200.f));
	TestTrue(TEXT("Light angle uses placed camera override"), FMath::IsNearlyEqual(Camera->SightLightComponent->OuterConeAngle, 70.f));
	TestTrue(TEXT("Idle light is white"), Camera->SightLightComponent->GetLightColor().Equals(FLinearColor::White, .01f));
	Advance(.6f);
	TestTrue(TEXT("Sight acquires front target"), SeesPlayer());
	TestTrue(TEXT("Visible player builds up without immediate incident"), Camera->GetDetectionProgress() > 0 && Camera->GetDetectionRevision() == 0);
	const FLinearColor PartialColor = Camera->SightLightComponent->GetLightColor();
	TestTrue(TEXT("Partial detection moves white toward red"), PartialColor.R > .99f && PartialColor.G < .95f && PartialColor.G > .1f);
	Player->SetActorLocation(FVector(-600, 0, 0));
	Advance(.3f);
	TestFalse(TEXT("Rear target leaves Sight"), SeesPlayer());
	TestEqual(TEXT("Sight loss clears partial buildup"), Camera->GetDetectionProgress(), 0.f);
	Advance(.3f);
	TestTrue(TEXT("Lost target restores white"), Camera->SightLightComponent->GetLightColor().Equals(FLinearColor::White, .025f));
	Player->SetActorLocation(FVector(550, 1000, 0));
	Advance(.3f);
	TestTrue(TEXT("Cone sees target outside former Box Y extent"), SeesPlayer());
	Player->SetActorLocation(FVector(2000, 0, 0));
	Advance(.3f);
	TestFalse(TEXT("Range limit still applies"), SeesPlayer());
	Player->SetActorLocation(FVector(100, 800, 0));
	Advance(.3f);
	TestFalse(TEXT("Outside half angle is not visible"), SeesPlayer());

	AActor* Wall = World->SpawnActor<AActor>();
	UBoxComponent* WallBox = NewObject<UBoxComponent>(Wall);
	Wall->SetRootComponent(WallBox);
	WallBox->SetBoxExtent(FVector(40, 250, 250));
	WallBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WallBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	WallBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	WallBox->RegisterComponent();
	Wall->SetActorLocation(FVector(300, 0, 0));
	Player->SetActorLocation(FVector(600, 0, 0));
	Advance(.6f);
	TestFalse(TEXT("Real Visibility blocker occludes front target"), SeesPlayer());
	TestEqual(TEXT("Occlusion prevents detection"), Camera->GetDetectionRevision(), 0);
	Wall->Destroy();
	Advance(1.8f);
	TestEqual(TEXT("Slower detection does not confirm at the old timing"), Camera->GetDetectionRevision(), 0);
	Advance(.6f);
	TestEqual(TEXT("Unoccluded target confirms after buildup"), Camera->GetDetectionRevision(), 1);
	TestTrue(TEXT("Confirmation remains red after build-up resets"), Camera->SightLightComponent->GetLightColor().Equals(Camera->AlertLightColor, .01f));
	Advance(2.f);
	TestEqual(TEXT("Cooldown prevents repeated incidents"), Camera->GetDetectionRevision(), 1);
	TestTrue(TEXT("Confirmation pulse expires to white"), Camera->SightLightComponent->GetLightColor().Equals(FLinearColor::White, .025f));
	TestTrue(TEXT("Visible target keeps sweep paused through confirmation cooldown"), Camera->IsSweepPaused());
	Player->SetActorLocation(FVector(-600, 0, 0));
	Advance(.3f);
	State->SetMatchPhase(EHeistMatchPhase::Lobby);
	Advance(.3f);
	TestFalse(TEXT("Phase exit disables camera"), Camera->IsCameraEnabled());
	TestFalse(TEXT("Phase exit hides light"), Camera->SightLightComponent->IsVisible());
	TestFalse(TEXT("Phase exit forgets Sight"), SeesPlayer());
	TestFalse(TEXT("Phase exit clears sweep pause"), Camera->IsSweepPaused());
	TestFalse(TEXT("Phase exit clears pending resume"), World->GetTimerManager().IsTimerActive(Camera->SweepResumeTimerHandle));
	State->SetMatchPhase(EHeistMatchPhase::InGame);
	Camera->SweepHalfAngleDegrees = 35;
	Advance(1.5f);
	Camera->Tick(.01f);
	FVector Eye; FRotator Rotation;
	Camera->GetActorEyesViewPoint(Eye, Rotation);
	TestTrue(TEXT("Lens viewpoint and moving mesh face the same direction"), Rotation.Vector().Equals(Camera->VisualMeshComponent->GetForwardVector(), .001f));
	TestTrue(TEXT("Sweep changes perception direction"), FMath::Abs(Rotation.Yaw) > 30);
	TestTrue(TEXT("Spot light rotates with Sight"), Rotation.Vector().Equals(Camera->SightLightComponent->GetForwardVector(), .001f));
	TestTrue(TEXT("Spot light originates at the lens"), Eye.Equals(Camera->SightLightComponent->GetComponentLocation(), .001f));

	// Acquire on a moving part of the sweep and verify the actual Sight callback freezes it.
	Camera->SweepEpochServerTime = World->GetTimeSeconds() - .5f;
	Player->SetActorLocation(Camera->ResolveSensorForward() * 600.f);
	Advance(.1f);
	TestTrue(TEXT("First sight pauses before full detection"), Camera->IsSweepPaused() && Camera->GetDetectionRevision() == 1);
	const float PausedYaw = Camera->GetResolvedSweepYawDegrees();
	Player->SetActorLocation(Camera->ResolveSensorForward().RotateAngleAxis(20.f, FVector::UpVector) * 600.f);
	Advance(.5f);
	TestTrue(TEXT("Visible movement does not rotate the camera toward the target"), FMath::IsNearlyEqual(Camera->GetResolvedSweepYawDegrees(), PausedYaw, .001f));
	Player->SetActorLocation(FVector(-600, 0, 0));
	Advance(.1f);
	TestEqual(TEXT("Loss immediately clears buildup while direction stays held"), Camera->GetDetectionProgress(), 0.f);
	Advance(1.2f);
	TestTrue(TEXT("Sweep remains paused before 1.5 second loss delay"), Camera->IsSweepPaused());
	Player->SetActorLocation(Camera->ResolveSensorForward() * 600.f);
	Advance(.1f);
	TestFalse(TEXT("Reacquisition cancels pending resume"), World->GetTimerManager().IsTimerActive(Camera->SweepResumeTimerHandle));
	Advance(.4f);
	TestTrue(TEXT("Reacquisition preserves the same paused direction"), FMath::IsNearlyEqual(Camera->GetResolvedSweepYawDegrees(), PausedYaw, .001f));
	Player->SetActorLocation(FVector(-600, 0, 0));
	Advance(.1f);
	Advance(1.2f);
	TestTrue(TEXT("Second loss starts a fresh full delay"), Camera->IsSweepPaused());
	Advance(.3f);
	TestFalse(TEXT("Sweep resumes after 1.5 seconds without a target"), Camera->IsSweepPaused());
	TestTrue(TEXT("Resume has no angle jump"), FMath::Abs(Camera->GetResolvedSweepYawDegrees() - PausedYaw) < 4.f);
	Advance(.25f);
	TestTrue(TEXT("Resume preserves forward sweep direction"), Camera->GetResolvedSweepYawDegrees() > PausedYaw);

	AHeistPlayerCharacter* OtherPlayer = World->SpawnActor<AHeistPlayerCharacter>(Camera->ResolveSensorForward() * 600.f, FRotator::ZeroRotator, Spawn);
	AHeistPlayerState* OtherPS = World->SpawnActor<AHeistPlayerState>();
	OtherPlayer->SetPlayerState(OtherPS);
	State->AddPlayerState(OtherPS);
	OtherPlayer->DispatchBeginPlay();
	Perception->RegisterSource<UAISense_Sight>(*OtherPlayer);
	Player->SetActorLocation(Camera->ResolveSensorForward() * 500.f);
	Advance(.3f);
	TestTrue(TEXT("Two visible players hold the sweep"), Camera->IsSweepPaused());
	Player->SetActorLocation(FVector(-600, 0, 0));
	Advance(1.8f);
	TestTrue(TEXT("One remaining player prevents resume"), Camera->IsSweepPaused());
	OtherPlayer->Destroy();
	Advance(1.8f);
	TestFalse(TEXT("Destroyed last target cannot leave camera permanently paused"), Camera->IsSweepPaused());
	Player->SetActorLocation(Camera->ResolveSensorForward() * 600.f);
	Advance(.3f);
	TestTrue(TEXT("Camera can pause again after target destruction"), Camera->IsSweepPaused());
	State->RemovePlayerState(PS);
	Advance(1.8f);
	TestFalse(TEXT("Disconnected player state no longer holds the camera"), Camera->IsSweepPaused());
	return true;
}

#endif
