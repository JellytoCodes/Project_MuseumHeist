#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "World/Actors/Security/HeistLaserBarrierActor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

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

#endif
