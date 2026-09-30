#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Character/HeistPlayerCharacter.h"
#include "Character/Components/HeistInteractionComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "World/Actors/Security/HeistDetentionDoorActor.h"
#include "Character/Components/HeistActionComponent.h"
#include "Character/Components/HeistForgeryComponent.h"
#include "Character/Components/HeistInventoryComponent.h"
#include "AI/HeistGuardCharacter.h"
#include "Core/HeistGameInstance.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistPlayerState.h"
#include "Core/HeistTypes.h"
#include "Data/HeistArtifactDataTypes.h"
#include "Data/HeistGameBalanceDataAsset.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "UI/ViewModels/HeistHUDViewModel.h"
#include "UObject/UnrealType.h"
#include "World/Actors/Loot/HeistLootActor.h"
#include "World/Actors/Loot/HeistObjectDisplayCaseActor.h"
#include "World/Actors/Loot/HeistPaintingDisplayCaseActor.h"
#include "World/Actors/Loot/HeistDroppedOriginalActor.h"
#include "UObject/StructOnScope.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistWeek7ReadabilityContractTest, "ProjectMuseumHeist.W7.ReadabilityContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistWeek7ReadabilityContractTest::RunTest(const FString& Parameters)
{
	const AHeistPlayerCharacter* CharacterCDO = GetDefault<AHeistPlayerCharacter>();
	TestNotNull(TEXT("Player character CDO exists"), CharacterCDO);
	if (CharacterCDO)
	{
		TestTrue(TEXT("Walk baseline is 300 cm/s"), FMath::IsNearlyEqual(CharacterCDO->CalculateMovementSpeedForPace(0.0f, false), 300.0f));
		TestTrue(TEXT("Sprint baseline is 600 cm/s"), FMath::IsNearlyEqual(CharacterCDO->CalculateMovementSpeedForPace(0.0f, true), 600.0f));
		TestTrue(TEXT("Walk weight penalty is 7.5 cm/s per kg"), FMath::IsNearlyEqual(CharacterCDO->CalculateMovementSpeedForPace(10.0f, false), 225.0f));
		TestTrue(TEXT("Sprint weight penalty is 15 cm/s per kg"), FMath::IsNearlyEqual(CharacterCDO->CalculateMovementSpeedForPace(10.0f, true), 450.0f));
		TestTrue(TEXT("Walk minimum is 150 cm/s"), FMath::IsNearlyEqual(CharacterCDO->CalculateMovementSpeedForPace(999.0f, false), 150.0f));
		TestTrue(TEXT("Sprint minimum is 250 cm/s"), FMath::IsNearlyEqual(CharacterCDO->CalculateMovementSpeedForPace(999.0f, true), 250.0f));
	}

	for (const EHeistCrewStatus Status : {EHeistCrewStatus::Active, EHeistCrewStatus::Forging, EHeistCrewStatus::Assembling, EHeistCrewStatus::CarryingOriginal,
			 EHeistCrewStatus::Heavy, EHeistCrewStatus::Stunned, EHeistCrewStatus::Arrested, EHeistCrewStatus::Escaped})
	{
		TestFalse(TEXT("Crew status full label is present"), HeistCrewStatus::ToDisplayText(Status).IsEmpty());
		TestFalse(TEXT("Crew status compact label is present"), HeistCrewStatus::ToCompactText(Status).IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistWeek7InputAssetContractTest, "ProjectMuseumHeist.W7.InputAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistWeek7InputAssetContractTest::RunTest(const FString& Parameters)
{
	const UInputAction* SprintAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Assets/Input/IA_Sprint.IA_Sprint"));
	const UInputAction* MapAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Assets/Input/IA_Map.IA_Map"));
	const UInputMappingContext* GameplayContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Assets/Input/IMC_Default.IMC_Default"));
	const UInputMappingContext* MapContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Assets/Input/IMC_Map.IMC_Map"));
	TestNotNull(TEXT("IA_Sprint exists"), SprintAction);
	TestNotNull(TEXT("IA_Map exists"), MapAction);
	TestNotNull(TEXT("IMC_Default exists"), GameplayContext);
	TestNotNull(TEXT("IMC_Map exists"), MapContext);

	auto HasMapping = [](const UInputMappingContext* Context, const UInputAction* Action, const FKey Key)
	{
		return IsValid(Context) && Context->GetMappings().ContainsByPredicate(
			[Action, Key](const FEnhancedActionKeyMapping& Mapping) { return Mapping.Action == Action && Mapping.Key == Key; });
	};
	TestTrue(TEXT("Gameplay maps Left Shift to sprint"), HasMapping(GameplayContext, SprintAction, EKeys::LeftShift));
	TestTrue(TEXT("Gameplay maps M to map"), HasMapping(GameplayContext, MapAction, EKeys::M));
	TestTrue(TEXT("Map mode maps M to close"), HasMapping(MapContext, MapAction, EKeys::M));

	const UClass* ControllerClass = LoadClass<AHeistPlayerController>(nullptr, TEXT("/Game/Blueprints/Player/BP_HeistPlayerController.BP_HeistPlayerController_C"));
	const AHeistPlayerController* ControllerCDO = IsValid(ControllerClass) ? Cast<AHeistPlayerController>(ControllerClass->GetDefaultObject()) : nullptr;
	TestNotNull(TEXT("BP_HeistPlayerController CDO exists"), ControllerCDO);
	TestTrue(TEXT("Controller W7 input defaults are assigned"), IsValid(ControllerCDO) && ControllerCDO->AreInputAssetsConfiguredForDebug());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistWeek7VariationContractTest, "ProjectMuseumHeist.W7.Variation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistWeek7VariationContractTest::RunTest(const FString& Parameters)
{
	const UHeistGameInstance* GameInstanceCDO = GetDefault<UHeistGameInstance>();
	const AHeistGameMode* GameModeCDO = GetDefault<AHeistGameMode>();
	const UDataTable* TemplateTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DataTable/DT_ForgeryTemplate.DT_ForgeryTemplate"));
	TestNotNull(TEXT("Heist GameMode CDO exists"), GameModeCDO);
	TestNotNull(TEXT("DT_ForgeryTemplate exists"), TemplateTable);
	TestTrue(TEXT("DT_ForgeryTemplate uses FHeistForgeryTemplateRow"), IsValid(TemplateTable) && TemplateTable->GetRowStruct() == FHeistForgeryTemplateRow::StaticStruct());
	TMap<FName, int32> TemplateCountByPool;
	int32 InvalidTemplateCount = 0;
	int32 MissingReferenceAssetCount = 0;
	int32 MissingMaskAssetCount = 0;
	if (IsValid(TemplateTable) && TemplateTable->GetRowStruct() == FHeistForgeryTemplateRow::StaticStruct())
	{
		for (const FName RowName : TemplateTable->GetRowNames())
		{
			const FHeistForgeryTemplateRow* Row = TemplateTable->FindRow<FHeistForgeryTemplateRow>(RowName, TEXT("FHeistWeek7VariationContractTest"), false);
			FHeistForgeryTemplateRow RuntimeDefinition;
			const bool bRuntimeLookupValid = IsValid(GameModeCDO) && GameModeCDO->TryGetForgeryTemplateDefinition(RowName, RuntimeDefinition);
			if (Row != nullptr && Row->TemplateId == RowName)
			{
				++TemplateCountByPool.FindOrAdd(Row->SurfacePoolId);
				const bool bValidDifficultyContract =
					(Row->AllowedPalette.Num() == HeistSurfaceForgeryInventory::EasyPaletteCount && FMath::IsNearlyEqual(Row->ForgeryDuration, 35.0f) && Row->StrokeLimit == 4096) ||
					(Row->AllowedPalette.Num() == HeistSurfaceForgeryInventory::MediumPaletteCount && FMath::IsNearlyEqual(Row->ForgeryDuration, 40.0f) && Row->StrokeLimit == 5120) ||
					(Row->AllowedPalette.Num() == HeistSurfaceForgeryInventory::HardPaletteCount && FMath::IsNearlyEqual(Row->ForgeryDuration, 45.0f) && Row->StrokeLimit == 6144);
				const bool bMissingRequiredMask = Row->BackgroundFilterMode == EHeistForgeryBackgroundFilter::None && Row->ReferenceMask.IsNull();
				const FSoftObjectPath ReferenceImagePath = Row->ReferenceImage.ToSoftObjectPath();
				const FSoftObjectPath ReferenceMaskPath = Row->ReferenceMask.ToSoftObjectPath();
				const bool bInvalidReferenceMaskPackage = !Row->ReferenceMask.IsNull() &&
					(!ReferenceMaskPath.IsValid() || !FPackageName::DoesPackageExist(ReferenceMaskPath.GetLongPackageName()));
				MissingReferenceAssetCount += !ReferenceImagePath.IsValid() || !FPackageName::DoesPackageExist(ReferenceImagePath.GetLongPackageName());
				MissingMaskAssetCount += bMissingRequiredMask || bInvalidReferenceMaskPackage;
				InvalidTemplateCount += !bRuntimeLookupValid || Row->ReferenceImage.IsNull() || bMissingRequiredMask || Row->ObservationDuration < 0.0f ||
					!FMath::IsWithinInclusive(Row->ForgeryDuration, 20.0f, 45.0f) || Row->BrushSize <= 0.0f || !bValidDifficultyContract;
			}
			else
			{
				++InvalidTemplateCount;
			}
		}
	}
	TestEqual(TEXT("All Surface templates satisfy the runtime interaction contract"), InvalidTemplateCount, 0);
	TestEqual(TEXT("All Surface reference image packages exist"), MissingReferenceAssetCount, 0);
	TestEqual(TEXT("All Surface reference mask packages exist"), MissingMaskAssetCount, 0);
	for (const FName PoolId : {FName(TEXT("M01")), FName(TEXT("M02")), FName(TEXT("M03"))})
	{
		const int32 TemplateCount = TemplateCountByPool.FindRef(PoolId);
		TestEqual(FString::Printf(TEXT("%s Surface pool contains the required 40 templates"), *PoolId.ToString()), TemplateCount, 40);
	}

	int32 DrawCount = 0;
	int32 FirstCycleUnique = 0;
	int32 SecondCycleUnique = 0;
	TestTrue(TEXT("Random map bag exhausts all maps before reuse"),
		GameInstanceCDO->RunRandomMapShuffleBagSelfTestForDebug(DrawCount, FirstCycleUnique, SecondCycleUnique));
	TestEqual(TEXT("Random map test draws two cycles"), DrawCount, 6);
	TestEqual(TEXT("First random map cycle has 3 unique maps"), FirstCycleUnique, 3);
	TestEqual(TEXT("Second random map cycle has 3 unique maps"), SecondCycleUnique, 3);

	int32 SurfaceDrawCount = 0;
	int32 SurfaceFirstUnique = 0;
	int32 SurfaceSecondUnique = 0;
	int32 RecentChecks = 0;
	int32 RecentPasses = 0;
	TestTrue(TEXT("Fixed-seed Surface template shuffle-bag is deterministic and protects recent history"), GameInstanceCDO->RunSurfaceTemplateShuffleBagSelfTestForDebug(
		40, SurfaceDrawCount, SurfaceFirstUnique, SurfaceSecondUnique, RecentChecks, RecentPasses));
	TestEqual(TEXT("Surface test draws two 40-template cycles"), SurfaceDrawCount, 80);
	TestEqual(TEXT("Surface first cycle unique count"), SurfaceFirstUnique, 40);
	TestEqual(TEXT("Surface second cycle unique count"), SurfaceSecondUnique, 40);
	TestEqual(TEXT("All recent-history checks pass"), RecentPasses, RecentChecks);

	int32 MatchSelectedCount = 0;
	int32 MatchUniqueCount = 0;
	int32 MatchBagCycle = 0;
	TestTrue(TEXT("A 40-template catalog selects 20 unique templates even across a shuffle-bag refill"),
		GameInstanceCDO->RunSurfaceTemplateMatchSelectionSelfTestForDebug(40, 20, MatchSelectedCount, MatchUniqueCount, MatchBagCycle));
	TestEqual(TEXT("Match selection returns 20 templates"), MatchSelectedCount, 20);
	TestEqual(TEXT("All match-selected templates are unique"), MatchUniqueCount, 20);
	TestTrue(TEXT("Match selection crossed the synthetic shuffle-bag refill"), MatchBagCycle >= 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistWeek7BalanceContractTest, "ProjectMuseumHeist.W7.Balance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistWeek7BalanceContractTest::RunTest(const FString& Parameters)
{
	const UHeistGameBalanceDataAsset* Balance = LoadObject<UHeistGameBalanceDataAsset>(nullptr, TEXT("/Game/Data/DataAsset/DA_GameBalance.DA_GameBalance"));
	TestNotNull(TEXT("DA_GameBalance exists"), Balance);
	if (!Balance)
	{
		return false;
	}
	const TArray<TPair<FString, TSoftObjectPtr<UDataTable>>> RequiredDataTables = {
		{TEXT("DT_ItemData"), Balance->ItemDataTable},
		{TEXT("DT_LootData"), Balance->LootDataTable},
		{TEXT("DT_ContractData"), Balance->ContractDataTable},
		{TEXT("DT_MapPresentation"), Balance->MapPresentationDataTable},
		{TEXT("DT_ArtifactData"), Balance->ArtifactDataTable},
		{TEXT("DT_ForgeryTemplate"), Balance->ForgeryTemplateDataTable},
		{TEXT("DT_ObjectAssemblyPart"), Balance->ObjectAssemblyPartDataTable},
		{TEXT("DT_ObjectAssemblyTemplate"), Balance->ObjectAssemblyTemplateDataTable},
		{TEXT("DT_UsableItemData"), Balance->UsableItemDataTable},
		{TEXT("DT_SoundPingData"), Balance->SoundPingDataTable},
		{TEXT("DT_GuardData"), Balance->GuardDataTable},
	};
	for (const TPair<FString, TSoftObjectPtr<UDataTable>>& RequiredDataTable : RequiredDataTables)
	{
		TestFalse(*FString::Printf(TEXT("%s is assigned"), *RequiredDataTable.Key), RequiredDataTable.Value.IsNull());
		TestNotNull(*FString::Printf(TEXT("%s loads"), *RequiredDataTable.Key), RequiredDataTable.Value.LoadSynchronous());
	}
	TestFalse(TEXT("BP_Loot shell is assigned"), Balance->WorldLootActorClass.IsNull());
	TestNotNull(TEXT("BP_Loot shell loads"), Balance->WorldLootActorClass.LoadSynchronous());
	TestFalse(TEXT("BP_ObjectDisplayCase shell is assigned"), Balance->ObjectDisplayCaseActorClass.IsNull());
	TestNotNull(TEXT("BP_ObjectDisplayCase shell loads"), Balance->ObjectDisplayCaseActorClass.LoadSynchronous());
	const float ExpectedGuardMultipliers[] = {0.75f, 1.00f, 1.25f, 1.50f};
	const float ExpectedDetectionMultipliers[] = {0.85f, 1.00f, 1.10f, 1.20f};
	const float ExpectedInspectionMultipliers[] = {1.20f, 1.00f, 0.90f, 0.80f};
	const int32 ExpectedGuardsForFourAuthored[] = {3, 4, 5, 6};
	for (int32 PlayerCount = 1; PlayerCount <= 4; ++PlayerCount)
	{
		FHeistPlayerCountDifficultyBaseline Baseline;
		TestTrue(FString::Printf(TEXT("Difficulty baseline exists for %d players"), PlayerCount), Balance->TryGetPlayerCountDifficultyBaseline(PlayerCount, Baseline));
		TestTrue(TEXT("Guard multiplier is positive"), Baseline.GuardCountMultiplier > 0.0f);
		TestTrue(TEXT("Detection multiplier is positive"), Baseline.DetectionMultiplier > 0.0f);
		TestTrue(TEXT("Inspection multiplier is positive"), Baseline.InspectionDurationMultiplier > 0.0f);
		TestTrue(FString::Printf(TEXT("Guard multiplier matches %d-player balance table"), PlayerCount),
			FMath::IsNearlyEqual(Baseline.GuardCountMultiplier, ExpectedGuardMultipliers[PlayerCount - 1]));
		TestTrue(FString::Printf(TEXT("Detection multiplier matches %d-player balance table"), PlayerCount),
			FMath::IsNearlyEqual(Baseline.DetectionMultiplier, ExpectedDetectionMultipliers[PlayerCount - 1]));
		TestTrue(FString::Printf(TEXT("Inspection multiplier matches %d-player balance table"), PlayerCount),
			FMath::IsNearlyEqual(Baseline.InspectionDurationMultiplier, ExpectedInspectionMultipliers[PlayerCount - 1]));
		TestEqual(FString::Printf(TEXT("Guard count rounding matches %d-player balance table"), PlayerCount),
			AHeistGameMode::CalculateDifficultyGuardCount(4, Baseline.GuardCountMultiplier), ExpectedGuardsForFourAuthored[PlayerCount - 1]);
	}
	TestTrue(TEXT("Reward multiplier bounds are ordered"), Balance->MinimumForgeryRewardMultiplier <= Balance->MaximumForgeryRewardMultiplier);
	TestTrue(TEXT("Alert reward penalty does not mutate quota"), Balance->AlertLevelRewardPenalty >= 0.0f && Balance->AlertLevelRewardPenalty <= 0.25f);
	TestTrue(TEXT("Arrest reward penalty is bounded"), Balance->ArrestRewardPenaltyPerPlayer >= 0.0f && Balance->ArrestRewardPenaltyPerPlayer <= 1.0f);
	TestTrue(TEXT("Minimum forgery reward matches balance table"), FMath::IsNearlyEqual(Balance->MinimumForgeryRewardMultiplier, 0.75f));
	TestTrue(TEXT("Maximum forgery reward matches balance table"), FMath::IsNearlyEqual(Balance->MaximumForgeryRewardMultiplier, 1.25f));
	TestTrue(TEXT("Alert reward penalty matches balance table"), FMath::IsNearlyEqual(Balance->AlertLevelRewardPenalty, 0.05f));
	TestTrue(TEXT("Minimum stealth reward matches balance table"), FMath::IsNearlyEqual(Balance->MinimumStealthRewardMultiplier, 0.75f));
	TestTrue(TEXT("Arrest reward penalty matches balance table"), FMath::IsNearlyEqual(Balance->ArrestRewardPenaltyPerPlayer, 0.10f));
	TestTrue(TEXT("Vent settlement unlocks at 180 seconds"), FMath::IsNearlyEqual(Balance->VentUnlockTime, 180.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistObservationReferenceTest, "ProjectMuseumHeist.W7.ObservationReference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistObservationReferenceTest::RunTest(const FString& Parameters)
{
	// Exercise the real HUD and RepNotify callbacks in an isolated world. This verifies
	// presentation convergence for either snapshot order, not multiplayer transport.
	const UWorld::InitializationValues WorldValues = UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &WorldValues);
	if (!TestNotNull(TEXT("Transient HUD test world exists"), World))
	{
		return false;
	}
	UHeistHUDViewModel* ViewModel = nullptr;
	ON_SCOPE_EXIT
	{
		if (IsValid(ViewModel))
		{
			ViewModel->GetPresentationChangedDelegate().Clear();
			ViewModel->ConditionalBeginDestroy();
		}
		World->DestroyWorld(false);
	};

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AHeistPlayerCharacter* FirstCharacter = World->SpawnActor<AHeistPlayerCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
	AHeistPlayerCharacter* SecondCharacter = World->SpawnActor<AHeistPlayerCharacter>(FVector(500.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParameters);
	AHeistGameState* GameState = World->SpawnActor<AHeistGameState>();
	if (!TestNotNull(TEXT("First local component owner exists"), FirstCharacter) || !TestNotNull(TEXT("Replacement component owner exists"), SecondCharacter) ||
		!TestNotNull(TEXT("Objective GameState exists"), GameState))
	{
		return false;
	}
	UHeistActionComponent* FirstAction = FirstCharacter->GetActionComponent();
	UHeistForgeryComponent* FirstForgery = FirstCharacter->GetForgeryComponent();
	UHeistActionComponent* SecondAction = SecondCharacter->GetActionComponent();
	UHeistForgeryComponent* SecondForgery = SecondCharacter->GetForgeryComponent();
	const FBoolProperty* CastActiveProperty = FindFProperty<FBoolProperty>(UHeistActionComponent::StaticClass(), TEXT("bObservationCastActive"));
	const FBoolProperty* ReferenceProperty = FindFProperty<FBoolProperty>(UHeistActionComponent::StaticClass(), TEXT("bObservationReferenceAvailable"));
	const FBoolProperty* PreparedProperty = FindFProperty<FBoolProperty>(UHeistForgeryComponent::StaticClass(), TEXT("bTemplatePrepared"));
	const FNameProperty* ArtifactProperty = FindFProperty<FNameProperty>(UHeistForgeryComponent::StaticClass(), TEXT("ActiveArtifactId"));
	UFunction* ActionNotify = FirstAction->FindFunction(TEXT("OnRep_ObservationCastActive"));
	UFunction* ForgeryNotify = FirstForgery->FindFunction(TEXT("OnRep_SessionRevision"));
	if (!TestTrue(TEXT("Existing replicated snapshot fields and RepNotify callbacks are available"),
		CastActiveProperty && ReferenceProperty && PreparedProperty && ArtifactProperty && ActionNotify && ForgeryNotify))
	{
		return false;
	}
	const auto ReceiveAction = [CastActiveProperty, ReferenceProperty, ActionNotify](UHeistActionComponent* Component, const bool bActive)
	{
		CastActiveProperty->SetPropertyValue_InContainer(Component, bActive);
		ReferenceProperty->SetPropertyValue_InContainer(Component, bActive);
		Component->ProcessEvent(ActionNotify, nullptr);
	};
	const auto ReceiveTemplate = [PreparedProperty, ArtifactProperty, ForgeryNotify](UHeistForgeryComponent* Component, const FName ArtifactId)
	{
		PreparedProperty->SetPropertyValue_InContainer(Component, !ArtifactId.IsNone());
		ArtifactProperty->SetPropertyValue_InContainer(Component, ArtifactId);
		Component->ProcessEvent(ForgeryNotify, nullptr);
	};
	const FName RequiredArtifact(TEXT("Required_A"));
	const FName ObservedArtifact(TEXT("Observed_B"));
	GameState->SetObjectiveSnapshot(RequiredArtifact, TEXT("RequiredCase"), EHeistObjectiveState::Available, nullptr);
	ViewModel = NewObject<UHeistHUDViewModel>();
	ViewModel->SetupViewModel(GameState, nullptr, FirstAction);

	ReceiveAction(FirstAction, true);
	TestTrue(TEXT("Action arriving before the template keeps the cast visible"), ViewModel->IsObservationCastActive());
	TestFalse(TEXT("An unprepared reference stays hidden"), ViewModel->IsObservationReferenceVisible());
	TestTrue(TEXT("Missing template does not substitute the contract target"), ViewModel->GetObservationReferenceArtifactId().IsNone() && ViewModel->GetObservationReferenceText().IsEmpty());
	ReceiveTemplate(FirstForgery, ObservedArtifact);
	TestTrue(TEXT("Late template notification reveals the reference without another Action event"), ViewModel->IsObservationReferenceVisible());
	TestEqual(TEXT("Observing B references B"), ViewModel->GetObservationReferenceArtifactId(), ObservedArtifact);
	TestTrue(TEXT("Reference text names B and not required A"), ViewModel->GetObservationReferenceText().ToString().Contains(TEXT("Observed B")) &&
		!ViewModel->GetObservationReferenceText().ToString().Contains(TEXT("Required A")));
	TestEqual(TEXT("Contract objective remains A while observing B"), ViewModel->GetObjectiveArtifactId(), RequiredArtifact);
	TestTrue(TEXT("Contract text remains A"), ViewModel->GetObjectiveStateText().ToString().Contains(TEXT("Required A")));

	ReceiveAction(FirstAction, false);
	TestTrue(TEXT("Cancellation immediately clears the reference before template cleanup arrives"), !ViewModel->IsObservationReferenceVisible() &&
		ViewModel->GetObservationReferenceArtifactId().IsNone() && ViewModel->GetObservationReferenceText().IsEmpty());
	ReceiveTemplate(FirstForgery, NAME_None);
	ReceiveTemplate(FirstForgery, ObservedArtifact);
	TestFalse(TEXT("Template arriving before the Action does not reveal an inactive cast"), ViewModel->IsObservationReferenceVisible());
	ReceiveAction(FirstAction, true);
	TestTrue(TEXT("Action notification reveals the already prepared reference"), ViewModel->IsObservationReferenceVisible());
	TestEqual(TEXT("Template-first order also references B"), ViewModel->GetObservationReferenceArtifactId(), ObservedArtifact);

	int32 RefreshCount = 0;
	ViewModel->GetPresentationChangedDelegate().AddLambda([&RefreshCount]() { ++RefreshCount; });
	ViewModel->SetupViewModel(GameState, nullptr, FirstAction);
	ViewModel->SetupViewModel(GameState, nullptr, FirstAction);
	RefreshCount = 0;
	ReceiveTemplate(FirstForgery, ObservedArtifact);
	TestEqual(TEXT("Repeated Setup subscribes to the template only once"), RefreshCount, 1);
	ViewModel->SetupViewModel(GameState, nullptr, SecondAction);
	RefreshCount = 0;
	ReceiveAction(FirstAction, false);
	ReceiveTemplate(FirstForgery, NAME_None);
	TestEqual(TEXT("Rebinding removes both previous component subscriptions"), RefreshCount, 0);
	ReceiveTemplate(SecondForgery, ObservedArtifact);
	ReceiveAction(SecondAction, true);
	TestTrue(TEXT("Replacement local components drive the reference"), ViewModel->IsObservationReferenceVisible() &&
		ViewModel->GetObservationReferenceArtifactId() == ObservedArtifact);
	ViewModel->GetPresentationChangedDelegate().Clear();
	ViewModel->ConditionalBeginDestroy();
	TestFalse(TEXT("HUD destruction unbinds Action"), SecondAction->GetActionStateChangedDelegate().IsBoundToObject(ViewModel));
	TestFalse(TEXT("HUD destruction unbinds template"), SecondForgery->GetSessionStateChangedDelegate().IsBoundToObject(ViewModel));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistDetentionRecoveryTest, "ProjectMuseumHeist.W7.DetentionRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistDetentionRecoveryTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues WorldValues = UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &WorldValues);
	if (!TestNotNull(TEXT("Detention test world exists"), World))
	{
		return false;
	}
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	AHeistGameState* GameState = World->SpawnActor<AHeistGameState>();
	World->SetGameState(GameState);
	GameState->SetMatchPhase(EHeistMatchPhase::InGame);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AHeistPlayerCharacter* Character = World->SpawnActor<AHeistPlayerCharacter>(FVector(1200, 1600, 100), FRotator::ZeroRotator, SpawnParameters);
	AHeistPlayerState* Player = World->SpawnActor<AHeistPlayerState>();
	AHeistPlayerState* Teammate = World->SpawnActor<AHeistPlayerState>();
	if (!TestTrue(TEXT("Detention actors exist"), IsValid(Character) && IsValid(Player) && IsValid(Teammate)))
	{
		return false;
	}
	Character->SetPlayerState(Player);
	GameState->AddPlayerState(Player);
	GameState->AddPlayerState(Teammate);
	const FVector DetentionLocation = Character->GetActorLocation();
	// Tick only this world's real timer manager; no test-only release entry point.
	const auto AdvanceTimers = [World](float Seconds)
	{
		++GFrameCounter;
		World->GetTimerManager().Tick(Seconds);
	};
	AdvanceTimers(0.01f);
	TestTrue(TEXT("Arrest starts"), Player->MarkArrested(nullptr));
	TestTrue(TEXT("Default restraint is 5 seconds"), FMath::IsNearlyEqual(Player->GetDetentionRemainingSeconds(), 5.0f));
	TestTrue(TEXT("Arrest disables movement"), Character->GetCharacterMovement()->MovementMode == MOVE_None);
	Teammate->MarkArrested(nullptr);
	TestTrue(TEXT("All crew are arrested"), GameState->AreAllRemainingCrewMembersArrested());
	TestFalse(TEXT("Recoverable arrest is not a terminal all-crew result"), GameState->AreAllCrewMembersResolved());
	AdvanceTimers(4.0f);
	TestTrue(TEXT("Movement remains locked before deadline"), Player->IsArrested() && Character->GetCharacterMovement()->MovementMode == MOVE_None);
	AdvanceTimers(1.1f);
	TestFalse(TEXT("Real timer clears arrest after deadline"), Player->IsArrested());
	TestFalse(TEXT("Release clears pending recovery"), Player->IsDetentionRecoveryPending());
	TestTrue(TEXT("Release restores walking"), Character->GetCharacterMovement()->MovementMode == MOVE_Walking);
	TestEqual(TEXT("Player must leave detention physically"), Character->GetActorLocation(), DetentionLocation);
	TestFalse(TEXT("Release is not extraction"), Player->IsEscaped());
	TestEqual(TEXT("Release grants no confiscated loot"), Player->GetTotalLootScore(), 0);
	TestEqual(TEXT("Crew status becomes active"), Player->GetCrewStatus(), EHeistCrewStatus::Active);

	Player->MarkArrested(nullptr);
	AdvanceTimers(2.0f);
	TestTrue(TEXT("Teammate rescue can clear arrest early"), Player->ClearArrested());
	TestFalse(TEXT("Early rescue removes timer state"), Player->IsDetentionRecoveryPending());
	Player->MarkArrested(nullptr);
	AdvanceTimers(3.1f);
	TestTrue(TEXT("Previous arrest deadline cannot release a new arrest"), Player->IsArrested());
	AdvanceTimers(2.0f);
	TestFalse(TEXT("New arrest releases at its own deadline"), Player->IsArrested());

	Player->MarkArrested(nullptr);
	GameState->SetMatchPhase(EHeistMatchPhase::End);
	TestFalse(TEXT("End cancels recovery"), Player->IsDetentionRecoveryPending());
	AdvanceTimers(6.0f);
	TestTrue(TEXT("No timer unlocks movement after match end"), Player->IsArrested() && Character->GetCharacterMovement()->MovementMode == MOVE_None);
	TestTrue(TEXT("Final arrest result is preserved"), GameState->AreAllCrewMembersResolved());
	Player->ClearArrested();
	Teammate->ClearArrested();
	GameState->SetMatchPhase(EHeistMatchPhase::InGame);
	Player->MarkArrested(nullptr);
	GameState->SetMatchPhase(EHeistMatchPhase::Lobby);
	GameState->SetMatchPhase(EHeistMatchPhase::InGame);
	AdvanceTimers(6.0f);
	TestTrue(TEXT("Lobby transition cancels old release even after returning to InGame"), Player->IsArrested() && !Player->IsDetentionRecoveryPending());
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistDetentionDoorTest, "ProjectMuseumHeist.W7.DetentionDoor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistDetentionDoorTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	AHeistGameState* State = World->SpawnActor<AHeistGameState>();
	World->SetGameState(State);
	State->SetMatchPhase(EHeistMatchPhase::InGame);
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AHeistPlayerCharacter* Character = World->SpawnActor<AHeistPlayerCharacter>(FVector(0,-80,0), FRotator::ZeroRotator, Spawn);
	AHeistPlayerState* Player = World->SpawnActor<AHeistPlayerState>();
	Character->SetPlayerState(Player);
	State->AddPlayerState(Player);
	AHeistDetentionDoorActor* Door = World->SpawnActor<AHeistDetentionDoorActor>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
	Character->DispatchBeginPlay();
	Door->DispatchBeginPlay();
	World->SetBegunPlay(true);
	World->GetPhysicsScene()->Flush();
	World->GetPhysicsScene()->Flush();
	Character->GetCapsuleComponent()->UpdateOverlaps();
	Door->SecureForArrest(Player);
	Player->MarkArrested(nullptr);
	const auto Advance = [World](float Seconds)
	{
		World->TimeSeconds += Seconds;
		++GFrameCounter;
		World->GetTimerManager().Tick(Seconds);
	};
	Advance(.01f);
	TestFalse(TEXT("Restrained prisoner cannot operate the lock"), Door->TryUse(Character, Door->GetRevision()));
	Advance(5.1f);
	TestFalse(TEXT("Five seconds restores input"), Player->IsArrested());
	TestFalse(TEXT("Input recovery does not open the cell"), Door->IsOpen());
	TestEqual(TEXT("Recovered but contained crew remains counted as detained"), State->GetArrestedCrewCount(), 1);
	TestFalse(TEXT("All-contained crew may still attempt escape"), State->AreAllCrewMembersResolved());
	TestTrue(TEXT("Result contribution retains detention"), Player->GetContribution().bArrested);
	TestTrue(TEXT("Closed cell protects its occupant from reacquisition"), Player->IsProtectedByDetention());
	TestEqual(TEXT("Crew remains detained after input recovery"), Player->GetCrewStatus(), EHeistCrewStatus::Arrested);
	const UPrimitiveComponent* InteractionBox = Cast<UPrimitiveComponent>(Door->GetRootComponent());
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	AddInfo(FString::Printf(TEXT("Overlap fixture: DoorPos=%s CapsulePos=%s RootBounds=%s RootPhysics=%d CapsulePhysics=%d RootChannel=%d CapsuleChannel=%d RootResponse=%d CapsuleResponse=%d RootOverlap=%d CapsuleOverlap=%d InteractionBegun=%d"),
		*InteractionBox->GetComponentLocation().ToString(), *Capsule->GetComponentLocation().ToString(), *InteractionBox->Bounds.BoxExtent.ToString(), InteractionBox->IsPhysicsStateCreated(), Capsule->IsPhysicsStateCreated(),
		(int32)InteractionBox->GetCollisionObjectType(),(int32)Capsule->GetCollisionObjectType(),(int32)InteractionBox->GetCollisionResponseToChannel(Capsule->GetCollisionObjectType()),(int32)Capsule->GetCollisionResponseToChannel(InteractionBox->GetCollisionObjectType()),InteractionBox->GetGenerateOverlapEvents(),Capsule->GetGenerateOverlapEvents(),Character->GetInteractionComponent()->HasBegunPlay()));
	TestTrue(TEXT("Real capsule overlaps door interaction box"), Character->GetInteractionComponent()->IsActorOverlappingInteractionArea(Door));
	if (!TestTrue(TEXT("Prisoner starts latch sequence"), Door->TryUse(Character, Door->GetRevision()))) return false;
	const int32 StaleRevision = Door->GetRevision();
	Advance(4.7f);
	TestTrue(TEXT("First timed press accepted"), Door->TryUse(Character, Door->GetRevision()));
	TestEqual(TEXT("First latch completed"), Door->GetCompletedLatches(), 1);
	TestFalse(TEXT("Stale request cannot advance a new round"), Door->TryUse(Character, StaleRevision));
	int32 NoiseCount = 0;
	State->GetSoundPingEventReportedDelegate().AddLambda([&NoiseCount](const FHeistSoundPingEvent& Event, int32*)
	{
		if (Event.PingType == EHeistSoundPingType::DetentionLock) ++NoiseCount;
	});
	Advance(.8f);
	Door->TryUse(Character, Door->GetRevision());
	TestEqual(TEXT("Mistimed press emits guard noise"), NoiseCount, 1);
	TestEqual(TEXT("Mistake preserves previously completed latch"), Door->GetCompletedLatches(), 1);
	Character->SetActorLocation(FVector(0,-260,0));
	Advance(.2f);
	TestNull(TEXT("Moving cancels lock operation"), Door->GetOperator());
	TestEqual(TEXT("Moving preserves completed latch"), Door->GetCompletedLatches(), 1);
	Character->SetActorLocation(FVector(0,-80,0));
	World->GetPhysicsScene()->Flush();
	Character->GetCapsuleComponent()->UpdateOverlaps();
	Door->TryUse(Character, Door->GetRevision());
	Advance(4.7f);
	Door->TryUse(Character, Door->GetRevision());
	TestEqual(TEXT("Second latch completed"), Door->GetCompletedLatches(), 2);
	Advance(4.7f);
	Door->TryUse(Character, Door->GetRevision());
	TestTrue(TEXT("Third timed latch opens the door"), Door->IsOpen());
	TestNull(TEXT("Opening clears detention association"), Player->GetDetentionDoor());
	TestEqual(TEXT("Opened cell restores active crew count"), State->GetActiveCrewCount(), 1);
	TestFalse(TEXT("Opening clears contribution detention"), Player->GetContribution().bArrested);
	TestFalse(TEXT("Released player loses protection"), Player->IsProtectedByDetention());
	TestFalse(TEXT("Door opening never extracts the player"), Player->IsEscaped());
	TestFalse(TEXT("Opened door rejects replay"), Door->TryUse(Character, Door->GetRevision()));

	Door->SecureForArrest(Player);
	TestEqual(TEXT("New arrest resets latches"), Door->GetCompletedLatches(), 0);
	Character->SetActorLocation(FVector(0,80,0));
	World->GetPhysicsScene()->Flush();
	Character->GetCapsuleComponent()->UpdateOverlaps();
	TestFalse(TEXT("Protection never extends outside cell bounds"), Player->IsProtectedByDetention());
	Advance(.2f);
	Door->TryUse(Character, Door->GetRevision());
	Advance(1.0f);
	Door->ReleaseRescue(Character);
	Advance(1.2f);
	TestFalse(TEXT("Releasing rescue early keeps door locked"), Door->IsOpen());
	Door->TryUse(Character, Door->GetRevision());
	Advance(2.1f);
	TestTrue(TEXT("Outside hold opens after two seconds"), Door->IsOpen());
	Character->SetActorLocation(FVector(0,-80,0));
	Door->SecureForArrest(Player);
	Character->SetActorLocation(FVector(0,80,0));
	World->GetPhysicsScene()->Flush();
	Character->GetCapsuleComponent()->UpdateOverlaps();
	Advance(.2f);
	Door->TryUse(Character, Door->GetRevision());
	State->SetMatchPhase(EHeistMatchPhase::End);
	Advance(3.0f);
	TestFalse(TEXT("Match end cancels pending rescue"), Door->IsOpen());
	TestNull(TEXT("Match end clears lock operator"), Door->GetOperator());
	State->GetSoundPingEventReportedDelegate().Clear();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistDetentionConcurrentTest, "ProjectMuseumHeist.W7.DetentionConcurrent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistDetentionConcurrentTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	AHeistGameState* State = World->SpawnActor<AHeistGameState>();
	World->SetGameState(State);
	State->SetMatchPhase(EHeistMatchPhase::InGame);
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AHeistPlayerCharacter* Crew[3];
	AHeistPlayerState* Players[3];
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Crew[Index] = World->SpawnActor<AHeistPlayerCharacter>(FVector(-60 + Index * 60, -80, 0), FRotator::ZeroRotator, Spawn);
		Players[Index] = World->SpawnActor<AHeistPlayerState>();
		Crew[Index]->SetPlayerState(Players[Index]);
		State->AddPlayerState(Players[Index]);
		Crew[Index]->DispatchBeginPlay();
	}
	AHeistDetentionDoorActor* Door = World->SpawnActor<AHeistDetentionDoorActor>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
	Door->DispatchBeginPlay();
	World->SetBegunPlay(true);
	const auto Move = [World](AHeistPlayerCharacter* Character, FVector Location)
	{
		Character->SetActorLocation(Location);
		World->GetPhysicsScene()->Flush();
		World->GetPhysicsScene()->Flush();
		Character->GetCapsuleComponent()->UpdateOverlaps();
	};
	const auto Advance = [World](float Seconds)
	{
		World->TimeSeconds += Seconds;
		++GFrameCounter;
		World->GetTimerManager().Tick(Seconds);
	};
	for (AHeistPlayerCharacter* Character : Crew) Move(Character, Character->GetActorLocation());
	Advance(.01f);
	Door->SecureForArrest(Players[0]);
	TestTrue(TEXT("First inmate starts"), Door->TryUse(Crew[0], Door->GetRevision()));
	Advance(4.7f);
	Door->TryUse(Crew[0], Door->GetRevision());
	const int32 FirstRoundRevision = Door->GetRevision();
	Door->SecureForArrest(Players[1]);
	TestEqual(TEXT("Additional inmate preserves completed latch"), Door->GetCompletedLatches(), 1);
	TestEqual(TEXT("Additional inmate preserves round revision"), Door->GetRevision(), FirstRoundRevision);
	TestTrue(TEXT("Additional inmate preserves operator"), Door->GetOperator() == Players[0]);
	TestTrue(TEXT("Busy door remains a presentation target"), Door->CanInteract(Crew[2]));
	TestFalse(TEXT("Another inside player cannot steal operation"), Door->TryUse(Crew[2], Door->GetRevision()));
	Move(Crew[1], FVector(0,80,0));
	TestTrue(TEXT("Outside rescuer takes over immediately"), Door->TryUse(Crew[1], Door->GetRevision()));
	TestTrue(TEXT("Takeover belongs to distinct outside player"), Door->GetOperator() == Players[1] && Door->IsRescueOperation());
	TestFalse(TEXT("Previous operator's in-flight revision is rejected"), Door->TryUse(Crew[0], FirstRoundRevision));
	TestFalse(TEXT("Inside player cannot steal outside rescue"), Door->TryUse(Crew[2], Door->GetRevision()));
	Advance(.6f);
	Door->ReleaseRescue(Crew[1]);
	TestFalse(TEXT("Early rescue release keeps door closed"), Door->IsOpen());
	TestEqual(TEXT("Interrupted rescue preserves latch"), Door->GetCompletedLatches(), 1);
	TestTrue(TEXT("Inmate resumes after interrupted rescue"), Door->TryUse(Crew[0], Door->GetRevision()));
	TestTrue(TEXT("Outside rescue can take over resumed lock"), Door->TryUse(Crew[1], Door->GetRevision()));
	Advance(2.1f);
	TestTrue(TEXT("Distinct rescuer opens after hold"), Door->IsOpen());

	Move(Crew[1], FVector(0,0,0));
	Door->SecureForArrest(Players[0]);
	Players[0]->MarkArrested(nullptr);
	TestTrue(TEXT("Blocked threshold delays closure instead of aborting detention"), Door->IsOpen());
	TestTrue(TEXT("Detention is associated while closure waits"), Players[0]->GetDetentionDoor() == Door);
	Advance(5.1f);
	TestFalse(TEXT("Blocked threshold never extends five-second restraint"), Players[0]->IsArrested());
	TestTrue(TEXT("Door remains open while occupied"), Door->IsOpen());
	Move(Crew[1], FVector(0,200,0));
	Advance(.2f);
	TestFalse(TEXT("Door closes only after threshold clears"), Door->IsOpen());
	TestTrue(TEXT("Delayed close updates contained crew status"), Players[0]->GetCrewStatus() == EHeistCrewStatus::Arrested);
	TestNull(TEXT("Visitor receives no arrest association"), Players[2]->GetDetentionDoor());
	State->RemovePlayerState(Players[0]);
	Players[0]->SetDetentionDoor(nullptr);
	TestTrue(TEXT("Visitor can unlock after actual inmate disconnects"), Door->TryUse(Crew[2], Door->GetRevision()));
	for (int32 Latch = 0; Latch < 3; ++Latch)
	{
		Advance(4.7f);
		TestTrue(TEXT("Visitor completes timed latch"), Door->TryUse(Crew[2], Door->GetRevision()));
	}
	TestTrue(TEXT("Visitor can escape without arrest registration"), Door->IsOpen());
	State->AddPlayerState(Players[0]);
	Move(Crew[1], FVector(0,0,0));
	Door->SecureForArrest(Players[0]);
	Move(Crew[0], FVector(-300,200,0));
	Move(Crew[1], FVector(0,200,0));
	Advance(.2f);
	TestTrue(TEXT("All detainees leaving cancels delayed closure"), Door->IsOpen());
	TestNull(TEXT("Departure clears pending detention reference"), Players[0]->GetDetentionDoor());
	Move(Crew[0], FVector(-60,-80,0));
	Advance(.2f);
	TestTrue(TEXT("Canceled closure cannot close behind a returning visitor"), Door->IsOpen());

	Door->SecureForArrest(Players[0]);
	Players[0]->SetCompressedPing(100); // 400 ms RTT, 200 ms server-side transit estimate.
	TestTrue(TEXT("Delayed player's operation starts"), Door->TryUse(Crew[0], Door->GetRevision()));
	Advance(.6f);
	TestFalse(TEXT("Compensation cannot reach before round start"), Door->TryUse(Crew[0], Door->GetRevision()));
	Advance(4.7f); // Arrival progress .80; input estimate .7667, within the first .56-.84 window.
	TestTrue(TEXT("First delayed latch accepted"), Door->TryUse(Crew[0], Door->GetRevision()));
	TestEqual(TEXT("First delayed latch advances"), Door->GetCompletedLatches(), 1);
	Advance(5.45f); // Arrival .825 is outside second window; estimated .7917 is inside.
	Door->TryUse(Crew[0], Door->GetRevision());
	TestEqual(TEXT("Second latch uses bounded transit compensation"), Door->GetCompletedLatches(), 2);
	Players[0]->SetCompressedPing(250); // A one-second RTT must never rewind more than 250 ms.
	Advance(5.42f); // Arrival .82, bounded estimate .7783: outside third window ending .77.
	Door->TryUse(Crew[0], Door->GetRevision());
	TestEqual(TEXT("High ping cannot enlarge rewind beyond cap"), Door->GetCompletedLatches(), 2);
	Door->CancelForPlayer(Crew[0]);
	Players[0]->SetCompressedPing(0);
	Move(Crew[1], FVector(0,80,0));
	Door->TryUse(Crew[1], Door->GetRevision());
	Advance(2.1f);
	Move(Crew[1], FVector(0,0,0));
	Door->SecureForArrest(Players[0]);
	State->SetMatchPhase(EHeistMatchPhase::End);
	Move(Crew[1], FVector(0,200,0));
	Advance(.2f);
	TestTrue(TEXT("Match End cancels deferred door closure"), Door->IsOpen());
	return true;
}

namespace
{
bool RunArrestEvidenceRecovery(FAutomationTestBase& Test, const bool bTeammateRecovery, const bool bCheckRejectedTransactions)
{
	// Exercise the authoritative transaction and pickup RPC handlers in an isolated
	// world. Forgery and natural Guard discovery are covered by separate scenarios.
	const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	if (!Test.TestNotNull(TEXT("Evidence transaction world exists"), World)) return false;
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	World->SetGameInstance(GameInstance);
	Context.OwningGameInstance = GameInstance;
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	FURL URL;
	URL.AddOption(TEXT("game=/Script/Project_MuseumHeist.HeistGameMode"));
	if (!Test.TestTrue(TEXT("World creates the production GameMode"), World->SetGameMode(URL))) return false;
	World->InitializeActorsForPlay(URL);
	AHeistGameMode* Mode = World->GetAuthGameMode<AHeistGameMode>();
	AHeistGameState* State = World->GetGameState<AHeistGameState>();
	if (!Test.TestTrue(TEXT("Authority GameMode and GameState exist"), IsValid(Mode) && IsValid(State))) return false;
	State->SetMatchPhase(EHeistMatchPhase::InGame);
	const FName TemplateId(TEXT("Template_M01_Portrait_01"));
	// Objective carrier changes reapply the GameState selection. Initialize it as
	// a real match does, so taking the Required Target cannot clear its grid template.
	if (!Test.TestTrue(TEXT("Match Surface template selection initializes"),
		State->InitializeSurfaceTemplateSelection(TEXT("M01"), TemplateId, 40, 1, 39, 1))) return false;
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AHeistPlayerCharacter* Crew[2] = {};
	AHeistPlayerController* Controllers[2] = {};
	AHeistPlayerState* Players[2] = {};
	for (int32 Index = 0; Index < 2; ++Index)
	{
		Crew[Index] = World->SpawnActor<AHeistPlayerCharacter>(FVector(800 + Index * 2400, 0, 100), FRotator::ZeroRotator, Spawn);
		Controllers[Index] = World->SpawnActor<AHeistPlayerController>();
		if (!Test.TestTrue(TEXT("Evidence crew and controller spawn"), IsValid(Crew[Index]) && IsValid(Controllers[Index]))) return false;
		if (!Controllers[Index]->GetPlayerState<AHeistPlayerState>()) Controllers[Index]->InitPlayerState();
		Players[Index] = Controllers[Index]->GetPlayerState<AHeistPlayerState>();
		if (!Test.TestNotNull(TEXT("Controller owns a production PlayerState"), Players[Index])) return false;
		Players[Index]->HeistPlayerId = Index + 1;
		State->AddPlayerState(Players[Index]);
		Controllers[Index]->Possess(Crew[Index]);
		Crew[Index]->DispatchBeginPlay();
	}
	AHeistPaintingDisplayCaseActor* Painting = World->SpawnActor<AHeistPaintingDisplayCaseActor>(Crew[0]->GetActorLocation(), FRotator::ZeroRotator, Spawn);
	AHeistDetentionDoorActor* Door = World->SpawnActor<AHeistDetentionDoorActor>(FVector(0, 0, 100), FRotator::ZeroRotator, Spawn);
	AHeistGuardCharacter* Guard = World->SpawnActorDeferred<AHeistGuardCharacter>(AHeistGuardCharacter::StaticClass(), FTransform(FVector(5000, 0, 100)),
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Test.TestTrue(TEXT("Arrest source, cell and guard exist"), IsValid(Painting) && IsValid(Door) && IsValid(Guard))) return false;
	UClass* DroppedOriginalClass = LoadClass<AHeistDroppedOriginalActor>(nullptr,
		TEXT("/Game/Blueprints/World/Actors/Loot/BP_DroppedOriginal.BP_DroppedOriginal_C"));
	const FClassProperty* DropClassProperty = FindFProperty<FClassProperty>(AHeistPaintingDisplayCaseActor::StaticClass(), TEXT("DroppedOriginalActorClass"));
	if (!Test.TestTrue(TEXT("Production dropped Original shell and assignment exist"), IsValid(DroppedOriginalClass) && DropClassProperty)) return false;
	DropClassProperty->SetObjectPropertyValue_InContainer(Painting, DroppedOriginalClass);
	Guard->AutoPossessAI = EAutoPossessAI::Disabled;
	Guard->FinishSpawning(FTransform(FVector(5000, 0, 100)));
	Door->DispatchBeginPlay();
	Painting->DispatchBeginPlay();
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
		Character->GetCharacterMovement()->StopMovementImmediately();
		World->GetPhysicsScene()->Flush();
		Character->GetCapsuleComponent()->UpdateOverlaps();
	};
	const auto Pickup = [](AHeistPlayerController* Controller, const FName FunctionName, AActor* Target)
	{
		UFunction* Function = Controller->FindFunction(FunctionName);
		if (!Function) return false;
		FStructOnScope Parameters(Function);
		for (TFieldIterator<FObjectProperty> It(Function); It; ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_Parm) && !It->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				It->SetObjectPropertyValue_InContainer(Parameters.GetStructMemory(), Target);
				Controller->ProcessEvent(Function, Parameters.GetStructMemory());
				return true;
			}
		}
		return false;
	};
	const auto EvidenceCount = [World]()
	{
		int32 Count = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
			if (It->ActorHasTag(TEXT("HeistArrestEvidence"))) ++Count;
		return Count;
	};
	AActor* Detention = World->SpawnActor<AActor>(FVector(0, -80, 100), FRotator::ZeroRotator, Spawn);
	if (!Test.TestNotNull(TEXT("Detention anchor exists"), Detention)) return false;
	// Bare anchor actors have no root; use the same explicit scene root that map anchors provide.
	USceneComponent* AnchorRoot = NewObject<USceneComponent>(Detention);
	Detention->SetRootComponent(AnchorRoot);
	AnchorRoot->RegisterComponent();
	Detention->SetActorLocation(FVector(0, -80, 100));
	Detention->Tags.Add(TEXT("HeistDetentionSpawn"));
	if (!Test.TestTrue(TEXT("Anchor belongs to the cell"), Door->ContainsLocation(Detention->GetActorLocation()))) return false;
	if (!Test.TestTrue(TEXT("Contract initializes for two crew"), State->InitializeContractSnapshot(TEXT("Contract_ArrestEvidence"), TEXT("M01"),
		3600.0f, 9412, 2, Painting->GetTargetArtifactId(), FText::FromString(TEXT("Evidence Original")), Painting->GetDisplayCaseId(), 12000))) return false;
	constexpr int32 PreviouslySecuredValue = 137;
	State->SetContractProgress(0, PreviouslySecuredValue, false);
	if (!Test.TestTrue(TEXT("Painting uses a release drawing template"), Painting->SetAssignedSurfaceTemplate(TEXT("M01"), TemplateId, 1))) return false;
	const FEnumProperty* CaseState = FindFProperty<FEnumProperty>(AHeistPaintingDisplayCaseActor::StaticClass(), TEXT("DisplayCaseState"));
	if (!Test.TestNotNull(TEXT("Fixture can start after approved Replica swap"), CaseState)) return false;
	CaseState->GetUnderlyingProperty()->SetIntPropertyValue(CaseState->ContainerPtrToValuePtr<void>(Painting), static_cast<int64>(EHeistDisplayCaseState::OriginalAvailable));
	if (!Test.TestTrue(TEXT("Production Original take succeeds"), Painting->TryTakeOriginal(Players[0]))) return false;
	if (!Test.TestEqual(TEXT("Taking Required Target preserves the selected grid template"), Painting->GetOriginalVisualTemplateId(), TemplateId)) return false;
	FHeistInventoryItem Original;
	if (!Test.TestTrue(TEXT("Original is in the carrier grid"), Crew[0]->GetInventoryComponent()->TryGetFirstOriginalArtifact(Original))) return false;
	FHeistLootDataRow LooseDefinition;
	if (!Test.TestTrue(TEXT("Loose Loot definition exists"), Mode->TryGetLootDefinition(TEXT("Loot_AncientSword"), LooseDefinition))) return false;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FHeistLootDropRequest Request;
		Request.DroppedBy = Crew[0];
		Request.ItemId = LooseDefinition.ItemId;
		Request.SourceInstanceId = Index + 1;
		Request.DropOrigin = Crew[0]->GetActorLocation();
		AHeistLootActor* Loot = nullptr;
		if (!Test.TestTrue(TEXT("Loose Loot fixture spawns through GameMode"), Mode->TrySpawnDroppedLoot(Request, Loot))) return false;
		Move(Crew[0], Loot->GetActorLocation());
		if (!Test.TestTrue(TEXT("Loot has a real interaction overlap"), Crew[0]->GetInteractionComponent()->IsActorOverlappingInteractionArea(Loot))) return false;
		if (!Test.TestTrue(TEXT("Owned Loot pickup handler exists"), Pickup(Controllers[0], TEXT("Server_RequestLootPickup"), Loot))) return false;
		if (!Test.TestFalse(TEXT("Initial Loot pickup commits availability"), Loot->IsLootAvailable())) return false;
	}
	FHeistArrestConfiscationPayload Before;
	const TCHAR* InventoryReason = nullptr;
	if (!Test.TestTrue(TEXT("Mixed Original and Loose snapshot is valid"), Crew[0]->GetInventoryComponent()->TryBuildArrestConfiscationPayload(Before, InventoryReason))) return false;
	Test.TestEqual(TEXT("Snapshot has one Original"), Before.GetOriginalItemCount(), 1);
	Test.TestEqual(TEXT("Snapshot stages one Original and two Loose actors"), Before.GetWorldActorCount(), 3);
	const int32 ExpectedCarriedValue = Original.ContractValue + 2 * LooseDefinition.ScoreValue;
	Test.TestEqual(TEXT("Contract counts the mixed carried cargo"), State->GetContractSnapshot().CarriedValue, ExpectedCarriedValue);
	Test.TestEqual(TEXT("Loose score excludes Original value"), Players[0]->GetTotalLootScore(), 2 * LooseDefinition.ScoreValue);
	const FTransform BeforeTransform = Crew[0]->GetActorTransform();
	const float BeforeWeight = Players[0]->GetTotalLootWeight();
	FName RejectReason;
	if (bCheckRejectedTransactions)
	{
		Test.TestFalse(TEXT("Missing Evidence rejects the whole transaction"), Mode->TryCompletePlayerArrest(Crew[0], Guard, RejectReason));
		Test.TestEqual(TEXT("Rejection identifies missing Evidence anchor"), RejectReason, FName(TEXT("MissingEvidenceTableAnchor")));
		FHeistArrestConfiscationPayload After;
		Test.TestTrue(TEXT("Missing anchor preserves inventory snapshot"), Crew[0]->GetInventoryComponent()->TryBuildArrestConfiscationPayload(After, InventoryReason) && After.Matches(Before));
		Test.TestTrue(TEXT("Missing anchor preserves position"), Crew[0]->GetActorTransform().Equals(BeforeTransform));
		Test.TestFalse(TEXT("Missing anchor never arrests"), Players[0]->IsArrested());
		Test.TestEqual(TEXT("Missing anchor creates no Evidence"), EvidenceCount(), 0);
	}
	for (int32 Index = 0; Index < 3; ++Index)
	{
		AActor* Slot = World->SpawnActor<AActor>();
		USceneComponent* Root = NewObject<USceneComponent>(Slot);
		Slot->SetRootComponent(Root);
		Root->RegisterComponent();
		Slot->SetActorLocation(FVector(2000 + 200 * Index, 0, 100));
		Slot->Tags.Add(TEXT("HeistEvidenceSlot"));
	}
	if (bCheckRejectedTransactions)
	{
		const FObjectProperty* BalanceProperty = FindFProperty<FObjectProperty>(AHeistGameMode::StaticClass(), TEXT("GameBalanceDataAsset"));
		if (!Test.TestNotNull(TEXT("Spawn failure fixture can override transient balance"), BalanceProperty)) return false;
		UObject* PreviousBalance = BalanceProperty->GetObjectPropertyValue_InContainer(Mode);
		UHeistGameBalanceDataAsset* Balance = DuplicateObject<UHeistGameBalanceDataAsset>(PreviousBalance ? CastChecked<UHeistGameBalanceDataAsset>(PreviousBalance) : GetDefault<UHeistGameBalanceDataAsset>(), Mode);
		Balance->WorldLootActorClass.Reset();
		BalanceProperty->SetObjectPropertyValue_InContainer(Mode, Balance);
		const bool bAccepted = Mode->TryCompletePlayerArrest(Crew[0], Guard, RejectReason);
		BalanceProperty->SetObjectPropertyValue_InContainer(Mode, PreviousBalance);
		Test.TestFalse(TEXT("Loose spawn failure rejects staged Original and whole transaction"), bAccepted);
		Test.TestEqual(TEXT("Failure identifies Loose Evidence spawning"), RejectReason, FName(TEXT("LootEvidenceSpawnFailed")));
		FHeistArrestConfiscationPayload After;
		Test.TestTrue(TEXT("Spawn failure preserves mixed inventory"), Crew[0]->GetInventoryComponent()->TryBuildArrestConfiscationPayload(After, InventoryReason) && After.Matches(Before));
		Test.TestTrue(TEXT("Spawn failure preserves position"), Crew[0]->GetActorTransform().Equals(BeforeTransform));
		Test.TestTrue(TEXT("Spawn failure preserves weight"), FMath::IsNearlyEqual(Players[0]->GetTotalLootWeight(), BeforeWeight));
		Test.TestTrue(TEXT("Spawn failure preserves Original carrier"), Painting->GetOriginalCarrier() == Players[0]);
		Test.TestFalse(TEXT("Spawn failure never arrests"), Players[0]->IsArrested());
		Test.TestEqual(TEXT("Staged Original is destroyed on rollback"), EvidenceCount(), 0);
		Test.TestEqual(TEXT("Rollback preserves carried contract value"), State->GetContractSnapshot().CarriedValue, ExpectedCarriedValue);
		Test.TestEqual(TEXT("Rollback preserves secured value"), State->GetContractSnapshot().SecuredValue, PreviouslySecuredValue);
	}
	// Prime this world's TimerManager before the arrest creates its restraint timer.
	Advance(.01f);
	if (!Test.TestTrue(TEXT("Mixed-cargo arrest commits"), Mode->TryCompletePlayerArrest(Crew[0], Guard, RejectReason))) return false;
	Test.TestTrue(TEXT("Arrest is committed after confiscation"), Players[0]->IsArrested());
	Test.TestTrue(TEXT("Player moves to authored detention anchor"), Crew[0]->GetActorLocation().Equals(Detention->GetActorLocation()));
	Test.TestTrue(TEXT("Closed cell is associated with the prisoner"), Players[0]->GetDetentionDoor() == Door && !Door->IsOpen());
	Test.TestEqual(TEXT("Only one batch of Evidence exists"), EvidenceCount(), 3);
	Test.TestEqual(TEXT("Arrest clears Loose score"), Players[0]->GetTotalLootScore(), 0);
	Test.TestTrue(TEXT("Arrest clears carried weight"), FMath::IsNearlyZero(Players[0]->GetTotalLootWeight()));
	Test.TestEqual(TEXT("Arrest clears Original inventory"), Crew[0]->GetInventoryComponent()->GetOriginalArtifactCount(), 0);
	Test.TestNull(TEXT("Arrest releases the source carrier"), Painting->GetOriginalCarrier());
	Test.TestEqual(TEXT("Arrest removes carried contract value"), State->GetContractSnapshot().CarriedValue, 0);
	Test.TestEqual(TEXT("Arrest never changes previously secured value"), State->GetContractSnapshot().SecuredValue, PreviouslySecuredValue);
	Test.TestFalse(TEXT("Confiscation is not Required Target extraction"), State->GetContractSnapshot().bRequiredTargetSecured);
	Test.TestFalse(TEXT("Repeated arrest request cannot spawn duplicates"), Mode->TryCompletePlayerArrest(Crew[0], Guard, RejectReason));
	Test.TestEqual(TEXT("Repeated arrest leaves exactly three Evidence actors"), EvidenceCount(), 3);
	AHeistDroppedOriginalActor* EvidenceOriginal = nullptr;
	TArray<AHeistLootActor*> EvidenceLoose;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (!It->ActorHasTag(TEXT("HeistArrestEvidence"))) continue;
		if (AHeistDroppedOriginalActor* Drop = Cast<AHeistDroppedOriginalActor>(*It)) EvidenceOriginal = Drop;
		else if (AHeistLootActor* Loot = Cast<AHeistLootActor>(*It)) EvidenceLoose.Add(Loot);
	}
	if (!Test.TestTrue(TEXT("Evidence contains one Original and two Loose pickups"), IsValid(EvidenceOriginal) && EvidenceLoose.Num() == 2)) return false;
	Test.TestEqual(TEXT("Evidence preserves Artifact identity"), EvidenceOriginal->GetArtifactId(), Original.ItemId);
	Test.TestEqual(TEXT("Evidence preserves Original contract value"), EvidenceOriginal->GetArtifactValue(), Original.ContractValue);
	Test.TestTrue(TEXT("Evidence preserves Original weight"), FMath::IsNearlyEqual(EvidenceOriginal->GetWeight(), Original.Weight));
	Test.TestTrue(TEXT("Evidence preserves Required Target and source case"), EvidenceOriginal->IsRequiredTarget() && EvidenceOriginal->GetSourceDisplayCase() == Painting);
	const int32 Recoverer = bTeammateRecovery ? 1 : 0;
	const auto RecoverOriginal = [&]()
	{
		Move(Crew[Recoverer], EvidenceOriginal->GetActorLocation());
		if (!Test.TestTrue(TEXT("Original Evidence has a real interaction overlap"), Crew[Recoverer]->GetInteractionComponent()->IsActorOverlappingInteractionArea(EvidenceOriginal))) return false;
		Pickup(Controllers[Recoverer], TEXT("Server_RequestDroppedOriginalPickup"), EvidenceOriginal);
		if (!Test.TestEqual(TEXT("Recovery grants exactly one Original"), Crew[Recoverer]->GetInventoryComponent()->GetOriginalArtifactCount(), 1)) return false;
		Pickup(Controllers[Recoverer], TEXT("Server_RequestDroppedOriginalPickup"), EvidenceOriginal);
		Pickup(Controllers[1 - Recoverer], TEXT("Server_RequestDroppedOriginalPickup"), EvidenceOriginal);
		Test.TestEqual(TEXT("Replayed and competing requests never duplicate the Original"), Crew[0]->GetInventoryComponent()->GetOriginalArtifactCount() + Crew[1]->GetInventoryComponent()->GetOriginalArtifactCount(), 1);
		Test.TestTrue(TEXT("Recovery assigns the source carrier"), Painting->GetOriginalCarrier() == Players[Recoverer]);
		return true;
	};
	if (bTeammateRecovery)
	{
		if (!RecoverOriginal()) return false;
		Test.TestTrue(TEXT("Teammate can recover Original while owner remains restrained"), Players[0]->IsArrested() && !Door->IsOpen());
		Test.TestEqual(TEXT("Pre-rescue recovery counts Original as carried"), State->GetContractSnapshot().CarriedValue, Original.ContractValue);
		Move(Crew[1], FVector(0, 80, 100));
		if (!Test.TestTrue(TEXT("Teammate begins outside rescue hold"), Door->TryUse(Crew[1], Door->GetRevision()))) return false;
		Advance(2.1f);
	}
	else
	{
		Advance(5.1f);
		Test.TestFalse(TEXT("Timer releases restraint for self-escape"), Players[0]->IsArrested());
		Move(Crew[0], FVector(0, -80, 100));
		if (!Test.TestTrue(TEXT("Recovered inmate starts inside latch sequence"), Door->TryUse(Crew[0], Door->GetRevision()))) return false;
		for (int32 Latch = 0; Latch < 3; ++Latch)
		{
			Advance(4.7f);
			if (!Test.TestTrue(TEXT("Timed inmate press is accepted"), Door->TryUse(Crew[0], Door->GetRevision()))) return false;
		}
	}
	if (!Test.TestTrue(TEXT("Self-escape or teammate rescue opens cell"), Door->IsOpen() && !Players[0]->IsArrested() && Players[0]->GetDetentionDoor() == nullptr)) return false;
	Test.TestEqual(TEXT("Opening never returns Loose Loot automatically"), Players[0]->GetTotalLootScore(), 0);
	Test.TestEqual(TEXT("Opening never returns Original automatically"), Crew[0]->GetInventoryComponent()->GetOriginalArtifactCount(), 0);
	Test.TestTrue(TEXT("Opening preserves unclaimed Loose Evidence"), EvidenceLoose[0]->IsLootAvailable() && EvidenceLoose[1]->IsLootAvailable());
	if (!bTeammateRecovery && !RecoverOriginal()) return false;
	for (AHeistLootActor* Loot : EvidenceLoose)
	{
		Test.TestEqual(TEXT("Loose Evidence preserves Item identity"), Loot->GetLootRowId(), LooseDefinition.ItemId);
		Test.TestEqual(TEXT("Loose Evidence preserves contract value"), Loot->GetScoreValue(), LooseDefinition.ScoreValue);
		Move(Crew[Recoverer], Loot->GetActorLocation());
		if (!Test.TestTrue(TEXT("Loose Evidence has a real interaction overlap"), Crew[Recoverer]->GetInteractionComponent()->IsActorOverlappingInteractionArea(Loot))) return false;
		Pickup(Controllers[Recoverer], TEXT("Server_RequestLootPickup"), Loot);
		Pickup(Controllers[Recoverer], TEXT("Server_RequestLootPickup"), Loot);
		Pickup(Controllers[1 - Recoverer], TEXT("Server_RequestLootPickup"), Loot);
		Test.TestFalse(TEXT("Recovered Loose Evidence is unavailable"), Loot->IsLootAvailable());
	}
	Test.TestEqual(TEXT("Recovered Loose quantity/value is not duplicated"), Players[Recoverer]->GetTotalLootScore(), 2 * LooseDefinition.ScoreValue);
	Test.TestTrue(TEXT("Recovered cargo restores its exact total weight"), FMath::IsNearlyEqual(Players[Recoverer]->GetTotalLootWeight(), BeforeWeight));
	Test.TestEqual(TEXT("Recovery restores exact carried contract value"), State->GetContractSnapshot().CarriedValue, ExpectedCarriedValue);
	Test.TestEqual(TEXT("Recovery preserves secured value"), State->GetContractSnapshot().SecuredValue, PreviouslySecuredValue);
	Test.TestFalse(TEXT("Recovery does not secure Required Target"), State->GetContractSnapshot().bRequiredTargetSecured);
	FHeistArrestConfiscationPayload Recovered;
	Test.TestTrue(TEXT("Recovered inventory preserves confiscated counts and value"),
		Crew[Recoverer]->GetInventoryComponent()->TryBuildArrestConfiscationPayload(Recovered, InventoryReason) && Recovered.GetOriginalItemCount() == 1 &&
		Recovered.GetWorldActorCount() == Before.GetWorldActorCount() && Recovered.LooseLootValue == Before.LooseLootValue);
	int32 RecoveredLooseQuantity = 0;
	for (const FHeistInventoryItem& Item : Recovered.ConfiscatedItems)
	{
		if (Item.IsOriginalArtifact())
		{
			Test.TestTrue(TEXT("Recovered Original preserves Artifact, Required Target and source identity"),
				Item.ItemId == Original.ItemId && Item.bRequiredTarget == Original.bRequiredTarget && Item.SourceDisplayCase == Original.SourceDisplayCase &&
				Item.ContractValue == Original.ContractValue && FMath::IsNearlyEqual(Item.Weight, Original.Weight));
		}
		else
		{
			Test.TestEqual(TEXT("Recovered Loose inventory preserves Item identity"), Item.ItemId, LooseDefinition.ItemId);
			RecoveredLooseQuantity += Item.Quantity;
		}
	}
	Test.TestEqual(TEXT("Recovered Loose inventory contains exactly two units"), RecoveredLooseQuantity, 2);
	Test.TestEqual(TEXT("Other crew receives no duplicate Loot value"), Players[1 - Recoverer]->GetTotalLootScore(), 0);
	Test.TestFalse(TEXT("Recovery does not extract either player"), Players[0]->IsEscaped() || Players[1]->IsEscaped());
	Test.AddInfo(FString::Printf(TEXT("Arrest Evidence flow: Recovery=%s RejectedTransactions=%s Original=1 Loose=2 Carried=%d Secured=%d DuplicateRequests=Checked NaturalGuardDiscovery=NotTested NetworkReplication=NotTested"),
		bTeammateRecovery ? TEXT("TeammateBeforeAndAfterRescue") : TEXT("SelfAfterLatchEscape"), bCheckRejectedTransactions ? TEXT("Checked") : TEXT("NotRequested"),
		State->GetContractSnapshot().CarriedValue, State->GetContractSnapshot().SecuredValue));
	return !Test.HasAnyErrors();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistArrestEvidenceSelfRecoveryTest, "ProjectMuseumHeist.W7.ArrestEvidence.SelfRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHeistArrestEvidenceSelfRecoveryTest::RunTest(const FString& Parameters)
{
	return RunArrestEvidenceRecovery(*this, false, false);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistArrestEvidenceTeammateRecoveryTest, "ProjectMuseumHeist.W7.ArrestEvidence.TeammateRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHeistArrestEvidenceTeammateRecoveryTest::RunTest(const FString& Parameters)
{
	return RunArrestEvidenceRecovery(*this, true, false);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistArrestEvidenceRollbackTest, "ProjectMuseumHeist.W7.ArrestEvidence.Rollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHeistArrestEvidenceRollbackTest::RunTest(const FString& Parameters)
{
	return RunArrestEvidenceRecovery(*this, false, true);
}

#endif
