#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Character/HeistPlayerCharacter.h"
#include "Character/Components/HeistInventoryComponent.h"
#include "Character/Components/HeistStatusComponent.h"
#include "Character/Components/HeistVisionComponent.h"
#include "Components/SpotLightComponent.h"
#include "Core/HeistGameState.h"
#include "Core/HeistGameplayTags.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistPlayerState.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UI/ViewModels/HeistHUDViewModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistFlashlightLifecycleTest, "ProjectMuseumHeist.Player.FlashlightLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistFlashlightLifecycleTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	AHeistGameState* State = World->SpawnActor<AHeistGameState>();
	World->SetGameState(State);
	State->SetMatchPhase(EHeistMatchPhase::InGame);
	UClass* PlayerClass = LoadClass<AHeistPlayerCharacter>(nullptr, TEXT("/Game/Blueprints/Player/BP_HeistPlayerCharacter.BP_HeistPlayerCharacter_C"));
	if (!TestNotNull(TEXT("Player Blueprint exists"), PlayerClass)) return false;
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AHeistPlayerCharacter* Player = World->SpawnActor<AHeistPlayerCharacter>(PlayerClass, FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
	AHeistPlayerState* PS = World->SpawnActor<AHeistPlayerState>();
	AHeistPlayerController* Controller = World->SpawnActor<AHeistPlayerController>();
	Controller->Possess(Player);
	Player->SetPlayerState(PS);
	Player->DispatchBeginPlay();
	UHeistVisionComponent* Vision = Player->GetVisionComponent();
	TInlineComponentArray<USpotLightComponent*> Lights(Player);
	USpotLightComponent* Light = nullptr;
	int32 LightCount = 0;
	for (USpotLightComponent* Candidate : Lights)
	{
		if (Candidate->ComponentHasTag(TEXT("Flashlight"))) { Light = Candidate; ++LightCount; }
	}
	TestEqual(TEXT("Exactly one flashlight in the actual BP"), LightCount, 1);
	if (!TestNotNull(TEXT("Actual BP supplies flashlight"), Light)) return false;
	TestEqual(TEXT("Light follows camera position"), Light->GetAttachParent()->GetFName(), FName(TEXT("FirstPersonCamera")));
	TestFalse(TEXT("Spawn starts OFF"), Vision->IsFlashlightEnabled() || Light->IsVisible());
	UHeistHUDViewModel* VM = NewObject<UHeistHUDViewModel>();
	VM->SetupViewModel(State, PS, Player->GetActionComponent());
	TestEqual(TEXT("Key is F"), AHeistPlayerController::GetFlashlightToggleKey(), EKeys::F);
	TestEqual(TEXT("Initial HUD includes key and state"), VM->GetFlashlightStatusText().ToString(), FString(TEXT("[F] 손전등 OFF")));
	Controller->Server_ToggleFlashlight_Implementation();
	TestTrue(TEXT("Server toggle turns on actual light and HUD"), Vision->IsFlashlightEnabled() && Light->IsVisible() && VM->IsFlashlightEnabled());
	TestEqual(TEXT("ON HUD retains shortcut"), VM->GetFlashlightStatusText().ToString(), FString(TEXT("[F] 손전등 ON")));
	Controller->Server_ToggleFlashlight_Implementation();
	TestFalse(TEXT("Second toggle switches off actual light and HUD"), Vision->IsFlashlightEnabled() || Light->IsVisible() || VM->IsFlashlightEnabled());
	Controller->Server_ToggleFlashlight_Implementation();
	Vision->UpdateFlashlightAimDirection(FRotator(-25, 70, 0).Vector());
	TestTrue(TEXT("Light follows pitch and yaw"), Light->GetForwardVector().Equals(FRotator(-25, 70, 0).Vector(), .001f));
	Player->GetInventoryComponent()->TrySetInventoryOpen(true);
	Controller->Server_ToggleFlashlight_Implementation();
	TestTrue(TEXT("Inventory rejects toggle and preserves light"), Vision->IsFlashlightEnabled());
	Player->GetInventoryComponent()->TrySetInventoryOpen(false);
	Player->GetStatusComponent()->ApplyTimedStatusTag(FHeistGameplayTags::Get().Event_Player_Stunned, 5.f);
	TestFalse(TEXT("Stun switches off light and HUD"), Vision->IsFlashlightEnabled() || Light->IsVisible() || VM->IsFlashlightEnabled());
	Controller->Server_ToggleFlashlight_Implementation();
	TestFalse(TEXT("Stun rejects toggle"), Vision->IsFlashlightEnabled());
	Player->GetStatusComponent()->ClearStatusTag(FHeistGameplayTags::Get().Event_Player_Stunned);
	Controller->Server_ToggleFlashlight_Implementation();
	TestTrue(TEXT("Recovery permits toggle"), Vision->IsFlashlightEnabled());
	PS->MarkArrested(nullptr);
	TestFalse(TEXT("Arrest switches off light"), Vision->IsFlashlightEnabled());
	Controller->Server_ToggleFlashlight_Implementation();
	TestFalse(TEXT("Arrest rejects toggle"), Vision->IsFlashlightEnabled());
	PS->ClearArrested();
	Controller->Server_ToggleFlashlight_Implementation();
	TestTrue(TEXT("Release permits toggle"), Vision->IsFlashlightEnabled());
	State->SetMatchPhase(EHeistMatchPhase::End);
	TestFalse(TEXT("Match end switches off light"), Vision->IsFlashlightEnabled());
	Controller->Server_ToggleFlashlight_Implementation();
	TestFalse(TEXT("Match end rejects toggle"), Vision->IsFlashlightEnabled());
	State->SetMatchPhase(EHeistMatchPhase::InGame);
	Controller->Server_ToggleFlashlight_Implementation();
	Controller->UnPossess();
	TestFalse(TEXT("Unpossess switches off light"), Vision->IsFlashlightEnabled());
	Controller->Possess(Player);
	Player->SetPlayerState(PS);
	Controller->Server_ToggleFlashlight_Implementation();
	TestTrue(TEXT("Repossess permits toggle"), Vision->IsFlashlightEnabled());
	PS->MarkEscaped();
	TestFalse(TEXT("Escape switches off light"), Vision->IsFlashlightEnabled());
	Controller->Server_ToggleFlashlight_Implementation();
	TestFalse(TEXT("Escaped player cannot switch on"), Vision->IsFlashlightEnabled());
	// Exercise the same callback used after network property delivery; not a network PASS.
	Vision->bFlashlightEnabled = true;
	Vision->OnRep_FlashlightEnabled();
	TestTrue(TEXT("RepNotify refreshes world light and HUD"), Light->IsVisible() && VM->IsFlashlightEnabled());
	VM->SetupViewModel(State, PS, nullptr);
	int32 Notifications = 0;
	const FDelegateHandle Handle = VM->GetPresentationChangedDelegate().AddLambda([&Notifications] { ++Notifications; });
	Vision->SetFlashlightEnabled(false);
	TestEqual(TEXT("Old pawn cannot refresh rebound HUD"), Notifications, 0);
	TestFalse(TEXT("Unbound HUD resets OFF"), VM->IsFlashlightEnabled());
	VM->GetPresentationChangedDelegate().Remove(Handle);
	VM->SetupViewModel(nullptr, nullptr, nullptr);
	return true;
}

#endif
