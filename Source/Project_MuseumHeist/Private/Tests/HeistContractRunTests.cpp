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
#include "Core/HeistTypes.h"
#include "Data/HeistGameBalanceDataAsset.h"
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
#include "UI/Widgets/HeistResultWidget.h"
#include "UI/ViewModels/HeistHUDViewModel.h"
#include "UObject/UObjectIterator.h"
#include "World/Actors/Escape/HeistVentActor.h"
#include "World/Actors/Loot/HeistObjectDisplayCaseActor.h"
#include "World/Actors/Loot/HeistLootActor.h"
#include "World/Actors/Loot/HeistPaintingDisplayCaseActor.h"
#include "World/Actors/Security/HeistLaserBarrierActor.h"
#include "World/Actors/Security/HeistSecurityCameraActor.h"
#include "World/Actors/Security/HeistSecurityHoldButtonActor.h"

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
	FName MapId = FName(TEXT("M01"));
	FHeistContractSnapshot FirstRunContract;
	TArray<FName> SelectedObjectCaseIds;
	FName SelectedHighValuePaintingCaseId = NAME_None;
	FName SelectedLootActorName = NAME_None;
	FName SelectedLootRowId = NAME_None;
	int32 SelectedLootValue = 0;
	int32 SecurityDetectionRevisionBaseline = 0;
	int32 SecurityIncidentCountBaseline = 0;
	int32 SecurityInvestigationCountBaseline = 0;
	float SecurityAlertMeterBaseline = 0.0f;
	TMap<TWeakObjectPtr<AHeistPlayerCharacter>, int32> CrewStatusFootstepBaselines;
};

bool AreContractRunWorldsReady(int32 PlayerCount, EHeistMatchPhase ExpectedPhase, bool bRequirePawn);

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

bool SetAllContractRunLobbyPlayersReady(UWorld* ServerWorld)
{
	AHeistGameState* GameState = IsValid(ServerWorld) ? ServerWorld->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(GameState))
	{
		return false;
	}
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (AHeistPlayerState* HeistPlayerState = Cast<AHeistPlayerState>(PlayerState))
		{
			HeistPlayerState->SetLobbyReady(true);
		}
	}
	return GameState->AreAllConnectedPlayersLobbyReady();
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

template <typename TActorType>
TActorType* FindOnlyActorOfType(UWorld* World)
{
	if (!IsValid(World))
	{
		return nullptr;
	}

	TActorType* Result = nullptr;
	for (TActorIterator<TActorType> It(World); It; ++It)
	{
		if (!IsValid(*It))
		{
			continue;
		}
		if (IsValid(Result))
		{
			return nullptr;
		}
		Result = *It;
	}
	return Result;
}

bool TeleportServerPlayerToLocation(const int32 PlayerId, const FVector& Destination)
{
	AHeistPlayerCharacter* Character = GetServerCharacterById(PlayerId);
	if (!IsValid(Character) || Destination.ContainsNaN())
	{
		return false;
	}
	if (UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}
	Character->SetActorLocation(Destination, false, nullptr, ETeleportType::TeleportPhysics);
	Character->ForceNetUpdate();
	return FVector::DistSquared(Character->GetActorLocation(), Destination) <= 1.0f;
}

bool BootstrapSandBoxSecurityContract()
{
	UWorld* ServerWorld = GetContractRunServerWorld();
	AHeistGameState* GameState = IsValid(ServerWorld) ? ServerWorld->GetGameState<AHeistGameState>() : nullptr;
	AHeistPaintingDisplayCaseActor* RequiredCase = FindPaintingCase(ServerWorld, FName(TEXT("Case_M01_Target")));
	AHeistPaintingDisplayCaseActor* ProtectedCase = FindPaintingCase(ServerWorld, FName(TEXT("Case_W8_SecurityTest_Laser")));
	AHeistLaserBarrierActor* Laser = FindOnlyActorOfType<AHeistLaserBarrierActor>(ServerWorld);
	if (!IsValid(GameState) || !IsValid(RequiredCase) || !IsValid(ProtectedCase) || !IsValid(Laser) ||
		Laser->GetProtectedPaintingCase() != ProtectedCase)
	{
		return false;
	}

	if (!GameState->IsContractInitialized() &&
		!GameState->InitializeContractSnapshot(FName(TEXT("Contract_MuseumSwap_01")), FName(TEXT("M01")),
			GameState->GetServerWorldTimeSeconds() + 1200.0f, 8008, 1, RequiredCase->GetTargetArtifactId(), FText::FromString(TEXT("테스트 필수 작품")),
			RequiredCase->GetDisplayCaseId(), 4000))
	{
		return false;
	}

	const FHeistContractSnapshot Contract = GameState->GetContractSnapshot();
	if (Contract.ContractStartPlayerCount != 1 || Contract.RequiredTargetCaseId != RequiredCase->GetDisplayCaseId() ||
		!ProtectedCase->SetContractExhibitActive(true))
	{
		return false;
	}

	Laser->ForceRestoreDefaultState();
	int32 EligibleGuardCount = 0;
	for (TActorIterator<AHeistGuardCharacter> It(ServerWorld); It; ++It)
	{
		AHeistGuardCharacter* Guard = *It;
		AHeistGuardAIController* GuardController = IsValid(Guard) ? Cast<AHeistGuardAIController>(Guard->GetController()) : nullptr;
		UHeistGuardStateComponent* GuardState = IsValid(Guard) ? Guard->GetGuardStateComponent() : nullptr;
		if (!IsValid(GuardController) || !IsValid(GuardState) || !Guard->IsDifficultyActive())
		{
			continue;
		}
		GuardController->SetAutomaticSightEnabled(false);
		GuardState->EnterPatrol();
		EligibleGuardCount += GuardController->CanAcceptSecurityInvestigation() ? 1 : 0;
	}
	return EligibleGuardCount > 0 && Laser->IsBarrierEnabled() && Laser->IsBeamActive();
}

bool IsSandBoxSecurityPreflightReady()
{
	UWorld* ServerWorld = GetContractRunServerWorld();
	if (!AreContractRunWorldsReady(2, EHeistMatchPhase::InGame, true) ||
		!IsValid(FindOnlyActorOfType<AHeistSecurityCameraActor>(ServerWorld)) || !IsValid(FindOnlyActorOfType<AHeistLaserBarrierActor>(ServerWorld)) ||
		!IsValid(FindOnlyActorOfType<AHeistSecurityHoldButtonActor>(ServerWorld)) ||
		!IsValid(FindPaintingCase(ServerWorld, FName(TEXT("Case_W8_SecurityTest_Laser")))))
	{
		return false;
	}

	for (TActorIterator<AHeistGuardCharacter> It(ServerWorld); It; ++It)
	{
		const AHeistGuardCharacter* Guard = *It;
		if (IsValid(Guard) && Guard->IsDifficultyActive() && IsValid(Cast<AHeistGuardAIController>(Guard->GetController())) &&
			IsValid(Guard->GetGuardStateComponent()))
		{
			return true;
		}
	}
	return false;
}

bool IsSandBoxSecurityRuntimeReplicated()
{
	const TArray<UWorld*> Worlds = GetContractRunPIEWorlds();
	if (Worlds.Num() != 2)
	{
		return false;
	}

	for (UWorld* World : Worlds)
	{
		const AHeistGameState* GameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
		const AHeistSecurityCameraActor* Camera = FindOnlyActorOfType<AHeistSecurityCameraActor>(World);
		const AHeistLaserBarrierActor* Laser = FindOnlyActorOfType<AHeistLaserBarrierActor>(World);
		const AHeistSecurityHoldButtonActor* Button = FindOnlyActorOfType<AHeistSecurityHoldButtonActor>(World);
		const AHeistPaintingDisplayCaseActor* ProtectedCase = FindPaintingCase(World, FName(TEXT("Case_W8_SecurityTest_Laser")));
		if (!IsValid(GameState) || !GameState->IsContractInitialized() || GameState->GetContractSnapshot().ContractStartPlayerCount != 1 ||
			!IsValid(Camera) || !Camera->IsCameraEnabled() || !IsValid(Laser) || !Laser->IsBarrierEnabled() || !Laser->IsBeamActive() ||
			!IsValid(Button) || !IsValid(ProtectedCase) || !ProtectedCase->IsContractExhibitActive() ||
			Laser->GetProtectedPaintingCase() != ProtectedCase || Button->GetLinkedLaserBarrier() != Laser)
		{
			return false;
		}
	}
	return true;
}

bool IsCameraDetectionReplicated(const int32 ExpectedRevision, const int32 ExpectedPlayerId)
{
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		const AHeistGameState* GameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
		const AHeistSecurityCameraActor* Camera = FindOnlyActorOfType<AHeistSecurityCameraActor>(World);
		const AHeistPlayerState* DetectedPlayerState = IsValid(Camera) ? Camera->GetLastDetectedPlayerState() : nullptr;
		if (!IsValid(GameState) || GameState->GetAlertLevel() < EHeistAlertLevel::Suspicious || !IsValid(Camera) ||
			Camera->GetDetectionRevision() != ExpectedRevision || !IsValid(DetectedPlayerState) || DetectedPlayerState->HeistPlayerId != ExpectedPlayerId)
		{
			return false;
		}
	}
	return true;
}

bool IsSecurityHoldStateReplicated(const bool bExpectedHolding, const bool bExpectedBypass, const bool bExpectedBeamActive)
{
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		const AHeistSecurityHoldButtonActor* Button = FindOnlyActorOfType<AHeistSecurityHoldButtonActor>(World);
		const AHeistLaserBarrierActor* Laser = FindOnlyActorOfType<AHeistLaserBarrierActor>(World);
		const AHeistPlayerState* Holder = IsValid(Button) ? Button->GetHolderPlayerState() : nullptr;
		if (!IsValid(Button) || !IsValid(Laser) || Button->IsHoldActive() != bExpectedHolding || Button->IsBypassActive() != bExpectedBypass ||
			Laser->IsBeamActive() != bExpectedBeamActive || (bExpectedHolding && (!IsValid(Holder) || Holder->HeistPlayerId != 1)) ||
			(!bExpectedHolding && IsValid(Holder)))
		{
			return false;
		}
	}
	return true;
}

bool IsLaserRearmingReplicated()
{
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		const AHeistSecurityHoldButtonActor* Button = FindOnlyActorOfType<AHeistSecurityHoldButtonActor>(World);
		const AHeistLaserBarrierActor* Laser = FindOnlyActorOfType<AHeistLaserBarrierActor>(World);
		if (!IsValid(Button) || Button->IsHoldActive() || Button->IsBypassActive() || !IsValid(Laser) || Laser->IsBeamActive() || !Laser->IsRearming())
		{
			return false;
		}
	}
	return true;
}

bool PrepareReleaseSecurityInteraction(const TSharedRef<FHeistContractRunAutomationState>& State)
{
	UWorld* ServerWorld = GetContractRunServerWorld();
	const AHeistSecurityCameraActor* Camera = FindOnlyActorOfType<AHeistSecurityCameraActor>(ServerWorld);
	const AHeistGameMode* GameMode = IsValid(ServerWorld) ? ServerWorld->GetAuthGameMode<AHeistGameMode>() : nullptr;
	if (!IsValid(Camera) || !IsValid(GameMode))
	{
		return false;
	}

	int32 ActiveGuardCount = 0;
	for (TActorIterator<AHeistGuardCharacter> It(ServerWorld); It; ++It)
	{
		AHeistGuardCharacter* Guard = *It;
		AHeistGuardAIController* GuardController = IsValid(Guard) ? Cast<AHeistGuardAIController>(Guard->GetController()) : nullptr;
		UHeistGuardStateComponent* GuardState = IsValid(Guard) ? Guard->GetGuardStateComponent() : nullptr;
		if (!IsValid(GuardController) || !IsValid(GuardState) || !Guard->IsDifficultyActive())
		{
			continue;
		}
		GuardController->SetAutomaticSightEnabled(false);
		GuardState->EnterPatrol();
		++ActiveGuardCount;
	}

	State->SecurityDetectionRevisionBaseline = Camera->GetDetectionRevision();
	State->SecurityIncidentCountBaseline = GameMode->GetProcessedSecurityIncidentCount();
	State->SecurityInvestigationCountBaseline = GameMode->GetProcessedGuardInvestigationCount();
	const AHeistGameState* GameState = ServerWorld->GetGameState<AHeistGameState>();
	State->SecurityAlertMeterBaseline = IsValid(GameState) ? GameState->GetAlertMeterValue() : 0.0f;
	return ActiveGuardCount > 0;
}

bool IsReleaseSecurityDetectionReady(const TSharedRef<FHeistContractRunAutomationState>& State)
{
	UWorld* ServerWorld = GetContractRunServerWorld();
	const AHeistGameMode* GameMode = IsValid(ServerWorld) ? ServerWorld->GetAuthGameMode<AHeistGameMode>() : nullptr;
	const AHeistGameState* GameState = IsValid(ServerWorld) ? ServerWorld->GetGameState<AHeistGameState>() : nullptr;
	return IsValid(GameMode) && GameMode->GetProcessedSecurityIncidentCount() == State->SecurityIncidentCountBaseline + 1 &&
		GameMode->GetProcessedGuardInvestigationCount() == State->SecurityInvestigationCountBaseline + 1 &&
		IsValid(GameState) && FMath::IsNearlyEqual(GameState->GetAlertMeterValue(), State->SecurityAlertMeterBaseline + 0.5f) &&
		IsCameraDetectionReplicated(State->SecurityDetectionRevisionBaseline + 1, 2);
}

bool IsReleaseSecurityIncidentOneShot(const TSharedRef<FHeistContractRunAutomationState>& State)
{
	UWorld* ServerWorld = GetContractRunServerWorld();
	const AHeistGameMode* GameMode = IsValid(ServerWorld) ? ServerWorld->GetAuthGameMode<AHeistGameMode>() : nullptr;
	const AHeistGameState* GameState = IsValid(ServerWorld) ? ServerWorld->GetGameState<AHeistGameState>() : nullptr;
	const AHeistSecurityCameraActor* Camera = FindOnlyActorOfType<AHeistSecurityCameraActor>(ServerWorld);
	return IsValid(GameMode) && IsValid(Camera) && Camera->GetDetectionRevision() == State->SecurityDetectionRevisionBaseline + 1 &&
		GameMode->GetProcessedSecurityIncidentCount() == State->SecurityIncidentCountBaseline + 1 &&
		GameMode->GetProcessedGuardInvestigationCount() == State->SecurityInvestigationCountBaseline + 1 && IsValid(GameState) &&
		FMath::IsNearlyEqual(GameState->GetAlertMeterValue(), State->SecurityAlertMeterBaseline + 0.5f);
}

bool ResetReleaseSecurityAfterCamera()
{
	UWorld* ServerWorld = GetContractRunServerWorld();
	AHeistGameState* GameState = IsValid(ServerWorld) ? ServerWorld->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(GameState) || !GameState->SetAlertSnapshot(0.0f, EHeistAlertLevel::Quiet, FName(TEXT("W8ReleaseCameraReset"))))
	{
		return false;
	}

	bool bFoundActiveGuard = false;
	for (TActorIterator<AHeistGuardCharacter> It(ServerWorld); It; ++It)
	{
		AHeistGuardCharacter* Guard = *It;
		UHeistGuardStateComponent* GuardState = IsValid(Guard) && Guard->IsDifficultyActive() ? Guard->GetGuardStateComponent() : nullptr;
		if (IsValid(GuardState))
		{
			GuardState->EnterPatrol();
			bFoundActiveGuard = true;
		}
	}
	return bFoundActiveGuard;
}

bool IsOwningLootActorRelevant(const int32 PlayerId, const FName ActorName)
{
	const AHeistPlayerController* PlayerController = GetOwningPlayerControllerById(PlayerId);
	return IsValid(PlayerController) && IsValid(FindLootActor(PlayerController->GetWorld(), ActorName));
}

bool IsOwningPaintingCaseRelevant(const int32 PlayerId, const FName CaseId)
{
	const AHeistPlayerController* PlayerController = GetOwningPlayerControllerById(PlayerId);
	return IsValid(PlayerController) && IsValid(FindPaintingCase(PlayerController->GetWorld(), CaseId));
}

bool IsOwningObjectCaseRelevant(const int32 PlayerId, const FName CaseId)
{
	const AHeistPlayerController* PlayerController = GetOwningPlayerControllerById(PlayerId);
	return IsValid(PlayerController) && IsValid(FindObjectCase(PlayerController->GetWorld(), CaseId));
}

bool TeleportServerPlayerIntoInteraction(const int32 PlayerId, AActor* TargetActor)
{
	AHeistPlayerCharacter* Character = GetServerCharacterById(PlayerId);
	if (!IsValid(Character) || !IsValid(TargetActor))
	{
		return false;
	}
	if (UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}
	const USphereComponent* InteractionSphere = TargetActor->FindComponentByClass<USphereComponent>();
	const FVector Destination = IsValid(InteractionSphere) ? InteractionSphere->GetComponentLocation() : TargetActor->GetActorLocation();
	Character->SetActorLocation(Destination, false, nullptr, ETeleportType::TeleportPhysics);
	if (UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}
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

		const AHeistGameState* GameState = World->GetGameState<AHeistGameState>();
		const AHeistPlayerCharacter* Character = FindHeistCharacterById(World, PlayerId);
		if (!IsValid(Character) || (IsValid(GameState) && GameState->GetMatchPhase() == EHeistMatchPhase::InGame &&
			Character->GetAppliedCrewStatusForDebug() != ExpectedStatus))
		{
			return false;
		}

		const AHeistPlayerController* LocalController = GetContractRunLocalHeistPlayerController(World);
		const AHeistHUD* HUD = IsValid(LocalController) ? LocalController->GetHUD<AHeistHUD>() : nullptr;
		const UHeistHUDViewModel* HUDViewModel = IsValid(HUD) ? HUD->GetHUDViewModel() : nullptr;
		const FHeistCrewStatusEntry* CrewEntry = IsValid(HUDViewModel)
			? HUDViewModel->GetCrewStatusEntries().FindByPredicate([PlayerId](const FHeistCrewStatusEntry& Entry) { return Entry.PlayerId == PlayerId; })
			: nullptr;
		if (!CrewEntry || CrewEntry->Status != ExpectedStatus || !CrewEntry->PlayerColor.Equals(PlayerState->PlayerColor) ||
			CrewEntry->PlayerName.ToString() != PlayerState->GetHeistDisplayName().ToString())
		{
			return false;
		}

		const AHeistPlayerState* LocalPlayerState = IsValid(LocalController) ? LocalController->GetPlayerState<AHeistPlayerState>() : nullptr;
		if (IsValid(LocalPlayerState) && LocalPlayerState->HeistPlayerId != PlayerId)
		{
			TInlineComponentArray<UWidgetComponent*> WidgetComponents(Character);
			const bool bRemoteNameplateReady = WidgetComponents.ContainsByPredicate([PlayerState](const UWidgetComponent* Component)
			{
				const UHeistNameplateWidget* Widget = IsValid(Component) ? Cast<UHeistNameplateWidget>(Component->GetUserWidgetObject()) : nullptr;
				return IsValid(Widget) && Widget->GetPresentedPlayerState() == PlayerState && Widget->IsPresentationContractSatisfied() &&
					Widget->GetClass()->GetPathName().Contains(TEXT("/Game/Blueprints/UI/HUD/WBP_HeistNameplate."));
			});
			if (!bRemoteNameplateReady)
			{
				return false;
			}
		}
	}
	return true;
}

bool CaptureCrewStatusFootstepBaseline(const TSharedRef<FHeistContractRunAutomationState>& State, const int32 PlayerId)
{
	State->CrewStatusFootstepBaselines.Reset();
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		AHeistPlayerCharacter* Character = FindHeistCharacterById(World, PlayerId);
		if (!IsValid(Character))
		{
			State->CrewStatusFootstepBaselines.Reset();
			return false;
		}
		State->CrewStatusFootstepBaselines.Add(TWeakObjectPtr<AHeistPlayerCharacter>(Character), Character->GetCrewStatusFootstepPlayCountForDebug());
	}
	return !State->CrewStatusFootstepBaselines.IsEmpty();
}

bool IsCrewStatusFootstepPresentationReady(const TSharedRef<FHeistContractRunAutomationState>& State, const int32 PlayerId, const EHeistCrewStatus ExpectedStatus)
{
	if (!IsCrewStatusReplicated(PlayerId, ExpectedStatus))
	{
		return false;
	}

	for (UWorld* World : GetContractRunPIEWorlds())
	{
		AHeistPlayerCharacter* Character = FindHeistCharacterById(World, PlayerId);
		const int32* Baseline = IsValid(Character) ? State->CrewStatusFootstepBaselines.Find(TWeakObjectPtr<AHeistPlayerCharacter>(Character)) : nullptr;
		if (!IsValid(Character) || !Character->AreCrewStatusAudioAssetsAssignedForDebug() ||
			Baseline == nullptr || Character->GetCrewStatusFootstepPlayCountForDebug() < *Baseline + 1)
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
			if (!IsValid(PlayerState) || !IsValid(NameplateComponent) || !IsValid(NameplateWidget) || NameplateWidget->GetPresentedPlayerState() != PlayerState ||
				!NameplateWidget->IsPresentationContractSatisfied() ||
				!NameplateWidget->GetClass()->GetPathName().Contains(TEXT("/Game/Blueprints/UI/HUD/WBP_HeistNameplate.")))
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

bool ToggleWeek7MapForPlayer(const int32 PlayerId, const bool bShouldOpen)
{
	AHeistPlayerController* PlayerController = GetOwningPlayerControllerById(PlayerId);
	AHeistHUD* HUD = IsValid(PlayerController) ? PlayerController->GetHUD<AHeistHUD>() : nullptr;
	return IsValid(PlayerController) && IsValid(HUD) && HUD->IsFloorPlanMapVisible() != bShouldOpen && PlayerController->ToggleFloorPlanMap();
}

bool IsWeek7MapContractReadyForPlayer(const int32 PlayerId, const bool bShouldBeOpen)
{
	const AHeistPlayerController* PlayerController = GetOwningPlayerControllerById(PlayerId);
	const AHeistHUD* HUD = IsValid(PlayerController) ? PlayerController->GetHUD<AHeistHUD>() : nullptr;
	const EHeistInputMode ExpectedMode = bShouldBeOpen ? EHeistInputMode::Map : EHeistInputMode::Gameplay;
	return IsValid(PlayerController) && IsValid(HUD) && HUD->IsFloorPlanMapVisible() == bShouldBeOpen && PlayerController->GetLocalInputMode() == ExpectedMode &&
		PlayerController->GetActiveHeistInputMappingContextCount() == 1 && (!bShouldBeOpen || PlayerController->IsLocalInputModeContractSatisfied());
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
		if (!IsValid(Guard) || !Guard->IsDifficultyActive())
		{
			continue;
		}
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
	const AHeistPlayerCharacter* OwningCharacter = GetOwningCharacterById(TargetPlayerId);
	const AHeistHUD* HUD = IsValid(OwningController) ? OwningController->GetHUD<AHeistHUD>() : nullptr;
	const UHeistHUDWidget* MainHUDWidget = IsValid(HUD) ? HUD->GetMainHUDWidget() : nullptr;
	return IsValid(OwningController) && IsValid(OwningCharacter) && IsValid(HUD) && IsValid(MainHUDWidget) && !HUD->IsFloorPlanMapVisible() &&
		OwningCharacter->IsLocalStunPostProcessEnabledForDebug() && OwningCharacter->IsStunSoundMixPushedForDebug() &&
		MainHUDWidget->IsLocalCrewStatusPresentationContractSatisfied() && OwningController->GetLocalInputMode() == EHeistInputMode::Gameplay &&
		OwningController->GetActiveHeistInputMappingContextCount() == 1 && !OwningController->bShowMouseCursor && OwningController->IsMoveInputIgnored() &&
		OwningController->IsLookInputIgnored();
}

FString DescribeWeek7StunPresentation(const int32 TargetPlayerId)
{
	TArray<FString> WorldStates;
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		const AHeistPlayerState* PlayerState = FindPlayerStateById(World, TargetPlayerId);
		const AHeistPlayerCharacter* Character = FindHeistCharacterById(World, TargetPlayerId);
		const AHeistPlayerController* LocalController = GetContractRunLocalHeistPlayerController(World);
		const AHeistHUD* HUD = IsValid(LocalController) ? LocalController->GetHUD<AHeistHUD>() : nullptr;
		const UHeistHUDWidget* MainHUDWidget = IsValid(HUD) ? HUD->GetMainHUDWidget() : nullptr;
		WorldStates.Add(FString::Printf(
			TEXT("World=%s NetMode=%d PS=%s Character=%s Applied=%s Local=%s PP=%s Mix=%s LocalPlayer=%d Input=%d Mappings=%d Map=%s Cursor=%s Move=%s Look=%s HUD=%s HUDContract=%s"),
			*GetNameSafe(World), IsValid(World) ? static_cast<int32>(World->GetNetMode()) : INDEX_NONE,
			IsValid(PlayerState) ? *UEnum::GetValueAsString(PlayerState->GetCrewStatus()) : TEXT("Missing"), *GetNameSafe(Character),
			IsValid(Character) ? *UEnum::GetValueAsString(Character->GetAppliedCrewStatusForDebug()) : TEXT("Missing"),
			IsValid(Character) && Character->IsLocallyControlled() ? TEXT("true") : TEXT("false"),
			IsValid(Character) && Character->IsLocalStunPostProcessEnabledForDebug() ? TEXT("true") : TEXT("false"),
			IsValid(Character) && Character->IsStunSoundMixPushedForDebug() ? TEXT("true") : TEXT("false"),
			IsValid(LocalController) && IsValid(LocalController->GetPlayerState<AHeistPlayerState>()) ? LocalController->GetPlayerState<AHeistPlayerState>()->HeistPlayerId : INDEX_NONE,
			IsValid(LocalController) ? static_cast<int32>(LocalController->GetLocalInputMode()) : INDEX_NONE,
			IsValid(LocalController) ? LocalController->GetActiveHeistInputMappingContextCount() : INDEX_NONE,
			IsValid(HUD) && HUD->IsFloorPlanMapVisible() ? TEXT("true") : TEXT("false"),
			IsValid(LocalController) && LocalController->bShowMouseCursor ? TEXT("true") : TEXT("false"),
			IsValid(LocalController) && LocalController->IsMoveInputIgnored() ? TEXT("true") : TEXT("false"),
			IsValid(LocalController) && LocalController->IsLookInputIgnored() ? TEXT("true") : TEXT("false"), *GetNameSafe(MainHUDWidget),
			IsValid(MainHUDWidget) && MainHUDWidget->IsLocalCrewStatusPresentationContractSatisfied() ? TEXT("true") : TEXT("false")));
		if (IsValid(MainHUDWidget))
		{
			MainHUDWidget->DebugDumpFirstPersonHUDState();
		}
	}
	return FString::Printf(TEXT("W7 Stun presentation diagnostic: Target=%d Replicated=%s %s"), TargetPlayerId,
		IsCrewStatusReplicated(TargetPlayerId, EHeistCrewStatus::Stunned) ? TEXT("true") : TEXT("false"), *FString::Join(WorldStates, TEXT(" | ")));
}

bool IsWeek7ArrestReplicated(const int32 TargetPlayerId)
{
	if (!IsCrewStatusReplicated(TargetPlayerId, EHeistCrewStatus::Arrested))
	{
		return false;
	}
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		const AHeistPlayerState* PlayerState = FindPlayerStateById(World, TargetPlayerId);
		if (!IsValid(PlayerState) || !PlayerState->IsArrested() || PlayerState->GetCrewStatus() != EHeistCrewStatus::Arrested)
		{
			return false;
		}
	}

	const AHeistPlayerController* OwningController = GetOwningPlayerControllerById(TargetPlayerId);
	const AHeistPlayerCharacter* OwningCharacter = GetOwningCharacterById(TargetPlayerId);
	const AHeistHUD* HUD = IsValid(OwningController) ? OwningController->GetHUD<AHeistHUD>() : nullptr;
	const UHeistHUDWidget* MainHUDWidget = IsValid(HUD) ? HUD->GetMainHUDWidget() : nullptr;
	return IsValid(OwningCharacter) && IsValid(MainHUDWidget) && !OwningCharacter->IsLocalStunPostProcessEnabledForDebug() &&
		!OwningCharacter->IsStunSoundMixPushedForDebug() && MainHUDWidget->IsLocalCrewStatusPresentationContractSatisfied() &&
		MainHUDWidget->GetArrestAudioPlayCountForDebug() >= 1;
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
	const AHeistPlayerCharacter* OwningCharacter = GetOwningCharacterById(TargetPlayerId);
	const AHeistHUD* HUD = IsValid(OwningController) ? OwningController->GetHUD<AHeistHUD>() : nullptr;
	const UHeistHUDWidget* MainHUDWidget = IsValid(HUD) ? HUD->GetMainHUDWidget() : nullptr;
	return IsValid(OwningController) && IsValid(OwningCharacter) && IsValid(MainHUDWidget) &&
		!OwningCharacter->IsLocalStunPostProcessEnabledForDebug() && !OwningCharacter->IsStunSoundMixPushedForDebug() &&
		MainHUDWidget->IsLocalCrewStatusPresentationContractSatisfied() && MainHUDWidget->GetRescueAudioPlayCountForDebug() >= 1 &&
		!OwningController->IsMoveInputIgnored() && !OwningController->IsLookInputIgnored();
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
	if (!ServerContract.IsInitialized() || ServerContract.MapId != State->MapId || ServerContract.Outcome != EHeistContractOutcome::None ||
		ServerContract.CarriedValue != 0 || ServerContract.SecuredValue != 0 || ServerContract.bRequiredTargetSecured)
	{
		return false;
	}

	TArray<FName> ObjectCaseIds;
	int32 RequiredTargetCaseCount = 0;
	int32 ActiveObjectCaseCount = 0;
	for (TActorIterator<AHeistPaintingDisplayCaseActor> It(ServerWorld); It; ++It)
	{
		if (IsValid(*It))
		{
			RequiredTargetCaseCount += It->GetDisplayCaseId() == ServerContract.RequiredTargetCaseId ? 1 : 0;
		}
	}
	for (TActorIterator<AHeistObjectDisplayCaseActor> It(ServerWorld); It; ++It)
	{
		if (IsValid(*It))
		{
			ObjectCaseIds.Add(It->GetObjectCaseId());
			ActiveObjectCaseCount += It->IsContractExhibitActive() ? 1 : 0;
			RequiredTargetCaseCount += It->GetObjectCaseId() == ServerContract.RequiredTargetCaseId ? 1 : 0;
			if (It->GetAssemblyState() != EHeistObjectAssemblyState::Secured || It->IsSessionLocked() || It->HasCommittedAssemblyResult() ||
				It->HasReplicaPreview() || It->IsRegisteredForInspection())
			{
				return false;
			}
		}
	}
	ObjectCaseIds.Sort([](const FName Left, const FName Right) { return Left.LexicalLess(Right); });
	if (HeistReleaseFeatures::IsObjectAssemblyRuntimeEnabled() || RequiredTargetCaseCount != 1 || ActiveObjectCaseCount != 0 ||
		ServerContract.ContractStartPlayerCount != State->PlayerCount)
	{
		return false;
	}

	AHeistGameMode* GameMode = ServerWorld->GetAuthGameMode<AHeistGameMode>();
	FHeistPlayerCountDifficultyBaseline DifficultyBaseline;
	if (!IsValid(GameMode) || !GameMode->TryGetPlayerCountDifficultyBaseline(State->PlayerCount, DifficultyBaseline) ||
		!GameMode->IsPlayerCountGuardScalingApplied() || GameMode->GetDifficultyExpectedGuardCount() != GameMode->GetDifficultyActiveGuardCount() ||
		GameMode->GetDifficultyAppliedPlayerCount() != State->PlayerCount ||
		!FMath::IsNearlyEqual(GameMode->GetDifficultyAppliedGuardCountMultiplier(), DifficultyBaseline.GuardCountMultiplier) ||
		!FMath::IsNearlyEqual(GameMode->GetDifficultyAppliedDetectionMultiplier(), DifficultyBaseline.DetectionMultiplier) ||
		!FMath::IsNearlyEqual(GameMode->GetDifficultyAppliedInspectionDurationMultiplier(), DifficultyBaseline.InspectionDurationMultiplier) ||
		GameMode->GetDifficultyExpectedGuardCount() !=
			AHeistGameMode::CalculateDifficultyGuardCount(GameMode->GetDifficultyAuthoredGuardCount(), DifficultyBaseline.GuardCountMultiplier))
	{
		return false;
	}
	int32 AuthoredGuardAliveCount = 0;
	int32 SupplementalGuardAliveCount = 0;
	float FirstDetectionGrace = -1.0f;
	float FirstInspectionDuration = -1.0f;
	for (TActorIterator<AHeistGuardCharacter> It(ServerWorld); It; ++It)
	{
		const AHeistGuardCharacter* Guard = *It;
		if (!IsValid(Guard))
		{
			continue;
		}
		if (Guard->IsDifficultySupplementalGuard())
		{
			++SupplementalGuardAliveCount;
		}
		else
		{
			++AuthoredGuardAliveCount;
		}
		if (!Guard->IsDifficultyActive())
		{
			continue;
		}
		const AHeistGuardAIController* GuardController = Cast<AHeistGuardAIController>(Guard->GetController());
		const float ExpectedDetectionGrace = FMath::Max(0.0f, Guard->GetGuardProfile().DetectionGrace / FMath::Max(0.01f, DifficultyBaseline.DetectionMultiplier));
		const float ExpectedInspectionDuration = FMath::Max(0.1f, 2.0f * DifficultyBaseline.InspectionDurationMultiplier);
		if (!IsValid(GuardController) || !FMath::IsNearlyEqual(GuardController->GetDetectionGraceDuration(), ExpectedDetectionGrace, 0.001f) ||
			!FMath::IsNearlyEqual(GuardController->GetInspectionCastDuration(), ExpectedInspectionDuration, 0.001f))
		{
			return false;
		}
		if (FirstDetectionGrace < 0.0f)
		{
			FirstDetectionGrace = GuardController->GetDetectionGraceDuration();
			FirstInspectionDuration = GuardController->GetInspectionCastDuration();
		}
	}
	if (AuthoredGuardAliveCount != GameMode->GetDifficultyAuthoredGuardCount())
	{
		return false;
	}
	AHeistPaintingDisplayCaseActor* SelectedHighValuePaintingCase = nullptr;
	for (TActorIterator<AHeistPaintingDisplayCaseActor> It(ServerWorld); It; ++It)
	{
		AHeistPaintingDisplayCaseActor* Candidate = *It;
		if (!IsValid(Candidate) || !Candidate->IsContractExhibitActive() || Candidate->GetDisplayCaseId() == ServerContract.RequiredTargetCaseId)
		{
			continue;
		}
		FHeistArtifactDataRow ArtifactDefinition;
		if (!GameMode->TryGetArtifactDefinition(Candidate->GetTargetArtifactId(), ArtifactDefinition) || ArtifactDefinition.ItemGrade != EHeistLootGrade::FourStar)
		{
			continue;
		}
		if (IsValid(SelectedHighValuePaintingCase))
		{
			return false;
		}
		SelectedHighValuePaintingCase = Candidate;
	}
	AHeistLaserBarrierActor* ReleaseLaser = nullptr;
	AHeistSecurityHoldButtonActor* ReleaseHoldButton = nullptr;
	int32 ReleaseLaserCount = 0;
	int32 ReleaseHoldButtonCount = 0;
	int32 ReleaseCameraCount = 0;
	for (TActorIterator<AHeistLaserBarrierActor> It(ServerWorld); It; ++It)
	{
		ReleaseLaser = *It;
		++ReleaseLaserCount;
	}
	for (TActorIterator<AHeistSecurityHoldButtonActor> It(ServerWorld); It; ++It)
	{
		ReleaseHoldButton = *It;
		++ReleaseHoldButtonCount;
	}
	for (TActorIterator<AHeistSecurityCameraActor> It(ServerWorld); It; ++It)
	{
		++ReleaseCameraCount;
	}
	if (!IsValid(SelectedHighValuePaintingCase) || ReleaseLaserCount != 1 || ReleaseHoldButtonCount != 1 || ReleaseCameraCount != 1 ||
		!IsValid(ReleaseLaser) || !IsValid(ReleaseHoldButton) || ReleaseLaser->GetProtectedPaintingCase() != SelectedHighValuePaintingCase ||
		ReleaseHoldButton->GetLinkedLaserBarrier() != ReleaseLaser)
	{
		return false;
	}
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
		if (!IsValid(GameState) || !(GameState->GetContractSnapshot() == ServerContract))
		{
			return false;
		}
	}

	State->SelectedObjectCaseIds = MoveTemp(ObjectCaseIds);
	State->SelectedHighValuePaintingCaseId = SelectedHighValuePaintingCase->GetDisplayCaseId();
	State->SelectedLootActorName = SelectedLootActor->GetFName();
	State->SelectedLootRowId = SelectedLootActor->GetLootRowId();
	State->SelectedLootValue = SelectedLootDefinition.ScoreValue;
	if (RunIndex == 1)
	{
		State->FirstRunContract = ServerContract;
	}
	Test->AddInfo(FString::Printf(TEXT("W6-010 preflight: Run=%d Players=%d Map=%s Seed=%d Target=%s DeferredObjectCases=%d ActiveObjectCases=0 Quota=%d SpawnSnapshot=PASS AssignmentSnapshotReplication=PASS ContributionReset=PASS InputLock=0"),
		RunIndex, State->PlayerCount, *ServerContract.MapId.ToString(), ServerContract.AssignmentSeed, *ServerContract.RequiredTargetCaseId.ToString(),
		State->SelectedObjectCaseIds.Num(), ServerContract.LootValueQuota));
	Test->AddInfo(FString::Printf(TEXT("W6-010 loose-loot fixture: Run=%d Actor=%s Row=%s Value=%d Grid=%dx%d Replication=PASS"), RunIndex,
		*State->SelectedLootActorName.ToString(), *State->SelectedLootRowId.ToString(), State->SelectedLootValue, SelectedItemDefinition.GridSize.X, SelectedItemDefinition.GridSize.Y));
	Test->AddInfo(FString::Printf(TEXT("W8 release security fixture: Run=%d Map=%s FourStarCase=%s CCTV=1 Laser=1 HoldButton=1 Links=PASS Active=PASS"), RunIndex,
		*State->MapId.ToString(), *State->SelectedHighValuePaintingCaseId.ToString()));
	Test->AddInfo(FString::Printf(
		TEXT("W7-001 guard balance: Run=%d Players=%d AuthoredAlive=%d SupplementalAlive=%d GuardMultiplier=%.2f DetectionMultiplier=%.2f InspectionMultiplier=%.2f "
			 "ExpectedActive=%d ActualActive=%d DetectionGrace=%.3f InspectionDuration=%.3f AuthorityRuntimeProfile=PASS"),
		RunIndex, State->PlayerCount, AuthoredGuardAliveCount, SupplementalGuardAliveCount, DifficultyBaseline.GuardCountMultiplier,
		DifficultyBaseline.DetectionMultiplier, DifficultyBaseline.InspectionDurationMultiplier, GameMode->GetDifficultyExpectedGuardCount(),
		GameMode->GetDifficultyActiveGuardCount(), FirstDetectionGrace, FirstInspectionDuration));
	return true;
}

bool IsSurfaceSessionReady(const int32 PlayerId, const FName CaseId, const EHeistCrewStatus ExpectedCrewStatus = EHeistCrewStatus::Forging)
{
	const AHeistPlayerCharacter* ServerCharacter = GetServerCharacterById(PlayerId);
	const UHeistForgeryComponent* ServerForgery = IsValid(ServerCharacter) ? ServerCharacter->GetForgeryComponent() : nullptr;
	const AHeistPlayerCharacter* OwningCharacter = GetOwningCharacterById(PlayerId);
	const UHeistForgeryComponent* OwningForgery = IsValid(OwningCharacter) ? OwningCharacter->GetForgeryComponent() : nullptr;
	return IsValid(ServerForgery) && ServerForgery->IsSessionActive() && ServerForgery->GetActiveDisplayCase() == FindPaintingCase(GetContractRunServerWorld(), CaseId) &&
		IsValid(OwningForgery) && OwningForgery->IsSessionActive() && OwningForgery->GetSessionRevision() == ServerForgery->GetSessionRevision() &&
		OwningForgery->GetActiveTemplateId() == ServerForgery->GetActiveTemplateId() && IsCrewStatusReplicated(PlayerId, ExpectedCrewStatus);
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
	const AHeistLootActor* ServerLootActor = FindLootActor(GetContractRunServerWorld(), State->SelectedLootActorName);
	if (!IsValid(ServerLootActor) || ServerLootActor->IsLootAvailable())
	{
		return false;
	}
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		const AHeistLootActor* LootActor = FindLootActor(World, State->SelectedLootActorName);
		if (IsValid(LootActor) && LootActor->IsLootAvailable())
		{
			return false;
		}
	}
	return true;
}

bool ValidateDeferredObjectAssemblyAttempt(const int32 PlayerId, const FName CaseId)
{
	AHeistPlayerCharacter* Character = GetServerCharacterById(PlayerId);
	AHeistPlayerState* PlayerState = IsValid(Character) ? Character->GetPlayerState<AHeistPlayerState>() : nullptr;
	UHeistObjectAssemblyComponent* Assembly = IsValid(Character) ? Character->GetObjectAssemblyComponent() : nullptr;
	AHeistObjectDisplayCaseActor* DisplayCase = FindObjectCase(GetContractRunServerWorld(), CaseId);
	if (HeistReleaseFeatures::IsObjectAssemblyRuntimeEnabled() || !IsValid(Character) || !IsValid(PlayerState) || !IsValid(Assembly) || !IsValid(DisplayCase) ||
		DisplayCase->IsContractExhibitActive() || DisplayCase->GetAssemblyState() != EHeistObjectAssemblyState::Secured || DisplayCase->IsSessionLocked() ||
		DisplayCase->HasCommittedAssemblyResult() || DisplayCase->HasReplicaPreview() || Assembly->IsSessionActive() || Assembly->HasPendingReplicaReview())
	{
		return false;
	}

	const EHeistObjectAssemblyState InitialCaseState = DisplayCase->GetAssemblyState();
	const int32 InitialCaseRevision = DisplayCase->GetAssemblyRevision();
	const int32 InitialSessionRevision = Assembly->GetSessionRevision();
	const int32 InitialPayloadRevision = Assembly->GetPayloadValidationRevision();
	const int32 InitialScoreRevision = Assembly->GetScoreRevision();
	const FName InitialCleanupReason = Assembly->GetLastCleanupReason();

	const bool bCaseSessionRejected = !DisplayCase->TryBeginSession(PlayerState);
	const bool bComponentBeginRejected = !Assembly->TryBeginAssemblySession(DisplayCase, 30.0f);
	const bool bActivationRejected = !DisplayCase->SetContractExhibitActive(true);
	const bool bTransitionRejected = !DisplayCase->TryTransitionToAssemblyState(EHeistObjectAssemblyState::Observed);
	const bool bOriginalTakeRejected = !DisplayCase->TryTakeOriginal(PlayerState);

	return bCaseSessionRejected && bComponentBeginRejected && bActivationRejected && bTransitionRejected && bOriginalTakeRejected &&
		!Assembly->IsSessionActive() && !Assembly->HasPendingReplicaReview() && Assembly->GetActiveDisplayCase() == nullptr &&
		Assembly->GetSessionRevision() == InitialSessionRevision && Assembly->GetPayloadValidationRevision() == InitialPayloadRevision &&
		Assembly->GetScoreRevision() == InitialScoreRevision && Assembly->GetLastCleanupReason() == InitialCleanupReason &&
		!DisplayCase->IsContractExhibitActive() && DisplayCase->GetAssemblyState() == InitialCaseState && DisplayCase->GetAssemblyRevision() == InitialCaseRevision &&
		!DisplayCase->IsSessionLocked() && !DisplayCase->HasCommittedAssemblyResult() && !DisplayCase->HasReplicaPreview() &&
		DisplayCase->GetOriginalCarrier() == nullptr && !DisplayCase->IsOriginalSecuredAtExit();
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
	constexpr int32 ExpectedRecoveredOriginalCount = 2;
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
		for (TActorIterator<AHeistPlayerCharacter> It(World); It; ++It)
		{
			if (IsValid(*It) && (It->GetAppliedCrewStatusForDebug() != EHeistCrewStatus::Active || It->IsLocalStunPostProcessEnabledForDebug() ||
				It->IsStunSoundMixPushedForDebug() || It->IsCrewStatusAudioPlayingForDebug() || !It->IsCrewStatusEffectPresentationCleanForDebug()))
			{
				return false;
			}
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
		2, *FString::Join(WorldStates, TEXT(" | ")));
}

bool IsLobbyStateClean(const int32 PlayerCount)
{
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		const AHeistGameState* GameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
		const AHeistPlayerController* PlayerController = GetContractRunLocalHeistPlayerController(World);
		const AHeistHUD* HUD = IsValid(PlayerController) ? PlayerController->GetHUD<AHeistHUD>() : nullptr;
		const UHeistHUDWidget* MainHUDWidget = IsValid(HUD) ? HUD->GetMainHUDWidget() : nullptr;
		const UHeistResultWidget* ResultWidget = IsValid(HUD) ? HUD->GetResultWidget() : nullptr;
		const bool bResultPresentationClean = !IsValid(ResultWidget) ||
			(ResultWidget->GetVisibility() == ESlateVisibility::Collapsed && ResultWidget->IsHiddenPresentationStateReset());
		if (!IsValid(GameState) || !IsValid(PlayerController) || !IsValid(HUD) || !IsValid(MainHUDWidget) ||
			GameState->GetMatchPhase() != EHeistMatchPhase::Lobby || GameState->PlayerArray.Num() != PlayerCount ||
			GameState->GetContractSnapshot().IsInitialized() || GameState->GetAlertLevel() != EHeistAlertLevel::Quiet || GameState->GetTeamResult().IsValid() ||
			!GameState->GetPlayerResults().IsEmpty() || !MainHUDWidget->IsHiddenPresentationStateReset() || !bResultPresentationClean)
		{
			return false;
		}
		for (TActorIterator<AHeistPlayerCharacter> It(World); It; ++It)
		{
			if (IsValid(*It) && (It->GetAppliedCrewStatusForDebug() != EHeistCrewStatus::Active || It->IsLocalStunPostProcessEnabledForDebug() ||
				It->IsStunSoundMixPushedForDebug() || It->IsCrewStatusAudioPlayingForDebug() || !It->IsCrewStatusEffectPresentationCleanForDebug()))
			{
				return false;
			}
		}
	}
	return true;
}

FString DescribeLobbyStateClean(const int32 PlayerCount)
{
	TArray<FString> WorldStates;
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		const AHeistGameState* GameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
		const AHeistPlayerController* PlayerController = GetContractRunLocalHeistPlayerController(World);
		const AHeistHUD* HUD = IsValid(PlayerController) ? PlayerController->GetHUD<AHeistHUD>() : nullptr;
		const UHeistHUDWidget* MainHUDWidget = IsValid(HUD) ? HUD->GetMainHUDWidget() : nullptr;
		const UHeistResultWidget* ResultWidget = IsValid(HUD) ? HUD->GetResultWidget() : nullptr;
		TArray<FString> CharacterStates;
		for (TActorIterator<AHeistPlayerCharacter> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				CharacterStates.Add(FString::Printf(TEXT("%s:Applied=%s,PP=%s,Mix=%s,CrewAudio=%s,StatusFXClean=%s"), *It->GetName(),
					*UEnum::GetValueAsString(It->GetAppliedCrewStatusForDebug()), It->IsLocalStunPostProcessEnabledForDebug() ? TEXT("true") : TEXT("false"),
					It->IsStunSoundMixPushedForDebug() ? TEXT("true") : TEXT("false"), It->IsCrewStatusAudioPlayingForDebug() ? TEXT("true") : TEXT("false"),
					It->IsCrewStatusEffectPresentationCleanForDebug() ? TEXT("true") : TEXT("false")));
			}
		}
		WorldStates.Add(FString::Printf(
			TEXT("World=%s NetMode=%d Phase=%s Players=%d/%d Contract=%s Alert=%s TeamResult=%s PlayerResults=%d PC=%s HUD=%s MainHUD=%s MainHUDReset=%s Result=%s ResultVisibility=%s ResultReset=%s Characters=[%s]"),
			*GetNameSafe(World), IsValid(World) ? static_cast<int32>(World->GetNetMode()) : INDEX_NONE,
			IsValid(GameState) ? *UEnum::GetValueAsString(GameState->GetMatchPhase()) : TEXT("Missing"),
			IsValid(GameState) ? GameState->PlayerArray.Num() : INDEX_NONE, PlayerCount,
			IsValid(GameState) && GameState->GetContractSnapshot().IsInitialized() ? TEXT("initialized") : TEXT("clear"),
			IsValid(GameState) ? *UEnum::GetValueAsString(GameState->GetAlertLevel()) : TEXT("Missing"),
			IsValid(GameState) && GameState->GetTeamResult().IsValid() ? TEXT("valid") : TEXT("clear"),
			IsValid(GameState) ? GameState->GetPlayerResults().Num() : INDEX_NONE, *GetNameSafe(PlayerController), *GetNameSafe(HUD), *GetNameSafe(MainHUDWidget),
			IsValid(MainHUDWidget) && MainHUDWidget->IsHiddenPresentationStateReset() ? TEXT("true") : TEXT("false"), *GetNameSafe(ResultWidget),
			IsValid(ResultWidget) ? *UEnum::GetValueAsString(ResultWidget->GetVisibility()) : TEXT("Missing"),
			IsValid(ResultWidget) && ResultWidget->IsHiddenPresentationStateReset() ? TEXT("true") : TEXT("false"), *FString::Join(CharacterStates, TEXT("; "))));
	}
	return FString::Printf(TEXT("W6-010 Lobby cleanup diagnostic: WorldsReady=%s LobbyClean=%s %s"),
		AreContractRunWorldsReady(PlayerCount, EHeistMatchPhase::Lobby, false) ? TEXT("true") : TEXT("false"), IsLobbyStateClean(PlayerCount) ? TEXT("true") : TEXT("false"),
		*FString::Join(WorldStates, TEXT(" | ")));
}

bool IsReleaseSecurityContentReady(UWorld* World)
{
	if (!IsValid(World))
	{
		return false;
	}
	AHeistLaserBarrierActor* Laser = nullptr;
	AHeistSecurityHoldButtonActor* HoldButton = nullptr;
	int32 LaserCount = 0;
	int32 HoldButtonCount = 0;
	int32 CameraCount = 0;
	for (TActorIterator<AHeistLaserBarrierActor> It(World); It; ++It)
	{
		Laser = *It;
		++LaserCount;
	}
	for (TActorIterator<AHeistSecurityHoldButtonActor> It(World); It; ++It)
	{
		HoldButton = *It;
		++HoldButtonCount;
	}
	for (TActorIterator<AHeistSecurityCameraActor> It(World); It; ++It)
	{
		++CameraCount;
	}
	const AHeistPaintingDisplayCaseActor* ProtectedCase = IsValid(Laser) ? Laser->GetProtectedPaintingCase() : nullptr;
	return LaserCount == 1 && HoldButtonCount == 1 && CameraCount == 1 && IsValid(Laser) && IsValid(HoldButton) && IsValid(ProtectedCase) &&
		ProtectedCase->IsContractExhibitActive() && HoldButton->GetLinkedLaserBarrier() == Laser && Laser->IsBarrierEnabled() && Laser->IsBeamActive();
}

bool IsGameplayContentReplicated(const int32 PlayerCount, const FName ExpectedMapId)
{
	UWorld* ServerWorld = GetContractRunServerWorld();
	const AHeistGameMode* GameMode = IsValid(ServerWorld) ? ServerWorld->GetAuthGameMode<AHeistGameMode>() : nullptr;
	const AHeistGameState* ServerGameState = IsValid(ServerWorld) ? ServerWorld->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(GameMode) || !GameMode->IsPlayerCountGuardScalingApplied() || !IsValid(ServerGameState) ||
		!ServerGameState->GetContractSnapshot().IsInitialized() ||
		!IsValid(FindPaintingCase(ServerWorld, ServerGameState->GetContractSnapshot().RequiredTargetCaseId)))
	{
		return false;
	}
	const int32 ExpectedActiveGuardCount = GameMode->GetDifficultyExpectedGuardCount();
	int32 ServerActiveObjectCaseCount = 0;
	for (TActorIterator<AHeistObjectDisplayCaseActor> It(ServerWorld); It; ++It)
	{
		ServerActiveObjectCaseCount += IsValid(*It) && It->IsContractExhibitActive() ? 1 : 0;
	}
	int32 ServerActiveGuardCount = 0;
	for (TActorIterator<AHeistGuardCharacter> It(ServerWorld); It; ++It)
	{
		ServerActiveGuardCount += IsValid(*It) && It->IsDifficultyActive() ? 1 : 0;
	}
	if (HeistReleaseFeatures::IsObjectAssemblyRuntimeEnabled() || ServerActiveObjectCaseCount != 0 || ServerActiveGuardCount != ExpectedActiveGuardCount)
	{
		return false;
	}
	const FHeistContractSnapshot ServerContract = ServerGameState->GetContractSnapshot();
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		const AHeistGameState* GameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
		if (!IsValid(GameState) || GameState->GetMatchPhase() != EHeistMatchPhase::InGame || !GameState->GetContractSnapshot().IsInitialized() ||
			GameState->GetContractSnapshot().MapId != ExpectedMapId || !(GameState->GetContractSnapshot() == ServerContract) || !IsReleaseSecurityContentReady(World))
		{
			return false;
		}
	}
	return true;
}

FString DescribeGameplayContentReplication(const int32 PlayerCount)
{
	UWorld* ServerWorld = GetContractRunServerWorld();
	const AHeistGameMode* GameMode = IsValid(ServerWorld) ? ServerWorld->GetAuthGameMode<AHeistGameMode>() : nullptr;
	const int32 ExpectedActiveGuardCount = IsValid(GameMode) ? GameMode->GetDifficultyExpectedGuardCount() : INDEX_NONE;
	TArray<FString> WorldStates;
	for (UWorld* World : GetContractRunPIEWorlds())
	{
		const AHeistGameState* GameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
		const FHeistContractSnapshot Contract = IsValid(GameState) ? GameState->GetContractSnapshot() : FHeistContractSnapshot();
		int32 ActiveObjectCaseCount = 0;
		int32 TotalObjectCaseCount = 0;
		for (TActorIterator<AHeistObjectDisplayCaseActor> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				++TotalObjectCaseCount;
				ActiveObjectCaseCount += It->IsContractExhibitActive() ? 1 : 0;
			}
		}
		int32 ActiveGuardCount = 0;
		int32 TotalGuardCount = 0;
		for (TActorIterator<AHeistGuardCharacter> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				++TotalGuardCount;
				ActiveGuardCount += It->IsDifficultyActive() ? 1 : 0;
			}
		}
		WorldStates.Add(FString::Printf(
			TEXT("World=%s NetMode=%d Phase=%s Players=%d Contract=%s Map=%s RequiredCase=%s RequiredCaseFound=%s ActiveObjectCases=%d TotalDeferredObjectCases=%d ExpectedActiveObjects=0 Guards=%d/%d ExpectedGuards=%d"),
			*GetNameSafe(World), IsValid(World) ? static_cast<int32>(World->GetNetMode()) : INDEX_NONE,
			IsValid(GameState) ? *UEnum::GetValueAsString(GameState->GetMatchPhase()) : TEXT("Missing"),
			IsValid(GameState) ? GameState->PlayerArray.Num() : INDEX_NONE,
			Contract.IsInitialized() ? TEXT("Initialized") : TEXT("Missing"), *Contract.MapId.ToString(), *Contract.RequiredTargetCaseId.ToString(),
			IsValid(FindPaintingCase(World, Contract.RequiredTargetCaseId)) ? TEXT("true") : TEXT("false"), ActiveObjectCaseCount,
			TotalObjectCaseCount, ActiveGuardCount, TotalGuardCount, ExpectedActiveGuardCount));
	}
	return FString::Printf(TEXT("W6-010 gameplay readiness diagnostic: authority owns exact Case/Guard counts; client actor totals are informational because distance relevancy applies. GameMode=%s GuardScalingApplied=%s %s"), *GetNameSafe(GameMode),
		IsValid(GameMode) && GameMode->IsPlayerCountGuardScalingApplied() ? TEXT("true") : TEXT("false"), *FString::Join(WorldStates, TEXT(" | ")));
}

bool IsGameplayResetClean(const int32 PlayerCount, const FName ExpectedMapId)
{
	if (!IsGameplayContentReplicated(PlayerCount, ExpectedMapId))
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
		if (!IsValid(ReplicatedGameState) || !IsValid(PlayerController) || !IsValid(PlayerController->GetPawn()) || !IsValid(HUD) || HUD->IsFloorPlanMapVisible() ||
			PlayerController->GetLocalInputMode() != EHeistInputMode::Gameplay ||
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
		for (TActorIterator<AHeistPlayerCharacter> It(World); It; ++It)
		{
			if (IsValid(*It) && (It->GetAppliedCrewStatusForDebug() != EHeistCrewStatus::Active || It->IsLocalStunPostProcessEnabledForDebug() ||
				It->IsStunSoundMixPushedForDebug() || It->IsCrewStatusAudioPlayingForDebug() || !It->IsCrewStatusEffectPresentationCleanForDebug()))
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

bool IsWeek7SurfaceDangerCloseReady(const int32 PlayerId)
{
	UWorld* ServerWorld = GetContractRunServerWorld();
	const AHeistPlayerCharacter* ServerCharacter = GetServerCharacterById(PlayerId);
	const UHeistForgeryComponent* Forgery = IsValid(ServerCharacter) ? ServerCharacter->GetForgeryComponent() : nullptr;
	if (!IsValid(ServerWorld) || (IsValid(Forgery) && Forgery->IsSessionActive()))
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
	const UHeistForgeryWidget* Widget = HUD->GetForgeryWidget();
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
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d %s worlds and replicated content"), RunIndex, *State->MapId.ToString()), [State]()
	{
		return AreContractRunWorldsReady(State->PlayerCount, EHeistMatchPhase::InGame, true) && IsGameplayContentReplicated(State->PlayerCount, State->MapId);
	}, 75.0, [State]() { return DescribeGameplayContentReplication(State->PlayerCount); }));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d clean gameplay state"), RunIndex), [State]()
	{
		return IsGameplayResetClean(State->PlayerCount, State->MapId);
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
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d prepare placed CCTV/Laser/Hold interaction"), RunIndex), [State, RunIndex]()
	{
		return State->PlayerCount != 2 || RunIndex != 1 || PrepareReleaseSecurityInteraction(State);
	}));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d move player 2 into placed CCTV coverage"), RunIndex), [State, RunIndex]()
	{
		if (State->PlayerCount != 2 || RunIndex != 1)
		{
			return true;
		}
		const AHeistSecurityCameraActor* Camera = FindOnlyActorOfType<AHeistSecurityCameraActor>(GetContractRunServerWorld());
		if (!IsValid(Camera))
		{
			return false;
		}
		FVector DetectionLocation = Camera->GetActorLocation() + Camera->GetActorForwardVector() * 600.0f;
		DetectionLocation.Z = 88.0f;
		return TeleportServerPlayerToLocation(2, DetectionLocation);
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d placed CCTV detection, Alert, and Guard investigation"), RunIndex), [State, RunIndex]()
	{
		return State->PlayerCount != 2 || RunIndex != 1 || IsReleaseSecurityDetectionReady(State);
	}, 12.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d move player 2 behind placed CCTV"), RunIndex), [State, RunIndex]()
	{
		if (State->PlayerCount != 2 || RunIndex != 1)
		{
			return true;
		}
		const AHeistSecurityCameraActor* Camera = FindOnlyActorOfType<AHeistSecurityCameraActor>(GetContractRunServerWorld());
		if (!IsValid(Camera))
		{
			return false;
		}
		FVector SafeLocation = Camera->GetActorLocation() - Camera->GetActorForwardVector() * 800.0f;
		SafeLocation.Z = 88.0f;
		return TeleportServerPlayerToLocation(2, SafeLocation);
	}));
	Test->AddCommand(new FWaitLatentCommand(0.75f));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d placed CCTV incident remains one-shot"), RunIndex), [State, RunIndex]()
	{
		return State->PlayerCount != 2 || RunIndex != 1 || IsReleaseSecurityIncidentOneShot(State);
	}));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d reset Alert after placed CCTV evidence"), RunIndex), [State, RunIndex]()
	{
		return State->PlayerCount != 2 || RunIndex != 1 || ResetReleaseSecurityAfterCamera();
	}));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d move player 1 into placed Hold Button interaction"), RunIndex), [State, RunIndex]()
	{
		return State->PlayerCount != 2 || RunIndex != 1 ||
			TeleportServerPlayerIntoInteraction(1, FindOnlyActorOfType<AHeistSecurityHoldButtonActor>(GetContractRunServerWorld()));
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d player 1 overlaps placed Hold Button"), RunIndex), [State, RunIndex]()
	{
		return State->PlayerCount != 2 || RunIndex != 1 ||
			IsServerPlayerOverlapping(1, FindOnlyActorOfType<AHeistSecurityHoldButtonActor>(GetContractRunServerWorld()));
	}, 5.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d begin placed Security Hold through server RPC"), RunIndex), [State, RunIndex]()
	{
		if (State->PlayerCount != 2 || RunIndex != 1)
		{
			return true;
		}
		AHeistPlayerController* OwningController = GetOwningPlayerControllerById(1);
		AHeistSecurityHoldButtonActor* LocalButton = IsValid(OwningController)
			? FindOnlyActorOfType<AHeistSecurityHoldButtonActor>(OwningController->GetWorld())
			: nullptr;
		return InvokeSingleActorServerRPC(OwningController, FName(TEXT("Server_RequestBeginSecurityHold")), LocalButton);
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d placed Hold bypasses Laser on both peers"), RunIndex), [State, RunIndex]()
	{
		return State->PlayerCount != 2 || RunIndex != 1 || IsSecurityHoldStateReplicated(true, true, false);
	}, 8.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d non-holder crosses placed bypassed Laser"), RunIndex), [State, RunIndex]()
	{
		if (State->PlayerCount != 2 || RunIndex != 1)
		{
			return true;
		}
		const AHeistLaserBarrierActor* Laser = FindOnlyActorOfType<AHeistLaserBarrierActor>(GetContractRunServerWorld());
		return IsValid(Laser) && TeleportServerPlayerToLocation(2, Laser->GetActorLocation());
	}));
	Test->AddCommand(new FWaitLatentCommand(0.75f));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d bypassed crossing adds no security incident"), RunIndex), [State, RunIndex]()
	{
		if (State->PlayerCount != 2 || RunIndex != 1)
		{
			return true;
		}
		UWorld* ServerWorld = GetContractRunServerWorld();
		const AHeistGameMode* GameMode = IsValid(ServerWorld) ? ServerWorld->GetAuthGameMode<AHeistGameMode>() : nullptr;
		return IsValid(GameMode) && IsSecurityHoldStateReplicated(true, true, false) &&
			GameMode->GetProcessedSecurityIncidentCount() == State->SecurityIncidentCountBaseline + 1 &&
			GameMode->GetProcessedGuardInvestigationCount() == State->SecurityInvestigationCountBaseline + 1;
	}));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d clear non-holder before placed Hold release"), RunIndex), [State, RunIndex]()
	{
		if (State->PlayerCount != 2 || RunIndex != 1)
		{
			return true;
		}
		const AHeistSecurityCameraActor* Camera = FindOnlyActorOfType<AHeistSecurityCameraActor>(GetContractRunServerWorld());
		if (!IsValid(Camera))
		{
			return false;
		}
		FVector SafeLocation = Camera->GetActorLocation() - Camera->GetActorForwardVector() * 800.0f;
		SafeLocation.Z = 88.0f;
		return TeleportServerPlayerToLocation(2, SafeLocation);
	}));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d release placed Security Hold through server RPC"), RunIndex), [State, RunIndex]()
	{
		if (State->PlayerCount != 2 || RunIndex != 1)
		{
			return true;
		}
		AHeistPlayerController* OwningController = GetOwningPlayerControllerById(1);
		AHeistSecurityHoldButtonActor* LocalButton = IsValid(OwningController)
			? FindOnlyActorOfType<AHeistSecurityHoldButtonActor>(OwningController->GetWorld())
			: nullptr;
		return InvokeSingleActorServerRPC(OwningController, FName(TEXT("Server_RequestEndSecurityHold")), LocalButton);
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d placed Laser rearm grace replicated"), RunIndex), [State, RunIndex]()
	{
		return State->PlayerCount != 2 || RunIndex != 1 || IsLaserRearmingReplicated();
	}, 3.0));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d placed Laser fully rearmed"), RunIndex), [State, RunIndex]()
	{
		return State->PlayerCount != 2 || RunIndex != 1 || IsSecurityHoldStateReplicated(false, false, true);
	}, 5.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d record placed security evidence"), RunIndex), [Test, State, RunIndex]()
	{
		if (State->PlayerCount == 2 && RunIndex == 1)
		{
			Test->AddInfo(FString::Printf(TEXT("W8 placed security: Map=%s Players=2 CCTVDetections=1 SecurityIncidents=1 GuardInvestigations=1 Holder=Player1 BypassedCrossing=PASS Rearmed=PASS Result=PASS"),
				*State->MapId.ToString()));
		}
		return true;
	}));
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
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d open Floor Plan before player 2 Stun transition"), RunIndex), [State]()
	{
		return State->PlayerCount == 1 || ToggleWeek7MapForPlayer(2, true);
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d player 2 Map ready before Stun transition"), RunIndex), [State]()
	{
		return State->PlayerCount == 1 || IsWeek7MapContractReadyForPlayer(2, true);
	}, 10.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d Guard contact applies Stun before Arrest"), RunIndex), [State]()
	{
		return State->PlayerCount == 1 || BeginWeek7GuardStun(2);
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d Stun HUD/input lock replicated"), RunIndex), [State]()
	{
		return State->PlayerCount == 1 || IsWeek7StunPresentationReady(2);
	}, 2.5, [State]() { return State->PlayerCount == 1 ? FString() : DescribeWeek7StunPresentation(2); }));
	Test->AddCommand(new FWaitLatentCommand(0.75f));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d Stun presentation sustained for 0.75 seconds"), RunIndex), [State]()
	{
		return State->PlayerCount == 1 || IsWeek7StunPresentationReady(2);
	}, 5.0));
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
			if (UHeistGuardStateComponent* GuardState = IsValid(*It) && It->IsDifficultyActive() ? It->GetGuardStateComponent() : nullptr; IsValid(GuardState))
			{
				GuardState->SetDisabled(true);
				bFoundGuard = true;
			}
		}
		return bFoundGuard;
	}));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d seed Heavy presentation weight"), RunIndex), []()
	{
		AHeistPlayerState* PlayerState = FindPlayerStateById(GetContractRunServerWorld(), 1);
		if (!IsValid(PlayerState))
		{
			return false;
		}
		PlayerState->DebugSetTotalLootWeight(10.0f);
		return true;
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d Heavy status replicated"), RunIndex), []()
	{
		return IsCrewStatusReplicated(1, EHeistCrewStatus::Heavy);
	}, 10.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d capture Heavy footstep presentation baseline"), RunIndex), [State]()
	{
		return CaptureCrewStatusFootstepBaseline(State, 1);
	}));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d play authoritative Heavy footstep presentation"), RunIndex), []()
	{
		AHeistPlayerCharacter* Character = GetServerCharacterById(1);
		if (!IsValid(Character))
		{
			return false;
		}
		Character->NotifyAuthoritativeCrewStatusFootstep(false);
		return true;
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d Heavy footstep presentation replicated"), RunIndex), [State]()
	{
		return IsCrewStatusFootstepPresentationReady(State, 1, EHeistCrewStatus::Heavy);
	}, 10.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d clear Heavy presentation weight"), RunIndex), []()
	{
		AHeistPlayerState* PlayerState = FindPlayerStateById(GetContractRunServerWorld(), 1);
		if (!IsValid(PlayerState))
		{
			return false;
		}
		PlayerState->DebugSetTotalLootWeight(0.0f);
		return true;
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d Active restored after Heavy presentation"), RunIndex), []()
	{
		return IsCrewStatusReplicated(1, EHeistCrewStatus::Active);
	}, 10.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d move player 1 to shared loose loot"), RunIndex), [State]()
	{
		return TeleportServerPlayerIntoInteraction(1, FindLootActor(GetContractRunServerWorld(), State->SelectedLootActorName));
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d shared loose-loot overlap"), RunIndex), [State]()
	{
		return IsServerPlayerOverlapping(1, FindLootActor(GetContractRunServerWorld(), State->SelectedLootActorName));
	}, 10.0));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d shared loose-loot relevant to owning peer"), RunIndex), [State]()
	{
		return IsOwningLootActorRelevant(1, State->SelectedLootActorName);
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
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d required Surface relevant to owning peer"), RunIndex), []()
	{
		const AHeistGameState* GameState = GetContractRunServerWorld()->GetGameState<AHeistGameState>();
		return IsValid(GameState) && IsOwningPaintingCaseRelevant(1, GameState->GetContractSnapshot().RequiredTargetCaseId);
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
		AHeistGameMode* GameMode = GetContractRunServerWorld()->GetAuthGameMode<AHeistGameMode>();
		bool bLevelChanged = false;
		return IsValid(GameMode) && GameMode->RequestAlertEscalation(EHeistAlertLevel::Alarmed, FName(TEXT("W7SurfaceDangerClose")), &bLevelChanged) && bLevelChanged;
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d W7 Surface danger close and Gameplay input restore"), RunIndex), [State, RunIndex]()
	{
		return State->PlayerCount != 4 || RunIndex != 1 || IsWeek7SurfaceDangerCloseReady(1);
	}, 10.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d W7 reset Alert after Surface danger close"), RunIndex), [State, RunIndex]()
	{
		if (State->PlayerCount != 4 || RunIndex != 1)
		{
			return true;
		}
		AHeistGameState* GameState = GetContractRunServerWorld()->GetGameState<AHeistGameState>();
		return IsValid(GameState) && GameState->SetAlertSnapshot(0.0f, EHeistAlertLevel::Quiet, FName(TEXT("W7SurfaceDangerCloseReset")));
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
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d first Surface session fully cleared on owner"), RunIndex), []()
	{
		const AHeistPlayerCharacter* ServerCharacter = GetServerCharacterById(1);
		const UHeistForgeryComponent* ServerForgery = IsValid(ServerCharacter) ? ServerCharacter->GetForgeryComponent() : nullptr;
		const AHeistPlayerCharacter* OwningCharacter = GetOwningCharacterById(1);
		const UHeistForgeryComponent* OwningForgery = IsValid(OwningCharacter) ? OwningCharacter->GetForgeryComponent() : nullptr;
		return IsValid(ServerForgery) && IsValid(OwningForgery) && !ServerForgery->IsSessionActive() && !ServerForgery->HasPendingReplicaReview() &&
			!OwningForgery->IsSessionActive() && !OwningForgery->HasPendingReplicaReview() && ServerForgery->GetActiveDisplayCase() == nullptr &&
			OwningForgery->GetActiveDisplayCase() == nullptr && OwningForgery->GetSessionRevision() == ServerForgery->GetSessionRevision();
	}, 10.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d move player 1 to active FourStar Surface case"), RunIndex), [State]()
	{
		return TeleportServerPlayerIntoInteraction(1, FindPaintingCase(GetContractRunServerWorld(), State->SelectedHighValuePaintingCaseId));
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d FourStar Surface overlap"), RunIndex), [State]()
	{
		return IsServerPlayerOverlapping(1, FindPaintingCase(GetContractRunServerWorld(), State->SelectedHighValuePaintingCaseId));
	}, 10.0));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d FourStar Surface relevant to owning peer"), RunIndex), [State]()
	{
		return IsOwningPaintingCaseRelevant(1, State->SelectedHighValuePaintingCaseId);
	}, 10.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d request FourStar Surface observation through server RPC"), RunIndex), [State]()
	{
		AHeistPlayerController* OwningPlayerController = GetOwningPlayerControllerById(1);
		AHeistPaintingDisplayCaseActor* LocalDisplayCase = IsValid(OwningPlayerController)
			? FindPaintingCase(OwningPlayerController->GetWorld(), State->SelectedHighValuePaintingCaseId)
			: nullptr;
		return InvokeSingleActorServerRPC(OwningPlayerController, FName(TEXT("Server_RequestObservation")), LocalDisplayCase);
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d owner FourStar Surface session"), RunIndex), [State]()
	{
		return IsSurfaceSessionReady(1, State->SelectedHighValuePaintingCaseId, EHeistCrewStatus::CarryingOriginal);
	}, 15.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d submit generated FourStar Surface strokes"), RunIndex), []()
	{
		return SubmitReferenceMatchedSurface(1);
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d authoritative FourStar Surface quality 70+"), RunIndex), [State]()
	{
		return HasSurfaceReplicaPreview(1, State->SelectedHighValuePaintingCaseId);
	}, 20.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d confirm FourStar Surface replica swap"), RunIndex), []()
	{
		AHeistPlayerController* PlayerController = GetOwningPlayerControllerById(1);
		if (!IsValid(PlayerController))
		{
			return false;
		}
		PlayerController->RequestConfirmForgeryReplicaSwap();
		return true;
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d FourStar Original carried and quota reached"), RunIndex), [State]()
	{
		const AHeistGameState* GameState = GetContractRunServerWorld()->GetGameState<AHeistGameState>();
		const AHeistPaintingDisplayCaseActor* DisplayCase = FindPaintingCase(GetContractRunServerWorld(), State->SelectedHighValuePaintingCaseId);
		return IsValid(GameState) && IsValid(DisplayCase) && DisplayCase->GetDisplayCaseState() == EHeistDisplayCaseState::OriginalRemoved &&
			HasOriginalForCase(1, DisplayCase) && GameState->GetContractSnapshot().CarriedValue >= GameState->GetContractSnapshot().LootValueQuota &&
			IsCrewStatusReplicated(1, EHeistCrewStatus::CarryingOriginal);
	}, 15.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d capture Original-carry footstep presentation baseline"), RunIndex), [State]()
	{
		return CaptureCrewStatusFootstepBaseline(State, 1);
	}));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("run %d play authoritative Original-carry footstep presentation"), RunIndex), []()
	{
		AHeistPlayerCharacter* Character = GetServerCharacterById(1);
		if (!IsValid(Character))
		{
			return false;
		}
		Character->NotifyAuthoritativeCrewStatusFootstep(false);
		return true;
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("run %d Original-carry footstep presentation replicated"), RunIndex), [State]()
	{
		return IsCrewStatusFootstepPresentationReady(State, 1, EHeistCrewStatus::CarryingOriginal);
	}, 10.0));

	Test->AddCommand(new FHeistContractRunActionCommand(Test, State,
		FString::Printf(TEXT("run %d Object Assembly is FeatureDisabled and leaves every authored Case unchanged"), RunIndex), [State]()
	{
		if (HeistReleaseFeatures::IsObjectAssemblyRuntimeEnabled())
		{
			return false;
		}
		for (const FName ObjectCaseId : State->SelectedObjectCaseIds)
		{
			if (!ValidateDeferredObjectAssemblyAttempt(1, ObjectCaseId))
			{
				return false;
			}
		}
		return true;
	}));

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
			if (UHeistGuardStateComponent* GuardState = IsValid(*It) && It->IsDifficultyActive() ? It->GetGuardStateComponent() : nullptr; IsValid(GuardState))
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
			FString::Printf(TEXT("run %d player %d first Vent transaction committed"), RunIndex, PlayerId), [PlayerId, State]()
		{
			const AHeistGameState* GameState = GetContractRunServerWorld()->GetGameState<AHeistGameState>();
			const AHeistPlayerController* ServerPlayerController = GetServerPlayerControllerById(PlayerId);
			const AHeistPlayerState* PlayerState = IsValid(ServerPlayerController) ? ServerPlayerController->GetPlayerState<AHeistPlayerState>() : nullptr;
			const AHeistPlayerCharacter* Character = IsValid(ServerPlayerController) ? Cast<AHeistPlayerCharacter>(ServerPlayerController->GetPawn()) : nullptr;
			const UHeistInventoryComponent* Inventory = IsValid(Character) ? Character->GetInventoryComponent() : nullptr;
			if (!IsValid(GameState) || !IsValid(PlayerState))
			{
				return false;
			}
			if (PlayerId != 1)
			{
				return PlayerState->IsEscaped();
			}

			const FHeistContractSnapshot Contract = GameState->GetContractSnapshot();
			const AHeistPaintingDisplayCaseActor* RequiredCase = FindPaintingCase(GetContractRunServerWorld(), Contract.RequiredTargetCaseId);
			const AHeistPaintingDisplayCaseActor* HighValueCase = FindPaintingCase(GetContractRunServerWorld(), State->SelectedHighValuePaintingCaseId);
			return !PlayerState->IsEscaped() && PlayerState->GetTotalLootScore() == 0 && IsValid(Inventory) && Inventory->IsCarryingOriginal() &&
				Inventory->GetOriginalArtifactCount() == 2 && HasOriginalForCase(PlayerId, RequiredCase) && HasOriginalForCase(PlayerId, HighValueCase) &&
				PlayerState->GetContribution().SecuredLootValue == State->SelectedLootValue && Contract.SecuredValue == State->SelectedLootValue &&
				!Contract.bRequiredTargetSecured;
		}, 15.0));
		Test->AddCommand(new FHeistContractRunActionCommand(Test, State,
			FString::Printf(TEXT("run %d player %d request final escape when settlement kept Original"), RunIndex, PlayerId), [PlayerId]()
		{
			AHeistPlayerController* PlayerController = GetOwningPlayerControllerById(PlayerId);
			const AHeistPlayerController* ServerPlayerController = GetServerPlayerControllerById(PlayerId);
			const AHeistPlayerState* ServerPlayerState = IsValid(ServerPlayerController) ? ServerPlayerController->GetPlayerState<AHeistPlayerState>() : nullptr;
			if (!IsValid(PlayerController) || !IsValid(ServerPlayerState))
			{
				return false;
			}
			if (ServerPlayerState->IsEscaped())
			{
				return true;
			}
			AHeistVentActor* LocalExit = FindSharedExit(PlayerController->GetWorld());
			return InvokeSingleActorServerRPC(PlayerController, FName(TEXT("Server_RequestEscape")), LocalExit);
		}));
		Test->AddCommand(new FHeistContractRunWaitCommand(Test, State,
			FString::Printf(TEXT("run %d player %d final escape committed"), RunIndex, PlayerId), [PlayerId]()
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
		constexpr int32 ExpectedRecoveredOriginalCount = 2;
		Test->AddInfo(FString::Printf(TEXT("W6-010 run evidence: Run=%d Players=%d Surface=2 ObjectAssembly=FeatureDisabled Originals=%d/%d LooseLoot=%s LooseValue=%d Secured=%d Quota=%d RequiredSecured=%s Escaped=%d ResultWidgetsPerWorld=1 Outcome=%s Result=PASS"),
			RunIndex, State->PlayerCount, RecoveredOriginalCount, ExpectedRecoveredOriginalCount, *State->SelectedLootRowId.ToString(),
			State->SelectedLootValue, Contract.SecuredValue, Contract.LootValueQuota, Contract.bRequiredTargetSecured ? TEXT("true") : TEXT("false"),
			GameState->GetEscapedCrewCount(), *UEnum::GetValueAsString(Contract.Outcome)));
		return Contract.IsSuccessConditionMet() && Contract.Outcome == EHeistContractOutcome::Success && RecoveredOriginalCount == ExpectedRecoveredOriginalCount;
	}));
}

bool EnqueueTwoRunContractScenario(FAutomationTestBase* Test, const int32 PlayerCount, const FName MapId)
{
	const TSharedRef<FHeistContractRunAutomationState> State = MakeShared<FHeistContractRunAutomationState>();
	State->PlayerCount = PlayerCount;
	State->MapId = MapId;
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
		Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("select %s for run %d"), *State->MapId.ToString(), RunIndex), [State]()
		{
			AHeistPlayerController* HostPlayerController = GetOwningPlayerControllerById(1);
			if (!IsValid(HostPlayerController))
			{
				return false;
			}
			HostPlayerController->RequestSetLobbyMapSelection(State->MapId);
			return true;
		}));
		Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, FString::Printf(TEXT("%s selection for run %d"), *State->MapId.ToString(), RunIndex), [State]()
		{
			UWorld* ServerWorld = GetContractRunServerWorld();
			const UHeistGameInstance* GameInstance = IsValid(ServerWorld) ? Cast<UHeistGameInstance>(ServerWorld->GetGameInstance()) : nullptr;
			return IsValid(GameInstance) && GameInstance->GetSelectedMapId() == State->MapId && !GameInstance->IsMapSelectionUpdatePending();
		}, 15.0));
		Test->AddCommand(new FHeistContractRunActionCommand(Test, State, FString::Printf(TEXT("start %s run %d"), *State->MapId.ToString(), RunIndex), []()
		{
			UWorld* ServerWorld = GetContractRunServerWorld();
			UHeistGameInstance* GameInstance = IsValid(ServerWorld) ? Cast<UHeistGameInstance>(ServerWorld->GetGameInstance()) : nullptr;
			return IsValid(GameInstance) && SetAllContractRunLobbyPlayersReady(ServerWorld) && GameInstance->RequestStartSelectedGameplayMap();
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
			}, 75.0, [State]() { return DescribeLobbyStateClean(State->PlayerCount); }));
		}
	}

	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, TEXT("record two-run PASS evidence"), [Test, State]()
	{
		Test->AddInfo(FString::Printf(TEXT("W6-010 two-run gate: Players=%d Map=%s RunsCompleted=2 LobbyReturn=true SecondRunCleanReset=true AuthorityFlows=true Result=PASS"),
			State->PlayerCount, *State->MapId.ToString()));
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

bool EnqueueSandBoxSecurityCooperationScenario(FAutomationTestBase* Test)
{
	const TSharedRef<FHeistContractRunAutomationState> State = MakeShared<FHeistContractRunAutomationState>();
	State->PlayerCount = 2;
	Test->AddCommand(new FEditorLoadMap(TEXT("/Game/Maps/SandBoxMap")));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, TEXT("configure two-player Sandbox listen-server PIE"), [State]()
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
		PlaySettings->SetPlayNumberOfClients(2);
		return true;
	}));
	Test->AddCommand(new FStartPIECommand(false));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, TEXT("Sandbox security actors, two players, and active guard"), []()
	{
		return IsSandBoxSecurityPreflightReady();
	}, 60.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, TEXT("bootstrap Sandbox-only contract and disable autonomous guard sight"), []()
	{
		return BootstrapSandBoxSecurityContract();
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, TEXT("Sandbox instance links and enabled security runtime replicated"), []()
	{
		return IsSandBoxSecurityRuntimeReplicated();
	}, 15.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, TEXT("capture CCTV incident baselines"), [State]()
	{
		UWorld* ServerWorld = GetContractRunServerWorld();
		const AHeistSecurityCameraActor* Camera = FindOnlyActorOfType<AHeistSecurityCameraActor>(ServerWorld);
		const AHeistGameMode* GameMode = IsValid(ServerWorld) ? ServerWorld->GetAuthGameMode<AHeistGameMode>() : nullptr;
		if (!IsValid(Camera) || !IsValid(GameMode))
		{
			return false;
		}
		State->SecurityDetectionRevisionBaseline = Camera->GetDetectionRevision();
		State->SecurityIncidentCountBaseline = GameMode->GetProcessedSecurityIncidentCount();
		State->SecurityInvestigationCountBaseline = GameMode->GetProcessedGuardInvestigationCount();
		const AHeistGameState* GameState = ServerWorld->GetGameState<AHeistGameState>();
		State->SecurityAlertMeterBaseline = IsValid(GameState) ? GameState->GetAlertMeterValue() : 0.0f;
		return true;
	}));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, TEXT("move player 2 into CCTV coverage"), []()
	{
		const AHeistSecurityCameraActor* Camera = FindOnlyActorOfType<AHeistSecurityCameraActor>(GetContractRunServerWorld());
		if (!IsValid(Camera))
		{
			return false;
		}
		FVector DetectionLocation = Camera->GetActorLocation() + Camera->GetActorForwardVector() * 600.0f;
		DetectionLocation.Z = 88.0f;
		return TeleportServerPlayerToLocation(2, DetectionLocation);
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, TEXT("one CCTV detection produces one Alert and one nearby guard investigation"), [State]()
	{
		UWorld* ServerWorld = GetContractRunServerWorld();
		const AHeistGameMode* GameMode = IsValid(ServerWorld) ? ServerWorld->GetAuthGameMode<AHeistGameMode>() : nullptr;
		const AHeistGameState* GameState = IsValid(ServerWorld) ? ServerWorld->GetGameState<AHeistGameState>() : nullptr;
		return IsValid(GameMode) &&
			GameMode->GetProcessedSecurityIncidentCount() == State->SecurityIncidentCountBaseline + 1 &&
			GameMode->GetProcessedGuardInvestigationCount() == State->SecurityInvestigationCountBaseline + 1 &&
			IsValid(GameState) && FMath::IsNearlyEqual(GameState->GetAlertMeterValue(), State->SecurityAlertMeterBaseline + 0.5f) &&
			IsCameraDetectionReplicated(State->SecurityDetectionRevisionBaseline + 1, 2);
	}, 12.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, TEXT("move player 2 out of CCTV coverage"), []()
	{
		return TeleportServerPlayerToLocation(2, FVector(-1500.0f, -1200.0f, 88.0f));
	}));
	Test->AddCommand(new FWaitLatentCommand(1.0f));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, TEXT("CCTV incident remains one-shot after leaving coverage"), [State]()
	{
		UWorld* ServerWorld = GetContractRunServerWorld();
		const AHeistGameMode* GameMode = IsValid(ServerWorld) ? ServerWorld->GetAuthGameMode<AHeistGameMode>() : nullptr;
		const AHeistGameState* GameState = IsValid(ServerWorld) ? ServerWorld->GetGameState<AHeistGameState>() : nullptr;
		const AHeistSecurityCameraActor* Camera = FindOnlyActorOfType<AHeistSecurityCameraActor>(ServerWorld);
		return IsValid(GameMode) && IsValid(Camera) && Camera->GetDetectionRevision() == State->SecurityDetectionRevisionBaseline + 1 &&
			GameMode->GetProcessedSecurityIncidentCount() == State->SecurityIncidentCountBaseline + 1 &&
			GameMode->GetProcessedGuardInvestigationCount() == State->SecurityInvestigationCountBaseline + 1 && IsValid(GameState) &&
			FMath::IsNearlyEqual(GameState->GetAlertMeterValue(), State->SecurityAlertMeterBaseline + 0.5f);
	}));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, TEXT("move player 1 into Security Hold Button interaction"), []()
	{
		return TeleportServerPlayerIntoInteraction(1, FindOnlyActorOfType<AHeistSecurityHoldButtonActor>(GetContractRunServerWorld()));
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, TEXT("player 1 overlaps Security Hold Button"), []()
	{
		return IsServerPlayerOverlapping(1, FindOnlyActorOfType<AHeistSecurityHoldButtonActor>(GetContractRunServerWorld()));
	}, 5.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, TEXT("player 1 begins hold through server RPC"), []()
	{
		AHeistPlayerController* OwningController = GetOwningPlayerControllerById(1);
		AHeistSecurityHoldButtonActor* LocalButton = IsValid(OwningController)
			? FindOnlyActorOfType<AHeistSecurityHoldButtonActor>(OwningController->GetWorld())
			: nullptr;
		return InvokeSingleActorServerRPC(OwningController, FName(TEXT("Server_RequestBeginSecurityHold")), LocalButton);
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, TEXT("single holder bypasses Laser on both peers"), []()
	{
		return IsSecurityHoldStateReplicated(true, true, false);
	}, 8.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, TEXT("non-holder player 2 crosses bypassed Laser"), []()
	{
		const AHeistLaserBarrierActor* Laser = FindOnlyActorOfType<AHeistLaserBarrierActor>(GetContractRunServerWorld());
		return IsValid(Laser) && TeleportServerPlayerToLocation(2, Laser->GetActorLocation());
	}));
	Test->AddCommand(new FWaitLatentCommand(0.75f));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, TEXT("bypassed crossing creates no additional security incident"), [State]()
	{
		UWorld* ServerWorld = GetContractRunServerWorld();
		const AHeistGameMode* GameMode = IsValid(ServerWorld) ? ServerWorld->GetAuthGameMode<AHeistGameMode>() : nullptr;
		return IsValid(GameMode) && IsSecurityHoldStateReplicated(true, true, false) &&
			GameMode->GetProcessedSecurityIncidentCount() == State->SecurityIncidentCountBaseline + 1 &&
			GameMode->GetProcessedGuardInvestigationCount() == State->SecurityInvestigationCountBaseline + 1;
	}));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, TEXT("move player 2 clear of Laser before release"), []()
	{
		return TeleportServerPlayerToLocation(2, FVector(-1500.0f, -1200.0f, 88.0f));
	}));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, TEXT("player 1 releases hold through server RPC"), []()
	{
		AHeistPlayerController* OwningController = GetOwningPlayerControllerById(1);
		AHeistSecurityHoldButtonActor* LocalButton = IsValid(OwningController)
			? FindOnlyActorOfType<AHeistSecurityHoldButtonActor>(OwningController->GetWorld())
			: nullptr;
		return InvokeSingleActorServerRPC(OwningController, FName(TEXT("Server_RequestEndSecurityHold")), LocalButton);
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, TEXT("Laser rearm grace replicated after holder release"), []()
	{
		return IsLaserRearmingReplicated();
	}, 3.0));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, TEXT("Laser fully rearmed on both peers"), []()
	{
		return IsSecurityHoldStateReplicated(false, false, true);
	}, 5.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, TEXT("record W8 Sandbox security cooperation evidence"), [Test, State]()
	{
		Test->AddInfo(FString::Printf(TEXT("W8 security cooperation: Map=SandBoxMap NetMode=ListenServer Players=2 ContractStartPlayerCount=1 DirectPIEFallback=true CCTVDetections=1 SecurityIncidents=%d GuardInvestigations=%d Holder=Player1 BypassedCrossing=true Rearmed=true Result=PASS"),
			State->SecurityIncidentCountBaseline + 1, State->SecurityInvestigationCountBaseline + 1));
		return true;
	}));
	Test->AddCommand(new FEndPlayMapCommand());
	Test->AddCommand(new FWaitLatentCommand(1.0f));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, TEXT("restore editor play settings after W8 Sandbox security test"), [State]()
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

bool EnqueueSoloPublicStartRejectedScenario(FAutomationTestBase* Test)
{
	const TSharedRef<FHeistContractRunAutomationState> State = MakeShared<FHeistContractRunAutomationState>();
	State->PlayerCount = 1;
	Test->AddCommand(new FEditorLoadMap(TEXT("/Game/Maps/TitleMenuMap")));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, TEXT("configure one-player listen-server PIE for public-start rejection"), [State]()
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
		PlaySettings->SetPlayNumberOfClients(1);
		return true;
	}));
	Test->AddCommand(new FStartPIECommand(false));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, TEXT("single title world"), []()
	{
		const TArray<UWorld*> Worlds = GetContractRunPIEWorlds();
		return Worlds.Num() == 1 && IsValid(GetContractRunServerWorld()) && IsValid(GetContractRunLocalHeistPlayerController(Worlds[0]));
	}, 45.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, TEXT("create one-player host session"), []()
	{
		UWorld* ServerWorld = GetContractRunServerWorld();
		UHeistGameInstance* GameInstance = IsValid(ServerWorld) ? Cast<UHeistGameInstance>(ServerWorld->GetGameInstance()) : nullptr;
		return IsValid(GameInstance) && GameInstance->RequestHostSession();
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, TEXT("one-player public lobby remains valid for waiting"), []()
	{
		UWorld* ServerWorld = GetContractRunServerWorld();
		const UHeistGameInstance* GameInstance = IsValid(ServerWorld) ? Cast<UHeistGameInstance>(ServerWorld->GetGameInstance()) : nullptr;
		return AreContractRunWorldsReady(1, EHeistMatchPhase::Lobby, false) && IsValid(GameInstance) && GameInstance->IsHostingOnlineSession() &&
			GameInstance->HasActiveNamedOnlineSession();
	}, 60.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, TEXT("select M01 in the one-player lobby"), []()
	{
		AHeistPlayerController* HostPlayerController = GetOwningPlayerControllerById(1);
		if (!IsValid(HostPlayerController))
		{
			return false;
		}
		HostPlayerController->RequestSetLobbyMapSelection(FName(TEXT("M01")));
		return true;
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, TEXT("M01 selection in the one-player lobby"), []()
	{
		UWorld* ServerWorld = GetContractRunServerWorld();
		const UHeistGameInstance* GameInstance = IsValid(ServerWorld) ? Cast<UHeistGameInstance>(ServerWorld->GetGameInstance()) : nullptr;
		return IsValid(GameInstance) && GameInstance->GetSelectedMapId() == FName(TEXT("M01")) && !GameInstance->IsMapSelectionUpdatePending();
	}, 15.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, TEXT("reject one-player public Contract start with MinimumPlayersRequired"), []()
	{
		UWorld* ServerWorld = GetContractRunServerWorld();
		UHeistGameInstance* GameInstance = IsValid(ServerWorld) ? Cast<UHeistGameInstance>(ServerWorld->GetGameInstance()) : nullptr;
		return IsValid(GameInstance) && !GameInstance->RequestStartSelectedGameplayMap() &&
			GameInstance->GetLastOnlineSessionFailure() == FName(TEXT("MinimumPlayersRequired")) && !GameInstance->IsSessionTravelPending();
	}));
	Test->AddCommand(new FHeistContractRunWaitCommand(Test, State, TEXT("rejected one-player public start stays in Lobby with session preserved"), []()
	{
		UWorld* ServerWorld = GetContractRunServerWorld();
		const UHeistGameInstance* GameInstance = IsValid(ServerWorld) ? Cast<UHeistGameInstance>(ServerWorld->GetGameInstance()) : nullptr;
		return AreContractRunWorldsReady(1, EHeistMatchPhase::Lobby, false) && IsValid(GameInstance) && GameInstance->HasActiveNamedOnlineSession() &&
			GameInstance->GetLastOnlineSessionFailure() == FName(TEXT("MinimumPlayersRequired"));
	}, 10.0));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, TEXT("record one-player public-start boundary evidence"), [Test]()
	{
		Test->AddInfo(TEXT("W6-010 solo boundary: HistoricalPath=SoloTwoRuns PublicStart=false Reject=MinimumPlayersRequired LobbyPreserved=true "
			"DirectGameplaySnapshotFallback=separate Result=PASS"));
		return true;
	}));
	Test->AddCommand(new FEndPlayMapCommand());
	Test->AddCommand(new FWaitLatentCommand(1.0f));
	Test->AddCommand(new FHeistContractRunActionCommand(Test, State, TEXT("restore editor play settings after solo-start boundary"), [State]()
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
	TestFalse(TEXT("One player is not a supported public Contract start"), HeistSessionContract::IsPublicStartPlayerCountSupported(1));
	TestTrue(TEXT("One player remains a direct gameplay/automation/disconnect-recovery snapshot fallback"),
		HeistSessionContract::IsSnapshotStartPlayerCountSupported(1));
	return HeistContractRunTest::EnqueueSoloPublicStartRejectedScenario(this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistFourPlayerContractRunTwoPassTest, "ProjectMuseumHeist.ContractRun.M01.FourPlayerTwoRuns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistFourPlayerContractRunTwoPassTest::RunTest(const FString& Parameters)
{
	return HeistContractRunTest::EnqueueTwoRunContractScenario(this, 4, FName(TEXT("M01")));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistTwoPlayerContractRunTwoPassTest, "ProjectMuseumHeist.ContractRun.M01.TwoPlayerTwoRuns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistTwoPlayerContractRunTwoPassTest::RunTest(const FString& Parameters)
{
	return HeistContractRunTest::EnqueueTwoRunContractScenario(this, 2, FName(TEXT("M01")));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistM02TwoPlayerContractRunTwoPassTest, "ProjectMuseumHeist.ContractRun.M02.TwoPlayerTwoRuns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistM02TwoPlayerContractRunTwoPassTest::RunTest(const FString& Parameters)
{
	return HeistContractRunTest::EnqueueTwoRunContractScenario(this, 2, FName(TEXT("M02")));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistM03TwoPlayerContractRunTwoPassTest, "ProjectMuseumHeist.ContractRun.M03.TwoPlayerTwoRuns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistM03TwoPlayerContractRunTwoPassTest::RunTest(const FString& Parameters)
{
	return HeistContractRunTest::EnqueueTwoRunContractScenario(this, 2, FName(TEXT("M03")));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistSandBoxSecurityCooperationTest, "ProjectMuseumHeist.W8.SecurityCooperation.SandBoxTwoPlayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistSandBoxSecurityCooperationTest::RunTest(const FString& Parameters)
{
	return HeistContractRunTest::EnqueueSandBoxSecurityCooperationScenario(this);
}

#endif
