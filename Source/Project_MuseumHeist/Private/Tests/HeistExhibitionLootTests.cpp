#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Character/HeistPlayerCharacter.h"
#include "Character/Components/HeistInteractionComponent.h"
#include "Character/Components/HeistInventoryComponent.h"
#include "Character/Components/HeistStatusComponent.h"
#include "AI/HeistGuardNoiseReactionComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistGameState.h"
#include "Core/HeistGameplayTags.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistPlayerState.h"
#include "Data/HeistGameBalanceDataAsset.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "TimerManager.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "World/Actors/Loot/HeistLootActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistExhibitionLootSecurityTest, "ProjectMuseumHeist.Loot.ExhibitionCaseSecurity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistExhibitionLootSecurityTest::RunTest(const FString& Parameters)
{
	// Production BP_Loot and server RPC handlers in an isolated authority world.
	// This proves transaction/state rules, not replicated visuals or natural input.
	const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	if (!TestNotNull(TEXT("Exhibition authority world exists"), World)) return false;
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	World->SetGameInstance(Instance);
	Context.OwningGameInstance = Instance;
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	FURL URL;
	URL.AddOption(TEXT("game=/Script/Project_MuseumHeist.HeistGameMode"));
	if (!TestTrue(TEXT("Production GameMode creates"), World->SetGameMode(URL))) return false;
	World->InitializeActorsForPlay(URL);
	AHeistGameState* State = World->GetGameState<AHeistGameState>();
	if (!TestNotNull(TEXT("Production GameState exists"), State)) return false;
	State->SetMatchPhase(EHeistMatchPhase::InGame);
	UClass* LootClass = LoadClass<AHeistLootActor>(nullptr, TEXT("/Game/Blueprints/World/Actors/Loot/BP_Loot.BP_Loot_C"));
	const UHeistGameBalanceDataAsset* Balance = GetDefault<UHeistGameBalanceDataAsset>();
	UDataTable* LootTable = Balance->LootDataTable.LoadSynchronous();
	if (!TestTrue(TEXT("Canonical BP_Loot and release table load"), IsValid(LootClass) && IsValid(LootTable))) return false;
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AHeistPlayerController* Controllers[2] = {};
	AHeistPlayerCharacter* Crew[2] = {};
	AHeistPlayerState* Players[2] = {};
	for (int32 Index = 0; Index < 2; ++Index)
	{
		Controllers[Index] = World->SpawnActor<AHeistPlayerController>();
		Crew[Index] = World->SpawnActor<AHeistPlayerCharacter>(FVector(0, Index * 20, 100), FRotator::ZeroRotator, Spawn);
		if (!TestTrue(TEXT("Crew and owning controller spawn"), IsValid(Controllers[Index]) && IsValid(Crew[Index]))) return false;
		if (!Controllers[Index]->GetPlayerState<AHeistPlayerState>()) Controllers[Index]->InitPlayerState();
		Players[Index] = Controllers[Index]->GetPlayerState<AHeistPlayerState>();
		if (!TestNotNull(TEXT("Owned PlayerState exists"), Players[Index])) return false;
		State->AddPlayerState(Players[Index]);
		Controllers[Index]->Possess(Crew[Index]);
		Crew[Index]->DispatchBeginPlay();
		if (!TestTrue(TEXT("Request context has matching Controller, Pawn and PlayerState ownership"),
			Controllers[Index]->GetPawn() == Crew[Index] && Crew[Index]->GetController() == Controllers[Index] &&
			Crew[Index]->GetPlayerState() == Players[Index] && Players[Index]->GetPawn() == Crew[Index])) return false;
	}
	World->SetBegunPlay(true);
	const auto Advance = [World](const float Seconds)
	{
		World->TimeSeconds += Seconds;
		++GFrameCounter;
		World->GetTimerManager().Tick(Seconds);
	};
	const auto Move = [World](AHeistPlayerCharacter* Character, const FVector& Location)
	{
		Character->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
		World->GetPhysicsScene()->Flush();
		Character->GetCapsuleComponent()->UpdateOverlaps();
		Character->GetInteractionComponent()->RefreshInteractionTarget();
	};
	const auto SpawnLoot = [World, LootClass, LootTable](const FVector& Location, bool bExhibition, bool bActive, bool bLocked)
	{
		const FTransform Transform(Location);
		AHeistLootActor* Loot = World->SpawnActorDeferred<AHeistLootActor>(LootClass, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!IsValid(Loot)) return static_cast<AHeistLootActor*>(nullptr);
		Loot->InitializeLootData(LootTable, TEXT("Loot_GoldenVase"));
		if (bExhibition) Loot->InitializeExhibitionLoot(bActive, bLocked);
		Loot->FinishSpawning(Transform);
		return Loot;
	};
	const auto Invoke = [](AHeistPlayerController* Controller, const FName Name, AHeistLootActor* Loot, int32 Revision = INDEX_NONE)
	{
		UFunction* Function = Controller->FindFunction(Name);
		if (!Function) return false;
		FStructOnScope Params(Function);
		for (TFieldIterator<FProperty> It(Function); It; ++It)
		{
			if (!It->HasAnyPropertyFlags(CPF_Parm) || It->HasAnyPropertyFlags(CPF_ReturnParm)) continue;
			if (FObjectProperty* Object = CastField<FObjectProperty>(*It)) Object->SetObjectPropertyValue_InContainer(Params.GetStructMemory(), Loot);
			if (FIntProperty* Integer = CastField<FIntProperty>(*It)) Integer->SetPropertyValue_InContainer(Params.GetStructMemory(), Revision);
		}
		Controller->ProcessEvent(Function, Params.GetStructMemory());
		return true;
	};
	AHeistLootActor* Locked = SpawnLoot(FVector(0, 0, 100), true, true, true);
	AHeistLootActor* Other = SpawnLoot(FVector(24, 0, 100), true, true, true);
	AHeistLootActor* Inactive = SpawnLoot(FVector(0, 0, 100), true, false, true);
	AHeistLootActor* Vault = SpawnLoot(FVector(0, 0, 100), true, true, false);
	AHeistLootActor* WorldDrop = SpawnLoot(FVector(0, 0, 100), false, true, false);
	if (!TestTrue(TEXT("All production case variants spawn"), IsValid(Locked) && IsValid(Other) && IsValid(Inactive) && IsValid(Vault) && IsValid(WorldDrop))) return false;
	const auto HasExpectedGlass = [](const AHeistLootActor* Loot, const bool bVisible)
	{
		TArray<UStaticMeshComponent*> Components;
		Loot->GetComponents(Components);
		int32 Panes = 0;
		for (const UStaticMeshComponent* Component : Components)
		{
			if (!Component->GetName().StartsWith(TEXT("CaseGlass"))) continue;
			++Panes;
			if (!IsValid(Component->GetStaticMesh()) || Component->IsVisible() != bVisible ||
				Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision) return false;
		}
		return Panes == 5;
	};
	TestTrue(TEXT("Locked active and decorative cases show the common glass enclosure"), HasExpectedGlass(Locked, true) && HasExpectedGlass(Inactive, true));
	TestTrue(TEXT("Vault and world drops have no glass enclosure"), HasExpectedGlass(Vault, false) && HasExpectedGlass(WorldDrop, false));
	Move(Crew[0], Locked->GetActorLocation());
	Move(Crew[1], Locked->GetActorLocation() + FVector(0, 20, 0));
	Advance(0.01f);
	if (!TestTrue(TEXT("Real capsule overlaps the locked case"), Crew[0]->GetInteractionComponent()->IsActorOverlappingInteractionArea(Locked))) return false;
	TestTrue(TEXT("Locked active case remains in available supply"), Locked->IsLootAvailable());
	TestEqual(TEXT("Case failure uses the same GuardNoise priority as detention failure"), UHeistGuardNoiseReactionComponent::ResolveCandidatePriority(EHeistSoundPingType::DisplayCaseLock), 1);
	TestTrue(TEXT("Small sculpture has the registered Item category"), FHeistGameplayTags::Get().Item_Loot_SmallSculpture.IsValid());
	TestFalse(TEXT("Locked case cannot reserve a direct pickup"), Locked->TryReserveForPickup(Crew[0]));
	TestFalse(TEXT("Inactive decoration is excluded from supply"), Inactive->IsLootAvailable());
	TestFalse(TEXT("Inactive decoration cannot interact or reserve"), Inactive->CanInteract(Crew[0]) || Inactive->TryReserveForPickup(Crew[0]));
	TestTrue(TEXT("Vault remains immediately pickup ready"), Vault->IsPickupReady());
	TestTrue(TEXT("Evidence/WorldDrop default remains immediately pickup ready"), !WorldDrop->IsExhibitionPresentation() && WorldDrop->IsPickupReady());
	Invoke(Controllers[0], TEXT("Server_RequestLootPickup"), Locked);
	Invoke(Controllers[0], TEXT("Server_RequestLootPickup"), Inactive);
	TestEqual(TEXT("Bypass requests never award value"), Players[0]->GetTotalLootScore(), 0);
	TestEqual(TEXT("Bypass requests never mutate the grid"), Crew[0]->GetInventoryComponent()->GetReplicatedInventory().Items.Num(), 0);
	Invoke(Controllers[0], TEXT("Server_RequestLootCase"), Locked, Locked->GetCaseRevision() + 1);
	TestNull(TEXT("Wrong revision cannot start a case"), Locked->GetCaseOperator());
	if (!TestTrue(TEXT("Owned start RPC exists"), Invoke(Controllers[0], TEXT("Server_RequestLootCase"), Locked, Locked->GetCaseRevision()))) return false;
	if (!TestTrue(TEXT("Owner exclusively starts case"), Locked->GetCaseOperator() == Players[0])) return false;
	Invoke(Controllers[1], TEXT("Server_RequestLootCase"), Locked, Locked->GetCaseRevision());
	Invoke(Controllers[1], TEXT("Server_CancelLootCase"), Locked);
	TestTrue(TEXT("Other player cannot steal or cancel ownership"), Locked->GetCaseOperator() == Players[0]);
	const int32 StartedRevision = Locked->GetCaseRevision();
	Invoke(Controllers[0], TEXT("Server_RequestLootCase"), Locked, StartedRevision);
	TestEqual(TEXT("Immediate repeated input cannot advance"), Locked->GetCompletedCaseLatches(), 0);
	const float FirstHalfWidth = Locked->GetCaseSuccessWindowWidth() * 0.5f;
	TestTrue(TEXT("Server random success window is fully inside the timing bar"), Locked->GetCaseSuccessWindowCenter() >= FirstHalfWidth && Locked->GetCaseSuccessWindowCenter() <= 1.0f - FirstHalfWidth);
	Advance(0.5f + Locked->GetCaseSuccessWindowCenter() * 6.0f);
	Invoke(Controllers[0], TEXT("Server_RequestLootCase"), Locked, Locked->GetCaseRevision());
	if (!TestEqual(TEXT("Server-time press completes first latch"), Locked->GetCompletedCaseLatches(), 1)) return false;
	Invoke(Controllers[0], TEXT("Server_RequestLootCase"), Locked, StartedRevision);
	TestEqual(TEXT("Old revision cannot replay a completed round"), Locked->GetCompletedCaseLatches(), 1);
	int32 NoiseCount = 0;
	State->GetSoundPingEventReportedDelegate().AddLambda([&NoiseCount](const FHeistSoundPingEvent& Noise, int32*)
	{
		if (Noise.PingType == EHeistSoundPingType::DisplayCaseLock && Noise.SoundPingTag == FHeistGameplayTags::Get().Event_SoundPing_DisplayCaseLock) ++NoiseCount;
	});
	const float MissProgress = Locked->GetCaseSuccessWindowCenter() < 0.5f ? 0.95f : 0.05f;
	Advance(0.5f + MissProgress * 6.0f);
	Invoke(Controllers[0], TEXT("Server_RequestLootCase"), Locked, Locked->GetCaseRevision());
	TestEqual(TEXT("Mistimed press reports one case-specific GuardNoise"), NoiseCount, 1);
	TestEqual(TEXT("Failure preserves completed latch"), Locked->GetCompletedCaseLatches(), 1);
	Invoke(Controllers[0], TEXT("Server_RequestLootCase"), Other, Other->GetCaseRevision());
	TestNull(TEXT("Starting another case cancels the former server session"), Locked->GetCaseOperator());
	TestTrue(TEXT("Only the new case remains owned"), Other->GetCaseOperator() == Players[0]);
	Invoke(Controllers[0], TEXT("Server_CancelLootCase"), Other);
	if (!TestNull(TEXT("Owner cancel RPC releases its active case"), Other->GetCaseOperator())) return false;
	Invoke(Controllers[1], TEXT("Server_RequestLootCase"), Locked, Locked->GetCaseRevision());
	if (!TestTrue(TEXT("Teammate resumes preserved progress"), Locked->GetCaseOperator() == Players[1] && Locked->GetCompletedCaseLatches() == 1)) return false;
	for (int32 Latch = 1; Latch < 3; ++Latch)
	{
		Advance(0.5f + Locked->GetCaseSuccessWindowCenter() * 6.0f);
		Invoke(Controllers[1], TEXT("Server_RequestLootCase"), Locked, Locked->GetCaseRevision());
		if (!TestEqual(TEXT("Each accepted round completes exactly one latch"), Locked->GetCompletedCaseLatches(), Latch + 1)) return false;
	}
	TestTrue(TEXT("Third success opens without auto pickup"), Locked->IsExhibitionCaseOpen() && Locked->IsPickupReady());
	TestTrue(TEXT("Opening hides every glass pane without hiding the loot"), HasExpectedGlass(Locked, false));
	TestNull(TEXT("Opening releases case operator"), Locked->GetCaseOperator());
	TestEqual(TEXT("Opening has not awarded value"), Players[1]->GetTotalLootScore(), 0);
	UHeistInventoryComponent* Inventory = Crew[1]->GetInventoryComponent();
	TArray<int32> FillIds;
	for (int32 Index = 0; Index < 25; ++Index)
	{
		int32 Added = INDEX_NONE;
		if (!TestTrue(TEXT("Fill all grid cells using real item additions"), Inventory->TryAddItem(TEXT("Throwable_Coin"), Added))) return false;
		FillIds.Add(Added);
	}
	Invoke(Controllers[1], TEXT("Server_RequestLootPickup"), Locked);
	TestTrue(TEXT("Full inventory keeps opened case and loot available"), Locked->IsExhibitionCaseOpen() && Locked->IsPickupReady());
	TestEqual(TEXT("Rejected full pickup never awards value"), Players[1]->GetTotalLootScore(), 0);
	for (const int32 Id : FillIds)
	{
		FHeistInventoryItem Removed;
		Inventory->TryRemoveItem(Id, Removed);
	}
	Invoke(Controllers[1], TEXT("Server_RequestLootPickup"), Locked);
	Invoke(Controllers[0], TEXT("Server_RequestLootPickup"), Locked);
	Invoke(Controllers[1], TEXT("Server_RequestLootPickup"), Locked);
	TestFalse(TEXT("Separate pickup commits availability once"), Locked->IsLootAvailable());
	TestEqual(TEXT("Exactly one winner receives the loot value"), Players[1]->GetTotalLootScore(), Locked->GetScoreValue());
	TestEqual(TEXT("Duplicate and other-player pickup grant no value"), Players[0]->GetTotalLootScore(), 0);
	TestEqual(TEXT("Exactly one grid entry commits"), Inventory->GetReplicatedInventory().Items.Num(), 1);
	Move(Crew[0], Other->GetActorLocation());
	Advance(0.2f);
	Players[0]->ExactPing = 6000.0f;
	Invoke(Controllers[0], TEXT("Server_RequestLootCase"), Other, Other->GetCaseRevision());
	if (!TestTrue(TEXT("Latency boundary fixture actually starts"), Other->GetCaseOperator() == Players[0])) return false;
	const float RightEdgeInputProgress = Other->GetCaseSuccessWindowCenter() + Other->GetCaseSuccessWindowWidth() * 0.5f - 0.005f;
	// Rewind by 0.25s puts this just inside the right edge. No rewind places it
	// outside; uncapped 3s half-RTT also places it outside the first latch window.
	Advance(0.5f + RightEdgeInputProgress * 6.0f + 0.25f);
	Invoke(Controllers[0], TEXT("Server_RequestLootCase"), Other, Other->GetCaseRevision());
	if (!TestEqual(TEXT("Measured half-RTT is applied and clamped to 0.25 seconds"), Other->GetCompletedCaseLatches(), 1)) return false;
	Players[0]->ExactPing = 0.0f;
	Invoke(Controllers[0], TEXT("Server_CancelLootCase"), Other);
	if (!TestNull(TEXT("Owner cancel RPC releases latency fixture"), Other->GetCaseOperator())) return false;
	TestEqual(TEXT("Owner cancellation preserves completed latch"), Other->GetCompletedCaseLatches(), 1);
	Advance(0.2f);
	Invoke(Controllers[0], TEXT("Server_RequestLootCase"), Other, Other->GetCaseRevision());
	if (!TestTrue(TEXT("Distance cancellation fixture actually starts"), Other->GetCaseOperator() == Players[0])) return false;
	Move(Crew[0], Other->GetActorLocation() + FVector(200, 0, 0));
	Advance(0.2f);
	TestNull(TEXT("Distance/overlap loss cancels ownership"), Other->GetCaseOperator());
	Invoke(Controllers[0], TEXT("Server_RequestLootCase"), Other, Other->GetCaseRevision());
	TestNull(TEXT("Out-of-range RPC cannot restart"), Other->GetCaseOperator());
	Move(Crew[0], Other->GetActorLocation());
	Advance(0.2f);
	Invoke(Controllers[0], TEXT("Server_RequestLootCase"), Other, Other->GetCaseRevision());
	if (!TestTrue(TEXT("Inventory cancellation fixture actually starts"), Other->GetCaseOperator() == Players[0])) return false;
	Crew[0]->GetInventoryComponent()->TrySetInventoryOpen(true);
	Advance(0.2f);
	TestNull(TEXT("Inventory UI transition cancels ownership"), Other->GetCaseOperator());
	Invoke(Controllers[0], TEXT("Server_RequestLootCase"), Other, Other->GetCaseRevision());
	TestNull(TEXT("Open inventory cannot bypass case state validation"), Other->GetCaseOperator());
	Crew[0]->GetInventoryComponent()->TrySetInventoryOpen(false);
	Advance(0.2f);
	Invoke(Controllers[0], TEXT("Server_RequestLootCase"), Other, Other->GetCaseRevision());
	if (!TestTrue(TEXT("Stun cancellation fixture actually starts"), Other->GetCaseOperator() == Players[0])) return false;
	Crew[0]->GetStatusComponent()->ApplyTimedStatusTag(FHeistGameplayTags::Get().Event_Player_Stunned, 10.0f);
	Advance(0.2f);
	TestNull(TEXT("Stun cancels ownership"), Other->GetCaseOperator());
	Crew[0]->GetStatusComponent()->ClearStatusTag(FHeistGameplayTags::Get().Event_Player_Stunned);
	Advance(0.2f);
	Invoke(Controllers[0], TEXT("Server_RequestLootCase"), Other, Other->GetCaseRevision());
	if (!TestTrue(TEXT("Roster-removal fixture actually starts"), Other->GetCaseOperator() == Players[0])) return false;
	State->RemovePlayerState(Players[0]);
	Advance(0.2f);
	TestNull(TEXT("Leaving the crew array releases the operator"), Other->GetCaseOperator());
	State->AddPlayerState(Players[0]);
	Advance(0.2f);
	Invoke(Controllers[0], TEXT("Server_RequestLootCase"), Other, Other->GetCaseRevision());
	if (!TestTrue(TEXT("Match-End cancellation fixture actually starts"), Other->GetCaseOperator() == Players[0])) return false;
	State->SetMatchPhase(EHeistMatchPhase::End);
	TestNull(TEXT("Match End synchronously clears operation"), Other->GetCaseOperator());
	Invoke(Controllers[0], TEXT("Server_RequestLootCase"), Other, Other->GetCaseRevision());
	TestNull(TEXT("Post-match RPC cannot start operation"), Other->GetCaseOperator());
	State->GetSoundPingEventReportedDelegate().Clear();
	if (!HasAnyErrors())
		AddInfo(TEXT("Canonical BP_Loot server authority fixture: Locked/Inactive bypass, stale revision, Controller/Pawn/PlayerState context, exclusive case ownership, timed latches, case GuardNoise, teammate resume, Open then Pickup, full-grid rollback, duplicate pickup, 0.25s half-RTT bound, distance/Inventory/Stun/roster-removal/End cancellation PASS. Map input, network RPC ownership transport, actual Disconnect, User PIE/replication/Steam not exercised."));
	return true;
}

#endif
