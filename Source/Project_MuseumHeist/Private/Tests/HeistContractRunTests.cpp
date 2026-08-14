#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "AI/HeistGuardAIController.h"
#include "AI/HeistGuardCharacter.h"
#include "AI/HeistGuardStateComponent.h"
#include "Character/Components/HeistActionComponent.h"
#include "Character/Components/HeistForgeryComponent.h"
#include "Character/Components/HeistInteractionComponent.h"
#include "Character/Components/HeistInventoryComponent.h"
#include "Character/Components/HeistObjectAssemblyComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Core/HeistGameInstance.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistGameState.h"
#include "Core/HeistHUD.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistPlayerState.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Inventory/HeistItemDataTypes.h"
#include "Misc/AutomationTest.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/Widgets/HeistHUDWidget.h"
#include "UI/Widgets/HeistForgeryWidget.h"
#include "UI/Widgets/HeistNameplateWidget.h"
#include "UI/Widgets/HeistObjectAssemblyWidget.h"
#include "UI/Widgets/HeistResultWidget.h"
#include "UI/ViewModels/HeistHUDViewModel.h"
#include "UObject/UObjectIterator.h"
#include "World/Actors/Escape/HeistVentActor.h"
#include "World/Actors/Loot/HeistObjectDisplayCaseActor.h"
#include "World/Actors/Loot/HeistLootActor.h"
#include "World/Actors/Loot/HeistPaintingDisplayCaseActor.h"

namespace HeistContractRunTest
{
struct FHeistContractRunAutomationState
{
	bool bAborted = false;
	bool bCapturedPlaySettings = false;
	EPlayNetMode OriginalNetMode = EPlayNetMode::PIE_Standalone;
	bool bOriginalRunUnderOneProcess = true;
	int32 OriginalClientCount = 1;
	int32 PlayerCount = 1;
	FHeistContractSnapshot FirstRunContract;
	TArray<FName> SelectedObjectCaseIds;
	FName SelectedLootActorName = NAME_None;
	FName SelectedLootRowId = NAME_None;
	int32 SelectedLootValue = 0;
};

TArray<UWorld*> GetContractRunPIEWorlds()
{
	TArray<UWorld*> Worlds;
	if (!IsValid(GEngine))
	{
		return Worlds;
	}
	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		UWorld* World = WorldContext.World();
		if (WorldContext.WorldType == EWorldType::PIE && IsValid(World))
		{
			Worlds.Add(World);
		}
	}
	Worlds.Sort([](const UWorld& Left, const UWorld& Right)
	{
		if (Left.GetNetMode() != Right.GetNetMode())
		{
			return static_cast<uint8>(Left.GetNetMode()) < static_cast<uint8>(Right.GetNetMode());
		}
		return Left.GetName() < Right.GetName();
	});
	return Worlds;
}

UWorld* GetContractRunServerWorld()
{
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		if (IsValid(World) && (World->GetNetMode() == NM_ListenServer || World->GetNetMode() == NM_Standalone))
		{
			return World;
		}
	}
	return nullptr;
}

AHeistPlayerController* GetContractRunLocalHeistPlayerController(UWorld* World)
{
	if (!IsValid(World))
	{
		return nullptr;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		AHeistPlayerController* PlayerController = Cast<AHeistPlayerController>(It->Get());
		if (IsValid(PlayerController) && PlayerController->IsLocalController())
		{
			return PlayerController;
		}
	}
	return nullptr;
}

AHeistPlayerController* GetServerPlayerControllerById(const int32 PlayerId)
{
	UWorld* ServerWorld = GetContractRunServerWorld();
	if (!IsValid(ServerWorld))
	{
		return nullptr;
	}
	for (FConstPlayerControllerIterator It = ServerWorld->GetPlayerControllerIterator(); It; ++It)
	{
		AHeistPlayerController* PlayerController = Cast<AHeistPlayerController>(It->Get());
		const AHeistPlayerState* PlayerState = IsValid(PlayerController) ? PlayerController->GetPlayerState<AHeistPlayerState>() : nullptr;
		if (IsValid(PlayerState) && PlayerState->HeistPlayerId == PlayerId)
		{
			return PlayerController;
		}
	}
	return nullptr;
}

AHeistPlayerController* GetOwningPlayerControllerById(const int32 PlayerId)
{
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		AHeistPlayerController* PlayerController = GetContractRunLocalHeistPlayerController(World);
		const AHeistPlayerState* PlayerState = IsValid(PlayerController) ? PlayerController->GetPlayerState<AHeistPlayerState>() : nullptr;
		if (IsValid(PlayerState) && PlayerState->HeistPlayerId == PlayerId)
		{
			return PlayerController;
		}
	}
	return nullptr;
}

AHeistPlayerCharacter* GetServerCharacterById(const int32 PlayerId)
{
	AHeistPlayerController* PlayerController = GetServerPlayerControllerById(PlayerId);
	return IsValid(PlayerController) ? PlayerController->GetPawn<AHeistPlayerCharacter>() : nullptr;
}

AHeistPlayerCharacter* FindHeistCharacterById(UWorld* World, const int32 PlayerId)
{
	if (!IsValid(World))
	{
		return nullptr;
	}
	for (TActorIterator<AHeistPlayerCharacter> It(World); It; ++It)
	{
		AHeistPlayerCharacter* Character = *It;
		const AHeistPlayerState* PlayerState = IsValid(Character) ? Character->GetPlayerState<AHeistPlayerState>() : nullptr;
		if (IsValid(PlayerState) && PlayerState->HeistPlayerId == PlayerId)
		{
			return Character;
		}
	}
	return nullptr;
}

AHeistPaintingDisplayCaseActor* FindPaintingCase(UWorld* World, const FName CaseId)
{
	if (!IsValid(World) || CaseId.IsNone())
	{
		return nullptr;
	}
	for (TActorIterator<AHeistPaintingDisplayCaseActor> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetDisplayCaseId() == CaseId)
		{
			return *It;
		}
	}
	return nullptr;
}

AHeistObjectDisplayCaseActor* FindObjectCase(UWorld* World, const FName CaseId)
{
	if (!IsValid(World) || CaseId.IsNone())
	{
		return nullptr;
	}
	for (TActorIterator<AHeistObjectDisplayCaseActor> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetObjectCaseId() == CaseId)
		{
			return *It;
		}
	}
	return nullptr;
}

AHeistVentActor* FindSharedExit(UWorld* World)
{
	if (!IsValid(World))
	{
		return nullptr;
	}
	for (TActorIterator<AHeistVentActor> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}
	return nullptr;
}

AHeistLootActor* FindLootActor(UWorld* World, const FName ActorName)
{
	if (!IsValid(World) || ActorName.IsNone())
	{
		return nullptr;
	}
	for (TActorIterator<AHeistLootActor> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetFName() == ActorName)
		{
			return *It;
		}
	}
	return nullptr;
}

bool TeleportServerPlayerIntoInteraction(const int32 PlayerId, AActor* TargetActor)
{
	AHeistPlayerCharacter* Character = GetServerCharacterById(PlayerId);
	if (!IsValid(Character) || !IsValid(TargetActor))
	{
		return false;
	}
	const USphereComponent* InteractionSphere = TargetActor->FindComponentByClass<USphereComponent>();
	const FVector Destination = IsValid(InteractionSphere) ? InteractionSphere->GetComponentLocation() : TargetActor->GetActorLocation();
	Character->SetActorLocation(Destination, false, nullptr, ETeleportType::TeleportPhysics);
	Character->ForceNetUpdate();
	return true;
}

bool IsServerPlayerOverlapping(const int32 PlayerId, const AActor* TargetActor)
{
	const AHeistPlayerCharacter* Character = GetServerCharacterById(PlayerId);
	const UHeistInteractionComponent* InteractionComponent = IsValid(Character) ? Character->GetInteractionComponent() : nullptr;
	return IsValid(InteractionComponent) && IsValid(TargetActor) && InteractionComponent->IsActorOverlappingInteractionArea(TargetActor);
}

template <typename TTargetActor>
bool InvokeSingleActorServerRPC(AHeistPlayerController* PlayerController, const FName FunctionName, TTargetActor* TargetActor)
{
	if (!IsValid(PlayerController) || !IsValid(TargetActor))
	{
		return false;
	}
	UFunction* Function = PlayerController->FindFunction(FunctionName);
	if (!IsValid(Function))
	{
		UE_LOG(LogTemp, Error, TEXT("W6-010 RPC lookup failed: Controller=%s Function=%s"), *GetNameSafe(PlayerController), *FunctionName.ToString());
		return false;
	}
	struct FActorRPCParameters
	{
		TTargetActor* Target = nullptr;
	};
	FActorRPCParameters Parameters;
	if (Function->ParmsSize != sizeof(FActorRPCParameters))
	{
		UE_LOG(LogTemp, Error, TEXT("W6-010 RPC parameter mismatch: Controller=%s Function=%s ReflectedBytes=%d TestBytes=%d"), *GetNameSafe(PlayerController),
			*FunctionName.ToString(), static_cast<int32>(Function->ParmsSize), static_cast<int32>(sizeof(FActorRPCParameters)));
		return false;
	}
	Parameters.Target = TargetActor;
	PlayerController->ProcessEvent(Function, &Parameters);
	return true;
}

bool AreContractRunWorldsReady(const int32 PlayerCount, const EHeistMatchPhase ExpectedPhase, const bool bRequirePawn)
{
	const TArray<UWorld*> Worlds = GetContractRunPIEWorlds();
	if (Worlds.Num() != PlayerCount || !IsValid(GetContractRunServerWorld()))
	{
		return false;
	}
	TSet<int32> LocalPlayerIds;
	for (UWorld* World : Worlds)
	{
		const AHeistGameState* GameState = World->GetGameState<AHeistGameState>();
		const AHeistPlayerController* LocalPlayerController = GetContractRunLocalHeistPlayerController(World);
		const AHeistPlayerState* LocalPlayerState = IsValid(LocalPlayerController) ? LocalPlayerController->GetPlayerState<AHeistPlayerState>() : nullptr;
		if (!IsValid(GameState) || GameState->GetMatchPhase() != ExpectedPhase || GameState->PlayerArray.Num() != PlayerCount || !IsValid(LocalPlayerController) ||
			!IsValid(LocalPlayerState) || LocalPlayerState->HeistPlayerId < 1 || LocalPlayerState->HeistPlayerId > PlayerCount ||
			(bRequirePawn && !IsValid(LocalPlayerController->GetPawn())))
		{
			return false;
		}
		LocalPlayerIds.Add(LocalPlayerState->HeistPlayerId);
	}
	return LocalPlayerIds.Num() == PlayerCount;
}

AHeistPlayerCharacter* GetOwningCharacterById(const int32 PlayerId)
{
	AHeistPlayerController* PlayerController = GetOwningPlayerControllerById(PlayerId);
	return IsValid(PlayerController) ? PlayerController->GetPawn<AHeistPlayerCharacter>() : nullptr;
}

AHeistPlayerState* FindPlayerStateById(UWorld* World, const int32 PlayerId)
{
	const AHeistGameState* GameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(GameState))
	{
		return nullptr;
	}
	for (APlayerState* PlayerStateBase : GameState->PlayerArray)
	{
		AHeistPlayerState* PlayerState = Cast<AHeistPlayerState>(PlayerStateBase);
		if (IsValid(PlayerState) && PlayerState->HeistPlayerId == PlayerId)
		{
			return PlayerState;
		}
	}
	return nullptr;
}

bool IsCrewStatusReplicated(const int32 PlayerId, const EHeistCrewStatus ExpectedStatus)
{
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		const AHeistPlayerState* PlayerState = FindPlayerStateById(World, PlayerId);
		if (!IsValid(PlayerState) || PlayerState->GetCrewStatus() != ExpectedStatus)
		{
			return false;
		}
	}
	return true;
}

bool IsWeek7ReadabilityPresentationReady(const int32 PlayerCount)
{
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		AHeistPlayerController* LocalController = GetContractRunLocalHeistPlayerController(World);
		AHeistHUD* HUD = IsValid(LocalController) ? LocalController->GetHUD<AHeistHUD>() : nullptr;
		const UHeistHUDViewModel* HUDViewModel = IsValid(HUD) ? HUD->GetHUDViewModel() : nullptr;
		const APawn* LocalPawn = IsValid(LocalController) ? LocalController->GetPawn() : nullptr;
		if (!IsValid(LocalController) || !IsValid(HUD) || !IsValid(HUD->GetMainHUDWidget()) || !IsValid(HUDViewModel) ||
			HUDViewModel->GetCrewStatusEntries().Num() != PlayerCount || !IsValid(LocalPawn))
		{
			return false;
		}

		int32 CharacterCount = 0;
		int32 RemoteNameplateCount = 0;
		for (TActorIterator<AHeistPlayerCharacter> It(World); It; ++It)
		{
			AHeistPlayerCharacter* Character = *It;
			AHeistPlayerState* PlayerState = IsValid(Character) ? Character->GetPlayerState<AHeistPlayerState>() : nullptr;
			UWidgetComponent* NameplateComponent = nullptr;
			UHeistNameplateWidget* NameplateWidget = nullptr;
			TInlineComponentArray<UWidgetComponent*> WidgetComponents(Character);
			for (UWidgetComponent* CandidateComponent : WidgetComponents)
			{
				if (UHeistNameplateWidget* CandidateWidget = IsValid(CandidateComponent) ? Cast<UHeistNameplateWidget>(CandidateComponent->GetUserWidgetObject()) : nullptr)
				{
					NameplateComponent = CandidateComponent;
					NameplateWidget = CandidateWidget;
					break;
				}
			}
			if (!IsValid(PlayerState) || !IsValid(NameplateComponent) || !IsValid(NameplateWidget) || NameplateWidget->GetPresentedPlayerState() != PlayerState)
			{
				return false;
			}
			const bool bLocalCharacter = Character == LocalPawn;
			if (NameplateComponent->IsVisible() == bLocalCharacter)
			{
				return false;
			}
			CharacterCount++;
			RemoteNameplateCount += bLocalCharacter ? 0 : 1;
		}
		if (CharacterCount != PlayerCount || RemoteNameplateCount != PlayerCount - 1)
		{
			return false;
		}
	}
	return true;
}

FString DescribeWeek7ReadabilityPresentation(const int32 PlayerCount)
{
	TArray<FString> WorldStates;
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		AHeistPlayerController* LocalController = GetContractRunLocalHeistPlayerController(World);
		AHeistHUD* HUD = IsValid(LocalController) ? LocalController->GetHUD<AHeistHUD>() : nullptr;
		const UHeistHUDViewModel* HUDViewModel = IsValid(HUD) ? HUD->GetHUDViewModel() : nullptr;
		const APawn* LocalPawn = IsValid(LocalController) ? LocalController->GetPawn() : nullptr;
		TArray<FString> CharacterStates;
		for (TActorIterator<AHeistPlayerCharacter> It(World); It; ++It)
		{
			AHeistPlayerCharacter* Character = *It;
			AHeistPlayerState* PlayerState = IsValid(Character) ? Character->GetPlayerState<AHeistPlayerState>() : nullptr;
			TInlineComponentArray<UWidgetComponent*> WidgetComponents(Character);
			UWidgetComponent* NameplateComponent = nullptr;
			UHeistNameplateWidget* NameplateWidget = nullptr;
			for (UWidgetComponent* CandidateComponent : WidgetComponents)
			{
				if (UHeistNameplateWidget* CandidateWidget = IsValid(CandidateComponent) ? Cast<UHeistNameplateWidget>(CandidateComponent->GetUserWidgetObject()) : nullptr)
				{
					NameplateComponent = CandidateComponent;
					NameplateWidget = CandidateWidget;
					break;
				}
			}
			CharacterStates.Add(FString::Printf(TEXT("Character=%s PlayerId=%d Local=%s WidgetComponents=%d Nameplate=%s Visible=%s PresentedPS=%s ActualPS=%s"),
				*GetNameSafe(Character), IsValid(PlayerState) ? PlayerState->HeistPlayerId : INDEX_NONE, Character == LocalPawn ? TEXT("true") : TEXT("false"),
				WidgetComponents.Num(), *GetNameSafe(NameplateWidget), IsValid(NameplateComponent) && NameplateComponent->IsVisible() ? TEXT("true") : TEXT("false"),
				*GetNameSafe(IsValid(NameplateWidget) ? NameplateWidget->GetPresentedPlayerState() : nullptr), *GetNameSafe(PlayerState)));
		}
		WorldStates.Add(FString::Printf(TEXT("World=%s NetMode=%d ExpectedPlayers=%d LocalController=%s HUD=%s MainHUD=%s CrewEntries=%d Characters=[%s]"),
			*GetNameSafe(World), IsValid(World) ? static_cast<int32>(World->GetNetMode()) : INDEX_NONE, PlayerCount, *GetNameSafe(LocalController), *GetNameSafe(HUD),
			*GetNameSafe(IsValid(HUD) ? HUD->GetMainHUDWidget() : nullptr), IsValid(HUDViewModel) ? HUDViewModel->GetCrewStatusEntries().Num() : INDEX_NONE,
			*FString::Join(CharacterStates, TEXT("; "))));
	}
	return FString::Printf(TEXT("W7 readability diagnostic: %s"), *FString::Join(WorldStates, TEXT(" | ")));
}

bool ToggleWeek7Maps(const bool bShouldOpen)
{
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		AHeistPlayerController* PlayerController = GetContractRunLocalHeistPlayerController(World);
		AHeistHUD* HUD = IsValid(PlayerController) ? PlayerController->GetHUD<AHeistHUD>() : nullptr;
		if (!IsValid(PlayerController) || !IsValid(HUD) || HUD->IsFloorPlanMapVisible() == bShouldOpen || !PlayerController->ToggleFloorPlanMap())
		{
			return false;
		}
	}
	return true;
}

bool AreWeek7MapContractsReady(const bool bShouldBeOpen)
{
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		const AHeistPlayerController* PlayerController = GetContractRunLocalHeistPlayerController(World);
		const AHeistHUD* HUD = IsValid(PlayerController) ? PlayerController->GetHUD<AHeistHUD>() : nullptr;
		const EHeistInputMode ExpectedMode = bShouldBeOpen ? EHeistInputMode::Map : EHeistInputMode::Gameplay;
		if (!IsValid(PlayerController) || !IsValid(HUD) || HUD->IsFloorPlanMapVisible() != bShouldBeOpen || PlayerController->GetLocalInputMode() != ExpectedMode ||
			!PlayerController->IsLocalInputModeContractSatisfied() || PlayerController->GetActiveHeistInputMappingContextCount() != 1)
		{
			return false;
		}
	}
	return true;
}

bool RequestWeek7Sprint(const bool bRequested)
{
	for (int32 PlayerId = 1; PlayerId <= GetContractRunPIEWorlds().Num(); ++PlayerId)
	{
		AHeistPlayerController* PlayerController = GetOwningPlayerControllerById(PlayerId);
		if (!IsValid(PlayerController))
		{
			return false;
		}
		PlayerController->RequestSetSprintRequested(bRequested);
	}
	return true;
}

bool IsWeek7SprintReplicated(const int32 PlayerCount, const bool bExpectedSprint)
{
	const float ExpectedSpeed = bExpectedSprint ? 600.0f : 300.0f;
	for (int32 PlayerId = 1; PlayerId <= PlayerCount; ++PlayerId)
	{
		const AHeistPlayerCharacter* ServerCharacter = GetServerCharacterById(PlayerId);
		if (!IsValid(ServerCharacter) || ServerCharacter->IsSprinting() != bExpectedSprint ||
			!FMath::IsNearlyEqual(ServerCharacter->GetCharacterMovement()->MaxWalkSpeed, ExpectedSpeed))
		{
			return false;
		}
		for (UWorld* World : GetContractRunPIEWorlds())
		{
			const AHeistPlayerCharacter* Character = FindHeistCharacterById(World, PlayerId);
			if (!IsValid(Character) || Character->IsSprinting() != bExpectedSprint ||
				!FMath::IsNearlyEqual(Character->GetCharacterMovement()->MaxWalkSpeed, ExpectedSpeed))
			{
				return false;
			}
		}
	}
	return true;
}

bool BeginWeek7GuardStun(const int32 TargetPlayerId)
{
	UWorld* ServerWorld = GetContractRunServerWorld();
	AHeistPlayerCharacter* TargetCharacter = GetServerCharacterById(TargetPlayerId);
	if (!IsValid(ServerWorld) || !IsValid(TargetCharacter))
	{
		return false;
	}
	for (TActorIterator<AHeistGuardCharacter> It(ServerWorld); It; ++It)
	{
		AHeistGuardCharacter* Guard = *It;
		AHeistGuardAIController* GuardController = IsValid(Guard) ? Cast<AHeistGuardAIController>(Guard->GetController()) : nullptr;
		UHeistGuardStateComponent* GuardState = IsValid(Guard) ? Guard->GetGuardStateComponent() : nullptr;
		if (!IsValid(GuardController) || !IsValid(GuardState))
		{
			continue;
		}
		GuardController->SetAutomaticSightEnabled(false);
		Guard->SetActorLocation(TargetCharacter->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
		Guard->ForceNetUpdate();
		if (!GuardState->EnterChasePlayer(TargetCharacter))
		{
			return false;
		}
		GuardController->TryArrestChaseTarget();
		const AHeistPlayerState* TargetPlayerState = TargetCharacter->GetPlayerState<AHeistPlayerState>();
		return IsValid(TargetPlayerState) && TargetPlayerState->GetCrewStatus() == EHeistCrewStatus::Stunned;
	}
	return false;
}

bool IsWeek7StunPresentationReady(const int32 TargetPlayerId)
{
	if (!IsCrewStatusReplicated(TargetPlayerId, EHeistCrewStatus::Stunned))
	{
		return false;
	}
	AHeistPlayerController* OwningController = GetOwningPlayerControllerById(TargetPlayerId);
	return IsValid(OwningController) && OwningController->IsMoveInputIgnored() && OwningController->IsLookInputIgnored();
}

bool IsWeek7ArrestReplicated(const int32 TargetPlayerId)
{
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		const AHeistPlayerState* PlayerState = FindPlayerStateById(World, TargetPlayerId);
		if (!IsValid(PlayerState) || !PlayerState->IsArrested() || PlayerState->GetCrewStatus() != EHeistCrewStatus::Arrested)
		{
			return false;
		}
	}
	return true;
}

bool MoveWeek7RescuerIntoRange(const int32 RescuerPlayerId, const int32 TargetPlayerId)
{
	AHeistPlayerCharacter* Rescuer = GetServerCharacterById(RescuerPlayerId);
	AHeistPlayerCharacter* Target = GetServerCharacterById(TargetPlayerId);
	if (!IsValid(Rescuer) || !IsValid(Target) || !Target->IsRescueInteractionAvailable())
	{
		return false;
	}
	Rescuer->SetActorLocation(Target->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
	Rescuer->ForceNetUpdate();
	return true;
}

bool RequestWeek7Rescue(const int32 RescuerPlayerId, const int32 TargetPlayerId)
{
	AHeistPlayerController* RescuerController = GetOwningPlayerControllerById(RescuerPlayerId);
	AHeistPlayerCharacter* LocalTarget = IsValid(RescuerController) ? FindHeistCharacterById(RescuerController->GetWorld(), TargetPlayerId) : nullptr;
	return InvokeSingleActorServerRPC(RescuerController, FName(TEXT("Server_RequestRescuePlayer")), LocalTarget);
}

bool IsWeek7RescueComplete(const int32 TargetPlayerId)
{
	if (!IsCrewStatusReplicated(TargetPlayerId, EHeistCrewStatus::Active))
	{
		return false;
	}
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		const AHeistPlayerState* PlayerState = FindPlayerStateById(World, TargetPlayerId);
		if (!IsValid(PlayerState) || PlayerState->IsArrested())
		{
			return false;
		}
	}
	AHeistPlayerController* OwningController = GetOwningPlayerControllerById(TargetPlayerId);
	return IsValid(OwningController) && !OwningController->IsMoveInputIgnored() && !OwningController->IsLookInputIgnored();
}

bool CaptureAndValidateGameplayPreflight(FAutomationTestBase* Test, const TSharedRef<FHeistContractRunAutomationState>& State, const int32 RunIndex)
{
	UWorld* ServerWorld = GetContractRunServerWorld();
	const AHeistGameState* ServerGameState = IsValid(ServerWorld) ? ServerWorld->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(ServerGameState))
	{
		return false;
	}
	const FHeistContractSnapshot ServerContract = ServerGameState->GetContractSnapshot();
	if (!ServerContract.IsInitialized() || ServerContract.MapId != FName(TEXT("M01")) || ServerContract.Outcome != EHeistContractOutcome::None ||
		ServerContract.CarriedValue != 0 || ServerContract.SecuredValue != 0 || ServerContract.bRequiredTargetSecured)
	{
		return false;
	}

	const int32 ExpectedOptionalCaseCount = State->PlayerCount + 1;
	TArray<FName> ObjectCaseIds;
	int32 RequiredTargetCaseCount = 0;
	for (TActorIterator<AHeistPaintingDisplayCaseActor> It(ServerWorld); It; ++It)
	{
		if (IsValid(*It))
		{
			RequiredTargetCaseCount += It->GetDisplayCaseId() == ServerContract.RequiredTargetCaseId ? 1 : 0;
		}
	}
	for (TActorIterator<AHeistObjectDisplayCaseActor> It(ServerWorld); It; ++It)
	{
		if (IsValid(*It) && It->IsContractExhibitActive())
		{
			ObjectCaseIds.Add(It->GetObjectCaseId());
			RequiredTargetCaseCount += It->GetObjectCaseId() == ServerContract.RequiredTargetCaseId ? 1 : 0;
		}
	}
	ObjectCaseIds.Sort([](const FName Left, const FName Right) { return Left.LexicalLess(Right); });
	const int32 RequiredAssemblyCaseCount = State->PlayerCount == 1 ? 1 : 3;
	if (RequiredTargetCaseCount != 1 || ObjectCaseIds.Num() != ExpectedOptionalCaseCount || ObjectCaseIds.Num() < RequiredAssemblyCaseCount)
	{
		return false;
	}

	AHeistGameMode* GameMode = ServerWorld->GetAuthGameMode<AHeistGameMode>();
	AHeistLootActor* SelectedLootActor = nullptr;
	FHeistItemDataRow SelectedItemDefinition;
	FHeistLootDataRow SelectedLootDefinition;
	int32 SelectedGridArea = MAX_int32;
	for (TActorIterator<AHeistLootActor> It(ServerWorld); It; ++It)
	{
		AHeistLootActor* Candidate = *It;
		FHeistItemDataRow CandidateItemDefinition;
		FHeistLootDataRow CandidateLootDefinition;
		if (!IsValid(Candidate) || !Candidate->IsLootAvailable() || Candidate->GetLootRowId().IsNone() || !IsValid(GameMode) ||
			!GameMode->TryGetItemDefinition(Candidate->GetLootRowId(), CandidateItemDefinition) ||
			!GameMode->TryGetLootDefinition(Candidate->GetLootRowId(), CandidateLootDefinition))
		{
			continue;
		}
		const int32 GridArea = CandidateItemDefinition.GridSize.X * CandidateItemDefinition.GridSize.Y;
		if (!IsValid(SelectedLootActor) || GridArea < SelectedGridArea ||
			(GridArea == SelectedGridArea && Candidate->GetFName().LexicalLess(SelectedLootActor->GetFName())))
		{
			SelectedLootActor = Candidate;
			SelectedItemDefinition = CandidateItemDefinition;
			SelectedLootDefinition = CandidateLootDefinition;
			SelectedGridArea = GridArea;
		}
	}
	if (!IsValid(SelectedLootActor) || SelectedLootDefinition.ScoreValue <= 0)
	{
		return false;
	}

	TSet<int32> PlayerIds;
	TArray<FVector> SpawnLocations;
	for (FConstPlayerControllerIterator It = ServerWorld->GetPlayerControllerIterator(); It; ++It)
	{
		const AHeistPlayerController* PlayerController = Cast<AHeistPlayerController>(It->Get());
		const AHeistPlayerState* PlayerState = IsValid(PlayerController) ? PlayerController->GetPlayerState<AHeistPlayerState>() : nullptr;
		const APawn* Pawn = IsValid(PlayerController) ? PlayerController->GetPawn() : nullptr;
		if (!IsValid(PlayerState) || !IsValid(Pawn) || PlayerState->HeistPlayerId < 1 || PlayerState->HeistPlayerId > State->PlayerCount)
		{
			return false;
		}
		PlayerIds.Add(PlayerState->HeistPlayerId);
		SpawnLocations.Add(Pawn->GetActorLocation());
	}
	if (PlayerIds.Num() != State->PlayerCount || SpawnLocations.Num() != State->PlayerCount)
	{
		return false;
	}
	for (int32 LeftIndex = 0; LeftIndex < SpawnLocations.Num(); ++LeftIndex)
	{
		for (int32 RightIndex = LeftIndex + 1; RightIndex < SpawnLocations.Num(); ++RightIndex)
		{
			if (FVector::DistSquared2D(SpawnLocations[LeftIndex], SpawnLocations[RightIndex]) < FMath::Square(10.0f))
			{
				return false;
			}
		}
	}

	for (UWorld* World : GetContractRunPIEWorlds())
	{
		const AHeistGameState* GameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
		if (!IsValid(GameState) || !(GameState->GetContractSnapshot() == ServerContract) || !IsValid(FindPaintingCase(World, ServerContract.RequiredTargetCaseId)))
		{
			return false;
		}
		for (const FName ObjectCaseId : ObjectCaseIds)
		{
			if (!IsValid(FindObjectCase(World, ObjectCaseId)))
			{
				return false;
			}
		}
		const AHeistLootActor* ReplicatedLootActor = FindLootActor(World, SelectedLootActor->GetFName());
		if (!IsValid(ReplicatedLootActor) || !ReplicatedLootActor->IsLootAvailable() || ReplicatedLootActor->GetLootRowId() != SelectedLootActor->GetLootRowId())
		{
			return false;
		}
	}

	State->SelectedObjectCaseIds = MoveTemp(ObjectCaseIds);
	State->SelectedLootActorName = SelectedLootActor->GetFName();
	State->SelectedLootRowId = SelectedLootActor->GetLootRowId();
	State->SelectedLootValue = SelectedLootDefinition.ScoreValue;
	if (RunIndex == 1)
	{
		State->FirstRunContract = ServerContract;
	}
	Test->AddInfo(FString::Printf(TEXT("W6-010 preflight: Run=%d Players=%d Map=%s Seed=%d Target=%s OptionalCases=%d Quota=%d SpawnSnapshot=PASS ContractReplication=PASS ContributionReset=PASS InputLock=0"),
		RunIndex, State->PlayerCount, *ServerContract.MapId.ToString(), ServerContract.AssignmentSeed, *ServerContract.RequiredTargetCaseId.ToString(),
		State->SelectedObjectCaseIds.Num(), ServerContract.LootValueQuota));
	Test->AddInfo(FString::Printf(TEXT("W6-010 loose-loot fixture: Run=%d Actor=%s Row=%s Value=%d Grid=%dx%d Replication=PASS"), RunIndex,
		*State->SelectedLootActorName.ToString(), *State->SelectedLootRowId.ToString(), State->SelectedLootValue, SelectedItemDefinition.GridSize.X, SelectedItemDefinition.GridSize.Y));
	return true;
}

bool IsSurfaceSessionReady(const int32 PlayerId, const FName CaseId)
{
	const AHeistPlayerCharacter* ServerCharacter = GetServerCharacterById(PlayerId);
	const UHeistForgeryComponent* ServerForgery = IsValid(ServerCharacter) ? ServerCharacter->GetForgeryComponent() : nullptr;
	const AHeistPlayerCharacter* OwningCharacter = GetOwningCharacterById(PlayerId);
	const UHeistForgeryComponent* OwningForgery = IsValid(OwningCharacter) ? OwningCharacter->GetForgeryComponent() : nullptr;
	return IsValid(ServerForgery) && ServerForgery->IsSessionActive() && ServerForgery->GetActiveDisplayCase() == FindPaintingCase(GetContractRunServerWorld(), CaseId) &&
		IsValid(OwningForgery) && OwningForgery->IsSessionActive() && OwningForgery->GetSessionRevision() == ServerForgery->GetSessionRevision() &&
		OwningForgery->GetActiveTemplateId() == ServerForgery->GetActiveTemplateId() && IsCrewStatusReplicated(PlayerId, EHeistCrewStatus::Forging);
}

bool SubmitReferenceMatchedSurface(const int32 PlayerId)
{
	AHeistPlayerController* OwningPlayerController = GetOwningPlayerControllerById(PlayerId);
	AHeistPlayerCharacter* OwningCharacter = IsValid(OwningPlayerController) ? OwningPlayerController->GetPawn<AHeistPlayerCharacter>() : nullptr;
	UHeistForgeryComponent* ForgeryComponent = IsValid(OwningCharacter) ? OwningCharacter->GetForgeryComponent() : nullptr;
	if (!IsValid(ForgeryComponent))
	{
		return false;
	}
	TArray<FVector2D> Points;
	TArray<int32> StrokePointCounts;
	TArray<uint8> PaletteIndices;
	TArray<uint8> BrushPresetIndices;
	FHeistForgeryResult PreviewResult;
	if (!ForgeryComponent->BuildReferenceMatchedStrokePayloadForAutomation(Points, StrokePointCounts, PaletteIndices, BrushPresetIndices, PreviewResult) ||
		PreviewResult.SimilarityScore < HeistReplicaAcceptance::MinimumQualityScore)
	{
		UE_LOG(LogTemp, Error, TEXT("W6-010 surface payload generation failed: PlayerId=%d Template=%s Preview=%.2f Strokes=%d Points=%d"), PlayerId,
			*ForgeryComponent->GetActiveTemplateId().ToString(), PreviewResult.SimilarityScore, StrokePointCounts.Num(), Points.Num());
		return false;
	}
	OwningPlayerController->RequestSubmitForgeryStrokes(Points, StrokePointCounts, PaletteIndices, BrushPresetIndices, ForgeryComponent->GetSessionRevision());
	return true;
}

bool HasSurfaceReplicaPreview(const int32 PlayerId, const FName CaseId)
{
	const AHeistPaintingDisplayCaseActor* DisplayCase = FindPaintingCase(GetContractRunServerWorld(), CaseId);
	const AHeistPlayerCharacter* Character = GetServerCharacterById(PlayerId);
	const UHeistForgeryComponent* ForgeryComponent = IsValid(Character) ? Character->GetForgeryComponent() : nullptr;
	return IsValid(DisplayCase) && DisplayCase->HasReplicaPreview() &&
		DisplayCase->GetCommittedForgeryResult().SimilarityScore >= HeistReplicaAcceptance::MinimumQualityScore && IsValid(ForgeryComponent) &&
		ForgeryComponent->HasPendingReplicaReview();
}

bool HasOriginalForCase(const int32 PlayerId, const AActor* SourceCase)
{
	const AHeistPlayerCharacter* Character = GetServerCharacterById(PlayerId);
	const UHeistInventoryComponent* InventoryComponent = IsValid(Character) ? Character->GetInventoryComponent() : nullptr;
	FHeistInventoryItem OriginalItem;
	return IsValid(InventoryComponent) && InventoryComponent->TryGetOriginalArtifactForSourceCase(SourceCase, OriginalItem) && OriginalItem.HasValidOriginalData();
}

bool HasReplicatedLooseLootPickup(const TSharedRef<FHeistContractRunAutomationState>& State, const int32 PlayerId)
{
	const AHeistPlayerCharacter* ServerCharacter = GetServerCharacterById(PlayerId);
	const UHeistInventoryComponent* Inventory = IsValid(ServerCharacter) ? ServerCharacter->GetInventoryComponent() : nullptr;
	const AHeistPlayerState* PlayerState = IsValid(ServerCharacter) ? ServerCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	bool bInventoryContainsLoot = false;
	if (IsValid(Inventory))
	{
		for (const FHeistInventoryFastArrayItem& Entry : Inventory->GetReplicatedInventory().Items)
		{
			bInventoryContainsLoot |= !Entry.InventoryItem.IsOriginalArtifact() && Entry.InventoryItem.ItemId == State->SelectedLootRowId;
		}
	}
	if (!bInventoryContainsLoot || !IsValid(PlayerState) || PlayerState->GetTotalLootScore() < State->SelectedLootValue)
	{
		return false;
	}
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		const AHeistLootActor* LootActor = FindLootActor(World, State->SelectedLootActorName);
		if (!IsValid(LootActor) || LootActor->IsLootAvailable())
		{
			return false;
		}
	}
	return true;
}

bool IsObjectSessionReady(const int32 PlayerId, const FName CaseId)
{
	const AHeistPlayerCharacter* ServerCharacter = GetServerCharacterById(PlayerId);
	const UHeistObjectAssemblyComponent* ServerAssembly = IsValid(ServerCharacter) ? ServerCharacter->GetObjectAssemblyComponent() : nullptr;
	const UHeistInventoryComponent* ServerInventory = IsValid(ServerCharacter) ? ServerCharacter->GetInventoryComponent() : nullptr;
	const AHeistPlayerCharacter* OwningCharacter = GetOwningCharacterById(PlayerId);
	const UHeistObjectAssemblyComponent* OwningAssembly = IsValid(OwningCharacter) ? OwningCharacter->GetObjectAssemblyComponent() : nullptr;
	const EHeistCrewStatus ExpectedStatus = IsValid(ServerInventory) && ServerInventory->GetOriginalArtifactCount() > 0
		? EHeistCrewStatus::CarryingOriginal
		: EHeistCrewStatus::Assembling;
	return IsValid(ServerAssembly) && ServerAssembly->IsSessionActive() && ServerAssembly->GetActiveDisplayCase() == FindObjectCase(GetContractRunServerWorld(), CaseId) &&
		IsValid(OwningAssembly) && OwningAssembly->IsSessionActive() && OwningAssembly->GetSessionRevision() == ServerAssembly->GetSessionRevision() &&
		OwningAssembly->GetActiveTemplateId() == ServerAssembly->GetActiveTemplateId() && IsCrewStatusReplicated(PlayerId, ExpectedStatus);
}

bool SubmitExactObjectAssembly(const int32 PlayerId)
{
	AHeistPlayerController* OwningPlayerController = GetOwningPlayerControllerById(PlayerId);
	AHeistPlayerCharacter* OwningCharacter = IsValid(OwningPlayerController) ? OwningPlayerController->GetPawn<AHeistPlayerCharacter>() : nullptr;
	UHeistObjectAssemblyComponent* AssemblyComponent = IsValid(OwningCharacter) ? OwningCharacter->GetObjectAssemblyComponent() : nullptr;
	AHeistGameMode* GameMode = IsValid(GetContractRunServerWorld()) ? GetContractRunServerWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	FHeistObjectAssemblyTemplateRow TemplateDefinition;
	if (!IsValid(AssemblyComponent) || !IsValid(GameMode) || !GameMode->TryGetObjectAssemblyTemplateDefinition(AssemblyComponent->GetActiveTemplateId(), TemplateDefinition) ||
		TemplateDefinition.RequiredParts.IsEmpty())
	{
		return false;
	}
	OwningPlayerController->RequestSubmitObjectAssembly(TemplateDefinition.RequiredParts, AssemblyComponent->GetSessionRevision());
	return true;
}

bool HasObjectReplicaPreview(const int32 PlayerId, const FName CaseId)
{
	const AHeistObjectDisplayCaseActor* DisplayCase = FindObjectCase(GetContractRunServerWorld(), CaseId);
	const AHeistPlayerCharacter* Character = GetServerCharacterById(PlayerId);
	const UHeistObjectAssemblyComponent* AssemblyComponent = IsValid(Character) ? Character->GetObjectAssemblyComponent() : nullptr;
	return IsValid(DisplayCase) && DisplayCase->HasReplicaPreview() && DisplayCase->HasCommittedAssemblyResult() &&
		DisplayCase->GetCommittedAssemblyResult().QualityScore >= HeistReplicaAcceptance::MinimumQualityScore && IsValid(AssemblyComponent) &&
		AssemblyComponent->HasPendingReplicaReview();
}

int32 CountVisibleResultWidgets(UWorld* World)
{
	int32 VisibleWidgetCount = 0;
	for (TObjectIterator<UHeistResultWidget> It; It; ++It)
	{
		const UHeistResultWidget* Widget = *It;
		VisibleWidgetCount += IsValid(Widget) && Widget->GetWorld() == World && Widget->IsVisible() ? 1 : 0;
	}
	return VisibleWidgetCount;
}

bool IsCompletedResultReady(const TSharedRef<FHeistContractRunAutomationState>& State)
{
	const int32 ExpectedRecoveredOriginalCount = State->PlayerCount == 1 ? 2 : 4;
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		const AHeistGameState* GameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
		const AHeistPlayerController* PlayerController = GetContractRunLocalHeistPlayerController(World);
		const AHeistHUD* HUD = IsValid(PlayerController) ? PlayerController->GetHUD<AHeistHUD>() : nullptr;
		const UHeistResultWidget* ResultWidget = IsValid(HUD) ? HUD->GetResultWidget() : nullptr;
		const UHeistHUDWidget* MainHUDWidget = IsValid(HUD) ? HUD->GetMainHUDWidget() : nullptr;
		if (!IsValid(GameState) || GameState->GetMatchPhase() != EHeistMatchPhase::End || !GameState->GetTeamResult().IsValid() ||
			GameState->GetTeamResult().Outcome != EHeistContractOutcome::Success || GameState->GetPlayerResults().Num() != State->PlayerCount ||
			!IsValid(ResultWidget) || !ResultWidget->IsVisible() || CountVisibleResultWidgets(World) != 1 ||
			!IsValid(MainHUDWidget) || !MainHUDWidget->IsHiddenPresentationStateReset())
		{
			return false;
		}
		const FHeistPlayerResult* LootCarrierResult = GameState->GetPlayerResults().FindByPredicate(
			[](const FHeistPlayerResult& PlayerResult) { return PlayerResult.PlayerId == 1; });
		if (LootCarrierResult == nullptr || LootCarrierResult->Contribution.SecuredLootValue < State->SelectedLootValue)
		{
			return false;
		}
		int32 RecoveredOriginalCount = 0;
		for (const FHeistPlayerResult& PlayerResult : GameState->GetPlayerResults())
		{
			RecoveredOriginalCount += PlayerResult.Contribution.ArtifactsRecovered;
		}
		if (RecoveredOriginalCount != ExpectedRecoveredOriginalCount)
		{
			return false;
		}
	}
	for (int32 PlayerId = 1; PlayerId <= State->PlayerCount; ++PlayerId)
	{
		const AHeistPlayerCharacter* Character = GetServerCharacterById(PlayerId);
		const UHeistInventoryComponent* Inventory = IsValid(Character) ? Character->GetInventoryComponent() : nullptr;
		if (!IsValid(Inventory) || !Inventory->GetReplicatedInventory().Items.IsEmpty())
		{
			return false;
		}
	}
	return true;
}

FString DescribeCompletedResultReadiness(const TSharedRef<FHeistContractRunAutomationState>& State)
{
	TArray<FString> WorldStates;
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		const AHeistGameState* GameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
		const AHeistPlayerController* PlayerController = GetContractRunLocalHeistPlayerController(World);
		const AHeistHUD* HUD = IsValid(PlayerController) ? PlayerController->GetHUD<AHeistHUD>() : nullptr;
		const UHeistResultWidget* ResultWidget = IsValid(HUD) ? HUD->GetResultWidget() : nullptr;
		const UHeistHUDWidget* MainHUDWidget = IsValid(HUD) ? HUD->GetMainHUDWidget() : nullptr;
		const FHeistPlayerResult* LootCarrierResult = IsValid(GameState) ? GameState->GetPlayerResults().FindByPredicate(
			[](const FHeistPlayerResult& PlayerResult) { return PlayerResult.PlayerId == 1; }) : nullptr;
		int32 RecoveredOriginalCount = 0;
		if (IsValid(GameState))
		{
			for (const FHeistPlayerResult& PlayerResult : GameState->GetPlayerResults())
			{
				RecoveredOriginalCount += PlayerResult.Contribution.ArtifactsRecovered;
			}
		}
		WorldStates.Add(FString::Printf(
			TEXT("World=%s NetMode=%d Phase=%s TeamResult=%s Outcome=%s PlayerResults=%d HUD=%s ResultWidget=%s ResultVisibility=%s VisibleResultWidgets=%d MainHUD=%s MainHUDReset=%s LootCarrierResult=%s SecuredLootContribution=%d RecoveredOriginals=%d"),
			*GetNameSafe(World), IsValid(World) ? static_cast<int32>(World->GetNetMode()) : INDEX_NONE,
			IsValid(GameState) ? *UEnum::GetValueAsString(GameState->GetMatchPhase()) : TEXT("Missing"),
			IsValid(GameState) && GameState->GetTeamResult().IsValid() ? TEXT("Valid") : TEXT("Invalid"),
			IsValid(GameState) ? *UEnum::GetValueAsString(GameState->GetTeamResult().Outcome) : TEXT("Missing"),
			IsValid(GameState) ? GameState->GetPlayerResults().Num() : INDEX_NONE, *GetNameSafe(HUD), *GetNameSafe(ResultWidget),
			IsValid(ResultWidget) ? *UEnum::GetValueAsString(ResultWidget->GetVisibility()) : TEXT("Missing"), CountVisibleResultWidgets(World), *GetNameSafe(MainHUDWidget),
			IsValid(MainHUDWidget) && MainHUDWidget->IsHiddenPresentationStateReset() ? TEXT("true") : TEXT("false"),
			LootCarrierResult != nullptr ? TEXT("true") : TEXT("false"), LootCarrierResult != nullptr ? LootCarrierResult->Contribution.SecuredLootValue : INDEX_NONE,
			RecoveredOriginalCount));
	}
	return FString::Printf(TEXT("W6-010 Result readiness diagnostic: ExpectedPlayers=%d ExpectedRecoveredOriginals=%d %s"), State->PlayerCount,
		State->PlayerCount == 1 ? 2 : 4, *FString::Join(WorldStates, TEXT(" | ")));
}

bool IsLobbyStateClean(const int32 PlayerCount)
{
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		const AHeistGameState* GameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
		const AHeistPlayerController* PlayerController = GetContractRunLocalHeistPlayerController(World);
		const AHeistHUD* HUD = IsValid(PlayerController) ? PlayerController->GetHUD<AHeistHUD>() : nullptr;
		const UHeistResultWidget* ResultWidget = IsValid(HUD) ? HUD->GetResultWidget() : nullptr;
		if (!IsValid(GameState) || GameState->GetMatchPhase() != EHeistMatchPhase::Lobby || GameState->PlayerArray.Num() != PlayerCount ||
			GameState->GetContractSnapshot().IsInitialized() || GameState->GetAlertLevel() != EHeistAlertLevel::Quiet || GameState->GetTeamResult().IsValid() ||
			!GameState->GetPlayerResults().IsEmpty() || (IsValid(ResultWidget) && (ResultWidget->GetVisibility() != ESlateVisibility::Collapsed || !ResultWidget->IsHiddenPresentationStateReset())))
		{
			return false;
		}
	}
	return true;
}

bool IsGameplayContentReplicated(const int32 PlayerCount)
{
	const int32 ExpectedOptionalCaseCount = PlayerCount + 1;
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		const AHeistGameState* GameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
		if (!IsValid(GameState) || GameState->GetMatchPhase() != EHeistMatchPhase::InGame || !GameState->GetContractSnapshot().IsInitialized() ||
			GameState->GetContractSnapshot().MapId != FName(TEXT("M01")) || !IsValid(FindPaintingCase(World, GameState->GetContractSnapshot().RequiredTargetCaseId)))
		{
			return false;
		}
		int32 ObjectCaseCount = 0;
		for (TActorIterator<AHeistObjectDisplayCaseActor> It(World); It; ++It)
		{
			ObjectCaseCount += IsValid(*It) && It->IsContractExhibitActive() ? 1 : 0;
		}
		if (ObjectCaseCount != ExpectedOptionalCaseCount)
		{
			return false;
		}
	}
	return true;
}

bool IsGameplayResetClean(const int32 PlayerCount)
{
	if (!IsGameplayContentReplicated(PlayerCount))
	{
		return false;
	}
	UWorld* ServerWorld = GetContractRunServerWorld();
	const AHeistGameState* GameState = IsValid(ServerWorld) ? ServerWorld->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(GameState) || GameState->GetAlertLevel() != EHeistAlertLevel::Quiet || GameState->GetContractSnapshot().CarriedValue != 0 ||
		GameState->GetContractSnapshot().SecuredValue != 0 || GameState->GetContractSnapshot().bRequiredTargetSecured || GameState->GetTeamResult().IsValid() ||
		!GameState->GetPlayerResults().IsEmpty())
	{
		return false;
	}
	const FHeistPlayerContribution EmptyContribution;
	for (APlayerState* RawPlayerState : GameState->PlayerArray)
	{
		const AHeistPlayerState* PlayerState = Cast<AHeistPlayerState>(RawPlayerState);
		const AHeistPlayerCharacter* Character = IsValid(PlayerState) ? Cast<AHeistPlayerCharacter>(PlayerState->GetPawn()) : nullptr;
		const UHeistInventoryComponent* Inventory = IsValid(Character) ? Character->GetInventoryComponent() : nullptr;
		const UHeistForgeryComponent* Forgery = IsValid(Character) ? Character->GetForgeryComponent() : nullptr;
		const UHeistObjectAssemblyComponent* Assembly = IsValid(Character) ? Character->GetObjectAssemblyComponent() : nullptr;
		if (!IsValid(PlayerState) || !IsValid(Inventory) || !Inventory->GetReplicatedInventory().Items.IsEmpty() || PlayerState->GetTotalLootScore() != 0 ||
			!FMath::IsNearlyZero(PlayerState->GetTotalLootWeight()) || PlayerState->IsEscaped() || PlayerState->IsArrested() || !(PlayerState->GetContribution() == EmptyContribution) ||
			(IsValid(Forgery) && (Forgery->IsSessionActive() || Forgery->HasPendingReplicaReview())) ||
			(IsValid(Assembly) && (Assembly->IsSessionActive() || Assembly->HasPendingReplicaReview())))
		{
			return false;
		}
	}
	for (TActorIterator<AHeistPaintingDisplayCaseActor> It(ServerWorld); It; ++It)
	{
		if (IsValid(*It) && (It->GetDisplayCaseState() != EHeistDisplayCaseState::Secured || It->HasCommittedForgeryResult() || It->IsSessionLocked()))
		{
			return false;
		}
	}
	for (TActorIterator<AHeistObjectDisplayCaseActor> It(ServerWorld); It; ++It)
	{
		if (IsValid(*It) && (It->GetAssemblyState() != EHeistObjectAssemblyState::Secured || It->HasCommittedAssemblyResult() || It->IsSessionLocked()))
		{
			return false;
		}
	}
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		const AHeistGameState* ReplicatedGameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
		const AHeistPlayerController* PlayerController = GetContractRunLocalHeistPlayerController(World);
		const AHeistHUD* HUD = IsValid(PlayerController) ? PlayerController->GetHUD<AHeistHUD>() : nullptr;
		const UHeistResultWidget* ResultWidget = IsValid(HUD) ? HUD->GetResultWidget() : nullptr;
		if (!IsValid(ReplicatedGameState) || !IsValid(PlayerController) || !IsValid(PlayerController->GetPawn()) || PlayerController->GetLocalInputMode() != EHeistInputMode::Gameplay ||
			PlayerController->GetActiveHeistInputMappingContextCount() != 1 || !PlayerController->IsLocalInputModeContractSatisfied() || PlayerController->bShowMouseCursor ||
			PlayerController->IsMoveInputIgnored() || PlayerController->IsLookInputIgnored() ||
			(IsValid(ResultWidget) && (ResultWidget->GetVisibility() != ESlateVisibility::Collapsed || !ResultWidget->IsHiddenPresentationStateReset())))
		{
			return false;
		}
		for (const APlayerState* RawPlayerState : ReplicatedGameState->PlayerArray)
		{
			const AHeistPlayerState* PlayerState = Cast<AHeistPlayerState>(RawPlayerState);
			if (!IsValid(PlayerState) || !(PlayerState->GetContribution() == EmptyContribution))
			{
				return false;
			}
		}
	}
	return true;
}

bool IsGuardChaseAndAlertReady(const int32 PlayerId)
{
	const AHeistPlayerCharacter* TargetCharacter = GetServerCharacterById(PlayerId);
	bool bChasingTarget = false;
	for (TActorIterator<AHeistGuardCharacter> It(GetContractRunServerWorld()); It; ++It)
	{
		const UHeistGuardStateComponent* GuardState = IsValid(*It) ? It->GetGuardStateComponent() : nullptr;
		bChasingTarget |= IsValid(GuardState) && GuardState->GetGuardState() == EHeistGuardState::ChasePlayer && GuardState->GetChaseTarget() == TargetCharacter;
	}
	if (!bChasingTarget)
	{
		return false;
	}
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		const AHeistGameState* GameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
		if (!IsValid(GameState) || GameState->GetAlertLevel() != EHeistAlertLevel::Alarmed)
		{
			return false;
		}
	}
	return true;
}

bool IsWeek7DangerCloseReady(const int32 PlayerId, const bool bSurface)
{
	UWorld* ServerWorld = GetContractRunServerWorld();
	const AHeistPlayerCharacter* ServerCharacter = GetServerCharacterById(PlayerId);
	const UHeistForgeryComponent* Forgery = IsValid(ServerCharacter) ? ServerCharacter->GetForgeryComponent() : nullptr;
	const UHeistObjectAssemblyComponent* Assembly = IsValid(ServerCharacter) ? ServerCharacter->GetObjectAssemblyComponent() : nullptr;
	if (!IsValid(ServerWorld) || (bSurface ? (IsValid(Forgery) && Forgery->IsSessionActive()) : (IsValid(Assembly) && Assembly->IsSessionActive())))
	{
		return false;
	}
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		const AHeistGameState* GameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
		if (!IsValid(GameState) || GameState->GetAlertLevel() != EHeistAlertLevel::Alarmed)
		{
			return false;
		}
	}
	const AHeistPlayerController* PlayerController = GetOwningPlayerControllerById(PlayerId);
	const AHeistHUD* HUD = IsValid(PlayerController) ? PlayerController->GetHUD<AHeistHUD>() : nullptr;
	if (!IsValid(PlayerController) || !IsValid(HUD) || PlayerController->GetLocalInputMode() != EHeistInputMode::Gameplay ||
		!PlayerController->IsLocalInputModeContractSatisfied() || PlayerController->bShowMouseCursor || PlayerController->IsMoveInputIgnored() || PlayerController->IsLookInputIgnored())
	{
		return false;
	}
	if (bSurface)
	{
		const UHeistForgeryWidget* Widget = HUD->GetForgeryWidget();
		return IsValid(Widget) && Widget->IsAlertWarningContractSatisfied();
	}
	const UHeistObjectAssemblyWidget* Widget = HUD->GetObjectAssemblyWidget();
	return IsValid(Widget) && Widget->IsAlertWarningContractSatisfied();
}

class FHeistContractRunWaitCommand final : public IAutomationLatentCommand
{
  public:
	FHeistContractRunWaitCommand(FAutomationTestBase* InTest, const TSharedRef<FHeistContractRunAutomationState>& InState, FString InDescription,
		TFunction<bool()> InPredicate, const double InTimeoutSeconds, TFunction<FString()> InDiagnostic = {})
		: Test(InTest), State(InState), Description(MoveTemp(InDescription)), Predicate(MoveTemp(InPredicate)), Diagnostic(MoveTemp(InDiagnostic)), TimeoutSeconds(InTimeoutSeconds)
	{
	}

	virtual bool Update() override
	{
		if (State->bAborted)
		{
			return true;
		}
		if (StartTimeSeconds <= 0.0)
		{
			StartTimeSeconds = FPlatformTime::Seconds();
		}
		if (Predicate())
		{
			Test->AddInfo(FString::Printf(TEXT("W6-010 ready: %s"), *Description));
			return true;
		}
		if (FPlatformTime::Seconds() - StartTimeSeconds >= TimeoutSeconds)
		{
			Test->AddError(FString::Printf(TEXT("W6-010 timeout: %s"), *Description));
			if (Diagnostic)
			{
				Test->AddInfo(Diagnostic());
			}
			State->bAborted = true;
			return true;
		}
		return false;
	}

  private:
	FAutomationTestBase* Test = nullptr;
	TSharedRef<FHeistContractRunAutomationState> State;
	FString Description;
	TFunction<bool()> Predicate;
	TFunction<FString()> Diagnostic;
	double TimeoutSeconds = 30.0;
	double StartTimeSeconds = 0.0;
};

class FHeistContractRunActionCommand final : public IAutomationLatentCommand
{
  public:
	FHeistContractRunActionCommand(FAutomationTestBase* InTest, const TSharedRef<FHeistContractRunAutomationState>& InState, FString InDescription,
		TFunction<bool()> InAction, const bool bInRunAfterAbort = false)
		: Test(InTest), State(InState), Description(MoveTemp(InDescription)), Action(MoveTemp(InAction)), bRunAfterAbort(bInRunAfterAbort)
	{
	}

	virtual bool Update() override
	{
		if (State->bAborted && !bRunAfterAbort)
		{
			return true;
		}
		const bool bPassed = Action();
		if (bPassed)
		{
			Test->AddInfo(FString::Printf(TEXT("W6-010 action: %s"), *Description));
		}
		else
		{
			Test->AddError(FString::Printf(TEXT("W6-010 action failed: %s"), *Description));
			State->bAborted = true;
		}
		return true;
	}

  private:
	FAutomationTestBase* Test = nullptr;
	TSharedRef<FHeistContractRunAutomationState> State;
	FString Description;
	TFunction<bool()> Action;
	bool bRunAfterAbort = false;
};

void AppendGameplayRunCommands(FAutomationTestBase* Test, const TSharedRef<FHeistContractRunAutomationState>& State, const int32 RunIndex)
{
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d M01 worlds and replicated content"), RunIndex), [State]()
	{
		return AreContractRunWorldsReady(State->PlayerCount, EHeistMatchPhase::InGame, true) && IsGameplayContentReplicated(State->PlayerCount);
	}, 75.0));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d clean gameplay state"), RunIndex), [State]()
	{
		return IsGameplayResetClean(State->PlayerCount);
	}, 30.0));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("capture run %d spawn and contract snapshot"), RunIndex), [Test, State, RunIndex]()
	{
		return CaptureAndValidateGameplayPreflight(Test, State, RunIndex);
	}, 15.0));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d W7 Team Status and remote Nameplates"), RunIndex), [State]()
	{
		return IsWeek7ReadabilityPresentationReady(State->PlayerCount);
	}, 15.0, [State]() { return DescribeWeek7ReadabilityPresentation(State->PlayerCount); }));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d open owner-only Floor Plan on every peer"), RunIndex), []()
	{
		return ToggleWeek7Maps(true);
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d Map input lock and mapping context"), RunIndex), []()
	{
		return AreWeek7MapContractsReady(true);
	}, 10.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d close Floor Plan on every peer"), RunIndex), []()
	{
		return ToggleWeek7Maps(false);
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d Gameplay input restored after Map"), RunIndex), []()
	{
		return AreWeek7MapContractsReady(false);
	}, 10.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d request Sprint on every peer"), RunIndex), []()
	{
		return RequestWeek7Sprint(true);
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d authoritative Sprint 600 replicated"), RunIndex), [State]()
	{
		return IsWeek7SprintReplicated(State->PlayerCount, true);
	}, 10.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d release Sprint on every peer"), RunIndex), []()
	{
		return RequestWeek7Sprint(false);
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d Walk 300 restored"), RunIndex), [State]()
	{
		return IsWeek7SprintReplicated(State->PlayerCount, false);
	}, 10.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d Guard contact applies Stun before Arrest"), RunIndex), [State]()
	{
		return State->PlayerCount == 1 || BeginWeek7GuardStun(2);
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d Stun HUD/input lock replicated"), RunIndex), [State]()
	{
		return State->PlayerCount == 1 || IsWeek7StunPresentationReady(2);
	}, 10.0));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d delayed Arrest replicated"), RunIndex), [State]()
	{
		return State->PlayerCount == 1 || IsWeek7ArrestReplicated(2);
	}, 15.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d move teammate into Rescue range"), RunIndex), [State]()
	{
		return State->PlayerCount == 1 || MoveWeek7RescuerIntoRange(1, 2);
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d Rescue overlap"), RunIndex), [State]()
	{
		return State->PlayerCount == 1 || IsServerPlayerOverlapping(1, GetServerCharacterById(2));
	}, 10.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d request teammate Rescue through server RPC"), RunIndex), [State]()
	{
		return State->PlayerCount == 1 || RequestWeek7Rescue(1, 2);
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d Rescue restores Active/input"), RunIndex), [State]()
	{
		return State->PlayerCount == 1 || IsWeek7RescueComplete(2);
	}, 15.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d disable autonomous Guard interference"), RunIndex), []()
	{
		UWorld* ServerWorld = GetContractRunServerWorld();
		if (!IsValid(ServerWorld))
		{
			return false;
		}
		bool bFoundGuard = false;
		for (TActorIterator<AHeistGuardCharacter> It(ServerWorld); It; ++It)
		{
			if (UHeistGuardStateComponent* GuardState = IsValid(*It) ? It->GetGuardStateComponent() : nullptr; IsValid(GuardState))
			{
				GuardState->SetDisabled(true);
				bFoundGuard = true;
			}
		}
		return bFoundGuard;
	}));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d move player 1 to shared loose loot"), RunIndex), [State]()
	{
		return TeleportServerPlayerIntoInteraction(1, FindLootActor(GetContractRunServerWorld(), State->SelectedLootActorName));
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d shared loose-loot overlap"), RunIndex), [State]()
	{
		return IsServerPlayerOverlapping(1, FindLootActor(GetContractRunServerWorld(), State->SelectedLootActorName));
	}, 10.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d request loose-loot pickup through server RPC"), RunIndex), [State]()
	{
		AHeistPlayerController* PlayerController = GetOwningPlayerControllerById(1);
		AHeistLootActor* LocalLootActor = IsValid(PlayerController) ? FindLootActor(PlayerController->GetWorld(), State->SelectedLootActorName) : nullptr;
		return InvokeSingleActorServerRPC(PlayerController, FName(TEXT("Server_RequestLootPickup")), LocalLootActor);
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d loose loot carried and unavailable on every peer"), RunIndex), [State]()
	{
		return HasReplicatedLooseLootPickup(State, 1);
	}, 15.0));

	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d move player 1 to required Surface case"), RunIndex), [State]()
	{
		const AHeistGameState* GameState = GetContractRunServerWorld()->GetGameState<AHeistGameState>();
		return IsValid(GameState) && TeleportServerPlayerIntoInteraction(1, FindPaintingCase(GetContractRunServerWorld(), GameState->GetContractSnapshot().RequiredTargetCaseId));
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d required Surface overlap"), RunIndex), []()
	{
		const AHeistGameState* GameState = GetContractRunServerWorld()->GetGameState<AHeistGameState>();
		return IsValid(GameState) && IsServerPlayerOverlapping(1, FindPaintingCase(GetContractRunServerWorld(), GameState->GetContractSnapshot().RequiredTargetCaseId));
	}, 10.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d request Surface observation through server RPC"), RunIndex), []()
	{
		AHeistPlayerController* OwningPlayerController = GetOwningPlayerControllerById(1);
		const AHeistGameState* GameState = IsValid(OwningPlayerController) ? OwningPlayerController->GetWorld()->GetGameState<AHeistGameState>() : nullptr;
		AHeistPaintingDisplayCaseActor* LocalDisplayCase = IsValid(GameState) ? FindPaintingCase(OwningPlayerController->GetWorld(), GameState->GetContractSnapshot().RequiredTargetCaseId) : nullptr;
		return InvokeSingleActorServerRPC(OwningPlayerController, FName(TEXT("Server_RequestObservation")), LocalDisplayCase);
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d owner Surface session"), RunIndex), []()
	{
		const AHeistGameState* GameState = GetContractRunServerWorld()->GetGameState<AHeistGameState>();
		return IsValid(GameState) && IsSurfaceSessionReady(1, GameState->GetContractSnapshot().RequiredTargetCaseId);
	}, 15.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d W7 force-close Surface on danger Alert"), RunIndex), [State, RunIndex]()
	{
		if (State->PlayerCount != 4 || RunIndex != 1)
		{
			return true;
		}
		AHeistGameState* GameState = GetContractRunServerWorld()->GetGameState<AHeistGameState>();
		return IsValid(GameState) && GameState->SetAlertSnapshot(EHeistAlertLevel::Alarmed, 0.0f, FName(TEXT("W7SurfaceDangerClose")));
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d W7 Surface danger close and Gameplay input restore"), RunIndex), [State, RunIndex]()
	{
		return State->PlayerCount != 4 || RunIndex != 1 || IsWeek7DangerCloseReady(1, true);
	}, 10.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d W7 reset Alert after Surface danger close"), RunIndex), [State, RunIndex]()
	{
		if (State->PlayerCount != 4 || RunIndex != 1)
		{
			return true;
		}
		AHeistGameState* GameState = GetContractRunServerWorld()->GetGameState<AHeistGameState>();
		return IsValid(GameState) && GameState->SetAlertSnapshot(EHeistAlertLevel::Quiet, 0.0f, FName(TEXT("W7SurfaceDangerCloseReset")));
	}));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d W7 restart Surface after danger close"), RunIndex), [State, RunIndex]()
	{
		if (State->PlayerCount != 4 || RunIndex != 1)
		{
			return true;
		}
		AHeistPlayerController* OwningPlayerController = GetOwningPlayerControllerById(1);
		const AHeistGameState* GameState = IsValid(OwningPlayerController) ? OwningPlayerController->GetWorld()->GetGameState<AHeistGameState>() : nullptr;
		AHeistPaintingDisplayCaseActor* LocalDisplayCase = IsValid(GameState) ? FindPaintingCase(OwningPlayerController->GetWorld(), GameState->GetContractSnapshot().RequiredTargetCaseId) : nullptr;
		return InvokeSingleActorServerRPC(OwningPlayerController, FName(TEXT("Server_RequestObservation")), LocalDisplayCase);
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d W7 restarted Surface session"), RunIndex), [State, RunIndex]()
	{
		if (State->PlayerCount != 4 || RunIndex != 1)
		{
			return true;
		}
		const AHeistGameState* GameState = GetContractRunServerWorld()->GetGameState<AHeistGameState>();
		return IsValid(GameState) && GameState->GetAlertLevel() == EHeistAlertLevel::Quiet && IsSurfaceSessionReady(1, GameState->GetContractSnapshot().RequiredTargetCaseId);
	}, 15.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d submit generated Surface strokes"), RunIndex), []()
	{
		return SubmitReferenceMatchedSurface(1);
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d authoritative Surface quality 70+"), RunIndex), []()
	{
		const AHeistGameState* GameState = GetContractRunServerWorld()->GetGameState<AHeistGameState>();
		return IsValid(GameState) && HasSurfaceReplicaPreview(1, GameState->GetContractSnapshot().RequiredTargetCaseId);
	}, 20.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d confirm Surface replica swap"), RunIndex), []()
	{
		AHeistPlayerController* PlayerController = GetOwningPlayerControllerById(1);
		if (!IsValid(PlayerController))
		{
			return false;
		}
		PlayerController->RequestConfirmForgeryReplicaSwap();
		return true;
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d required Original carried"), RunIndex), []()
	{
		const AHeistGameState* GameState = GetContractRunServerWorld()->GetGameState<AHeistGameState>();
		AHeistPaintingDisplayCaseActor* DisplayCase = IsValid(GameState) ? FindPaintingCase(GetContractRunServerWorld(), GameState->GetContractSnapshot().RequiredTargetCaseId) : nullptr;
		return IsValid(DisplayCase) && DisplayCase->GetDisplayCaseState() == EHeistDisplayCaseState::OriginalRemoved && HasOriginalForCase(1, DisplayCase) &&
			IsCrewStatusReplicated(1, EHeistCrewStatus::CarryingOriginal);
	}, 15.0));

	const int32 AssemblyCaseCount = State->PlayerCount == 1 ? 1 : 3;
	for (int32 AssemblyIndex = 0; AssemblyIndex < AssemblyCaseCount; ++AssemblyIndex)
	{
		const int32 PlayerId = State->PlayerCount == 1 ? 1 : AssemblyIndex + 2;
		Test->AddCommand(new FHeistContractRunActionCommand(Test, State,
			FString::Printf(TEXT("run %d move player %d to Assembly case %d"), RunIndex, PlayerId, AssemblyIndex + 1), [State, PlayerId, AssemblyIndex]()
		{
			return State->SelectedObjectCaseIds.IsValidIndex(AssemblyIndex) &&
				TeleportServerPlayerIntoInteraction(PlayerId, FindObjectCase(GetContractRunServerWorld(), State->SelectedObjectCaseIds[AssemblyIndex]));
		}));
		Test->AddCommand(new FHeistContractRunWaitCommand(Test, State,
			FString::Printf(TEXT("run %d player %d Assembly overlap"), RunIndex, PlayerId), [State, PlayerId, AssemblyIndex]()
		{
			return State->SelectedObjectCaseIds.IsValidIndex(AssemblyIndex) &&
				IsServerPlayerOverlapping(PlayerId, FindObjectCase(GetContractRunServerWorld(), State->SelectedObjectCaseIds[AssemblyIndex]));
		}, 10.0));
		Test->AddCommand(new FHeistContractRunActionCommand(Test, State,
			FString::Printf(TEXT("run %d player %d request Object observation through server RPC"), RunIndex, PlayerId), [State, PlayerId, AssemblyIndex]()
		{
			AHeistPlayerController* PlayerController = GetOwningPlayerControllerById(PlayerId);
			AHeistObjectDisplayCaseActor* LocalDisplayCase = IsValid(PlayerController) && State->SelectedObjectCaseIds.IsValidIndex(AssemblyIndex)
				? FindObjectCase(PlayerController->GetWorld(), State->SelectedObjectCaseIds[AssemblyIndex])
				: nullptr;
			return InvokeSingleActorServerRPC(PlayerController, FName(TEXT("Server_RequestObjectObservation")), LocalDisplayCase);
		}));
		Test->AddCommand(new FHeistContractRunWaitCommand(Test, State,
			FString::Printf(TEXT("run %d player %d owner Assembly session"), RunIndex, PlayerId), [State, PlayerId, AssemblyIndex]()
		{
			return State->SelectedObjectCaseIds.IsValidIndex(AssemblyIndex) && IsObjectSessionReady(PlayerId, State->SelectedObjectCaseIds[AssemblyIndex]);
		}, 15.0));
		Test->AddCommand(new FHeistContractRunActionCommand(Test, State,
			FString::Printf(TEXT("run %d W7 force-close player %d Assembly on danger Alert"), RunIndex, PlayerId), [State, RunIndex, PlayerId, AssemblyIndex]()
		{
			if (State->PlayerCount != 4 || RunIndex != 1 || AssemblyIndex != 0)
			{
				return true;
			}
			AHeistGameState* GameState = GetContractRunServerWorld()->GetGameState<AHeistGameState>();
			return IsValid(GameState) && GameState->SetAlertSnapshot(EHeistAlertLevel::Alarmed, 0.0f, FName(TEXT("W7AssemblyDangerClose")));
		}));
		Test->AddCommand(new FHeistContractRunWaitCommand(Test, State,
			FString::Printf(TEXT("run %d W7 player %d Assembly danger close and Gameplay input restore"), RunIndex, PlayerId), [State, RunIndex, PlayerId, AssemblyIndex]()
		{
			return State->PlayerCount != 4 || RunIndex != 1 || AssemblyIndex != 0 || IsWeek7DangerCloseReady(PlayerId, false);
		}, 10.0));
		Test->AddCommand(new FHeistContractRunActionCommand(Test, State,
			FString::Printf(TEXT("run %d W7 reset Alert after player %d Assembly danger close"), RunIndex, PlayerId), [State, RunIndex, AssemblyIndex]()
		{
			if (State->PlayerCount != 4 || RunIndex != 1 || AssemblyIndex != 0)
			{
				return true;
			}
			AHeistGameState* GameState = GetContractRunServerWorld()->GetGameState<AHeistGameState>();
			return IsValid(GameState) && GameState->SetAlertSnapshot(EHeistAlertLevel::Quiet, 0.0f, FName(TEXT("W7AssemblyDangerCloseReset")));
		}));
		Test->AddCommand(new FHeistContractRunActionCommand(Test, State,
			FString::Printf(TEXT("run %d W7 restart player %d Assembly after danger close"), RunIndex, PlayerId), [State, RunIndex, PlayerId, AssemblyIndex]()
		{
			if (State->PlayerCount != 4 || RunIndex != 1 || AssemblyIndex != 0)
			{
				return true;
			}
			AHeistPlayerController* PlayerController = GetOwningPlayerControllerById(PlayerId);
			AHeistObjectDisplayCaseActor* LocalDisplayCase = IsValid(PlayerController) && State->SelectedObjectCaseIds.IsValidIndex(AssemblyIndex)
				? FindObjectCase(PlayerController->GetWorld(), State->SelectedObjectCaseIds[AssemblyIndex])
				: nullptr;
			if (!IsValid(PlayerController) || !IsValid(LocalDisplayCase))
			{
				return false;
			}
			PlayerController->RequestBeginObjectAssembly(LocalDisplayCase);
			return true;
		}));
		Test->AddCommand(new FHeistContractRunWaitCommand(Test, State,
			FString::Printf(TEXT("run %d W7 restarted player %d Assembly session"), RunIndex, PlayerId), [State, RunIndex, PlayerId, AssemblyIndex]()
		{
			if (State->PlayerCount != 4 || RunIndex != 1 || AssemblyIndex != 0)
			{
				return true;
			}
			const AHeistGameState* GameState = GetContractRunServerWorld()->GetGameState<AHeistGameState>();
			return IsValid(GameState) && GameState->GetAlertLevel() == EHeistAlertLevel::Quiet && State->SelectedObjectCaseIds.IsValidIndex(AssemblyIndex) &&
				IsObjectSessionReady(PlayerId, State->SelectedObjectCaseIds[AssemblyIndex]);
		}, 15.0));
		Test->AddCommand(new FHeistContractRunActionCommand(Test, State,
			FString::Printf(TEXT("run %d player %d submit exact Assembly entries"), RunIndex, PlayerId), [PlayerId]()
		{
			return SubmitExactObjectAssembly(PlayerId);
		}));
		Test->AddCommand(new FHeistContractRunWaitCommand(Test, State,
			FString::Printf(TEXT("run %d player %d authoritative Assembly quality 70+"), RunIndex, PlayerId), [State, PlayerId, AssemblyIndex]()
		{
			return State->SelectedObjectCaseIds.IsValidIndex(AssemblyIndex) && HasObjectReplicaPreview(PlayerId, State->SelectedObjectCaseIds[AssemblyIndex]);
		}, 15.0));
		Test->AddCommand(new FHeistContractRunActionCommand(Test, State,
			FString::Printf(TEXT("run %d player %d confirm Assembly replica swap"), RunIndex, PlayerId), [PlayerId]()
		{
			AHeistPlayerController* PlayerController = GetOwningPlayerControllerById(PlayerId);
			if (!IsValid(PlayerController))
			{
				return false;
			}
			PlayerController->RequestConfirmObjectAssemblyReplicaSwap();
			return true;
		}));
		Test->AddCommand(new FHeistContractRunWaitCommand(Test, State,
			FString::Printf(TEXT("run %d player %d Object Original carried"), RunIndex, PlayerId), [State, PlayerId, AssemblyIndex]()
		{
			AHeistObjectDisplayCaseActor* DisplayCase = State->SelectedObjectCaseIds.IsValidIndex(AssemblyIndex)
				? FindObjectCase(GetContractRunServerWorld(), State->SelectedObjectCaseIds[AssemblyIndex])
				: nullptr;
			return IsValid(DisplayCase) && DisplayCase->GetAssemblyState() == EHeistObjectAssemblyState::OriginalRemoved && HasOriginalForCase(PlayerId, DisplayCase) &&
				IsCrewStatusReplicated(PlayerId, EHeistCrewStatus::CarryingOriginal);
		}, 15.0));
	}

	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d refresh carried value and trigger Guard Chase/Alert"), RunIndex), []()
	{
		UWorld* ServerWorld = GetContractRunServerWorld();
		AHeistGameState* GameState = IsValid(ServerWorld) ? ServerWorld->GetGameState<AHeistGameState>() : nullptr;
		AHeistPlayerController* HostPlayerController = GetOwningPlayerControllerById(1);
		if (!IsValid(GameState) || !GameState->RefreshContractCarriedValue() || GameState->GetContractSnapshot().CarriedValue < GameState->GetContractSnapshot().LootValueQuota ||
			!IsValid(HostPlayerController) || !HostPlayerController->HasAuthority())
		{
			return false;
		}
		bool bFoundGuard = false;
		for (TActorIterator<AHeistGuardCharacter> It(ServerWorld); It; ++It)
		{
			if (UHeistGuardStateComponent* GuardState = IsValid(*It) ? It->GetGuardStateComponent() : nullptr; IsValid(GuardState))
			{
				GuardState->SetDisabled(false);
				bFoundGuard = true;
			}
		}
		if (!bFoundGuard)
		{
			return false;
		}
		UHeistDebugFunctionLibrary::DebugGuardSetState(HostPlayerController, TEXT("Chase"), 0.0f);
		UHeistDebugFunctionLibrary::DebugAlertRequest(HostPlayerController, TEXT("Alarmed"));
		return true;
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d Guard Chase and replicated Alarmed state"), RunIndex), []()
	{
		return IsGuardChaseAndAlertReady(1);
	}, 15.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d stop Guard interference and open Shared Exit"), RunIndex), []()
	{
		UWorld* ServerWorld = GetContractRunServerWorld();
		AHeistPlayerController* HostPlayerController = GetOwningPlayerControllerById(1);
		if (!IsValid(ServerWorld) || !IsValid(HostPlayerController) || !HostPlayerController->HasAuthority())
		{
			return false;
		}
		for (TActorIterator<AHeistGuardCharacter> It(ServerWorld); It; ++It)
		{
			if (UHeistGuardStateComponent* GuardState = IsValid(*It) ? It->GetGuardStateComponent() : nullptr; IsValid(GuardState))
			{
				GuardState->SetDisabled(true);
			}
		}
		UHeistDebugFunctionLibrary::DebugDepositOpen(HostPlayerController);
		return true;
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d Shared Exit replicated open"), RunIndex), []()
	{
		for (UWorld* World : GetContractRunPIEWorlds())
		{
			const AHeistGameState* GameState = World->GetGameState<AHeistGameState>();
			const AHeistVentActor* SharedExit = FindSharedExit(World);
			if (!IsValid(GameState) || !GameState->IsEscapePhaseOpen() || !IsValid(SharedExit) || !SharedExit->IsVentActive())
			{
				return false;
			}
		}
		return true;
	}, 15.0));

	for (int32 PlayerId = 1; PlayerId <= State->PlayerCount; ++PlayerId)
	{
		Test->AddCommand(new FHeistContractRunActionCommand(Test, State,
			FString::Printf(TEXT("run %d move player %d to Shared Exit"), RunIndex, PlayerId), [PlayerId]()
		{
			return TeleportServerPlayerIntoInteraction(PlayerId, FindSharedExit(GetContractRunServerWorld()));
		}));
		Test->AddCommand(new FHeistContractRunWaitCommand(Test, State,
			FString::Printf(TEXT("run %d player %d Shared Exit overlap"), RunIndex, PlayerId), [PlayerId]()
		{
			return IsServerPlayerOverlapping(PlayerId, FindSharedExit(GetContractRunServerWorld()));
		}, 10.0));
		Test->AddCommand(new FHeistContractRunActionCommand(Test, State,
			FString::Printf(TEXT("run %d player %d request exit through server RPC"), RunIndex, PlayerId), [PlayerId]()
		{
			AHeistPlayerController* PlayerController = GetOwningPlayerControllerById(PlayerId);
			AHeistVentActor* LocalExit = IsValid(PlayerController) ? FindSharedExit(PlayerController->GetWorld()) : nullptr;
			return InvokeSingleActorServerRPC(PlayerController, FName(TEXT("Server_RequestEscape")), LocalExit);
		}));
		Test->AddCommand(new FHeistContractRunWaitCommand(Test, State,
			FString::Printf(TEXT("run %d player %d deposit and escape committed"), RunIndex, PlayerId), [PlayerId]()
		{
			const AHeistPlayerController* ServerPlayerController = GetServerPlayerControllerById(PlayerId);
			const AHeistPlayerState* PlayerState = IsValid(ServerPlayerController) ? ServerPlayerController->GetPlayerState<AHeistPlayerState>() : nullptr;
			return IsValid(PlayerState) && PlayerState->IsEscaped();
		}, 15.0));
	}

	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d single Result presentation on every player"), RunIndex), [State]()
	{
		return IsCompletedResultReady(State);
	}, 30.0, [State]() { return DescribeCompletedResultReadiness(State); }));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("record run %d authority E2E evidence"), RunIndex), [Test, State, RunIndex]()
	{
		const AHeistGameState* GameState = GetContractRunServerWorld()->GetGameState<AHeistGameState>();
		if (!IsValid(GameState))
		{
			return false;
		}
		const FHeistContractSnapshot Contract = GameState->GetContractSnapshot();
		int32 RecoveredOriginalCount = 0;
		for (const FHeistPlayerResult& PlayerResult : GameState->GetPlayerResults())
		{
			RecoveredOriginalCount += PlayerResult.Contribution.ArtifactsRecovered;
		}
		const int32 ExpectedRecoveredOriginalCount = State->PlayerCount == 1 ? 2 : 4;
		Test->AddInfo(FString::Printf(TEXT("W6-010 run evidence: Run=%d Players=%d Surface=1 Assemblies=%d Originals=%d/%d LooseLoot=%s LooseValue=%d Secured=%d Quota=%d RequiredSecured=%s Escaped=%d ResultWidgetsPerWorld=1 Outcome=%s Result=PASS"),
			RunIndex, State->PlayerCount, State->PlayerCount == 1 ? 1 : 3, RecoveredOriginalCount, ExpectedRecoveredOriginalCount, *State->SelectedLootRowId.ToString(),
			State->SelectedLootValue, Contract.SecuredValue, Contract.LootValueQuota, Contract.bRequiredTargetSecured ? TEXT("true") : TEXT("false"),
			GameState->GetEscapedCrewCount(), *UEnum::GetValueAsString(Contract.Outcome)));
		return Contract.IsSuccessConditionMet() && Contract.Outcome == EHeistContractOutcome::Success && RecoveredOriginalCount == ExpectedRecoveredOriginalCount;
	}));
}

bool EnqueueTwoRunContractScenario(FAutomationTestBase* Test, const int32 PlayerCount)
{
	const TSharedRef<FHeistContractRunAutomationState> State = MakeShared<FHeistContractRunAutomationState>();
	State->PlayerCount = PlayerCount;
	Test->AddCommand(new FEditorLoadMap(TEXT("/Game/Maps/TitleMenuMap")));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, TEXT("configure listen-server PIE"), [State]()
	{
		ULevelEditorPlaySettings* PlaySettings = GetMutableDefault<ULevelEditorPlaySettings>();
		if (!IsValid(PlaySettings))
		{
			return false;
		}
		PlaySettings->GetPlayNetMode(State->OriginalNetMode);
		PlaySettings->GetRunUnderOneProcess(State->bOriginalRunUnderOneProcess);
		PlaySettings->GetPlayNumberOfClients(State->OriginalClientCount);
		State->bCapturedPlaySettings = true;
		PlaySettings->SetRunUnderOneProcess(true);
		PlaySettings->SetPlayNetMode(EPlayNetMode::PIE_ListenServer);
		PlaySettings->SetPlayNumberOfClients(State->PlayerCount);
		return true;
	}));
	Test->AddCommand(new FStartPIECommand(false));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, TEXT("title worlds"), [State]()
	{
		const TArray<UWorld*> Worlds = GetContractRunPIEWorlds();
		if (Worlds.Num() != State->PlayerCount || !IsValid(GetContractRunServerWorld()))
		{
			return false;
		}
		for (UWorld* World : Worlds)
		{
			if (!IsValid(GetContractRunLocalHeistPlayerController(World)))
			{
				return false;
			}
		}
		return true;
	}, 45.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, TEXT("create host session"), []()
	{
		UWorld* ServerWorld = GetContractRunServerWorld();
		UHeistGameInstance* GameInstance = IsValid(ServerWorld) ? Cast<UHeistGameInstance>(ServerWorld->GetGameInstance()) : nullptr;
		return IsValid(GameInstance) && GameInstance->RequestHostSession();
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, TEXT("lobby session and players"), [State]()
	{
		UWorld* ServerWorld = GetContractRunServerWorld();
		UHeistGameInstance* GameInstance = IsValid(ServerWorld) ? Cast<UHeistGameInstance>(ServerWorld->GetGameInstance()) : nullptr;
		return AreContractRunWorldsReady(State->PlayerCount, EHeistMatchPhase::Lobby, false) && IsValid(GameInstance) && GameInstance->IsHostingOnlineSession() &&
			GameInstance->HasActiveNamedOnlineSession();
	}, 60.0));

	for (int32 RunIndex = 1; RunIndex <= 2; ++RunIndex)
	{
		Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("select M01 for run %d"), RunIndex), []()
		{
			AHeistPlayerController* HostPlayerController = GetOwningPlayerControllerById(1);
			if (!IsValid(HostPlayerController))
			{
				return false;
			}
			HostPlayerController->RequestSetLobbyMapSelection(FName(TEXT("M01")));
			return true;
		}));
		Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("M01 selection for run %d"), RunIndex), []()
		{
			UWorld* ServerWorld = GetContractRunServerWorld();
			const UHeistGameInstance* GameInstance = IsValid(ServerWorld) ? Cast<UHeistGameInstance>(ServerWorld->GetGameInstance()) : nullptr;
			return IsValid(GameInstance) && GameInstance->GetSelectedMapId() == FName(TEXT("M01")) && !GameInstance->IsMapSelectionUpdatePending();
		}, 15.0));
		Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("start M01 run %d"), RunIndex), []()
		{
			UWorld* ServerWorld = GetContractRunServerWorld();
			UHeistGameInstance* GameInstance = IsValid(ServerWorld) ? Cast<UHeistGameInstance>(ServerWorld->GetGameInstance()) : nullptr;
			return IsValid(GameInstance) && GameInstance->RequestStartSelectedGameplayMap();
		}));
		AppendGameplayRunCommands(Test, State, RunIndex);
		if (RunIndex == 1)
		{
			Test->AddCommand(new FHeistContractRunActionCommand(Test, State, TEXT("return to lobby with session preserved"), []()
			{
				UWorld* ServerWorld = GetContractRunServerWorld();
				UHeistGameInstance* GameInstance = IsValid(ServerWorld) ? Cast<UHeistGameInstance>(ServerWorld->GetGameInstance()) : nullptr;
				return IsValid(GameInstance) && GameInstance->RequestReturnToLobby();
			}));
			Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, TEXT("clean lobby before run 2"), [State]()
			{
				UWorld* ServerWorld = GetContractRunServerWorld();
				const UHeistGameInstance* GameInstance = IsValid(ServerWorld) ? Cast<UHeistGameInstance>(ServerWorld->GetGameInstance()) : nullptr;
				return AreContractRunWorldsReady(State->PlayerCount, EHeistMatchPhase::Lobby, false) && IsLobbyStateClean(State->PlayerCount) && IsValid(GameInstance) &&
					GameInstance->HasActiveNamedOnlineSession();
			}, 75.0));
		}
	}

	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, TEXT("record two-run PASS evidence"), [Test, State]()
	{
		Test->AddInfo(FString::Printf(TEXT("W6-010 two-run gate: Players=%d Map=M01 RunsCompleted=2 LobbyReturn=true SecondRunCleanReset=true AuthorityFlows=true Result=PASS"),
			State->PlayerCount));
		return true;
	}));
	Test->AddCommand(new FEndPlayMapCommand());
	Test->AddCommand(new FWaitLatentCommand(1.0f));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, TEXT("restore editor play settings"), [State]()
	{
		if (!State->bCapturedPlaySettings)
		{
			return true;
		}
		ULevelEditorPlaySettings* PlaySettings = GetMutableDefault<ULevelEditorPlaySettings>();
		if (!IsValid(PlaySettings))
		{
			return false;
		}
		PlaySettings->SetRunUnderOneProcess(State->bOriginalRunUnderOneProcess);
		PlaySettings->SetPlayNetMode(State->OriginalNetMode);
		PlaySettings->SetPlayNumberOfClients(State->OriginalClientCount);
		return true;
	}, true));
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistSoloContractRunTwoPassTest, "ProjectMuseumHeist.ContractRun.M01.SoloTwoRuns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistSoloContractRunTwoPassTest::RunTest(const FString& Parameters)
{
	return HeistContractRunTest::EnqueueTwoRunContractScenario(this, 1);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistFourPlayerContractRunTwoPassTest, "ProjectMuseumHeist.ContractRun.M01.FourPlayerTwoRuns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistFourPlayerContractRunTwoPassTest::RunTest(const FString& Parameters)
{
	return HeistContractRunTest::EnqueueTwoRunContractScenario(this, 4);
}

#endif
