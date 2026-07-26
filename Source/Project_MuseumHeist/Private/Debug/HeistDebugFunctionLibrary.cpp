#include "Debug/HeistDebugFunctionLibrary.h"

#include "AI/HeistGuardCharacter.h"
#include "AI/HeistGuardAIController.h"
#include "AI/HeistGuardNoiseReactionComponent.h"
#include "AI/HeistPatrolPathComponent.h"
#include "AI/HeistGuardStateComponent.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Character/Components/HeistActionComponent.h"
#include "Character/Components/HeistForgeryComponent.h"
#include "Character/Components/HeistInventoryComponent.h"
#include "Character/Components/HeistStatusComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Core/HeistGameInstance.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistGameState.h"
#include "Core/HeistHUD.h"
#include "Core/HeistTypes.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistPlayerState.h"
#include "Core/HeistLogChannels.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTagContainer.h"
#include "Inventory/HeistInventoryTypes.h"
#include "World/Actors/Escape/HeistVentActor.h"
#include "World/Actors/Loot/HeistPaintingDisplayCaseActor.h"
#include "World/AI/HeistGuardWaypoint.h"
#include "World/Interaction/HeistInteractableActor.h"
#include "World/Spawn/HeistLootSpawnPoint.h"
#include "UI/ViewModels/HeistLobbyViewModel.h"
#include "UI/ViewModels/HeistForgeryViewModel.h"
#include "UI/ViewModels/HeistHUDViewModel.h"
#include "UI/Widgets/HeistForgeryWidget.h"
#include "UI/Widgets/HeistHUDWidget.h"

#pragma region InternalHelpers

namespace
{
AHeistPlayerController* ResolveHeistPlayerController(APlayerController* PlayerController)
{
	return Cast<AHeistPlayerController>(PlayerController);
}

UHeistInventoryComponent* ResolveInventoryComponent(APlayerController* PlayerController)
{
	const AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	const AHeistPlayerCharacter* HeistCharacter = IsValid(HeistPlayerController) ? HeistPlayerController->GetPawn<AHeistPlayerCharacter>() : nullptr;
	return IsValid(HeistCharacter) ? HeistCharacter->GetInventoryComponent() : nullptr;
}

UHeistStatusComponent* ResolveStatusComponent(APlayerController* PlayerController)
{
	const AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	const AHeistPlayerCharacter* HeistCharacter = IsValid(HeistPlayerController) ? HeistPlayerController->GetPawn<AHeistPlayerCharacter>() : nullptr;
	return IsValid(HeistCharacter) ? HeistCharacter->GetStatusComponent() : nullptr;
}

UHeistForgeryComponent* ResolveForgeryComponent(APlayerController* PlayerController)
{
	const AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	const AHeistPlayerCharacter* HeistCharacter = IsValid(HeistPlayerController) ? HeistPlayerController->GetPawn<AHeistPlayerCharacter>() : nullptr;
	return IsValid(HeistCharacter) ? HeistCharacter->GetForgeryComponent() : nullptr;
}

UHeistForgeryComponent* ResolveScoredForgeryComponent(APlayerController* PlayerController, bool& bOutUsedAuthorityFallback)
{
	bOutUsedAuthorityFallback = false;
	UHeistForgeryComponent* DirectComponent = ResolveForgeryComponent(PlayerController);
	if (IsValid(DirectComponent) && DirectComponent->HasAuthoritativeForgeryResult())
	{
		return DirectComponent;
	}

	if (!IsValid(PlayerController) || !PlayerController->HasAuthority() || !IsValid(PlayerController->GetWorld()))
	{
		return DirectComponent;
	}

	UHeistForgeryComponent* LatestScoredComponent = nullptr;
	int32 LatestScoreRevision = INDEX_NONE;
	for (TActorIterator<AHeistPlayerCharacter> CharacterIterator(PlayerController->GetWorld()); CharacterIterator; ++CharacterIterator)
	{
		AHeistPlayerCharacter* CandidateCharacter = *CharacterIterator;
		UHeistForgeryComponent* CandidateComponent = IsValid(CandidateCharacter) ? CandidateCharacter->GetForgeryComponent() : nullptr;
		if (!IsValid(CandidateComponent) || !CandidateComponent->HasAuthoritativeForgeryResult() || CandidateComponent->GetForgeryScoreRevision() < LatestScoreRevision)
		{
			continue;
		}

		LatestScoredComponent = CandidateComponent;
		LatestScoreRevision = CandidateComponent->GetForgeryScoreRevision();
	}

	bOutUsedAuthorityFallback = IsValid(LatestScoredComponent) && LatestScoredComponent != DirectComponent;
	return IsValid(LatestScoredComponent) ? LatestScoredComponent : DirectComponent;
}

AHeistGuardCharacter* ResolveNearestGuard(APlayerController* PlayerController)
{
	if (!IsValid(PlayerController) || !IsValid(PlayerController->GetWorld()))
	{
		return nullptr;
	}

	const APawn* ReferencePawn = PlayerController->GetPawn();
	const FVector ReferenceLocation = IsValid(ReferencePawn) ? ReferencePawn->GetActorLocation() : FVector::ZeroVector;
	AHeistGuardCharacter* NearestGuard = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<AHeistGuardCharacter> GuardIterator(PlayerController->GetWorld()); GuardIterator; ++GuardIterator)
	{
		AHeistGuardCharacter* CandidateGuard = *GuardIterator;
		if (!IsValid(CandidateGuard))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(ReferenceLocation, CandidateGuard->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestGuard = CandidateGuard;
		}
	}

	return NearestGuard;
}

const TCHAR* ToQuickSlotText(const EHeistQuickSlotType SlotType)
{
	switch (SlotType)
	{
	case EHeistQuickSlotType::Coin:
		return TEXT("Coin");
	default:
		return TEXT("None");
	}
}

const TCHAR* ToInputModeText(const EHeistInputMode InputMode)
{
	switch (InputMode)
	{
	case EHeistInputMode::Gameplay:
		return TEXT("Gameplay");
	case EHeistInputMode::Inventory:
		return TEXT("Inventory");
	case EHeistInputMode::Forgery:
		return TEXT("Forgery");
	default:
		return TEXT("Unknown");
	}
}

bool TryParseQuickSlotName(const FString& SlotName, EHeistQuickSlotType& OutSlotType)
{
	const FString NormalizedSlotName = SlotName.TrimStartAndEnd().ToLower();
	if (NormalizedSlotName == TEXT("coin") || NormalizedSlotName == TEXT("q"))
	{
		OutSlotType = EHeistQuickSlotType::Coin;
		return true;
	}

	OutSlotType = EHeistQuickSlotType::None;
	return false;
}

FString FormatOptionalDistance(const float Distance)
{
	return Distance >= 0.0f ? FString::Printf(TEXT(" Distance=%.1f"), Distance) : FString();
}

FString FormatStatusTags(const TArray<FHeistTimedTagState>& StatusTags)
{
	if (StatusTags.IsEmpty())
	{
		return TEXT("None");
	}

	TArray<FString> Entries;
	Entries.Reserve(StatusTags.Num());
	for (const FHeistTimedTagState& StatusTagState : StatusTags)
	{
		Entries.Add(FString::Printf(TEXT("%s@%.2f"), *StatusTagState.StateTag.ToString(), StatusTagState.EndServerTime));
	}

	return FString::Join(Entries, TEXT(", "));
}

FString FormatResultEntry(const FHeistPlayerResult& PlayerResult)
{
	return FString::Printf(TEXT("PlayerId=%d Escaped=%s LootScore=%d FinalScore=%d LootWeight=%.2f EscapeTime=%.2f"), PlayerResult.PlayerId, PlayerResult.bEscaped ? TEXT("true") : TEXT("false"),
						   PlayerResult.LootScore, PlayerResult.FinalScore, PlayerResult.LootWeight, PlayerResult.EscapeTimeSeconds);
}

AHeistPaintingDisplayCaseActor* ResolveNearestPaintingDisplayCase(APlayerController* PlayerController)
{
	if (!IsValid(PlayerController) || !IsValid(PlayerController->GetWorld()))
	{
		return nullptr;
	}

	const APawn* ReferencePawn = PlayerController->GetPawn();
	const FVector ReferenceLocation = IsValid(ReferencePawn) ? ReferencePawn->GetActorLocation() : FVector::ZeroVector;
	AHeistPaintingDisplayCaseActor* NearestDisplayCase = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<AHeistPaintingDisplayCaseActor> DisplayCaseIterator(PlayerController->GetWorld()); DisplayCaseIterator; ++DisplayCaseIterator)
	{
		AHeistPaintingDisplayCaseActor* CandidateDisplayCase = *DisplayCaseIterator;
		if (!IsValid(CandidateDisplayCase))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(ReferenceLocation, CandidateDisplayCase->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDisplayCase = CandidateDisplayCase;
			NearestDistanceSquared = DistanceSquared;
		}
	}

	return NearestDisplayCase;
}

AHeistPlayerState* ResolveHeistPlayerStateById(APlayerController* PlayerController, const int32 PlayerId)
{
	if (!IsValid(PlayerController))
	{
		return nullptr;
	}

	if (PlayerId == INDEX_NONE)
	{
		return PlayerController->GetPlayerState<AHeistPlayerState>();
	}

	const AHeistGameState* HeistGameState = IsValid(PlayerController->GetWorld()) ? PlayerController->GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistGameState))
	{
		return nullptr;
	}

	for (APlayerState* CandidatePlayerState : HeistGameState->PlayerArray)
	{
		AHeistPlayerState* HeistPlayerState = Cast<AHeistPlayerState>(CandidatePlayerState);
		if (IsValid(HeistPlayerState) && HeistPlayerState->HeistPlayerId == PlayerId)
		{
			return HeistPlayerState;
		}
	}

	return nullptr;
}

bool TryParseObjectiveState(const FString& StateName, EHeistObjectiveState& OutState)
{
	const FString NormalizedStateName = StateName.TrimStartAndEnd().ToLower();
	if (NormalizedStateName == TEXT("inactive"))
	{
		OutState = EHeistObjectiveState::Inactive;
		return true;
	}
	if (NormalizedStateName == TEXT("available"))
	{
		OutState = EHeistObjectiveState::Available;
		return true;
	}
	if (NormalizedStateName == TEXT("inprogress") || NormalizedStateName == TEXT("progress"))
	{
		OutState = EHeistObjectiveState::InProgress;
		return true;
	}
	if (NormalizedStateName == TEXT("completed") || NormalizedStateName == TEXT("complete"))
	{
		OutState = EHeistObjectiveState::Completed;
		return true;
	}
	if (NormalizedStateName == TEXT("failed") || NormalizedStateName == TEXT("fail"))
	{
		OutState = EHeistObjectiveState::Failed;
		return true;
	}

	return false;
}

bool TryParseDisplayCaseState(const FString& StateName, EHeistDisplayCaseState& OutState)
{
	const FString NormalizedStateName = StateName.TrimStartAndEnd().ToLower();
	if (NormalizedStateName == TEXT("secured"))
	{
		OutState = EHeistDisplayCaseState::Secured;
		return true;
	}
	if (NormalizedStateName == TEXT("observed"))
	{
		OutState = EHeistDisplayCaseState::Observed;
		return true;
	}
	if (NormalizedStateName == TEXT("forgeryinprogress") || NormalizedStateName == TEXT("forgery"))
	{
		OutState = EHeistDisplayCaseState::ForgeryInProgress;
		return true;
	}
	if (NormalizedStateName == TEXT("replicaready"))
	{
		OutState = EHeistDisplayCaseState::ReplicaReady;
		return true;
	}
	if (NormalizedStateName == TEXT("replicaplaced"))
	{
		OutState = EHeistDisplayCaseState::ReplicaPlaced;
		return true;
	}
	if (NormalizedStateName == TEXT("originalavailable"))
	{
		OutState = EHeistDisplayCaseState::OriginalAvailable;
		return true;
	}
	if (NormalizedStateName == TEXT("originalremoved"))
	{
		OutState = EHeistDisplayCaseState::OriginalRemoved;
		return true;
	}

	return false;
}

bool TryParseMatchPhase(const FString& PhaseName, EHeistMatchPhase& OutPhase)
{
	const FString NormalizedPhaseName = PhaseName.TrimStartAndEnd().ToLower();
	if (NormalizedPhaseName == TEXT("none"))
	{
		OutPhase = EHeistMatchPhase::None;
		return true;
	}
	if (NormalizedPhaseName == TEXT("lobby"))
	{
		OutPhase = EHeistMatchPhase::Lobby;
		return true;
	}
	if (NormalizedPhaseName == TEXT("loadout"))
	{
		OutPhase = EHeistMatchPhase::Loadout;
		return true;
	}
	if (NormalizedPhaseName == TEXT("readycountdown") || NormalizedPhaseName == TEXT("ready"))
	{
		OutPhase = EHeistMatchPhase::ReadyCountdown;
		return true;
	}
	if (NormalizedPhaseName == TEXT("ingame") || NormalizedPhaseName == TEXT("game"))
	{
		OutPhase = EHeistMatchPhase::InGame;
		return true;
	}
	if (NormalizedPhaseName == TEXT("end"))
	{
		OutPhase = EHeistMatchPhase::End;
		return true;
	}

	return false;
}
}

#pragma endregion

#pragma region ObjectiveDebug

void UHeistDebugFunctionLibrary::DebugObjectiveHelp(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	Message(
		PlayerController,
		TEXT(
			"Objective debug commands: HeistObjectiveDump | HeistM01ObjectiveDump | HeistGrayboxDump <M02|M03> | HeistObjectiveSet <ArtifactId> <CaseId> <Inactive|Available|InProgress|Completed|Failed> <UseLocalPlayerAsCarrier 0|1>. Run placement dump and Set in the listen-server window."),
		EHeistDebugLevel::Info, true, 10.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugObjectiveDump(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	const AHeistGameState* HeistGameState = IsValid(PlayerController) && IsValid(PlayerController->GetWorld()) ? PlayerController->GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistGameState))
	{
		Message(PlayerController, TEXT("Objective dump failed: missing Heist GameState."), EHeistDebugLevel::Warning, true);
		return;
	}

	const AHeistPlayerState* CarrierCandidate = HeistGameState->GetOriginalCarrierCandidate();
	Message(PlayerController,
			FString::Printf(TEXT("Objective dump: Target=%s CaseId=%s State=%s CarrierCandidate=%s CarrierPlayerId=%d Revision=%d Authority=%s"),
							*HeistGameState->GetActiveTargetArtifactId().ToString(), *HeistGameState->GetActiveTargetCaseId().ToString(), *UEnum::GetValueAsString(HeistGameState->GetObjectiveState()),
							*GetNameSafe(CarrierCandidate), IsValid(CarrierCandidate) ? CarrierCandidate->HeistPlayerId : INDEX_NONE, HeistGameState->GetObjectiveRevision(),
							HeistGameState->HasAuthority() ? TEXT("true") : TEXT("false")),
			EHeistDebugLevel::Info, true, 10.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugM01ObjectivePlacementDump(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	UWorld* World = IsValid(PlayerController) ? PlayerController->GetWorld() : nullptr;
	const AHeistGameState* HeistGameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(World) || !IsValid(HeistGameState))
	{
		Message(PlayerController, TEXT("M01 objective placement: Result=FAIL Reason=MissingWorldOrGameState"), EHeistDebugLevel::Error, true);
		return;
	}

	TArray<APlayerStart*> EntryCandidates;
	for (TActorIterator<APlayerStart> PlayerStartIterator(World); PlayerStartIterator; ++PlayerStartIterator)
	{
		if (APlayerStart* PlayerStart = *PlayerStartIterator; IsValid(PlayerStart))
		{
			EntryCandidates.Add(PlayerStart);
		}
	}

	const FName ExpectedCaseId = HeistGameState->GetActiveTargetCaseId().IsNone() ? FName(TEXT("Case_M01_Target")) : HeistGameState->GetActiveTargetCaseId();
	TArray<AHeistPaintingDisplayCaseActor*> MatchingTargetCases;
	for (TActorIterator<AHeistPaintingDisplayCaseActor> DisplayCaseIterator(World); DisplayCaseIterator; ++DisplayCaseIterator)
	{
		AHeistPaintingDisplayCaseActor* DisplayCase = *DisplayCaseIterator;
		if (IsValid(DisplayCase) && DisplayCase->GetDisplayCaseId() == ExpectedCaseId)
		{
			MatchingTargetCases.Add(DisplayCase);
		}
	}

	TArray<AHeistGuardCharacter*> GuardCandidates;
	for (TActorIterator<AHeistGuardCharacter> GuardIterator(World); GuardIterator; ++GuardIterator)
	{
		if (AHeistGuardCharacter* Guard = *GuardIterator; IsValid(Guard))
		{
			GuardCandidates.Add(Guard);
		}
	}

	TArray<AHeistVentActor*> ExitCandidates;
	for (TActorIterator<AHeistVentActor> VentIterator(World); VentIterator; ++VentIterator)
	{
		if (AHeistVentActor* Vent = *VentIterator; IsValid(Vent))
		{
			ExitCandidates.Add(Vent);
		}
	}

	AHeistPaintingDisplayCaseActor* TargetDisplayCase = MatchingTargetCases.Num() == 1 ? MatchingTargetCases[0] : nullptr;
	APlayerStart* NearestEntry = nullptr;
	AHeistGuardCharacter* NearestGuard = nullptr;
	AHeistVentActor* NearestExit = nullptr;
	float EntryToCaseDistance = -1.0f;
	float GuardToCaseDistance = -1.0f;
	float CaseToExitDistance = -1.0f;

	if (IsValid(TargetDisplayCase))
	{
		for (APlayerStart* EntryCandidate : EntryCandidates)
		{
			const float CandidateDistance = FVector::Dist(EntryCandidate->GetActorLocation(), TargetDisplayCase->GetActorLocation());
			if (!IsValid(NearestEntry) || CandidateDistance < EntryToCaseDistance)
			{
				NearestEntry = EntryCandidate;
				EntryToCaseDistance = CandidateDistance;
			}
		}

		for (AHeistGuardCharacter* GuardCandidate : GuardCandidates)
		{
			const float CandidateDistance = FVector::Dist(GuardCandidate->GetActorLocation(), TargetDisplayCase->GetActorLocation());
			if (!IsValid(NearestGuard) || CandidateDistance < GuardToCaseDistance)
			{
				NearestGuard = GuardCandidate;
				GuardToCaseDistance = CandidateDistance;
			}
		}

		for (AHeistVentActor* ExitCandidate : ExitCandidates)
		{
			const float CandidateDistance = FVector::Dist(TargetDisplayCase->GetActorLocation(), ExitCandidate->GetActorLocation());
			if (!IsValid(NearestExit) || CandidateDistance < CaseToExitDistance)
			{
				NearestExit = ExitCandidate;
				CaseToExitDistance = CandidateDistance;
			}
		}
	}

	const bool bObjectiveSnapshotReady = IsValid(TargetDisplayCase) && HeistGameState->GetActiveTargetArtifactId() == TargetDisplayCase->GetTargetArtifactId() &&
										 HeistGameState->GetActiveTargetCaseId() == TargetDisplayCase->GetDisplayCaseId() && HeistGameState->GetObjectiveState() != EHeistObjectiveState::Inactive;
	const bool bTargetCaseReady = IsValid(TargetDisplayCase) && TargetDisplayCase->GetDisplayCaseState() == EHeistDisplayCaseState::Secured && !TargetDisplayCase->GetTargetArtifactId().IsNone();
	const bool bRouteCandidatesReady = IsValid(NearestEntry) && IsValid(NearestGuard) && IsValid(NearestExit) && EntryToCaseDistance > KINDA_SMALL_NUMBER && GuardToCaseDistance > KINDA_SMALL_NUMBER &&
									   CaseToExitDistance > KINDA_SMALL_NUMBER;
	const bool bPlacementContractPass =
		EntryCandidates.Num() > 0 && MatchingTargetCases.Num() == 1 && GuardCandidates.Num() > 0 && ExitCandidates.Num() > 0 && bObjectiveSnapshotReady && bTargetCaseReady && bRouteCandidatesReady;

	if (IsValid(TargetDisplayCase))
	{
		const FVector CaseLocation = TargetDisplayCase->GetActorLocation();
		DrawDebugSphere(World, CaseLocation, 75.0f, 16, FColor::Yellow, false, 20.0f, 0, 3.0f);
		if (IsValid(NearestEntry))
		{
			DrawDebugLine(World, NearestEntry->GetActorLocation(), CaseLocation, FColor::Cyan, false, 20.0f, 0, 5.0f);
		}
		if (IsValid(NearestExit))
		{
			DrawDebugLine(World, CaseLocation, NearestExit->GetActorLocation(), FColor::Green, false, 20.0f, 0, 5.0f);
		}
		if (IsValid(NearestGuard))
		{
			DrawDebugLine(World, NearestGuard->GetActorLocation(), CaseLocation, FColor::Orange, false, 20.0f, 0, 5.0f);
		}
	}

	Message(
		PlayerController,
		FString::Printf(
			TEXT(
				"M01 objective placement: Map=%s Entries=%d TargetCaseId=%s MatchingTargetCases=%d TargetCaseState=%s TargetCaseReady=%s Guards=%d ExitCandidates=%d ObjectiveTarget=%s ObjectiveState=%s ObjectiveSnapshotReady=%s RouteCandidatesReady=%s Entry=%s EntryToCase=%.1f Guard=%s GuardToCase=%.1f Exit=%s CaseToExit=%.1f Result=%s"),
			*World->GetMapName(), EntryCandidates.Num(), *ExpectedCaseId.ToString(), MatchingTargetCases.Num(),
			IsValid(TargetDisplayCase) ? *UEnum::GetValueAsString(TargetDisplayCase->GetDisplayCaseState()) : TEXT("Missing"), bTargetCaseReady ? TEXT("true") : TEXT("false"), GuardCandidates.Num(),
			ExitCandidates.Num(), *HeistGameState->GetActiveTargetArtifactId().ToString(), *UEnum::GetValueAsString(HeistGameState->GetObjectiveState()),
			bObjectiveSnapshotReady ? TEXT("true") : TEXT("false"), bRouteCandidatesReady ? TEXT("true") : TEXT("false"), *GetNameSafe(NearestEntry), EntryToCaseDistance, *GetNameSafe(NearestGuard),
			GuardToCaseDistance, *GetNameSafe(NearestExit), CaseToExitDistance, bPlacementContractPass ? TEXT("PASS") : TEXT("FAIL")),
		bPlacementContractPass ? EHeistDebugLevel::Info : EHeistDebugLevel::Error, true, 12.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugCoreGrayboxDump(APlayerController* PlayerController, const FString& MapId)
{
#if !UE_BUILD_SHIPPING
	UWorld* World = IsValid(PlayerController) ? PlayerController->GetWorld() : nullptr;
	const AHeistGameState* HeistGameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(World) || !IsValid(HeistGameState))
	{
		Message(PlayerController, TEXT("Core graybox: Result=FAIL Reason=MissingWorldOrGameState"), EHeistDebugLevel::Error, true);
		return;
	}

	FString NormalizedMapId = MapId.TrimStartAndEnd();
	NormalizedMapId.ToUpperInline();
	if (NormalizedMapId != TEXT("M02") && NormalizedMapId != TEXT("M03"))
	{
		Message(PlayerController, FString::Printf(TEXT("Core graybox: RequestedMapId=%s Result=FAIL Reason=ExpectedM02OrM03"), *MapId), EHeistDebugLevel::Error, true);
		return;
	}

	const FName ExpectedCaseId(*FString::Printf(TEXT("Case_%s_Target"), *NormalizedMapId));

	TArray<APlayerStart*> EntryCandidates;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		if (APlayerStart* PlayerStart = *It; IsValid(PlayerStart))
		{
			EntryCandidates.Add(PlayerStart);
		}
	}

	TArray<AHeistPaintingDisplayCaseActor*> MatchingTargetCases;
	for (TActorIterator<AHeistPaintingDisplayCaseActor> It(World); It; ++It)
	{
		AHeistPaintingDisplayCaseActor* DisplayCase = *It;
		if (IsValid(DisplayCase) && DisplayCase->GetDisplayCaseId() == ExpectedCaseId)
		{
			MatchingTargetCases.Add(DisplayCase);
		}
	}

	TArray<AHeistGuardCharacter*> GuardCandidates;
	for (TActorIterator<AHeistGuardCharacter> It(World); It; ++It)
	{
		if (AHeistGuardCharacter* Guard = *It; IsValid(Guard))
		{
			GuardCandidates.Add(Guard);
		}
	}

	TArray<AHeistGuardWaypoint*> WaypointCandidates;
	for (TActorIterator<AHeistGuardWaypoint> It(World); It; ++It)
	{
		if (AHeistGuardWaypoint* Waypoint = *It; IsValid(Waypoint))
		{
			WaypointCandidates.Add(Waypoint);
		}
	}

	TArray<AHeistLootSpawnPoint*> LooseLootCandidates;
	for (TActorIterator<AHeistLootSpawnPoint> It(World); It; ++It)
	{
		AHeistLootSpawnPoint* SpawnPoint = *It;
		if (!IsValid(SpawnPoint) || !SpawnPoint->IsSpawnEnabled())
		{
			continue;
		}

		const EHeistSpawnCategory SpawnCategory = SpawnPoint->GetSpawnCategory();
		if (SpawnCategory == EHeistSpawnCategory::VaultFixed || SpawnCategory == EHeistSpawnCategory::ExhibitionRoom)
		{
			LooseLootCandidates.Add(SpawnPoint);
		}
	}

	TArray<AHeistVentActor*> ExitCandidates;
	for (TActorIterator<AHeistVentActor> It(World); It; ++It)
	{
		if (AHeistVentActor* Vent = *It; IsValid(Vent))
		{
			ExitCandidates.Add(Vent);
		}
	}

	AHeistPaintingDisplayCaseActor* TargetDisplayCase = MatchingTargetCases.Num() == 1 ? MatchingTargetCases[0] : nullptr;
	APlayerStart* NearestEntry = nullptr;
	AHeistGuardCharacter* NearestGuard = nullptr;
	AHeistLootSpawnPoint* NearestLoot = nullptr;
	AHeistVentActor* NearestExit = nullptr;
	float EntryToCaseDistance = -1.0f;
	float GuardToCaseDistance = -1.0f;
	float CaseToLootDistance = -1.0f;
	float CaseToExitDistance = -1.0f;

	if (IsValid(TargetDisplayCase))
	{
		const FVector CaseLocation = TargetDisplayCase->GetActorLocation();
		for (APlayerStart* EntryCandidate : EntryCandidates)
		{
			const float CandidateDistance = FVector::Dist(EntryCandidate->GetActorLocation(), CaseLocation);
			if (!IsValid(NearestEntry) || CandidateDistance < EntryToCaseDistance)
			{
				NearestEntry = EntryCandidate;
				EntryToCaseDistance = CandidateDistance;
			}
		}

		for (AHeistGuardCharacter* GuardCandidate : GuardCandidates)
		{
			const float CandidateDistance = FVector::Dist(GuardCandidate->GetActorLocation(), CaseLocation);
			if (!IsValid(NearestGuard) || CandidateDistance < GuardToCaseDistance)
			{
				NearestGuard = GuardCandidate;
				GuardToCaseDistance = CandidateDistance;
			}
		}

		for (AHeistLootSpawnPoint* LootCandidate : LooseLootCandidates)
		{
			const float CandidateDistance = FVector::Dist(LootCandidate->GetActorLocation(), CaseLocation);
			if (!IsValid(NearestLoot) || CandidateDistance < CaseToLootDistance)
			{
				NearestLoot = LootCandidate;
				CaseToLootDistance = CandidateDistance;
			}
		}

		for (AHeistVentActor* ExitCandidate : ExitCandidates)
		{
			const float CandidateDistance = FVector::Dist(ExitCandidate->GetActorLocation(), CaseLocation);
			if (!IsValid(NearestExit) || CandidateDistance < CaseToExitDistance)
			{
				NearestExit = ExitCandidate;
				CaseToExitDistance = CandidateDistance;
			}
		}
	}

	int32 ReadyGuardRoutes = 0;
	FName FirstReadyRouteId = NAME_None;
	int32 FirstReadyRouteWaypointCount = 0;
	for (const AHeistGuardCharacter* GuardCandidate : GuardCandidates)
	{
		const UHeistPatrolPathComponent* PatrolPath = IsValid(GuardCandidate) ? GuardCandidate->GetPatrolPathComponent() : nullptr;
		if (!IsValid(PatrolPath) || PatrolPath->GetPatrolRouteId().IsNone())
		{
			continue;
		}

		TSet<int32> UniquePatrolOrders;
		for (const AHeistGuardWaypoint* WaypointCandidate : WaypointCandidates)
		{
			if (IsValid(WaypointCandidate) && WaypointCandidate->GetPatrolRouteId() == PatrolPath->GetPatrolRouteId())
			{
				UniquePatrolOrders.Add(WaypointCandidate->GetPatrolOrder());
			}
		}

		if (UniquePatrolOrders.Num() >= 2)
		{
			++ReadyGuardRoutes;
			if (FirstReadyRouteId.IsNone())
			{
				FirstReadyRouteId = PatrolPath->GetPatrolRouteId();
				FirstReadyRouteWaypointCount = UniquePatrolOrders.Num();
			}
		}
	}

	const bool bTargetCaseReady = IsValid(TargetDisplayCase) && TargetDisplayCase->GetDisplayCaseState() == EHeistDisplayCaseState::Secured && !TargetDisplayCase->GetTargetArtifactId().IsNone();
	const bool bObjectiveSnapshotReady = IsValid(TargetDisplayCase) && HeistGameState->GetActiveTargetCaseId() == ExpectedCaseId &&
										 HeistGameState->GetActiveTargetArtifactId() == TargetDisplayCase->GetTargetArtifactId() &&
										 HeistGameState->GetObjectiveState() != EHeistObjectiveState::Inactive;
	const bool bLayoutContractPass = EntryCandidates.Num() > 0 && MatchingTargetCases.Num() == 1 && bTargetCaseReady && GuardCandidates.Num() > 0 && ReadyGuardRoutes > 0 &&
									 !LooseLootCandidates.IsEmpty() && !ExitCandidates.IsEmpty() && bObjectiveSnapshotReady;

	if (IsValid(TargetDisplayCase))
	{
		const FVector CaseLocation = TargetDisplayCase->GetActorLocation();
		DrawDebugSphere(World, CaseLocation, 75.0f, 16, FColor::Yellow, false, 20.0f, 0, 3.0f);
		if (IsValid(NearestEntry))
		{
			DrawDebugLine(World, NearestEntry->GetActorLocation(), CaseLocation, FColor::Cyan, false, 20.0f, 0, 5.0f);
		}
		if (IsValid(NearestLoot))
		{
			DrawDebugLine(World, CaseLocation, NearestLoot->GetActorLocation(), FColor::Magenta, false, 20.0f, 0, 5.0f);
		}
		if (IsValid(NearestExit))
		{
			DrawDebugLine(World, CaseLocation, NearestExit->GetActorLocation(), FColor::Green, false, 20.0f, 0, 5.0f);
		}
	}

	Message(
		PlayerController,
		FString::Printf(
			TEXT(
				"Core graybox: RequestedMapId=%s RuntimeMap=%s Entries=%d TargetCaseId=%s MatchingTargetCases=%d TargetCaseReady=%s Guards=%d GuardRoutesReady=%d FirstRoute=%s RouteWaypoints=%d LooseLootCandidates=%d ExitCandidates=%d ObjectiveSnapshotReady=%s EntryToCase=%.1f GuardToCase=%.1f CaseToLoot=%.1f CaseToExit=%.1f LayoutResult=%s Traversal=USER_CHECK"),
			*NormalizedMapId, *World->GetMapName(), EntryCandidates.Num(), *ExpectedCaseId.ToString(), MatchingTargetCases.Num(), bTargetCaseReady ? TEXT("true") : TEXT("false"),
			GuardCandidates.Num(), ReadyGuardRoutes, *FirstReadyRouteId.ToString(), FirstReadyRouteWaypointCount, LooseLootCandidates.Num(), ExitCandidates.Num(),
			bObjectiveSnapshotReady ? TEXT("true") : TEXT("false"), EntryToCaseDistance, GuardToCaseDistance, CaseToLootDistance, CaseToExitDistance,
			bLayoutContractPass ? TEXT("PASS") : TEXT("FAIL")),
		bLayoutContractPass ? EHeistDebugLevel::Info : EHeistDebugLevel::Error, true, 15.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugObjectiveSet(APlayerController* PlayerController, const FName ArtifactId, const FName CaseId, const FString& StateName, const bool bUseLocalPlayerAsCarrier)
{
#if !UE_BUILD_SHIPPING
	AHeistGameState* HeistGameState = IsValid(PlayerController) && IsValid(PlayerController->GetWorld()) ? PlayerController->GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistGameState))
	{
		Message(PlayerController, TEXT("Objective set failed: missing Heist GameState."), EHeistDebugLevel::Warning, true);
		return;
	}

	EHeistObjectiveState ParsedState = EHeistObjectiveState::Inactive;
	if (!TryParseObjectiveState(StateName, ParsedState))
	{
		Message(PlayerController, FString::Printf(TEXT("Objective set failed: invalid state '%s'."), *StateName), EHeistDebugLevel::Warning, true);
		return;
	}

	AHeistPlayerState* CarrierCandidate = bUseLocalPlayerAsCarrier && IsValid(PlayerController) ? PlayerController->GetPlayerState<AHeistPlayerState>() : nullptr;
	const bool bChanged = HeistGameState->SetObjectiveSnapshot(ArtifactId, CaseId, ParsedState, CarrierCandidate);

	Message(PlayerController,
			FString::Printf(TEXT("Objective debug set: Result=%s Target=%s CaseId=%s State=%s CarrierCandidate=%s"), bChanged ? TEXT("PASS") : TEXT("REJECTED"), *ArtifactId.ToString(),
							*CaseId.ToString(), *UEnum::GetValueAsString(ParsedState), *GetNameSafe(CarrierCandidate)),
			bChanged ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 10.0f);
#endif
}

#pragma endregion

#pragma region DisplayCaseDebug

void UHeistDebugFunctionLibrary::DebugDisplayCaseHelp(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	Message(
		PlayerController,
		TEXT(
			"Display case commands: HeistCaseSpawn <Distance> | HeistCaseSpawnFor <PlayerId> <Distance> | HeistCaseDump | HeistCaseBegin <PlayerId|-1> | HeistCaseCancel <PlayerId|-1> | HeistCasePhase <Phase> | HeistCaseAdvance | HeistCaseSet <State>. Run mutating commands in the listen-server window."),
		EHeistDebugLevel::Info, true, 10.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugDisplayCaseSpawnFor(APlayerController* PlayerController, const int32 PlayerId, const float Distance)
{
#if !UE_BUILD_SHIPPING
	if (!IsValid(PlayerController) || !PlayerController->HasAuthority() || !IsValid(PlayerController->GetWorld()))
	{
		Message(PlayerController, TEXT("Display case spawn-for rejected: Reason=NotAuthorityOrMissingWorld"), EHeistDebugLevel::Warning, true);
		return;
	}

	AHeistPlayerState* TargetPlayerState = ResolveHeistPlayerStateById(PlayerController, PlayerId);
	APawn* TargetPawn = IsValid(TargetPlayerState) ? TargetPlayerState->GetPawn() : nullptr;
	if (!IsValid(TargetPlayerState) || !IsValid(TargetPawn))
	{
		Message(PlayerController, FString::Printf(TEXT("Display case spawn-for rejected: PlayerId=%d Reason=MissingPlayerStateOrPawn"), PlayerId), EHeistDebugLevel::Warning, true);
		return;
	}

	const FVector SpawnLocation = TargetPawn->GetActorLocation() + TargetPawn->GetActorForwardVector() * FMath::Max(100.0f, Distance);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AHeistPaintingDisplayCaseActor* DisplayCase =
		PlayerController->GetWorld()->SpawnActor<AHeistPaintingDisplayCaseActor>(AHeistPaintingDisplayCaseActor::StaticClass(), SpawnLocation, FRotator::ZeroRotator, SpawnParameters);

	Message(PlayerController,
			FString::Printf(TEXT("Display case debug spawn-for: Result=%s Case=%s PlayerId=%d Distance=%.1f"), IsValid(DisplayCase) ? TEXT("PASS") : TEXT("FAIL"), *GetNameSafe(DisplayCase), PlayerId,
							FVector::Distance(TargetPawn->GetActorLocation(), SpawnLocation)),
			IsValid(DisplayCase) ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 10.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugDisplayCaseSpawn(APlayerController* PlayerController, const float Distance)
{
#if !UE_BUILD_SHIPPING
	if (!IsValid(PlayerController) || !PlayerController->HasAuthority() || !IsValid(PlayerController->GetWorld()))
	{
		Message(PlayerController, TEXT("Display case spawn rejected: Reason=NotAuthorityOrMissingWorld"), EHeistDebugLevel::Warning, true);
		return;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector SpawnLocation = ViewLocation + ViewRotation.Vector() * FMath::Max(100.0f, Distance);

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AHeistPaintingDisplayCaseActor* DisplayCase =
		PlayerController->GetWorld()->SpawnActor<AHeistPaintingDisplayCaseActor>(AHeistPaintingDisplayCaseActor::StaticClass(), SpawnLocation, FRotator::ZeroRotator, SpawnParameters);

	Message(PlayerController,
			FString::Printf(TEXT("Display case debug spawn: Result=%s Case=%s State=%s Authority=%s"), IsValid(DisplayCase) ? TEXT("PASS") : TEXT("FAIL"), *GetNameSafe(DisplayCase),
							IsValid(DisplayCase) ? *UEnum::GetValueAsString(DisplayCase->GetDisplayCaseState()) : TEXT("None"),
							IsValid(DisplayCase) && DisplayCase->HasAuthority() ? TEXT("true") : TEXT("false")),
			IsValid(DisplayCase) ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 10.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugDisplayCaseDump(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	const AHeistPaintingDisplayCaseActor* DisplayCase = ResolveNearestPaintingDisplayCase(PlayerController);
	if (!IsValid(DisplayCase))
	{
		Message(PlayerController, TEXT("Display case dump failed: Reason=MissingDisplayCase"), EHeistDebugLevel::Warning, true);
		return;
	}

	const AHeistGameState* HeistGameState = IsValid(PlayerController) && IsValid(PlayerController->GetWorld()) ? PlayerController->GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const AHeistPlayerState* SessionOwner = DisplayCase->GetSessionOwner();
	bool bExpectedOriginalVisible = false;
	bool bExpectedReplicaVisible = false;
	int32 OriginalComponentCount = 0;
	int32 ReplicaComponentCount = 0;
	bool bVisualComponentsMatchExpectedState = false;
	DisplayCase->GetPlaceholderVisualDebugState(bExpectedOriginalVisible, bExpectedReplicaVisible, OriginalComponentCount, ReplicaComponentCount, bVisualComponentsMatchExpectedState);
	Message(PlayerController,
			FString::Printf(TEXT("Display case dump: Case=%s State=%s Owner=%s OwnerPlayerId=%d Locked=%s Revision=%d MaxDistance=%.1f MatchPhase=%s Authority=%s Replicates=%s"),
							*GetNameSafe(DisplayCase), *UEnum::GetValueAsString(DisplayCase->GetDisplayCaseState()), *GetNameSafe(SessionOwner),
							IsValid(SessionOwner) ? SessionOwner->HeistPlayerId : INDEX_NONE, DisplayCase->IsSessionLocked() ? TEXT("true") : TEXT("false"), DisplayCase->GetSessionRevision(),
							DisplayCase->GetMaximumSessionDistance(), IsValid(HeistGameState) ? *UEnum::GetValueAsString(HeistGameState->GetMatchPhase()) : TEXT("MissingGameState"),
							DisplayCase->HasAuthority() ? TEXT("true") : TEXT("false"), DisplayCase->GetIsReplicated() ? TEXT("true") : TEXT("false")),
			EHeistDebugLevel::Info, true, 10.0f);

	const bool bHasRequiredVisualComponents = OriginalComponentCount > 0 && ReplicaComponentCount > 0;
	Message(PlayerController,
			FString::Printf(
				TEXT("Display case placeholder visual: Case=%s State=%s OriginalVisible=%s ReplicaVisible=%s OriginalComponents=%d ReplicaComponents=%d ComponentsMatch=%s Authority=%s Result=%s"),
				*GetNameSafe(DisplayCase), *UEnum::GetValueAsString(DisplayCase->GetDisplayCaseState()), bExpectedOriginalVisible ? TEXT("true") : TEXT("false"),
				bExpectedReplicaVisible ? TEXT("true") : TEXT("false"), OriginalComponentCount, ReplicaComponentCount, bVisualComponentsMatchExpectedState ? TEXT("true") : TEXT("false"),
				DisplayCase->HasAuthority() ? TEXT("true") : TEXT("false"),
				!bHasRequiredVisualComponents		  ? TEXT("MISSING_COMPONENTS")
				: bVisualComponentsMatchExpectedState ? TEXT("PASS")
													  : TEXT("FAIL")),
			bVisualComponentsMatchExpectedState ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 10.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugDisplayCaseBegin(APlayerController* PlayerController, const int32 PlayerId)
{
#if !UE_BUILD_SHIPPING
	AHeistPaintingDisplayCaseActor* DisplayCase = ResolveNearestPaintingDisplayCase(PlayerController);
	AHeistPlayerState* RequestingPlayerState = ResolveHeistPlayerStateById(PlayerController, PlayerId);
	if (!IsValid(DisplayCase))
	{
		Message(PlayerController, FString::Printf(TEXT("Display case debug begin: Result=REJECTED PlayerId=%d Reason=MissingDisplayCase"), PlayerId), EHeistDebugLevel::Warning, true);
		return;
	}

	const bool bBegan = DisplayCase->TryBeginSession(RequestingPlayerState);
	Message(PlayerController,
			FString::Printf(TEXT("Display case debug begin: Result=%s PlayerId=%d OwnerPlayerId=%d Locked=%s Revision=%d"), bBegan ? TEXT("PASS") : TEXT("REJECTED"),
							IsValid(RequestingPlayerState) ? RequestingPlayerState->HeistPlayerId : PlayerId,
							IsValid(DisplayCase->GetSessionOwner()) ? DisplayCase->GetSessionOwner()->HeistPlayerId : INDEX_NONE, DisplayCase->IsSessionLocked() ? TEXT("true") : TEXT("false"),
							DisplayCase->GetSessionRevision()),
			bBegan ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 10.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugDisplayCaseCancel(APlayerController* PlayerController, const int32 PlayerId)
{
#if !UE_BUILD_SHIPPING
	AHeistPaintingDisplayCaseActor* DisplayCase = ResolveNearestPaintingDisplayCase(PlayerController);
	AHeistPlayerState* RequestingPlayerState = ResolveHeistPlayerStateById(PlayerController, PlayerId);
	if (!IsValid(DisplayCase) || !IsValid(RequestingPlayerState))
	{
		Message(PlayerController, FString::Printf(TEXT("Display case debug cancel: Result=REJECTED PlayerId=%d Reason=MissingCaseOrPlayerState"), PlayerId), EHeistDebugLevel::Warning, true);
		return;
	}

	const bool bCancelled = DisplayCase->TryCancelSession(RequestingPlayerState);
	Message(PlayerController,
			FString::Printf(TEXT("Display case debug cancel: Result=%s PlayerId=%d Locked=%s Revision=%d"), bCancelled ? TEXT("PASS") : TEXT("REJECTED"), RequestingPlayerState->HeistPlayerId,
							DisplayCase->IsSessionLocked() ? TEXT("true") : TEXT("false"), DisplayCase->GetSessionRevision()),
			bCancelled ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 10.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugDisplayCasePhase(APlayerController* PlayerController, const FString& PhaseName)
{
#if !UE_BUILD_SHIPPING
	AHeistGameState* HeistGameState = IsValid(PlayerController) && IsValid(PlayerController->GetWorld()) ? PlayerController->GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	EHeistMatchPhase ParsedPhase = EHeistMatchPhase::None;
	if (!IsValid(HeistGameState) || !TryParseMatchPhase(PhaseName, ParsedPhase))
	{
		Message(PlayerController, FString::Printf(TEXT("Display case debug phase: Result=REJECTED Phase='%s' Reason=MissingGameStateOrInvalidPhase"), *PhaseName), EHeistDebugLevel::Warning, true);
		return;
	}

	const EHeistMatchPhase PreviousPhase = HeistGameState->GetMatchPhase();
	const bool bChanged = HeistGameState->SetMatchPhase(ParsedPhase);
	Message(PlayerController,
			FString::Printf(TEXT("Display case debug phase: Result=%s Previous=%s New=%s"), bChanged ? TEXT("PASS") : TEXT("REJECTED"), *UEnum::GetValueAsString(PreviousPhase),
							*UEnum::GetValueAsString(HeistGameState->GetMatchPhase())),
			bChanged ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 10.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugDisplayCaseAdvance(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	AHeistPaintingDisplayCaseActor* DisplayCase = ResolveNearestPaintingDisplayCase(PlayerController);
	if (!IsValid(DisplayCase))
	{
		Message(PlayerController, TEXT("Display case advance failed: Reason=MissingDisplayCase"), EHeistDebugLevel::Warning, true);
		return;
	}

	const EHeistDisplayCaseState PreviousState = DisplayCase->GetDisplayCaseState();
	const bool bChanged = DisplayCase->TryAdvanceDisplayCaseState();
	Message(PlayerController,
			FString::Printf(TEXT("Display case debug advance: Result=%s Previous=%s New=%s"), bChanged ? TEXT("PASS") : TEXT("REJECTED"), *UEnum::GetValueAsString(PreviousState),
							*UEnum::GetValueAsString(DisplayCase->GetDisplayCaseState())),
			bChanged ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 10.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugDisplayCaseSet(APlayerController* PlayerController, const FString& StateName)
{
#if !UE_BUILD_SHIPPING
	AHeistPaintingDisplayCaseActor* DisplayCase = ResolveNearestPaintingDisplayCase(PlayerController);
	if (!IsValid(DisplayCase))
	{
		Message(PlayerController, TEXT("Display case set failed: Reason=MissingDisplayCase"), EHeistDebugLevel::Warning, true);
		return;
	}

	EHeistDisplayCaseState ParsedState = EHeistDisplayCaseState::Secured;
	if (!TryParseDisplayCaseState(StateName, ParsedState))
	{
		Message(PlayerController, FString::Printf(TEXT("Display case set failed: invalid state '%s'."), *StateName), EHeistDebugLevel::Warning, true);
		return;
	}

	const EHeistDisplayCaseState PreviousState = DisplayCase->GetDisplayCaseState();
	const bool bChanged = DisplayCase->TryTransitionToDisplayCaseState(ParsedState);
	Message(PlayerController,
			FString::Printf(TEXT("Display case debug set: Result=%s Previous=%s Requested=%s Current=%s"), bChanged ? TEXT("PASS") : TEXT("REJECTED"), *UEnum::GetValueAsString(PreviousState),
							*UEnum::GetValueAsString(ParsedState), *UEnum::GetValueAsString(DisplayCase->GetDisplayCaseState())),
			bChanged ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 10.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugOriginalHelp(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	Message(PlayerController,
			TEXT("Original carry commands: HeistOriginalDump | HeistOriginalTake | HeistOriginalDrop. Prepare nearest case in OriginalAvailable. Drop policy returns the Original to its source case."),
			EHeistDebugLevel::Info, true, 10.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugOriginalDump(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	const AHeistPlayerController* HeistPlayerController = Cast<AHeistPlayerController>(PlayerController);
	const AHeistPlayerCharacter* PlayerCharacter = IsValid(HeistPlayerController) ? HeistPlayerController->GetPawn<AHeistPlayerCharacter>() : nullptr;
	const AHeistPlayerState* HeistPlayerState = IsValid(HeistPlayerController) ? HeistPlayerController->GetPlayerState<AHeistPlayerState>() : nullptr;
	const UHeistInventoryComponent* InventoryComponent = IsValid(PlayerCharacter) ? PlayerCharacter->GetInventoryComponent() : nullptr;
	const AHeistPaintingDisplayCaseActor* DisplayCase = ResolveNearestPaintingDisplayCase(PlayerController);
	if (!IsValid(HeistPlayerState) || !IsValid(InventoryComponent))
	{
		Message(PlayerController, TEXT("Original carry dump: Result=FAIL Reason=MissingPlayerStateOrInventory"), EHeistDebugLevel::Warning, true);
		return;
	}

	const FHeistOriginalCarryEntry& CarryEntry = InventoryComponent->GetOriginalCarryEntry();
	const AHeistPlayerState* CaseCarrier = IsValid(DisplayCase) ? DisplayCase->GetOriginalCarrier() : nullptr;
	const bool bCarryContractConsistent =
		(!CarryEntry.IsValid() && !IsValid(CaseCarrier)) || (CarryEntry.IsValid() && IsValid(DisplayCase) && CarryEntry.SourceDisplayCase == DisplayCase && CaseCarrier == HeistPlayerState &&
															 DisplayCase->GetDisplayCaseState() == EHeistDisplayCaseState::OriginalRemoved);
	Message(
		PlayerController,
		FString::Printf(
			TEXT(
				"Original carry dump: PlayerId=%d CarryActive=%s Artifact=%s CarryWeight=%.1f TotalWeight=%.1f SourceCase=%s NearestCase=%s CaseId=%s CaseState=%s CaseCarrierPlayerId=%d CarryRevision=%d ContractConsistent=%s Authority=%s Result=%s"),
			HeistPlayerState->HeistPlayerId, CarryEntry.IsValid() ? TEXT("true") : TEXT("false"), *CarryEntry.ArtifactId.ToString(), CarryEntry.Weight, HeistPlayerState->GetTotalLootWeight(),
			*GetNameSafe(CarryEntry.SourceDisplayCase.Get()), *GetNameSafe(DisplayCase), IsValid(DisplayCase) ? *DisplayCase->GetDisplayCaseId().ToString() : TEXT("None"),
			IsValid(DisplayCase) ? *UEnum::GetValueAsString(DisplayCase->GetDisplayCaseState()) : TEXT("MissingCase"), IsValid(CaseCarrier) ? CaseCarrier->HeistPlayerId : INDEX_NONE,
			IsValid(DisplayCase) ? DisplayCase->GetOriginalCarryRevision() : INDEX_NONE, bCarryContractConsistent ? TEXT("true") : TEXT("false"),
			HeistPlayerController->HasAuthority() ? TEXT("true") : TEXT("false"), bCarryContractConsistent ? TEXT("PASS") : TEXT("FAIL")),
		bCarryContractConsistent ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 10.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugOriginalTake(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	AHeistPlayerController* HeistPlayerController = Cast<AHeistPlayerController>(PlayerController);
	AHeistPaintingDisplayCaseActor* DisplayCase = ResolveNearestPaintingDisplayCase(PlayerController);
	if (!IsValid(HeistPlayerController) || !IsValid(DisplayCase))
	{
		Message(PlayerController, TEXT("Original carry debug take: Result=REJECTED Reason=MissingControllerOrDisplayCase"), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->RequestTakeOriginal(DisplayCase);
	Message(PlayerController, FString::Printf(TEXT("Original carry debug take requested: Case=%s Artifact=%s"), *GetNameSafe(DisplayCase), *DisplayCase->GetTargetArtifactId().ToString()),
			EHeistDebugLevel::Info, true);
#endif
}

void UHeistDebugFunctionLibrary::DebugOriginalDrop(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	AHeistPlayerController* HeistPlayerController = Cast<AHeistPlayerController>(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Original carry debug drop: Result=REJECTED Reason=MissingController"), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->RequestDropCarriedOriginal();
	Message(PlayerController, TEXT("Original carry debug drop requested."), EHeistDebugLevel::Info, true);
#endif
}

#pragma endregion

#pragma region ForgeryDebug

void UHeistDebugFunctionLibrary::DebugForgeryHelp(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	Message(
		PlayerController,
		TEXT(
			"Forgery commands: HeistCasePhase InGame | HeistCaseSpawn 250 | interact/hold E | HeistForgeryTemplateDump | HeistForgeryStrokeDump | HeistForgeryTransportDump | HeistForgeryTransportTest <Valid|Bounds|Count|Size|Brush|Revision|Empty|Short|Palette|Timeout|NearTimeout|Duplicate> | HeistForgeryScoreDump | HeistForgeryScoreTest | HeistForgerySwapDump | HeistForgeryVisualDump | HeistForgeryPaintingDump | HeistForgeryDump | HeistForgeryInputDump | HeistForgerySubmit | HeistForgeryCancel | HeistForgeryTimeout | HeistForgeryRecoveryDump | HeistForgeryRecoveryRace <CancelSubmit|SubmitCancel> | HeistForgeryUIDump | HeistForgeryUIPreview <None|Observation|Drawing|Validation|Result>. Run RecoveryRace in the owning client after drawing, then run RecoveryDump after replication settles. Timeout and NearTimeout transport tests run in the listen-server window. Run RecoveryDump in the listen server after client disconnect to detect orphan locks."),
		EHeistDebugLevel::Info, true, 12.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryDump(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	const AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	const AHeistPlayerCharacter* HeistCharacter = IsValid(HeistPlayerController) ? HeistPlayerController->GetPawn<AHeistPlayerCharacter>() : nullptr;
	const AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter) ? HeistCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	const UHeistForgeryComponent* ForgeryComponent = ResolveForgeryComponent(PlayerController);
	if (!IsValid(ForgeryComponent))
	{
		Message(PlayerController, TEXT("Forgery session dump: Result=FAIL Reason=MissingForgeryComponent"), EHeistDebugLevel::Warning, true);
		return;
	}

	const AHeistPaintingDisplayCaseActor* DisplayCase = ForgeryComponent->GetActiveDisplayCase();
	const bool bSessionActive = ForgeryComponent->IsSessionActive();
	const bool bInactiveSnapshotClean = !bSessionActive && !ForgeryComponent->IsSubmitPending() && !IsValid(DisplayCase) && FMath::IsNearlyZero(ForgeryComponent->GetSessionEndServerTime());
	const bool bActiveSnapshotConsistent = bSessionActive && IsValid(DisplayCase) && DisplayCase->IsSessionLocked() && DisplayCase->GetSessionOwner() == HeistPlayerState &&
										   DisplayCase->GetDisplayCaseState() == EHeistDisplayCaseState::ForgeryInProgress && ForgeryComponent->GetSessionEndServerTime() > 0.0f;
	const bool bContractConsistent = bInactiveSnapshotClean || bActiveSnapshotConsistent;

	Message(
		PlayerController,
		FString::Printf(
			TEXT(
				"Forgery session dump: Character=%s PlayerId=%d Active=%s SubmitPending=%s Case=%s CaseState=%s CaseOwnerPlayerId=%d CaseLocked=%s EndServerTime=%.2f Revision=%d LastCleanup=%s ContractConsistent=%s Authority=%s Result=%s"),
			*GetNameSafe(HeistCharacter), IsValid(HeistPlayerState) ? HeistPlayerState->HeistPlayerId : INDEX_NONE, bSessionActive ? TEXT("true") : TEXT("false"),
			ForgeryComponent->IsSubmitPending() ? TEXT("true") : TEXT("false"), *GetNameSafe(DisplayCase),
			IsValid(DisplayCase) ? *UEnum::GetValueAsString(DisplayCase->GetDisplayCaseState()) : TEXT("None"),
			IsValid(DisplayCase) && IsValid(DisplayCase->GetSessionOwner()) ? DisplayCase->GetSessionOwner()->HeistPlayerId : INDEX_NONE,
			IsValid(DisplayCase) && DisplayCase->IsSessionLocked() ? TEXT("true") : TEXT("false"), ForgeryComponent->GetSessionEndServerTime(), ForgeryComponent->GetSessionRevision(),
			ForgeryComponent->GetLastCleanupReason().IsNone() ? TEXT("None") : *ForgeryComponent->GetLastCleanupReason().ToString(), bContractConsistent ? TEXT("true") : TEXT("false"),
			IsValid(HeistPlayerController) && HeistPlayerController->HasAuthority() ? TEXT("true") : TEXT("false"), bContractConsistent ? TEXT("PASS") : TEXT("FAIL")),
		bContractConsistent ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 12.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryInputDump(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	const AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	const AHeistPlayerCharacter* HeistCharacter = IsValid(HeistPlayerController) ? HeistPlayerController->GetPawn<AHeistPlayerCharacter>() : nullptr;
	const UHeistForgeryComponent* ForgeryComponent = IsValid(HeistCharacter) ? HeistCharacter->GetForgeryComponent() : nullptr;
	const UHeistInventoryComponent* InventoryComponent = IsValid(HeistCharacter) ? HeistCharacter->GetInventoryComponent() : nullptr;
	if (!IsValid(HeistPlayerController) || !HeistPlayerController->IsLocalController() || !IsValid(HeistCharacter) || !IsValid(ForgeryComponent) || !IsValid(InventoryComponent))
	{
		Message(PlayerController, TEXT("Forgery input dump: Result=FAIL Reason=MissingLocalInputContext"), EHeistDebugLevel::Warning, true);
		return;
	}

	const bool bGameplayContext = HeistPlayerController->IsLocalInputMappingContextActive(EHeistInputMode::Gameplay);
	const bool bInventoryContext = HeistPlayerController->IsLocalInputMappingContextActive(EHeistInputMode::Inventory);
	const bool bForgeryContext = HeistPlayerController->IsLocalInputMappingContextActive(EHeistInputMode::Forgery);
	const bool bContractPassed = HeistPlayerController->IsLocalInputModeContractSatisfied();
	Message(
		PlayerController,
		FString::Printf(
			TEXT(
				"Forgery input dump: Controller=%s Mode=%s SessionActive=%s InventoryOpen=%s GameplayContext=%s InventoryContext=%s ForgeryContext=%s ActiveContexts=%d Cursor=%s IgnoreMove=%s IgnoreLook=%s GameplayActionsAllowed=%s Contract=%s Result=%s"),
			*GetNameSafe(HeistPlayerController), ToInputModeText(HeistPlayerController->GetLocalInputMode()), ForgeryComponent->IsSessionActive() ? TEXT("true") : TEXT("false"),
			InventoryComponent->IsInventoryOpen() ? TEXT("true") : TEXT("false"), bGameplayContext ? TEXT("true") : TEXT("false"), bInventoryContext ? TEXT("true") : TEXT("false"),
			bForgeryContext ? TEXT("true") : TEXT("false"), HeistPlayerController->GetActiveHeistInputMappingContextCount(), HeistPlayerController->bShowMouseCursor ? TEXT("true") : TEXT("false"),
			HeistPlayerController->IsMoveInputIgnored() ? TEXT("true") : TEXT("false"), HeistPlayerController->IsLookInputIgnored() ? TEXT("true") : TEXT("false"),
			HeistCharacter->CanPerformGameplayActions() ? TEXT("true") : TEXT("false"), bContractPassed ? TEXT("PASS") : TEXT("FAIL"), bContractPassed ? TEXT("PASS") : TEXT("FAIL")),
		bContractPassed ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 12.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryTemplateDump(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	const UHeistForgeryComponent* ForgeryComponent = ResolveForgeryComponent(PlayerController);
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	AHeistHUD* HeistHUD = IsValid(HeistPlayerController) ? HeistPlayerController->GetHUD<AHeistHUD>() : nullptr;
	if (IsValid(HeistHUD))
	{
		HeistHUD->RefreshPresentationSources();
	}
	const UHeistForgeryViewModel* ForgeryViewModel = IsValid(HeistHUD) ? HeistHUD->GetForgeryViewModel() : nullptr;
	if (!IsValid(ForgeryComponent))
	{
		Message(PlayerController, TEXT("Forgery template dump: Result=FAIL Reason=MissingForgeryComponent"), EHeistDebugLevel::Warning, true);
		return;
	}

	UTexture2D* ReferenceImage = ForgeryComponent->LoadReferenceImage();
	UTexture2D* ReferenceMask = ForgeryComponent->LoadReferenceMask();
	const bool bTemplateContract = ForgeryComponent->HasPreparedForgeryTemplate() && !ForgeryComponent->GetActiveArtifactId().IsNone() && !ForgeryComponent->GetActiveTemplateId().IsNone() &&
								   ForgeryComponent->GetReferenceImageAsset().ToSoftObjectPath().IsValid() && ForgeryComponent->GetReferenceMaskAsset().ToSoftObjectPath().IsValid() &&
								   IsValid(ReferenceImage) && IsValid(ReferenceMask) && ForgeryComponent->GetTemplateObservationDuration() >= 0.0f &&
								   ForgeryComponent->GetTemplateForgeryDuration() > 0.0f && ForgeryComponent->GetTemplateStrokeLimit() > 0 && ForgeryComponent->GetTemplateBrushSize() > 0.0f &&
								   FMath::IsWithinInclusive(ForgeryComponent->GetTemplateAllowedPalette().Num(), 2, 8);
	const bool bHandoffContract = !ForgeryComponent->IsSessionActive() || (IsValid(ForgeryViewModel) && ForgeryViewModel->IsDrawingVisible() && !ForgeryViewModel->IsObservationVisible() &&
																		   ForgeryViewModel->GetReferenceTemplateId() == ForgeryComponent->GetActiveTemplateId());
	const bool bContractPassed = bTemplateContract && bHandoffContract;

	Message(
		PlayerController,
		FString::Printf(
			TEXT(
				"Forgery template dump: Prepared=%s Artifact=%s Template=%s ReferenceImage=%s ImageLoaded=%s ReferenceMask=%s MaskLoaded=%s PaletteColors=%d Observation=%.2f Forgery=%.2f StrokeLimit=%d Brush=%.4f SessionActive=%s ObservationUI=%s DrawingUI=%s TemplateContract=%s HandoffContract=%s Result=%s"),
			ForgeryComponent->HasPreparedForgeryTemplate() ? TEXT("true") : TEXT("false"), *ForgeryComponent->GetActiveArtifactId().ToString(), *ForgeryComponent->GetActiveTemplateId().ToString(),
			*ForgeryComponent->GetReferenceImageAsset().ToSoftObjectPath().ToString(), IsValid(ReferenceImage) ? TEXT("true") : TEXT("false"),
			*ForgeryComponent->GetReferenceMaskAsset().ToSoftObjectPath().ToString(), IsValid(ReferenceMask) ? TEXT("true") : TEXT("false"), ForgeryComponent->GetTemplateAllowedPalette().Num(),
			ForgeryComponent->GetTemplateObservationDuration(), ForgeryComponent->GetTemplateForgeryDuration(), ForgeryComponent->GetTemplateStrokeLimit(), ForgeryComponent->GetTemplateBrushSize(),
			ForgeryComponent->IsSessionActive() ? TEXT("true") : TEXT("false"), IsValid(ForgeryViewModel) && ForgeryViewModel->IsObservationVisible() ? TEXT("true") : TEXT("false"),
			IsValid(ForgeryViewModel) && ForgeryViewModel->IsDrawingVisible() ? TEXT("true") : TEXT("false"), bTemplateContract ? TEXT("PASS") : TEXT("FAIL"),
			bHandoffContract ? TEXT("PASS") : TEXT("FAIL"), bContractPassed ? TEXT("PASS") : TEXT("FAIL")),
		bContractPassed ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 15.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryStrokeDump(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	AHeistHUD* HeistHUD = IsValid(HeistPlayerController) ? HeistPlayerController->GetHUD<AHeistHUD>() : nullptr;
	UHeistForgeryWidget* ForgeryWidget = IsValid(HeistHUD) ? HeistHUD->GetForgeryWidget() : nullptr;
	UHeistForgeryViewModel* ForgeryViewModel = IsValid(HeistHUD) ? HeistHUD->GetForgeryViewModel() : nullptr;
	if (!IsValid(HeistPlayerController) || !HeistPlayerController->IsLocalController() || !IsValid(ForgeryWidget) || !IsValid(ForgeryViewModel))
	{
		Message(PlayerController, TEXT("Forgery stroke dump: Result=FAIL Reason=MissingLocalForgeryPresentation"), EHeistDebugLevel::Warning, true);
		return;
	}

	const int32 StrokeCount = ForgeryWidget->GetCollectedStrokeCount();
	const int32 PointCount = ForgeryWidget->GetCollectedPointCount();
	const int32 SegmentCount = ForgeryWidget->GetCollectedSegmentCount();
	const int32 ErasedCount = ForgeryWidget->GetErasedStrokeCount();
	const int32 StrokeLimit = ForgeryWidget->GetConfiguredStrokeLimit();
	const float BrushSize = ForgeryWidget->GetConfiguredBrushSize();
	const FVector2D CanvasSize = ForgeryWidget->GetDrawingSurfaceSize();
	const bool bCanvasReady = ForgeryWidget->IsDrawingSurfaceReady();
	const bool bDrawingVisible = ForgeryViewModel->IsDrawingVisible();
	const bool bNormalized = ForgeryWidget->AreCollectedPointsNormalized();
	const int32 PaletteColorCount = ForgeryViewModel->GetAllowedPalette().Num();
	const int32 VisiblePaletteButtonCount = ForgeryWidget->GetVisiblePaletteButtonCount();
	const int32 ActivePaletteIndex = ForgeryWidget->GetActivePaletteIndex();
	const bool bPaletteControlsReady =
		FMath::IsWithinInclusive(PaletteColorCount, 2, 8) && VisiblePaletteButtonCount == PaletteColorCount && FMath::IsWithinInclusive(ActivePaletteIndex, 0, PaletteColorCount - 1);
	const bool bLimitRespected = StrokeLimit > 0 && PointCount <= StrokeLimit;
	const bool bBrushValid = FMath::IsWithinInclusive(BrushSize, 0.001f, 0.25f);
	const bool bCollectionReady = StrokeCount > 0 && PointCount > 1 && SegmentCount > 0;
	const bool bEraseVerified = ErasedCount > 0;
	const bool bContractPassed = bCanvasReady && bDrawingVisible && bPaletteControlsReady && bNormalized && bLimitRespected && bBrushValid && bCollectionReady && bEraseVerified;

	Message(
		PlayerController,
		FString::Printf(
			TEXT(
				"Forgery stroke dump: DrawingVisible=%s CanvasReady=%s CanvasSize=(%.1f,%.1f) EmptyCanvas=%s PaletteColors=%d PaletteButtons=%d ActivePalette=%d PaletteControls=%s Preview='%s' Strokes=%d Points=%d Segments=%d ErasedStrokes=%d StrokeLimit=%d LimitRespected=%s Brush=%.4f BrushValid=%s NormalizedPoints=%s Collection=%s Erase=%s Result=%s"),
			bDrawingVisible ? TEXT("true") : TEXT("false"), bCanvasReady ? TEXT("true") : TEXT("false"), CanvasSize.X, CanvasSize.Y, PointCount == 0 ? TEXT("true") : TEXT("false"), PaletteColorCount,
			VisiblePaletteButtonCount, ActivePaletteIndex + 1, bPaletteControlsReady ? TEXT("PASS") : TEXT("FAIL"), *ForgeryWidget->GetPreviewScoreText(), StrokeCount, PointCount, SegmentCount,
			ErasedCount, StrokeLimit, bLimitRespected ? TEXT("true") : TEXT("false"), BrushSize, bBrushValid ? TEXT("true") : TEXT("false"), bNormalized ? TEXT("true") : TEXT("false"),
			bCollectionReady ? TEXT("PASS") : TEXT("FAIL"), bEraseVerified ? TEXT("PASS") : TEXT("FAIL"), bContractPassed ? TEXT("PASS") : TEXT("FAIL")),
		bContractPassed ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 15.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryTransportDump(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	UHeistForgeryComponent* ForgeryComponent = ResolveForgeryComponent(PlayerController);
	if (!IsValid(ForgeryComponent))
	{
		Message(PlayerController, TEXT("Forgery transport dump: Result=FAIL Reason=MissingForgeryComponent"), EHeistDebugLevel::Warning, true);
		return;
	}

	const bool bAcceptedContract = ForgeryComponent->HasValidatedStrokePayload() && ForgeryComponent->WasLastStrokeValidationAccepted() && ForgeryComponent->IsSubmitPending() &&
								   ForgeryComponent->GetValidatedStrokeCount() > 0 && ForgeryComponent->GetValidatedPointCount() > 1 &&
								   ForgeryComponent->GetValidatedPointCount() <= ForgeryComponent->GetTemplateStrokeLimit() && ForgeryComponent->GetValidatedPayloadBytes() > 0 &&
								   ForgeryComponent->GetValidatedPayloadBytes() <= 48 * 1024 &&
								   FMath::IsNearlyEqual(ForgeryComponent->GetValidatedBrushSize(), ForgeryComponent->GetTemplateBrushSize(), 0.0001f);
	const TCHAR* ResultText = bAcceptedContract ? TEXT("PASS") : ForgeryComponent->GetStrokeValidationRevision() > 0 ? TEXT("REJECTED") : TEXT("NOT_TESTED");

	Message(
		PlayerController,
		FString::Printf(
			TEXT(
				"Forgery transport dump: SessionActive=%s SubmitPending=%s HasValidatedPayload=%s LastAccepted=%s LastReason=%s ValidationRevision=%d Strokes=%d Points=%d StrokeLimit=%d PayloadBytes=%d PayloadLimitBytes=%d Brush=%.4f TemplateBrush=%.4f RenderTargetReplicated=false Authority=%s Result=%s"),
			ForgeryComponent->IsSessionActive() ? TEXT("true") : TEXT("false"), ForgeryComponent->IsSubmitPending() ? TEXT("true") : TEXT("false"),
			ForgeryComponent->HasValidatedStrokePayload() ? TEXT("true") : TEXT("false"), ForgeryComponent->WasLastStrokeValidationAccepted() ? TEXT("true") : TEXT("false"),
			ForgeryComponent->GetLastStrokeValidationReason().IsNone() ? TEXT("None") : *ForgeryComponent->GetLastStrokeValidationReason().ToString(), ForgeryComponent->GetStrokeValidationRevision(),
			ForgeryComponent->GetValidatedStrokeCount(), ForgeryComponent->GetValidatedPointCount(), ForgeryComponent->GetTemplateStrokeLimit(), ForgeryComponent->GetValidatedPayloadBytes(),
			48 * 1024, ForgeryComponent->GetValidatedBrushSize(), ForgeryComponent->GetTemplateBrushSize(), PlayerController && PlayerController->HasAuthority() ? TEXT("true") : TEXT("false"),
			ResultText),
		bAcceptedContract ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 15.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryTransportTest(APlayerController* PlayerController, const FString& Scenario)
{
#if !UE_BUILD_SHIPPING
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	UHeistForgeryComponent* ForgeryComponent = ResolveForgeryComponent(PlayerController);
	if (!IsValid(HeistPlayerController) || !HeistPlayerController->IsLocalController() || !IsValid(ForgeryComponent))
	{
		Message(PlayerController, TEXT("Forgery transport test: Result=FAIL Reason=MissingLocalForgeryContext"), EHeistDebugLevel::Warning, true);
		return;
	}

	const FString NormalizedScenario = Scenario.TrimStartAndEnd().ToLower();
	TArray<FVector2D> Points;
	TArray<int32> StrokePointCounts;
	float ClientBrushSize = ForgeryComponent->GetTemplateBrushSize();
	int32 ClientSessionRevision = ForgeryComponent->GetSessionRevision();
	bool bUseInvalidPaletteIndex = false;
	const TCHAR* ExpectedResult = TEXT("REJECTED_UNKNOWN");

	if (NormalizedScenario == TEXT("valid") || NormalizedScenario == TEXT("duplicate"))
	{
		Points = {FVector2D(0.1, 0.1), FVector2D(0.2, 0.2), FVector2D(0.3, 0.25)};
		StrokePointCounts = {3};
		ExpectedResult = NormalizedScenario == TEXT("valid") ? TEXT("PASS") : TEXT("REJECTED_DuplicateSubmit");
	}
	else if (NormalizedScenario == TEXT("bounds"))
	{
		Points = {FVector2D(0.1, 0.1), FVector2D(1.25, 0.2)};
		StrokePointCounts = {2};
		ExpectedResult = TEXT("REJECTED_PointOutOfBounds");
	}
	else if (NormalizedScenario == TEXT("count"))
	{
		Points = {FVector2D(0.1, 0.1), FVector2D(0.2, 0.2)};
		StrokePointCounts = {3};
		ExpectedResult = TEXT("REJECTED_StrokeLayoutMismatch");
	}
	else if (NormalizedScenario == TEXT("size"))
	{
		const int32 PointCount = FMath::Max(2, ForgeryComponent->GetTemplateStrokeLimit() + 1);
		Points.Init(FVector2D(0.5, 0.5), PointCount);
		StrokePointCounts = {PointCount};
		ExpectedResult = TEXT("REJECTED_PointCountLimit");
	}
	else if (NormalizedScenario == TEXT("brush"))
	{
		Points = {FVector2D(0.1, 0.1), FVector2D(0.2, 0.2)};
		StrokePointCounts = {2};
		ClientBrushSize = FMath::IsNearlyEqual(ClientBrushSize, 0.25f) ? 0.001f : FMath::Min(0.25f, ClientBrushSize * 2.0f);
		ExpectedResult = TEXT("REJECTED_BrushSizeMismatch");
	}
	else if (NormalizedScenario == TEXT("revision"))
	{
		Points = {FVector2D(0.1, 0.1), FVector2D(0.2, 0.2)};
		StrokePointCounts = {2};
		--ClientSessionRevision;
		ExpectedResult = TEXT("REJECTED_SessionRevisionMismatch");
	}
	else if (NormalizedScenario == TEXT("empty"))
	{
		ExpectedResult = TEXT("REJECTED_EmptyPayload");
	}
	else if (NormalizedScenario == TEXT("short"))
	{
		Points = {FVector2D(0.1, 0.1)};
		StrokePointCounts = {1};
		ExpectedResult = TEXT("REJECTED_StrokeTooShort");
	}
	else if (NormalizedScenario == TEXT("palette"))
	{
		Points = {FVector2D(0.1, 0.1), FVector2D(0.2, 0.2)};
		StrokePointCounts = {2};
		bUseInvalidPaletteIndex = true;
		ExpectedResult = TEXT("REJECTED_PaletteIndexOutOfBounds");
	}
	else if (NormalizedScenario == TEXT("timeout"))
	{
		if (!HeistPlayerController->HasAuthority() || !ForgeryComponent->ForceExpireSubmissionWindowForDebug())
		{
			Message(PlayerController, TEXT("Forgery transport test: Scenario=timeout Result=FAIL Reason=ListenServerAuthorityAndActiveSessionRequired"), EHeistDebugLevel::Warning, true);
			return;
		}
		Points = {FVector2D(0.1, 0.1), FVector2D(0.2, 0.2)};
		StrokePointCounts = {2};
		ExpectedResult = TEXT("REJECTED_SessionExpired");
	}
	else if (NormalizedScenario == TEXT("neartimeout"))
	{
		if (!HeistPlayerController->HasAuthority() || !ForgeryComponent->ForceNearExpirySubmissionWindowForDebug())
		{
			Message(PlayerController, TEXT("Forgery transport test: Scenario=neartimeout Result=FAIL Reason=ListenServerAuthorityAndActiveSessionRequired"), EHeistDebugLevel::Warning, true);
			return;
		}
		Points = {FVector2D(0.1, 0.1), FVector2D(0.2, 0.2)};
		StrokePointCounts = {2};
		ExpectedResult = TEXT("PASS_NearTimeoutAccepted");
	}
	else
	{
		Message(PlayerController,
				TEXT("Forgery transport test: Result=FAIL Reason=UnknownScenario Expected=<Valid|Bounds|Count|Size|Brush|Revision|Empty|Short|Palette|Timeout|NearTimeout|Duplicate>"),
				EHeistDebugLevel::Warning, true);
		return;
	}

	TArray<uint8> StrokePaletteIndices;
	StrokePaletteIndices.Init(0, StrokePointCounts.Num());
	if (bUseInvalidPaletteIndex && !StrokePaletteIndices.IsEmpty())
	{
		StrokePaletteIndices[0] = MAX_uint8;
	}
	HeistPlayerController->RequestSubmitForgeryStrokes(Points, StrokePointCounts, StrokePaletteIndices, ClientBrushSize, ClientSessionRevision);
	Message(PlayerController,
			FString::Printf(TEXT("Forgery transport test requested: Scenario=%s Strokes=%d Points=%d Brush=%.4f ClientSessionRevision=%d Expected=%s Result=REQUESTED"), *NormalizedScenario,
							StrokePointCounts.Num(), Points.Num(), ClientBrushSize, ClientSessionRevision, ExpectedResult),
			EHeistDebugLevel::Info, true, 12.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryScoreDump(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	bool bUsedAuthorityFallback = false;
	UHeistForgeryComponent* ForgeryComponent = ResolveScoredForgeryComponent(PlayerController, bUsedAuthorityFallback);
	if (!IsValid(ForgeryComponent))
	{
		Message(PlayerController, TEXT("Forgery score dump: Result=FAIL Reason=MissingForgeryComponent"), EHeistDebugLevel::Warning, true);
		return;
	}

	const FHeistForgeryResult& ScoreResult = ForgeryComponent->GetAuthoritativeForgeryResult();
	const bool bHasScore = ForgeryComponent->HasAuthoritativeForgeryResult();
	const bool bScoreInRange = FMath::IsWithinInclusive(ScoreResult.SimilarityScore, 0.0f, 100.0f);
	const bool bBreakdownValid = ScoreResult.CoverageScore >= 0.0f && ScoreResult.MajorShapeScore >= 0.0f && ScoreResult.ColorAccuracyScore >= 0.0f && ScoreResult.PaintToReferenceRatio >= 0.0f &&
								 ScoreResult.MissingShapePenalty >= 0.0f && ScoreResult.ExtraStrokePenalty >= 0.0f && ScoreResult.TimeoutPenalty >= 0.0f;
	const bool bResolutionValid = FMath::IsWithinInclusive(ForgeryComponent->GetForgeryScoreResolution(), 128, 256);
	const bool bMaskCountsValid = ForgeryComponent->GetReferenceMaskPixelCount() > 0 && ForgeryComponent->GetSubmittedMaskPixelCount() > 0;
	const bool bIdentityValid = !ScoreResult.ArtifactId.IsNone() && !ScoreResult.TemplateId.IsNone() && ScoreResult.ForgeryType == EHeistForgeryType::Drawing;
	const bool bContractPassed = bHasScore && bScoreInRange && bBreakdownValid && bResolutionValid && bMaskCountsValid && bIdentityValid && ScoreResult.bReplicaPlaced;

	Message(
		PlayerController,
		FString::Printf(
			TEXT(
				"Forgery score dump: Backend=OpenCV ShapeMetric=DistanceDiceGeometric ColorMetric=LabSSIMHistogramGeometric Fusion=WeightedGeometricBottleneck Calibration=OpenCVHistogramBonus0.75x3 ScoreCurve=Shape1.15Color1.10 PaintCompletenessExponent=0.65 TargetCharacter=%s TargetSelection=%s HasScore=%s Artifact=%s Template=%s Score=%.2f Coverage=%.2f MajorShape=%.2f ColorAccuracy=%.2f MissingPenalty=%.2f ExtraPenalty=%.2f TimeoutPenalty=%.2f CompletionTime=%.2f PaintToReference=%.2f PaintCompleteness=%.3f AntiFill=%s ReplicaPlaced=%s Resolution=%dx%d ReferencePixels=%d SubmittedPixels=%d ScoreRevision=%d OwnerOnlySummary=true RawStrokeReplicated=false Authority=%s Result=%s"),
			*GetNameSafe(ForgeryComponent->GetOwner()), bUsedAuthorityFallback ? TEXT("AuthorityScoredFallback") : TEXT("OwningPlayer"), bHasScore ? TEXT("true") : TEXT("false"),
			*ScoreResult.ArtifactId.ToString(), *ScoreResult.TemplateId.ToString(), ScoreResult.SimilarityScore, ScoreResult.CoverageScore, ScoreResult.MajorShapeScore, ScoreResult.ColorAccuracyScore,
			ScoreResult.MissingShapePenalty, ScoreResult.ExtraStrokePenalty, ScoreResult.TimeoutPenalty, ScoreResult.CompletionTime, ScoreResult.PaintToReferenceRatio,
			FMath::Pow(FMath::Clamp(ScoreResult.PaintToReferenceRatio, 0.0f, 1.0f), 0.65f), ScoreResult.bAntiFillTriggered ? TEXT("true") : TEXT("false"),
			ScoreResult.bReplicaPlaced ? TEXT("true") : TEXT("false"), ForgeryComponent->GetForgeryScoreResolution(), ForgeryComponent->GetForgeryScoreResolution(),
			ForgeryComponent->GetReferenceMaskPixelCount(), ForgeryComponent->GetSubmittedMaskPixelCount(), ForgeryComponent->GetForgeryScoreRevision(),
			PlayerController && PlayerController->HasAuthority() ? TEXT("true") : TEXT("false"), bContractPassed ? TEXT("PASS") : TEXT("FAIL")),
		bContractPassed ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 15.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugForgerySwapDump(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	AHeistPaintingDisplayCaseActor* DisplayCase = ResolveNearestPaintingDisplayCase(PlayerController);
	UHeistForgeryComponent* ForgeryComponent = ResolveForgeryComponent(PlayerController);
	if (!IsValid(DisplayCase))
	{
		Message(PlayerController, TEXT("Forgery swap dump: Result=FAIL Reason=MissingDisplayCase"), EHeistDebugLevel::Warning, true);
		return;
	}

	const EHeistDisplayCaseState CaseState = DisplayCase->GetDisplayCaseState();
	const bool bStateValid = CaseState == EHeistDisplayCaseState::OriginalAvailable || CaseState == EHeistDisplayCaseState::OriginalRemoved;
	const bool bHasCommittedResult = DisplayCase->HasCommittedForgeryResult();
	const FHeistForgeryResult CaseResult = DisplayCase->GetCommittedForgeryResult();
	const bool bCommittedResultValid = bHasCommittedResult && CaseResult.ArtifactId == DisplayCase->GetTargetArtifactId() && !CaseResult.TemplateId.IsNone() &&
									   CaseResult.ForgeryType == EHeistForgeryType::Drawing && FMath::IsWithinInclusive(CaseResult.SimilarityScore, 0.0f, 100.0f) && CaseResult.bReplicaPlaced;

	bool bExpectedOriginalVisible = false;
	bool bExpectedReplicaVisible = false;
	int32 OriginalComponentCount = 0;
	int32 ReplicaComponentCount = 0;
	bool bVisualComponentsMatch = false;
	DisplayCase->GetPlaceholderVisualDebugState(bExpectedOriginalVisible, bExpectedReplicaVisible, OriginalComponentCount, ReplicaComponentCount, bVisualComponentsMatch);

	const AHeistPlayerState* OriginalCarrier = DisplayCase->GetOriginalCarrier();
	const bool bOriginalStateContract = (CaseState == EHeistDisplayCaseState::OriginalAvailable && !IsValid(OriginalCarrier) && !bExpectedOriginalVisible && bExpectedReplicaVisible) ||
										(CaseState == EHeistDisplayCaseState::OriginalRemoved && IsValid(OriginalCarrier) && !bExpectedOriginalVisible && bExpectedReplicaVisible);
	const bool bLocalSessionInactive = !IsValid(ForgeryComponent) || !ForgeryComponent->IsSessionActive();
	const bool bLocalHasScore = IsValid(ForgeryComponent) && ForgeryComponent->HasAuthoritativeForgeryResult();
	const bool bLocalScoreRelevant = bLocalHasScore && ForgeryComponent->GetAuthoritativeForgeryResult().ArtifactId == CaseResult.ArtifactId;
	const bool bLocalScoreLinked = !bLocalScoreRelevant || (ForgeryComponent->GetAuthoritativeForgeryResult().TemplateId == CaseResult.TemplateId &&
															FMath::IsNearlyEqual(ForgeryComponent->GetAuthoritativeForgeryResult().SimilarityScore, CaseResult.SimilarityScore, 0.01f) &&
															ForgeryComponent->GetAuthoritativeForgeryResult().bReplicaPlaced);
	const bool bContractPassed =
		bStateValid && bCommittedResultValid && !DisplayCase->IsSessionLocked() && bVisualComponentsMatch && bOriginalStateContract && bLocalSessionInactive && bLocalScoreLinked;

	Message(
		PlayerController,
		FString::Printf(
			TEXT(
				"Forgery swap dump: Case=%s CaseId=%s State=%s HasCommittedResult=%s Artifact=%s Template=%s Score=%.2f ReplicaPlaced=%s CaseLocked=%s OriginalCarrier=%s OriginalVisible=%s ReplicaVisible=%s OriginalComponents=%d ReplicaComponents=%d VisualsMatch=%s LocalSessionActive=%s LocalHasScore=%s LocalScoreRelevant=%s LocalScoreLinked=%s Revision=%d Authority=%s Result=%s"),
			*GetNameSafe(DisplayCase), *DisplayCase->GetDisplayCaseId().ToString(), *UEnum::GetValueAsString(CaseState), bHasCommittedResult ? TEXT("true") : TEXT("false"),
			*CaseResult.ArtifactId.ToString(), *CaseResult.TemplateId.ToString(), CaseResult.SimilarityScore, CaseResult.bReplicaPlaced ? TEXT("true") : TEXT("false"),
			DisplayCase->IsSessionLocked() ? TEXT("true") : TEXT("false"), *GetNameSafe(OriginalCarrier), bExpectedOriginalVisible ? TEXT("true") : TEXT("false"),
			bExpectedReplicaVisible ? TEXT("true") : TEXT("false"), OriginalComponentCount, ReplicaComponentCount, bVisualComponentsMatch ? TEXT("true") : TEXT("false"),
			IsValid(ForgeryComponent) && ForgeryComponent->IsSessionActive() ? TEXT("true") : TEXT("false"), bLocalHasScore ? TEXT("true") : TEXT("false"),
			bLocalScoreRelevant ? TEXT("true") : TEXT("false"), bLocalScoreLinked ? TEXT("true") : TEXT("false"), DisplayCase->GetCommittedForgeryRevision(),
			PlayerController && PlayerController->HasAuthority() ? TEXT("true") : TEXT("false"), bContractPassed ? TEXT("PASS") : TEXT("FAIL")),
		bContractPassed ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 18.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryVisualDump(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	AHeistPaintingDisplayCaseActor* DisplayCase = ResolveNearestPaintingDisplayCase(PlayerController);
	if (!IsValid(DisplayCase))
	{
		Message(PlayerController, TEXT("Forgery visual dump: Result=FAIL Reason=MissingDisplayCase"), EHeistDebugLevel::Warning, true);
		return;
	}

	bool bReplicaExpectedVisible = false;
	bool bHasReplicaMesh = false;
	int32 ExpectedTier = INDEX_NONE;
	int32 AppliedTier = INDEX_NONE;
	FName TierName = NAME_None;
	bool bUsingTierMaterial = false;
	bool bUsingTransformFallback = false;
	bool bCustomPrimitiveDataApplied = false;
	bool bContractPassed = false;
	DisplayCase->GetReplicaWorldVisualDebugState(bReplicaExpectedVisible, bHasReplicaMesh, ExpectedTier, AppliedTier, TierName, bUsingTierMaterial, bUsingTransformFallback,
												 bCustomPrimitiveDataApplied, bContractPassed);

	const FHeistForgeryResult CaseResult = DisplayCase->GetCommittedForgeryResult();
	Message(
		PlayerController,
		FString::Printf(
			TEXT(
				"Forgery visual dump: Case=%s CaseId=%s State=%s HasCommittedResult=%s Template=%s Score=%.2f Coverage=%.2f ColorAccuracy=%.2f ReplicaExpectedVisible=%s HasReplicaMesh=%s ExpectedTier=%d AppliedTier=%d TierName=%s TierMaterial=%s TransformFallback=%s CustomPrimitiveData=%s Revision=%d Authority=%s Result=%s"),
			*GetNameSafe(DisplayCase), *DisplayCase->GetDisplayCaseId().ToString(), *UEnum::GetValueAsString(DisplayCase->GetDisplayCaseState()),
			DisplayCase->HasCommittedForgeryResult() ? TEXT("true") : TEXT("false"), *CaseResult.TemplateId.ToString(), CaseResult.SimilarityScore, CaseResult.CoverageScore,
			CaseResult.ColorAccuracyScore, bReplicaExpectedVisible ? TEXT("true") : TEXT("false"), bHasReplicaMesh ? TEXT("true") : TEXT("false"), ExpectedTier, AppliedTier,
			TierName.IsNone() ? TEXT("None") : *TierName.ToString(), bUsingTierMaterial ? TEXT("true") : TEXT("false"), bUsingTransformFallback ? TEXT("true") : TEXT("false"),
			bCustomPrimitiveDataApplied ? TEXT("true") : TEXT("false"), DisplayCase->GetCommittedForgeryRevision(), PlayerController && PlayerController->HasAuthority() ? TEXT("true") : TEXT("false"),
			bContractPassed ? TEXT("PASS") : TEXT("FAIL")),
		bContractPassed ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 18.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryPaintingDump(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	AHeistPaintingDisplayCaseActor* DisplayCase = ResolveNearestPaintingDisplayCase(PlayerController);
	if (!IsValid(DisplayCase))
	{
		Message(PlayerController, TEXT("Forgery painting dump: Result=FAIL Reason=MissingDisplayCase"), EHeistDebugLevel::Warning, true);
		return;
	}

	int32 Resolution = 0;
	int32 PaletteColorCount = 0;
	int32 PackedByteCount = 0;
	int32 PaintingRevision = 0;
	bool bTextureBuilt = false;
	bool bDynamicMaterialBuilt = false;
	bool bTextureParameterApplied = false;
	bool bContractPassed = false;
	DisplayCase->GetReplicaPaintingDebugState(Resolution, PaletteColorCount, PackedByteCount, PaintingRevision, bTextureBuilt, bDynamicMaterialBuilt, bTextureParameterApplied, bContractPassed);

	Message(
		PlayerController,
		FString::Printf(
			TEXT(
				"Forgery painting dump: Case=%s CaseId=%s State=%s HasCommittedResult=%s HasPaintingData=%s Resolution=%d PaletteColors=%d PackedBytes=%d PaintingRevision=%d CommittedRevision=%d TextureBuilt=%s DynamicMaterialBuilt=%s TextureParameterApplied=%s ReplicaVisible=%s Authority=%s Result=%s"),
			*GetNameSafe(DisplayCase), *DisplayCase->GetDisplayCaseId().ToString(), *UEnum::GetValueAsString(DisplayCase->GetDisplayCaseState()),
			DisplayCase->HasCommittedForgeryResult() ? TEXT("true") : TEXT("false"), DisplayCase->HasReplicaPaintingData() ? TEXT("true") : TEXT("false"), Resolution, PaletteColorCount,
			PackedByteCount, PaintingRevision, DisplayCase->GetCommittedForgeryRevision(), bTextureBuilt ? TEXT("true") : TEXT("false"), bDynamicMaterialBuilt ? TEXT("true") : TEXT("false"),
			bTextureParameterApplied ? TEXT("true") : TEXT("false"), DisplayCase->ShouldDisplayReplicaPlaceholder() ? TEXT("true") : TEXT("false"),
			PlayerController && PlayerController->HasAuthority() ? TEXT("true") : TEXT("false"), bContractPassed ? TEXT("PASS") : TEXT("FAIL")),
		bContractPassed ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 18.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryScoreTest(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	AHeistPlayerController* HeistPlayerController = Cast<AHeistPlayerController>(PlayerController);
	if (IsValid(HeistPlayerController) && !HeistPlayerController->HasAuthority())
	{
		HeistPlayerController->DebugRequestForgeryScoreTest();
		Message(PlayerController, TEXT("Forgery score deterministic test: Result=REQUESTED Target=ServerAuthority"), EHeistDebugLevel::Info, true);
		return;
	}

	bool bUsedAuthorityFallback = false;
	UHeistForgeryComponent* ForgeryComponent = ResolveScoredForgeryComponent(PlayerController, bUsedAuthorityFallback);
	if (!IsValid(PlayerController) || !PlayerController->HasAuthority() || !IsValid(ForgeryComponent) || !ForgeryComponent->HasAuthoritativeForgeryResult())
	{
		Message(PlayerController, TEXT("Forgery score deterministic test: Result=FAIL Reason=ServerAuthorityAndCommittedScoreRequired"), EHeistDebugLevel::Warning, true);
		return;
	}

	FHeistForgeryResult FirstResult;
	FHeistForgeryResult SecondResult;
	int32 FirstReferencePixels = 0;
	int32 FirstSubmittedPixels = 0;
	int32 SecondReferencePixels = 0;
	int32 SecondSubmittedPixels = 0;
	const bool bFirstCalculated = ForgeryComponent->RecalculateValidatedForgeryScoreForDebug(FirstResult, FirstReferencePixels, FirstSubmittedPixels);
	const bool bSecondCalculated = ForgeryComponent->RecalculateValidatedForgeryScoreForDebug(SecondResult, SecondReferencePixels, SecondSubmittedPixels);
	const auto HasSameScoreBreakdown = [](const FHeistForgeryResult& Left, const FHeistForgeryResult& Right)
	{
		return Left.ArtifactId == Right.ArtifactId && Left.TemplateId == Right.TemplateId && FMath::IsNearlyEqual(Left.SimilarityScore, Right.SimilarityScore, 0.001f) &&
			   FMath::IsNearlyEqual(Left.CoverageScore, Right.CoverageScore, 0.001f) && FMath::IsNearlyEqual(Left.MajorShapeScore, Right.MajorShapeScore, 0.001f) &&
			   FMath::IsNearlyEqual(Left.ColorAccuracyScore, Right.ColorAccuracyScore, 0.001f) && FMath::IsNearlyEqual(Left.PaintToReferenceRatio, Right.PaintToReferenceRatio, 0.001f) &&
			   Left.bAntiFillTriggered == Right.bAntiFillTriggered && FMath::IsNearlyEqual(Left.MissingShapePenalty, Right.MissingShapePenalty, 0.001f) &&
			   FMath::IsNearlyEqual(Left.ExtraStrokePenalty, Right.ExtraStrokePenalty, 0.001f) && FMath::IsNearlyEqual(Left.TimeoutPenalty, Right.TimeoutPenalty, 0.001f);
	};
	const bool bDeterministic =
		bFirstCalculated && bSecondCalculated && HasSameScoreBreakdown(FirstResult, SecondResult) && FirstReferencePixels == SecondReferencePixels && FirstSubmittedPixels == SecondSubmittedPixels;
	const FHeistForgeryResult& CommittedResult = ForgeryComponent->GetAuthoritativeForgeryResult();
	const bool bCommittedScoreMatches = bFirstCalculated && HasSameScoreBreakdown(FirstResult, CommittedResult);
	FString OpenCVSelfTestSummary;
	const bool bOpenCVContractPassed = ForgeryComponent->RunOpenCVScoringSelfTestForDebug(OpenCVSelfTestSummary);
	const bool bContractPassed = bDeterministic && bCommittedScoreMatches && bOpenCVContractPassed;

	Message(
		PlayerController,
		FString::Printf(
			TEXT(
				"Forgery score deterministic test: Backend=OpenCV ShapeMetric=DistanceDiceGeometric ColorMetric=LabSSIMHistogramGeometric Fusion=WeightedGeometricBottleneck Calibration=OpenCVHistogramBonus0.75x3 ScoreCurve=Shape1.15Color1.10 TargetCharacter=%s TargetSelection=%s FirstCalculated=%s SecondCalculated=%s FirstScore=%.2f SecondScore=%.2f CommittedScore=%.2f ReferencePixels=%d/%d SubmittedPixels=%d/%d SameBreakdown=%s CommittedMatches=%s SelfTest={%s} Resolution=%dx%d Result=%s"),
			*GetNameSafe(ForgeryComponent->GetOwner()), bUsedAuthorityFallback ? TEXT("AuthorityScoredFallback") : TEXT("OwningPlayer"), bFirstCalculated ? TEXT("true") : TEXT("false"),
			bSecondCalculated ? TEXT("true") : TEXT("false"), FirstResult.SimilarityScore, SecondResult.SimilarityScore, CommittedResult.SimilarityScore, FirstReferencePixels, SecondReferencePixels,
			FirstSubmittedPixels, SecondSubmittedPixels, bDeterministic ? TEXT("true") : TEXT("false"), bCommittedScoreMatches ? TEXT("true") : TEXT("false"), *OpenCVSelfTestSummary,
			ForgeryComponent->GetForgeryScoreResolution(), ForgeryComponent->GetForgeryScoreResolution(), bContractPassed ? TEXT("PASS") : TEXT("FAIL")),
		bContractPassed ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 15.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryBegin(APlayerController* PlayerController, const float DurationSeconds)
{
#if !UE_BUILD_SHIPPING
	UHeistForgeryComponent* ForgeryComponent = ResolveForgeryComponent(PlayerController);
	AHeistPaintingDisplayCaseActor* DisplayCase = ResolveNearestPaintingDisplayCase(PlayerController);
	AHeistPlayerState* HeistPlayerState = IsValid(PlayerController) ? PlayerController->GetPlayerState<AHeistPlayerState>() : nullptr;
	if (!IsValid(PlayerController) || !PlayerController->HasAuthority() || !IsValid(ForgeryComponent) || !IsValid(DisplayCase) || !IsValid(HeistPlayerState))
	{
		Message(PlayerController, TEXT("Forgery session debug begin: Result=REJECTED Reason=MissingAuthorityComponentOrCase"), EHeistDebugLevel::Warning, true);
		return;
	}

	bool bSessionAcquiredForDebug = false;
	if (!DisplayCase->IsSessionLocked())
	{
		bSessionAcquiredForDebug = DisplayCase->TryBeginSession(HeistPlayerState);
		if (!bSessionAcquiredForDebug)
		{
			Message(PlayerController, TEXT("Forgery session debug begin: Result=REJECTED Reason=CaseSessionAcquireFailed"), EHeistDebugLevel::Warning, true);
			return;
		}
	}
	else if (DisplayCase->GetSessionOwner() != HeistPlayerState)
	{
		Message(PlayerController,
				FString::Printf(TEXT("Forgery session debug begin: Result=REJECTED Reason=CaseOwnedByOtherPlayer Owner=%s"), *GetNameSafe(DisplayCase->GetSessionOwner())),
				EHeistDebugLevel::Warning, true);
		return;
	}

	if (DisplayCase->GetDisplayCaseState() == EHeistDisplayCaseState::Secured && !DisplayCase->TryTransitionToDisplayCaseState(EHeistDisplayCaseState::Observed))
	{
		if (bSessionAcquiredForDebug)
		{
			DisplayCase->TryCancelSession(HeistPlayerState);
		}
		Message(PlayerController, TEXT("Forgery session debug begin: Result=REJECTED Reason=DebugObservationTransitionFailed"), EHeistDebugLevel::Warning, true);
		return;
	}

	const bool bBegan = ForgeryComponent->TryBeginForgerySession(DisplayCase, DurationSeconds);
	if (!bBegan && bSessionAcquiredForDebug)
	{
		DisplayCase->TryCancelSession(HeistPlayerState);
	}
	Message(PlayerController,
			FString::Printf(TEXT("Forgery session debug begin: Result=%s Case=%s CaseState=%s CaseLocked=%s CaseOwner=%s DebugSessionAcquired=%s Active=%s SubmitPending=%s "
								 "EndServerTime=%.2f Revision=%d"),
							bBegan ? TEXT("PASS") : TEXT("REJECTED"), *GetNameSafe(DisplayCase), *UEnum::GetValueAsString(DisplayCase->GetDisplayCaseState()),
							DisplayCase->IsSessionLocked() ? TEXT("true") : TEXT("false"), *GetNameSafe(DisplayCase->GetSessionOwner()),
							bSessionAcquiredForDebug ? TEXT("true") : TEXT("false"), ForgeryComponent->IsSessionActive() ? TEXT("true") : TEXT("false"),
							ForgeryComponent->IsSubmitPending() ? TEXT("true") : TEXT("false"), ForgeryComponent->GetSessionEndServerTime(), ForgeryComponent->GetSessionRevision()),
			bBegan ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 12.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugForgerySubmit(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	AHeistHUD* HeistHUD = IsValid(HeistPlayerController) ? HeistPlayerController->GetHUD<AHeistHUD>() : nullptr;
	UHeistForgeryWidget* ForgeryWidget = IsValid(HeistHUD) ? HeistHUD->GetForgeryWidget() : nullptr;
	const bool bRequested = IsValid(ForgeryWidget) && ForgeryWidget->RequestSubmitCollectedStrokes();
	Message(PlayerController, FString::Printf(TEXT("Forgery stroke debug submit: Widget=%s Result=%s"), *GetNameSafe(ForgeryWidget), bRequested ? TEXT("REQUESTED") : TEXT("REJECTED_LOCAL")),
			bRequested ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 12.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryCancel(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	UHeistForgeryComponent* ForgeryComponent = ResolveForgeryComponent(PlayerController);
	const bool bCancelled = IsValid(ForgeryComponent) && ForgeryComponent->CancelForgerySession(FName(TEXT("DebugCancelled")));
	Message(PlayerController,
			FString::Printf(TEXT("Forgery session debug cancel: Result=%s Active=%s SubmitPending=%s Case=%s Revision=%d LastCleanup=%s"), bCancelled ? TEXT("PASS") : TEXT("REJECTED"),
							IsValid(ForgeryComponent) && ForgeryComponent->IsSessionActive() ? TEXT("true") : TEXT("false"),
							IsValid(ForgeryComponent) && ForgeryComponent->IsSubmitPending() ? TEXT("true") : TEXT("false"),
							IsValid(ForgeryComponent) ? *GetNameSafe(ForgeryComponent->GetActiveDisplayCase()) : TEXT("None"),
							IsValid(ForgeryComponent) ? ForgeryComponent->GetSessionRevision() : INDEX_NONE,
							IsValid(ForgeryComponent) && !ForgeryComponent->GetLastCleanupReason().IsNone() ? *ForgeryComponent->GetLastCleanupReason().ToString() : TEXT("None")),
			bCancelled ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 12.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryTimeout(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	UHeistForgeryComponent* ForgeryComponent = ResolveForgeryComponent(PlayerController);
	const bool bTimedOut = IsValid(ForgeryComponent) && ForgeryComponent->ForceTimeoutForDebug();
	Message(PlayerController,
			FString::Printf(TEXT("Forgery session debug timeout: Result=%s Active=%s SubmitPending=%s Case=%s Revision=%d LastCleanup=%s"), bTimedOut ? TEXT("PASS") : TEXT("REJECTED"),
							IsValid(ForgeryComponent) && ForgeryComponent->IsSessionActive() ? TEXT("true") : TEXT("false"),
							IsValid(ForgeryComponent) && ForgeryComponent->IsSubmitPending() ? TEXT("true") : TEXT("false"),
							IsValid(ForgeryComponent) ? *GetNameSafe(ForgeryComponent->GetActiveDisplayCase()) : TEXT("None"),
							IsValid(ForgeryComponent) ? ForgeryComponent->GetSessionRevision() : INDEX_NONE,
							IsValid(ForgeryComponent) && !ForgeryComponent->GetLastCleanupReason().IsNone() ? *ForgeryComponent->GetLastCleanupReason().ToString() : TEXT("None")),
			bTimedOut ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 12.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryRecoveryDump(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	AHeistPlayerCharacter* HeistCharacter = IsValid(HeistPlayerController) ? HeistPlayerController->GetPawn<AHeistPlayerCharacter>() : nullptr;
	AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter) ? HeistCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	UHeistForgeryComponent* ForgeryComponent = IsValid(HeistCharacter) ? HeistCharacter->GetForgeryComponent() : nullptr;
	AHeistHUD* HeistHUD = IsValid(HeistPlayerController) ? HeistPlayerController->GetHUD<AHeistHUD>() : nullptr;
	if (IsValid(HeistHUD))
	{
		HeistHUD->RefreshPresentationSources();
	}

	const UHeistForgeryViewModel* ForgeryViewModel = IsValid(HeistHUD) ? HeistHUD->GetForgeryViewModel() : nullptr;
	const UHeistForgeryWidget* ForgeryWidget = IsValid(HeistHUD) ? HeistHUD->GetForgeryWidget() : nullptr;
	if (!IsValid(HeistPlayerController) || !HeistPlayerController->IsLocalController() || !IsValid(HeistCharacter) || !IsValid(HeistPlayerState) || !IsValid(ForgeryComponent) ||
		!IsValid(ForgeryViewModel) || !IsValid(ForgeryWidget))
	{
		Message(PlayerController, TEXT("Forgery recovery dump: Result=FAIL Reason=MissingLocalRecoveryContext"), EHeistDebugLevel::Warning, true, 15.0f);
		return;
	}

	const bool bAuthority = HeistPlayerController->HasAuthority();
	const bool bSessionActive = ForgeryComponent->IsSessionActive();
	const AHeistPaintingDisplayCaseActor* ActiveDisplayCase = ForgeryComponent->GetActiveDisplayCase();
	int32 LockedCases = 0;
	int32 LocalOwnedLocks = 0;
	int32 OrphanLocks = 0;
	int32 ActiveSessions = 0;
	int32 OrphanSessions = 0;
	AHeistGameState* HeistGameState = HeistPlayerController->GetWorld() ? HeistPlayerController->GetWorld()->GetGameState<AHeistGameState>() : nullptr;

	for (TActorIterator<AHeistPaintingDisplayCaseActor> CaseIterator(HeistPlayerController->GetWorld()); CaseIterator; ++CaseIterator)
	{
		const AHeistPaintingDisplayCaseActor* DisplayCase = *CaseIterator;
		if (!IsValid(DisplayCase) || !DisplayCase->IsSessionLocked())
		{
			continue;
		}

		++LockedCases;
		AHeistPlayerState* SessionOwner = DisplayCase->GetSessionOwner();
		if (SessionOwner == HeistPlayerState)
		{
			++LocalOwnedLocks;
		}

		if (bAuthority)
		{
			const bool bOwnerInMatch = IsValid(SessionOwner) && IsValid(HeistGameState) &&
									   HeistGameState->PlayerArray.ContainsByPredicate([SessionOwner](const TObjectPtr<APlayerState>& Candidate) { return Candidate.Get() == SessionOwner; });
			const AHeistPlayerCharacter* SessionCharacter = IsValid(SessionOwner) ? Cast<AHeistPlayerCharacter>(SessionOwner->GetPawn()) : nullptr;
			const UHeistForgeryComponent* SessionComponent = IsValid(SessionCharacter) ? SessionCharacter->GetForgeryComponent() : nullptr;
			const bool bBackedByActiveSession = IsValid(SessionComponent) && SessionComponent->IsSessionActive() && SessionComponent->GetActiveDisplayCase() == DisplayCase;
			if (!bOwnerInMatch || !bBackedByActiveSession)
			{
				++OrphanLocks;
			}
		}
	}

	if (bAuthority)
	{
		for (TActorIterator<AHeistPlayerCharacter> CharacterIterator(HeistPlayerController->GetWorld()); CharacterIterator; ++CharacterIterator)
		{
			const AHeistPlayerCharacter* CandidateCharacter = *CharacterIterator;
			const UHeistForgeryComponent* CandidateComponent = IsValid(CandidateCharacter) ? CandidateCharacter->GetForgeryComponent() : nullptr;
			if (!IsValid(CandidateComponent) || !CandidateComponent->IsSessionActive())
			{
				continue;
			}

			++ActiveSessions;
			const AHeistPaintingDisplayCaseActor* CandidateCase = CandidateComponent->GetActiveDisplayCase();
			const AHeistPlayerState* CandidatePlayerState = CandidateCharacter->GetPlayerState<AHeistPlayerState>();
			if (!IsValid(CandidateCase) || !CandidateCase->IsSessionLocked() || CandidateCase->GetSessionOwner() != CandidatePlayerState)
			{
				++OrphanSessions;
			}
		}
	}

	TArray<UUserWidget*> ForgeryWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(HeistPlayerController, ForgeryWidgets, ForgeryWidget->GetClass(), true);
	const int32 LocalForgeryWidgetCount =
		ForgeryWidgets
			.FilterByPredicate([HeistPlayerController](const UUserWidget* CandidateWidget) { return IsValid(CandidateWidget) && CandidateWidget->GetOwningPlayer() == HeistPlayerController; })
			.Num();

	const bool bInactiveSnapshotClean =
		!bSessionActive && !ForgeryComponent->IsSubmitPending() && !IsValid(ActiveDisplayCase) && FMath::IsNearlyZero(ForgeryComponent->GetSessionEndServerTime()) && LocalOwnedLocks == 0;
	const bool bActiveSnapshotConsistent = bSessionActive && !ForgeryComponent->IsSubmitPending() && IsValid(ActiveDisplayCase) && ActiveDisplayCase->IsSessionLocked() &&
										   ActiveDisplayCase->GetSessionOwner() == HeistPlayerState && LocalOwnedLocks == 1;
	const bool bSessionContract = bInactiveSnapshotClean || bActiveSnapshotConsistent;
	const bool bExpectedPresentationVisible = bSessionActive;
	const bool bUIContract = ForgeryViewModel->IsPresentationVisible() == bExpectedPresentationVisible && ForgeryWidget->IsWidgetPresentationVisible() == bExpectedPresentationVisible &&
							 ForgeryViewModel->GetVisibleStateCount() == (bExpectedPresentationVisible ? 1 : 0) && ForgeryWidget->IsOwnerOnlyContractSatisfied() && LocalForgeryWidgetCount == 1;
	const bool bInputContract = HeistPlayerController->IsLocalInputModeContractSatisfied() &&
								(bSessionActive ? HeistPlayerController->GetLocalInputMode() == EHeistInputMode::Forgery : HeistPlayerController->GetLocalInputMode() != EHeistInputMode::Forgery);
	const bool bGlobalContract = !bAuthority || (OrphanLocks == 0 && OrphanSessions == 0 && LockedCases == ActiveSessions);
	const bool bContractPassed = bSessionContract && bUIContract && bInputContract && bGlobalContract;

	Message(
		PlayerController,
		FString::Printf(
			TEXT(
				"Forgery recovery dump: Active=%s SubmitPending=%s ActiveCase=%s LastCleanup=%s LocalOwnedLocks=%d LockedCases=%d ActiveSessions=%d OrphanLocks=%d OrphanSessions=%d WidgetInstances=%d UIVisible=%s InputMode=%s ActiveContexts=%d SessionContract=%s UIContract=%s InputContract=%s GlobalContract=%s Authority=%s Result=%s"),
			bSessionActive ? TEXT("true") : TEXT("false"), ForgeryComponent->IsSubmitPending() ? TEXT("true") : TEXT("false"), *GetNameSafe(ActiveDisplayCase),
			ForgeryComponent->GetLastCleanupReason().IsNone() ? TEXT("None") : *ForgeryComponent->GetLastCleanupReason().ToString(), LocalOwnedLocks, LockedCases,
			bAuthority ? ActiveSessions : INDEX_NONE, bAuthority ? OrphanLocks : INDEX_NONE, bAuthority ? OrphanSessions : INDEX_NONE, LocalForgeryWidgetCount,
			ForgeryWidget->IsWidgetPresentationVisible() ? TEXT("true") : TEXT("false"),
			HeistPlayerController->GetLocalInputMode() == EHeistInputMode::Gameplay	   ? TEXT("Gameplay")
			: HeistPlayerController->GetLocalInputMode() == EHeistInputMode::Inventory ? TEXT("Inventory")
																					   : TEXT("Forgery"),
			HeistPlayerController->GetActiveHeistInputMappingContextCount(), bSessionContract ? TEXT("true") : TEXT("false"), bUIContract ? TEXT("true") : TEXT("false"),
			bInputContract ? TEXT("true") : TEXT("false"), bGlobalContract ? TEXT("true") : TEXT("false"), bAuthority ? TEXT("true") : TEXT("false"), bContractPassed ? TEXT("PASS") : TEXT("FAIL")),
		bContractPassed ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 20.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryRecoveryRace(APlayerController* PlayerController, const FString& Order)
{
#if !UE_BUILD_SHIPPING
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	AHeistHUD* HeistHUD = IsValid(HeistPlayerController) ? HeistPlayerController->GetHUD<AHeistHUD>() : nullptr;
	UHeistForgeryWidget* ForgeryWidget = IsValid(HeistHUD) ? HeistHUD->GetForgeryWidget() : nullptr;
	UHeistForgeryComponent* ForgeryComponent = ResolveForgeryComponent(PlayerController);
	const bool bCancelFirst = Order.Equals(TEXT("CancelSubmit"), ESearchCase::IgnoreCase);
	const bool bSubmitFirst = Order.Equals(TEXT("SubmitCancel"), ESearchCase::IgnoreCase);
	if (!IsValid(HeistPlayerController) || !HeistPlayerController->IsLocalController() || !IsValid(ForgeryWidget) || !IsValid(ForgeryComponent) || !ForgeryComponent->IsSessionActive() ||
		(!bCancelFirst && !bSubmitFirst))
	{
		Message(PlayerController, FString::Printf(TEXT("Forgery recovery race: Order=%s Result=FAIL Reason=InvalidLocalActiveSessionOrOrder Expected=<CancelSubmit|SubmitCancel>"), *Order),
				EHeistDebugLevel::Warning, true, 15.0f);
		return;
	}

	bool bSubmitRequested = false;
	if (bCancelFirst)
	{
		HeistPlayerController->RequestCancelForgery();
		bSubmitRequested = ForgeryWidget->RequestSubmitCollectedStrokes();
	}
	else
	{
		bSubmitRequested = ForgeryWidget->RequestSubmitCollectedStrokes();
		HeistPlayerController->RequestCancelForgery();
	}

	Message(PlayerController,
			FString::Printf(TEXT("Forgery recovery race: Order=%s SubmitRequested=%s CancelRequested=true ClientSessionRevision=%d Result=REQUESTED"),
							bCancelFirst ? TEXT("CancelSubmit") : TEXT("SubmitCancel"), bSubmitRequested ? TEXT("true") : TEXT("false"), ForgeryComponent->GetSessionRevision()),
			EHeistDebugLevel::Info, true, 15.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryUIDump(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	AHeistHUD* HeistHUD = IsValid(HeistPlayerController) ? HeistPlayerController->GetHUD<AHeistHUD>() : nullptr;
	if (IsValid(HeistHUD))
	{
		HeistHUD->RefreshPresentationSources();
	}

	const UHeistForgeryViewModel* ForgeryViewModel = IsValid(HeistHUD) ? HeistHUD->GetForgeryViewModel() : nullptr;
	const UHeistForgeryWidget* ForgeryWidget = IsValid(HeistHUD) ? HeistHUD->GetForgeryWidget() : nullptr;
	const bool bLocalController = IsValid(HeistPlayerController) && HeistPlayerController->IsLocalController();
	const bool bViewModelReady = IsValid(ForgeryViewModel);
	const bool bWidgetReady = IsValid(ForgeryWidget);
	const bool bExpectedVisible = bViewModelReady && ForgeryViewModel->IsPresentationVisible();
	const bool bWidgetVisible = bWidgetReady && ForgeryWidget->IsWidgetPresentationVisible();
	const int32 VisibleStateCount = bViewModelReady ? ForgeryViewModel->GetVisibleStateCount() : 0;
	const bool bExclusiveStateContract = (!bExpectedVisible && VisibleStateCount == 0) || (bExpectedVisible && VisibleStateCount == 1);
	const bool bOwnerOnlyContract = bWidgetReady && ForgeryWidget->IsOwnerOnlyContractSatisfied();
	const bool bVisibilityContract = bExpectedVisible == bWidgetVisible;
	const bool bContractPassed = bLocalController && bViewModelReady && bWidgetReady && bOwnerOnlyContract && bExclusiveStateContract && bVisibilityContract;

	Message(
		PlayerController,
		FString::Printf(
			TEXT(
				"Forgery UI dump: Controller=%s Local=%s HUD=%s ViewModel=%s Widget=%s Preview=%s Visible=%s WidgetVisible=%s Observation=%s Drawing=%s Validation=%s Result=%s StateCount=%d Exclusive=%s OwnerOnly=%s VisibilityMatch=%s Artifact=%s Case=%s EndServerTime=%.2f ResultScore=%.0f Result=%s"),
			*GetNameSafe(HeistPlayerController), bLocalController ? TEXT("true") : TEXT("false"), *GetNameSafe(HeistHUD), *GetNameSafe(ForgeryViewModel), *GetNameSafe(ForgeryWidget),
			bViewModelReady ? *ForgeryViewModel->GetDebugPreviewState().ToString() : TEXT("None"), bExpectedVisible ? TEXT("true") : TEXT("false"), bWidgetVisible ? TEXT("true") : TEXT("false"),
			bViewModelReady && ForgeryViewModel->IsObservationVisible() ? TEXT("true") : TEXT("false"), bViewModelReady && ForgeryViewModel->IsDrawingVisible() ? TEXT("true") : TEXT("false"),
			bViewModelReady && ForgeryViewModel->IsValidationVisible() ? TEXT("true") : TEXT("false"), bViewModelReady && ForgeryViewModel->IsResultVisible() ? TEXT("true") : TEXT("false"),
			VisibleStateCount, bExclusiveStateContract ? TEXT("true") : TEXT("false"), bOwnerOnlyContract ? TEXT("true") : TEXT("false"), bVisibilityContract ? TEXT("true") : TEXT("false"),
			bViewModelReady ? *ForgeryViewModel->GetReferenceArtifactId().ToString() : TEXT("None"), bViewModelReady ? *ForgeryViewModel->GetActiveDisplayCaseName().ToString() : TEXT("None"),
			bViewModelReady ? ForgeryViewModel->GetStateEndServerTime() : 0.0f, bViewModelReady ? ForgeryViewModel->GetResultScore() : 0.0f, bContractPassed ? TEXT("PASS") : TEXT("FAIL")),
		bContractPassed ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 15.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryUIPreview(APlayerController* PlayerController, const FString& State)
{
#if !UE_BUILD_SHIPPING
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	AHeistHUD* HeistHUD = IsValid(HeistPlayerController) ? HeistPlayerController->GetHUD<AHeistHUD>() : nullptr;
	if (IsValid(HeistHUD))
	{
		HeistHUD->RefreshPresentationSources();
	}

	UHeistForgeryViewModel* ForgeryViewModel = IsValid(HeistHUD) ? HeistHUD->GetForgeryViewModel() : nullptr;
	FName PreviewState = State.IsEmpty() || State.Equals(TEXT("None"), ESearchCase::IgnoreCase) ? NAME_None : FName(*State);
	if (PreviewState == FName(TEXT("Observation")))
	{
		PreviewState = FName(TEXT("Observation"));
	}
	else if (PreviewState == FName(TEXT("Drawing")))
	{
		PreviewState = FName(TEXT("Drawing"));
	}
	else if (PreviewState == FName(TEXT("Validation")))
	{
		PreviewState = FName(TEXT("Validation"));
	}
	else if (PreviewState == FName(TEXT("Result")))
	{
		PreviewState = FName(TEXT("Result"));
	}

	const bool bApplied = IsValid(ForgeryViewModel) && ForgeryViewModel->SetDebugPreviewState(PreviewState);
	Message(PlayerController,
			FString::Printf(TEXT("Forgery UI preview: Requested=%s Applied=%s LocalOnly=true Result=%s"), PreviewState.IsNone() ? TEXT("None") : *PreviewState.ToString(),
							bApplied ? TEXT("true") : TEXT("false"), bApplied ? TEXT("PASS") : TEXT("REJECTED")),
			bApplied ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 10.0f);
	if (bApplied)
	{
		DebugForgeryUIDump(PlayerController);
	}
#endif
}

#pragma endregion

#pragma region SoundPingDebug

void UHeistDebugFunctionLibrary::DebugSoundPingHelp(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(PlayerController, TEXT("Sound Ping gameplay debug commands: HeistFootstepWeight <Weight> | HeistCoinThrow <Distance>. Player-facing SoundPing markers are removed."),
			EHeistDebugLevel::Info, true, 8.0f);
#endif
}

#pragma endregion

#pragma region GuardDebug

void UHeistDebugFunctionLibrary::DebugGuardHelp(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		PlayerController,
		TEXT(
			"Guard debug commands: HeistDifficultyDump | HeistGuardSpawn <Distance> | HeistGuardDump | HeistInspectionTargetSelect | HeistInspectionTargetDump | HeistInspectionBegin | HeistInspectionStateDump | HeistInspectionProtectionDump | HeistInspectionTimerProtectionTest | HeistAlertDump | HeistLockdownDump | HeistGuardAlertModifiersDump | HeistAlertRequest <Quiet|Suspicious|Searching|Alarmed|Lockdown> | HeistGuardState <Disabled|Patrol|Investigate|Chase|Search|Return> <Duration> | HeistGuardSightCheck | HeistGuardSightAuto <0|1> | HeistGuardNoise <Distance> | HeistArrest | HeistRelease | HeistArrestDump"),
		EHeistDebugLevel::Info, true, 10.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugDifficultyDump(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Difficulty baseline dump failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->DebugRequestDumpDifficultyBaseline();
	Message(PlayerController, TEXT("Difficulty baseline dump requested."), EHeistDebugLevel::Info, true);
#endif
}

void UHeistDebugFunctionLibrary::DebugGuardSpawn(APlayerController* PlayerController, const float Distance)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Guard debug spawn failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	const float SafeDistance = FMath::Clamp(Distance, 100.0f, 3000.0f);
	HeistPlayerController->DebugRequestSpawnGuard(SafeDistance);
	Message(PlayerController, FString::Printf(TEXT("Guard debug spawn requested: Distance=%.1f"), SafeDistance), EHeistDebugLevel::Info, true);
#endif
}

void UHeistDebugFunctionLibrary::DebugGuardDump(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	const AHeistGuardCharacter* GuardCharacter = ResolveNearestGuard(PlayerController);
	const UHeistGuardStateComponent* GuardStateComponent = IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
	const UHeistPatrolPathComponent* PatrolPathComponent = IsValid(GuardCharacter) ? GuardCharacter->GetPatrolPathComponent() : nullptr;
	if (!IsValid(GuardCharacter) || !IsValid(GuardStateComponent))
	{
		Message(PlayerController, TEXT("Guard dump failed: no replicated Guard exists."), EHeistDebugLevel::Warning, true);
		return;
	}

	const float ServerTime = PlayerController->GetWorld() && PlayerController->GetWorld()->GetGameState() ? PlayerController->GetWorld()->GetGameState()->GetServerWorldTimeSeconds() : 0.0f;
	const float RemainingSeconds = FMath::Max(0.0f, GuardStateComponent->GetStateEndServerTime() - ServerTime);
	const FVector FocusLocation = GuardStateComponent->GetStateFocusLocation();
	const FVector GuardLocation = GuardCharacter->GetActorLocation();
	Message(PlayerController,
			FString::Printf(TEXT("Guard dump: Guard=%s State=%s Remaining=%.2f Location=(%.1f,%.1f,%.1f) Focus=(%.1f,%.1f,%.1f) RouteId=%s Waypoint=%d/%d Authority=%s"), *GetNameSafe(GuardCharacter),
							*UEnum::GetValueAsString(GuardStateComponent->GetGuardState()), RemainingSeconds, GuardLocation.X, GuardLocation.Y, GuardLocation.Z, FocusLocation.X, FocusLocation.Y,
							FocusLocation.Z, IsValid(PatrolPathComponent) ? *PatrolPathComponent->GetPatrolRouteId().ToString() : TEXT("None"),
							IsValid(PatrolPathComponent) ? PatrolPathComponent->GetCurrentWaypointIndex() : INDEX_NONE, IsValid(PatrolPathComponent) ? PatrolPathComponent->GetWaypointCount() : 0,
							GuardCharacter->HasAuthority() ? TEXT("true") : TEXT("false")),
			EHeistDebugLevel::Info, true, 8.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugInspectionTargetSelect(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	if (!IsValid(PlayerController) || !PlayerController->HasAuthority() || !IsValid(PlayerController->GetWorld()))
	{
		Message(PlayerController, TEXT("Inspection target select: Authority=false Result=FAIL Reason=ServerOnly"), EHeistDebugLevel::Warning, true);
		return;
	}

	int32 GuardCount = 0;
	int32 SelectedCount = 0;
	for (TActorIterator<AHeistGuardAIController> It(PlayerController->GetWorld()); It; ++It)
	{
		AHeistGuardAIController* GuardController = *It;
		if (!IsValid(GuardController) || !IsValid(GuardController->GetPawn()))
		{
			continue;
		}

		++GuardCount;
		SelectedCount += GuardController->TrySelectInspectionTarget() ? 1 : 0;
	}

	const bool bPassed = GuardCount > 0 && SelectedCount == GuardCount;
	Message(PlayerController, FString::Printf(TEXT("Inspection target select: Guards=%d Selected=%d Authority=true Result=%s"), GuardCount, SelectedCount, bPassed ? TEXT("PASS") : TEXT("FAIL")),
			bPassed ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 12.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugInspectionTargetDump(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	if (!IsValid(PlayerController) || !IsValid(PlayerController->GetWorld()))
	{
		return;
	}

	int32 CaseCount = 0;
	int32 RegisteredCount = 0;
	int32 ValidCandidateCount = 0;
	int32 ScheduledCount = 0;
	int32 PendingDelayCount = 0;
	int32 MaximumRegistrationRevision = 0;
	FString ScheduleSummary;
	for (TActorIterator<AHeistPaintingDisplayCaseActor> It(PlayerController->GetWorld()); It; ++It)
	{
		const AHeistPaintingDisplayCaseActor* DisplayCase = *It;
		if (!IsValid(DisplayCase))
		{
			continue;
		}

		++CaseCount;
		RegisteredCount += DisplayCase->IsRegisteredForInspection() ? 1 : 0;
		ValidCandidateCount += DisplayCase->IsValidInspectionCandidate() ? 1 : 0;
		ScheduledCount += DisplayCase->GetInspectionScheduleRevision() > 0 ? 1 : 0;
		PendingDelayCount += DisplayCase->GetInspectionDelayRemaining() > KINDA_SMALL_NUMBER ? 1 : 0;
		MaximumRegistrationRevision = FMath::Max(MaximumRegistrationRevision, DisplayCase->GetInspectionRegistrationRevision());
		if (DisplayCase->GetInspectionScheduleRevision() > 0)
		{
			if (!ScheduleSummary.IsEmpty())
			{
				ScheduleSummary += TEXT(",");
			}
			ScheduleSummary += FString::Printf(TEXT("%s:Score=%.2f,Band=%s,Delay=%.2f,Remaining=%.2f,Case=%s,Alert=%s,Rev=%d"), *GetNameSafe(DisplayCase),
											   DisplayCase->GetCommittedForgeryResult().SimilarityScore, *DisplayCase->GetInspectionScoreBand().ToString(), DisplayCase->GetResolvedInspectionDelay(),
											   DisplayCase->GetInspectionDelayRemaining(), *UEnum::GetValueAsString(DisplayCase->GetResolvedInspectionCaseOutcome()),
											   *UEnum::GetValueAsString(DisplayCase->GetResolvedInspectionAlertOutcome()), DisplayCase->GetInspectionScheduleRevision());
		}
	}

	int32 GuardCount = 0;
	int32 ValidSelectionCount = 0;
	FString SelectionSummary;
	for (TActorIterator<AHeistGuardAIController> It(PlayerController->GetWorld()); It; ++It)
	{
		const AHeistGuardAIController* GuardController = *It;
		if (!IsValid(GuardController) || !IsValid(GuardController->GetPawn()))
		{
			continue;
		}

		++GuardCount;
		const bool bSelectionValid = GuardController->IsInspectionTargetValid();
		ValidSelectionCount += bSelectionValid ? 1 : 0;
		if (!SelectionSummary.IsEmpty())
		{
			SelectionSummary += TEXT(",");
		}
		SelectionSummary +=
			FString::Printf(TEXT("%s->%s@%d"), *GetNameSafe(GuardController->GetPawn()), *GetNameSafe(GuardController->GetInspectionTarget()), GuardController->GetInspectionTargetSelectionRevision());
	}

	const bool bAuthority = PlayerController->HasAuthority();
	FString MappingSummary;
	const float TestScores[] = {95.0f, 80.0f, 60.0f, 40.0f, 20.0f};
	const float ExpectedDelays[] = {32.0f, 16.0f, 8.0f, 4.0f, 0.0f};
	const FName ExpectedBands[] = {FName(TEXT("90-100")), FName(TEXT("70-89")), FName(TEXT("50-69")), FName(TEXT("30-49")), FName(TEXT("0-29"))};
	const EHeistAlertLevel ExpectedAlertOutcomes[] = {EHeistAlertLevel::Quiet, EHeistAlertLevel::Suspicious, EHeistAlertLevel::Searching, EHeistAlertLevel::Alarmed, EHeistAlertLevel::Alarmed};
	const EHeistDisplayCaseState ExpectedCaseOutcomes[] = {EHeistDisplayCaseState::Completed, EHeistDisplayCaseState::Suspected, EHeistDisplayCaseState::Suspected, EHeistDisplayCaseState::Alarmed,
														   EHeistDisplayCaseState::Alarmed};
	bool bMappingPassed = true;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(TestScores); ++Index)
	{
		float Delay = 0.0f;
		FName Band;
		EHeistAlertLevel AlertOutcome = EHeistAlertLevel::Quiet;
		EHeistDisplayCaseState CaseOutcome = EHeistDisplayCaseState::Failed;
		const bool bMapped = AHeistPaintingDisplayCaseActor::CalculateInspectionSchedule(TestScores[Index], 8.0f, Delay, Band, AlertOutcome, CaseOutcome);
		bMappingPassed = bMappingPassed && bMapped && FMath::IsNearlyEqual(Delay, ExpectedDelays[Index]) && Band == ExpectedBands[Index] && AlertOutcome == ExpectedAlertOutcomes[Index] &&
						 CaseOutcome == ExpectedCaseOutcomes[Index];
		if (!MappingSummary.IsEmpty())
		{
			MappingSummary += TEXT(",");
		}
		MappingSummary += FString::Printf(TEXT("%.0f:%s/%.0f/%s/%s"), TestScores[Index], *Band.ToString(), Delay, *UEnum::GetValueAsString(CaseOutcome), *UEnum::GetValueAsString(AlertOutcome));
	}

	const bool bRuntimeScheduleValid = ScheduledCount > 0 && ValidCandidateCount <= RegisteredCount && PendingDelayCount + RegisteredCount <= CaseCount;
	const bool bPassed = bAuthority && CaseCount > 0 && GuardCount > 0 && bMappingPassed && bRuntimeScheduleValid;
	Message(
		PlayerController,
		FString::Printf(
			TEXT(
				"Inspection target dump: Cases=%d Registered=%d ValidCandidates=%d Scheduled=%d PendingDelay=%d RegistrationRevision=%d Guards=%d ValidSelections=%d Selections=%s Schedules=%s MappingBase8={%s} Mapping=%s Authority=%s Result=%s"),
			CaseCount, RegisteredCount, ValidCandidateCount, ScheduledCount, PendingDelayCount, MaximumRegistrationRevision, GuardCount, ValidSelectionCount,
			SelectionSummary.IsEmpty() ? TEXT("None") : *SelectionSummary, ScheduleSummary.IsEmpty() ? TEXT("None") : *ScheduleSummary, *MappingSummary, bMappingPassed ? TEXT("PASS") : TEXT("FAIL"),
			bAuthority ? TEXT("true") : TEXT("false"), bPassed ? TEXT("PASS") : TEXT("FAIL")),
		bPassed ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 15.0f);

#endif
}

void UHeistDebugFunctionLibrary::DebugInspectionBegin(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	if (!IsValid(PlayerController) || !PlayerController->HasAuthority() || !IsValid(PlayerController->GetWorld()))
	{
		Message(PlayerController, TEXT("Inspection begin: Authority=false Result=FAIL Reason=ServerOnly"), EHeistDebugLevel::Warning, true);
		return;
	}

	int32 EligibleCaseCount = 0;
	for (TActorIterator<AHeistPaintingDisplayCaseActor> It(PlayerController->GetWorld()); It; ++It)
	{
		EligibleCaseCount += IsValid(*It) && It->IsValidInspectionCandidate() ? 1 : 0;
	}

	int32 GuardCount = 0;
	int32 EligibleGuardCount = 0;
	int32 StartedCount = 0;
	for (TActorIterator<AHeistGuardAIController> It(PlayerController->GetWorld()); It; ++It)
	{
		AHeistGuardAIController* GuardController = *It;
		const AHeistGuardCharacter* GuardCharacter = IsValid(GuardController) ? Cast<AHeistGuardCharacter>(GuardController->GetPawn()) : nullptr;
		const UHeistGuardStateComponent* GuardStateComponent = IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
		if (!IsValid(GuardController) || !IsValid(GuardCharacter) || !IsValid(GuardStateComponent))
		{
			continue;
		}

		++GuardCount;
		if (GuardStateComponent->GetGuardState() != EHeistGuardState::Patrol)
		{
			continue;
		}

		++EligibleGuardCount;
		StartedCount += GuardController->TryBeginInspection() ? 1 : 0;
	}

	const int32 ExpectedStartedCount = FMath::Min(EligibleGuardCount, EligibleCaseCount);
	const int32 DuplicateClaimsBlocked = FMath::Max(0, EligibleGuardCount - StartedCount);
	const bool bPassed = EligibleGuardCount >= 2 && EligibleCaseCount == 1 && StartedCount == 1 && ExpectedStartedCount == 1 && DuplicateClaimsBlocked >= 1;
	Message(PlayerController,
			FString::Printf(TEXT("Inspection begin: Guards=%d EligibleGuards=%d EligibleCases=%d Started=%d ExpectedStarted=%d DuplicateClaimsBlocked=%d Authority=true Result=%s"),
							GuardCount, EligibleGuardCount, EligibleCaseCount, StartedCount, ExpectedStartedCount, DuplicateClaimsBlocked, bPassed ? TEXT("PASS") : TEXT("FAIL")),
			bPassed ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 12.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugInspectionStateDump(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	if (!IsValid(PlayerController) || !IsValid(PlayerController->GetWorld()))
	{
		return;
	}

	int32 GuardCount = 0;
	int32 InspectingGuardCount = 0;
	int32 SuspectedCaseCount = 0;
	FString GuardSummary;
	for (TActorIterator<AHeistGuardAIController> It(PlayerController->GetWorld()); It; ++It)
	{
		const AHeistGuardAIController* GuardController = *It;
		const AHeistGuardCharacter* GuardCharacter = IsValid(GuardController) ? Cast<AHeistGuardCharacter>(GuardController->GetPawn()) : nullptr;
		const UHeistGuardStateComponent* GuardStateComponent = IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
		if (!IsValid(GuardStateComponent))
		{
			continue;
		}

		++GuardCount;
		InspectingGuardCount += GuardStateComponent->GetGuardState() == EHeistGuardState::InspectExhibit ? 1 : 0;
		if (!GuardSummary.IsEmpty())
		{
			GuardSummary += TEXT(",");
		}
		GuardSummary +=
			FString::Printf(TEXT("%s:%s->%s"), *GetNameSafe(GuardCharacter), *UEnum::GetValueAsString(GuardStateComponent->GetGuardState()), *GetNameSafe(GuardController->GetInspectionTarget()));
	}

	for (TActorIterator<AHeistPaintingDisplayCaseActor> It(PlayerController->GetWorld()); It; ++It)
	{
		const AHeistPaintingDisplayCaseActor* DisplayCase = *It;
		if (IsValid(DisplayCase) && DisplayCase->GetDisplayCaseState() == EHeistDisplayCaseState::Suspected)
		{
			++SuspectedCaseCount;
		}
	}

	const bool bAuthority = PlayerController->HasAuthority();
	const bool bPassed = bAuthority && GuardCount > 0 && (InspectingGuardCount > 0 || SuspectedCaseCount > 0);
	Message(PlayerController,
			FString::Printf(TEXT("Inspection state dump: Guards=%d Inspecting=%d SuspectedCases=%d GuardStates=%s Authority=%s Result=%s"), GuardCount, InspectingGuardCount, SuspectedCaseCount,
							GuardSummary.IsEmpty() ? TEXT("None") : *GuardSummary, bAuthority ? TEXT("true") : TEXT("false"), bPassed ? TEXT("PASS") : TEXT("FAIL")),
			bPassed ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 15.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugInspectionProtectionDump(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	if (!IsValid(PlayerController) || !IsValid(PlayerController->GetWorld()))
	{
		return;
	}

	UWorld* World = PlayerController->GetWorld();
	const AHeistGameState* HeistGameState = World->GetGameState<AHeistGameState>();
	const AHeistGameMode* HeistGameMode = World->GetAuthGameMode<AHeistGameMode>();
	if (!IsValid(HeistGameState))
	{
		Message(PlayerController, TEXT("Inspection protection dump: Result=FAIL Reason=MissingGameState"), EHeistDebugLevel::Warning, true);
		return;
	}

	TMap<const AHeistPaintingDisplayCaseActor*, int32> InspectingGuardsByCase;
	TSet<const AHeistPaintingDisplayCaseActor*> OwnerMatchedCases;
	int32 GuardCount = 0;
	int32 InspectingGuardCount = 0;
	int32 GuardStateTimerCount = 0;
	int32 OrphanInspectingGuardCount = 0;
	for (TActorIterator<AHeistGuardAIController> It(World); It; ++It)
	{
		const AHeistGuardAIController* GuardController = *It;
		const AHeistGuardCharacter* GuardCharacter = IsValid(GuardController) ? Cast<AHeistGuardCharacter>(GuardController->GetPawn()) : nullptr;
		const UHeistGuardStateComponent* GuardStateComponent = IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
		if (!IsValid(GuardController) || !IsValid(GuardCharacter) || !IsValid(GuardStateComponent))
		{
			continue;
		}

		++GuardCount;
		GuardStateTimerCount += GuardStateComponent->IsStateTimerActive() ? 1 : 0;
		if (GuardStateComponent->GetGuardState() != EHeistGuardState::InspectExhibit)
		{
			continue;
		}

		++InspectingGuardCount;
		const AHeistPaintingDisplayCaseActor* Target = GuardController->GetInspectionTarget();
		if (!IsValid(Target))
		{
			++OrphanInspectingGuardCount;
			continue;
		}

		++InspectingGuardsByCase.FindOrAdd(Target);
		if (Target->GetInspectingGuard() == GuardCharacter)
		{
			OwnerMatchedCases.Add(Target);
		}
	}

	int32 CaseCount = 0;
	int32 ScheduledCaseCount = 0;
	int32 ActiveClaimCount = 0;
	int32 InspectionDelayTimerCount = 0;
	int32 ClaimViolationCount = 0;
	int32 ResultViolationCount = 0;
	int32 ResultApplicationCount = 0;
	int32 DuplicateResultBlockCount = 0;
	int32 MaximumInspectingGuardsPerCase = 0;
	for (TActorIterator<AHeistPaintingDisplayCaseActor> It(World); It; ++It)
	{
		const AHeistPaintingDisplayCaseActor* DisplayCase = *It;
		if (!IsValid(DisplayCase))
		{
			continue;
		}

		++CaseCount;
		ScheduledCaseCount += DisplayCase->GetInspectionScheduleRevision() > 0 ? 1 : 0;
		InspectionDelayTimerCount += DisplayCase->IsInspectionDelayTimerActive() ? 1 : 0;
		ResultApplicationCount += DisplayCase->GetInspectionResultApplicationCount();
		DuplicateResultBlockCount += DisplayCase->GetInspectionDuplicateBlockCount();
		ResultViolationCount += DisplayCase->GetInspectionResultApplicationCount() > 1 ? 1 : 0;

		const int32 AssignedGuardCount = InspectingGuardsByCase.FindRef(DisplayCase);
		MaximumInspectingGuardsPerCase = FMath::Max(MaximumInspectingGuardsPerCase, AssignedGuardCount);
		if (DisplayCase->IsInspectionClaimActive())
		{
			++ActiveClaimCount;
			if (AssignedGuardCount != 1 || !OwnerMatchedCases.Contains(DisplayCase))
			{
				++ClaimViolationCount;
			}
		}
		else if (AssignedGuardCount > 0)
		{
			++ClaimViolationCount;
		}
	}

	const int32 MatchTimerCount = IsValid(HeistGameMode) ? HeistGameMode->GetActiveMatchTimerCount() : 0;
	const bool bMatchEnded = HeistGameState->GetMatchPhase() == EHeistMatchPhase::End;
	const int32 PostMatchResidualCount = bMatchEnded ? ActiveClaimCount + InspectingGuardCount + InspectionDelayTimerCount + GuardStateTimerCount + MatchTimerCount : 0;
	const bool bDuplicateProtectionValid = MaximumInspectingGuardsPerCase <= 1 && ClaimViolationCount == 0 && OrphanInspectingGuardCount == 0 && ResultViolationCount == 0;
	const bool bTimerProtectionValid = !bMatchEnded || PostMatchResidualCount == 0;
	const bool bPassed = PlayerController->HasAuthority() && CaseCount > 0 && GuardCount >= 2 && ScheduledCaseCount > 0 && bDuplicateProtectionValid && bTimerProtectionValid;

	Message(
		PlayerController,
		FString::Printf(
			TEXT(
				"Inspection protection dump: Phase=%s Cases=%d ScheduledCases=%d Guards=%d Claims=%d InspectingGuards=%d MaxGuardsPerCase=%d ClaimViolations=%d OrphanGuards=%d "
				"ResultApplications=%d DuplicateResultBlocks=%d ResultViolations=%d DelayTimers=%d GuardStateTimers=%d MatchTimers=%d PostMatchResidual=%d DuplicateProtection=%s "
				"TimerProtection=%s Authority=%s Result=%s"),
			*UEnum::GetValueAsString(HeistGameState->GetMatchPhase()), CaseCount, ScheduledCaseCount, GuardCount, ActiveClaimCount, InspectingGuardCount,
			MaximumInspectingGuardsPerCase, ClaimViolationCount, OrphanInspectingGuardCount, ResultApplicationCount, DuplicateResultBlockCount, ResultViolationCount,
			InspectionDelayTimerCount, GuardStateTimerCount, MatchTimerCount, PostMatchResidualCount, bDuplicateProtectionValid ? TEXT("PASS") : TEXT("FAIL"),
			bTimerProtectionValid ? TEXT("PASS") : TEXT("FAIL"), PlayerController->HasAuthority() ? TEXT("true") : TEXT("false"), bPassed ? TEXT("PASS") : TEXT("FAIL")),
		bPassed ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 15.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugInspectionTimerProtectionTest(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	if (!IsValid(PlayerController) || !PlayerController->HasAuthority() || !IsValid(PlayerController->GetWorld()))
	{
		Message(PlayerController, TEXT("Inspection timer protection test: Authority=false Result=FAIL Reason=ServerOnly"), EHeistDebugLevel::Warning, true);
		return;
	}

	UWorld* World = PlayerController->GetWorld();
	AHeistGameState* HeistGameState = World->GetGameState<AHeistGameState>();
	AHeistGameMode* HeistGameMode = World->GetAuthGameMode<AHeistGameMode>();
	if (!IsValid(HeistGameState) || !IsValid(HeistGameMode) || HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame)
	{
		Message(PlayerController, TEXT("Inspection timer protection test: Result=FAIL Reason=InvalidInGameState"), EHeistDebugLevel::Warning, true);
		return;
	}

	int32 GuardCount = 0;
	int32 InspectingGuardCountBefore = 0;
	int32 GuardStateTimerCountBefore = 0;
	for (TActorIterator<AHeistGuardAIController> It(World); It; ++It)
	{
		AHeistGuardAIController* GuardController = *It;
		AHeistGuardCharacter* GuardCharacter = IsValid(GuardController) ? Cast<AHeistGuardCharacter>(GuardController->GetPawn()) : nullptr;
		UHeistGuardStateComponent* GuardStateComponent = IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
		if (!IsValid(GuardController) || !IsValid(GuardStateComponent))
		{
			continue;
		}

		++GuardCount;
		if (GuardStateComponent->GetGuardState() != EHeistGuardState::InspectExhibit)
		{
			continue;
		}

		++InspectingGuardCountBefore;
		if (!GuardStateComponent->IsStateTimerActive())
		{
			GuardController->StartInspectionCast();
		}
		GuardStateTimerCountBefore += GuardStateComponent->IsStateTimerActive() ? 1 : 0;
	}

	int32 ActiveClaimCountBefore = 0;
	for (TActorIterator<AHeistPaintingDisplayCaseActor> It(World); It; ++It)
	{
		ActiveClaimCountBefore += IsValid(*It) && It->IsInspectionClaimActive() ? 1 : 0;
	}
	const int32 MatchTimerCountBefore = HeistGameMode->GetActiveMatchTimerCount();
	const bool bPreconditionsValid =
		GuardCount >= 2 && InspectingGuardCountBefore == 1 && ActiveClaimCountBefore == 1 && GuardStateTimerCountBefore == 1;
	if (!bPreconditionsValid)
	{
		Message(PlayerController,
				FString::Printf(
					TEXT("Inspection timer protection test: Guards=%d InspectingBefore=%d ClaimsBefore=%d GuardStateTimersBefore=%d MatchTimersBefore=%d Result=FAIL Reason=InvalidPreconditions"),
					GuardCount, InspectingGuardCountBefore, ActiveClaimCountBefore, GuardStateTimerCountBefore, MatchTimerCountBefore),
				EHeistDebugLevel::Warning, true, 15.0f);
		return;
	}

	const bool bMatchEnded = HeistGameState->SetMatchPhase(EHeistMatchPhase::End);
	int32 InspectingGuardCountAfter = 0;
	int32 GuardStateTimerCountAfter = 0;
	for (TActorIterator<AHeistGuardAIController> It(World); It; ++It)
	{
		const AHeistGuardAIController* GuardController = *It;
		const AHeistGuardCharacter* GuardCharacter = IsValid(GuardController) ? Cast<AHeistGuardCharacter>(GuardController->GetPawn()) : nullptr;
		const UHeistGuardStateComponent* GuardStateComponent = IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
		if (!IsValid(GuardStateComponent))
		{
			continue;
		}

		InspectingGuardCountAfter += GuardStateComponent->GetGuardState() == EHeistGuardState::InspectExhibit ? 1 : 0;
		GuardStateTimerCountAfter += GuardStateComponent->IsStateTimerActive() ? 1 : 0;
	}

	int32 ActiveClaimCountAfter = 0;
	int32 InspectionDelayTimerCountAfter = 0;
	for (TActorIterator<AHeistPaintingDisplayCaseActor> It(World); It; ++It)
	{
		const AHeistPaintingDisplayCaseActor* DisplayCase = *It;
		if (!IsValid(DisplayCase))
		{
			continue;
		}

		ActiveClaimCountAfter += DisplayCase->IsInspectionClaimActive() ? 1 : 0;
		InspectionDelayTimerCountAfter += DisplayCase->IsInspectionDelayTimerActive() ? 1 : 0;
	}

	const int32 MatchTimerCountAfter = HeistGameMode->GetActiveMatchTimerCount();
	const int32 ResidualTimerCount = GuardStateTimerCountAfter + InspectionDelayTimerCountAfter + MatchTimerCountAfter;
	const bool bPassed = bMatchEnded && HeistGameState->GetMatchPhase() == EHeistMatchPhase::End && InspectingGuardCountAfter == 0 && ActiveClaimCountAfter == 0 &&
						 ResidualTimerCount == 0;
	Message(
		PlayerController,
		FString::Printf(
			TEXT(
				"Inspection timer protection test: Guards=%d InspectingBefore=%d ClaimsBefore=%d GuardStateTimersBefore=%d MatchTimersBefore=%d PhaseAfter=%s InspectingAfter=%d "
				"ClaimsAfter=%d GuardStateTimersAfter=%d DelayTimersAfter=%d MatchTimersAfter=%d ResidualTimers=%d Authority=true Result=%s"),
			GuardCount, InspectingGuardCountBefore, ActiveClaimCountBefore, GuardStateTimerCountBefore, MatchTimerCountBefore,
			*UEnum::GetValueAsString(HeistGameState->GetMatchPhase()), InspectingGuardCountAfter, ActiveClaimCountAfter, GuardStateTimerCountAfter,
			InspectionDelayTimerCountAfter, MatchTimerCountAfter, ResidualTimerCount, bPassed ? TEXT("PASS") : TEXT("FAIL")),
		bPassed ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 15.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugAlertDump(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	if (!IsValid(PlayerController) || !IsValid(PlayerController->GetWorld()))
	{
		return;
	}

	const AHeistGameState* HeistGameState = PlayerController->GetWorld()->GetGameState<AHeistGameState>();
	const AHeistGameMode* HeistGameMode = PlayerController->GetWorld()->GetAuthGameMode<AHeistGameMode>();
	if (!IsValid(HeistGameState))
	{
		Message(PlayerController, TEXT("Global alert dump: Result=FAIL Reason=MissingGameState"), EHeistDebugLevel::Warning, true);
		return;
	}

	const EHeistAlertLevel AlertLevel = HeistGameState->GetAlertLevel();
	const bool bTransitionExpected = AlertLevel == EHeistAlertLevel::Suspicious || AlertLevel == EHeistAlertLevel::Searching || AlertLevel == EHeistAlertLevel::Alarmed;
	const bool bSnapshotValid = bTransitionExpected ? HeistGameState->GetAlertNextTransitionServerTime() > 0.0f : HeistGameState->GetAlertNextTransitionServerTime() <= 0.0f;
	const bool bAuthority = PlayerController->HasAuthority();
	const bool bTimerValid = !bAuthority || !bTransitionExpected || (IsValid(HeistGameMode) && HeistGameMode->IsAlertTransitionTimerActive());
	const bool bPassed = bSnapshotValid && bTimerValid;
	Message(
		PlayerController,
		FString::Printf(
			TEXT(
				"Global alert dump: Level=%s NextTransitionServerTime=%.2f Remaining=%.2f Revision=%d LastTrigger=%s TimerActive=%s ProcessedTriggers=%d Chain=Quiet>Suspicious>Searching>Alarmed>Lockdown Snapshot=%s Authority=%s Result=%s"),
			*UEnum::GetValueAsString(AlertLevel), HeistGameState->GetAlertNextTransitionServerTime(), HeistGameState->GetAlertTransitionRemainingSeconds(), HeistGameState->GetAlertRevision(),
			*HeistGameState->GetLastAlertTriggerId().ToString(), IsValid(HeistGameMode) && HeistGameMode->IsAlertTransitionTimerActive() ? TEXT("true") : TEXT("false"),
			IsValid(HeistGameMode) ? HeistGameMode->GetProcessedAlertTriggerCount() : INDEX_NONE, bSnapshotValid ? TEXT("PASS") : TEXT("FAIL"), bAuthority ? TEXT("true") : TEXT("false"),
			bPassed ? TEXT("PASS") : TEXT("FAIL")),
		bPassed ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 15.0f);

	AHeistHUD* HeistHUD = Cast<AHeistHUD>(PlayerController->GetHUD());
	if (IsValid(HeistHUD))
	{
		HeistHUD->RefreshPresentationSources();
	}

	UHeistHUDWidget* HUDWidget = IsValid(HeistHUD) ? HeistHUD->GetMainHUDWidget() : nullptr;
	UHeistForgeryWidget* ForgeryWidget = IsValid(HeistHUD) ? HeistHUD->GetForgeryWidget() : nullptr;
	if (IsValid(HUDWidget))
	{
		HUDWidget->DebugDumpAlertPresentationState();
	}
	else
	{
		Message(PlayerController, TEXT("Alert HUD presentation: Result=FAIL Reason=MissingHUDWidget"), EHeistDebugLevel::Warning, true);
	}

	if (IsValid(ForgeryWidget))
	{
		ForgeryWidget->DebugDumpAlertWarningState();
	}
	else
	{
		Message(PlayerController, TEXT("Forgery alert warning: Result=FAIL Reason=MissingForgeryWidget"), EHeistDebugLevel::Warning, true);
	}
#endif
}

void UHeistDebugFunctionLibrary::DebugLockdownDump(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	if (!IsValid(PlayerController) || !IsValid(PlayerController->GetWorld()))
	{
		return;
	}

	const AHeistGameState* HeistGameState = PlayerController->GetWorld()->GetGameState<AHeistGameState>();
	if (!IsValid(HeistGameState))
	{
		Message(PlayerController, TEXT("Lockdown dump: Result=FAIL Reason=MissingGameState"), EHeistDebugLevel::Warning, true);
		return;
	}

	int32 PlayerCount = 0;
	int32 ActiveActionCount = 0;
	int32 ActiveForgeryCount = 0;
	int32 OpenInventoryCount = 0;
	int32 MovementEnabledCount = 0;
	for (TActorIterator<AHeistPlayerCharacter> PlayerIterator(PlayerController->GetWorld()); PlayerIterator; ++PlayerIterator)
	{
		const AHeistPlayerCharacter* PlayerCharacter = *PlayerIterator;
		if (!IsValid(PlayerCharacter))
		{
			continue;
		}

		++PlayerCount;
		const UHeistActionComponent* ActionComponent = PlayerCharacter->GetActionComponent();
		const UHeistForgeryComponent* ForgeryComponent = PlayerCharacter->GetForgeryComponent();
		const UHeistInventoryComponent* InventoryComponent = PlayerCharacter->GetInventoryComponent();
		const UCharacterMovementComponent* MovementComponent = PlayerCharacter->GetCharacterMovement();
		ActiveActionCount += IsValid(ActionComponent) && ActionComponent->IsGameplayCastActive() ? 1 : 0;
		ActiveForgeryCount += IsValid(ForgeryComponent) && ForgeryComponent->IsSessionActive() ? 1 : 0;
		OpenInventoryCount += IsValid(InventoryComponent) && InventoryComponent->IsInventoryOpen() ? 1 : 0;
		MovementEnabledCount += IsValid(MovementComponent) && MovementComponent->MovementMode != MOVE_None ? 1 : 0;
	}

	int32 VentCount = 0;
	int32 ActiveVentCount = 0;
	for (TActorIterator<AHeistVentActor> VentIterator(PlayerController->GetWorld()); VentIterator; ++VentIterator)
	{
		const AHeistVentActor* VentActor = *VentIterator;
		if (IsValid(VentActor))
		{
			++VentCount;
			ActiveVentCount += VentActor->IsVentActive() ? 1 : 0;
		}
	}

	int32 InteractableCount = 0;
	int32 AvailableInteractionCount = 0;
	const AActor* RequestingActor = PlayerController->GetPawn();
	for (TActorIterator<AHeistInteractableActor> InteractableIterator(PlayerController->GetWorld()); InteractableIterator; ++InteractableIterator)
	{
		const AHeistInteractableActor* InteractableActor = *InteractableIterator;
		if (IsValid(InteractableActor))
		{
			++InteractableCount;
			AvailableInteractionCount += InteractableActor->CanInteract(RequestingActor) ? 1 : 0;
		}
	}

	const EHeistAlertLevel AlertLevel = HeistGameState->GetAlertLevel();
	const bool bCountdownActive = HeistGameState->IsLockdownCountdownActive();
	const bool bWorldRestricted = HeistGameState->AreWorldInteractionsRestricted();
	const bool bAlarmedStateValid = AlertLevel == EHeistAlertLevel::Alarmed && bCountdownActive && !bWorldRestricted && HeistGameState->GetMatchPhase() == EHeistMatchPhase::InGame &&
		HeistGameState->GetObjectiveState() != EHeistObjectiveState::Failed;
	const bool bLockdownStateValid =
		AlertLevel == EHeistAlertLevel::Lockdown && !bCountdownActive && bWorldRestricted && HeistGameState->GetMatchPhase() == EHeistMatchPhase::End &&
		HeistGameState->GetObjectiveState() == EHeistObjectiveState::Failed && PlayerCount > 0 && ActiveActionCount == 0 && ActiveForgeryCount == 0 && OpenInventoryCount == 0 &&
		MovementEnabledCount == 0 && !HeistGameState->IsEscapePhaseOpen() && VentCount > 0 && ActiveVentCount == 0 && AvailableInteractionCount == 0;
	const bool bPassed = PlayerController->HasAuthority() && (bAlarmedStateValid || bLockdownStateValid);

	Message(
		PlayerController,
		FString::Printf(
			TEXT(
				"Lockdown dump: Alert=%s CountdownActive=%s Remaining=%.2f LockdownActive=%s WorldRestricted=%s MatchPhase=%s Objective=%s EscapeOpen=%s Players=%d ActiveActions=%d ActiveForgeries=%d OpenInventories=%d MovementEnabled=%d Vents=%d ActiveVents=%d Interactables=%d AvailableInteractions=%d Authority=%s Result=%s"),
			*UEnum::GetValueAsString(AlertLevel), bCountdownActive ? TEXT("true") : TEXT("false"), HeistGameState->GetLockdownCountdownRemainingSeconds(),
			HeistGameState->IsLockdownActive() ? TEXT("true") : TEXT("false"), bWorldRestricted ? TEXT("true") : TEXT("false"),
			*UEnum::GetValueAsString(HeistGameState->GetMatchPhase()), *UEnum::GetValueAsString(HeistGameState->GetObjectiveState()),
			HeistGameState->IsEscapePhaseOpen() ? TEXT("true") : TEXT("false"), PlayerCount, ActiveActionCount, ActiveForgeryCount, OpenInventoryCount, MovementEnabledCount, VentCount,
			ActiveVentCount, InteractableCount, AvailableInteractionCount, PlayerController->HasAuthority() ? TEXT("true") : TEXT("false"), bPassed ? TEXT("PASS") : TEXT("FAIL")),
		bPassed ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 15.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugGuardAlertModifiersDump(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistGuardCharacter* GuardCharacter = ResolveNearestGuard(PlayerController);
	AHeistGuardAIController* GuardController = IsValid(GuardCharacter) ? Cast<AHeistGuardAIController>(GuardCharacter->GetController()) : nullptr;
	const UHeistGuardStateComponent* GuardStateComponent = IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
	const UHeistGuardNoiseReactionComponent* NoiseReactionComponent = IsValid(GuardCharacter) ? GuardCharacter->GetNoiseReactionComponent() : nullptr;
	const AHeistGameState* HeistGameState = IsValid(PlayerController) && IsValid(PlayerController->GetWorld()) ? PlayerController->GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(GuardCharacter) || !IsValid(GuardController) || !IsValid(GuardStateComponent) || !IsValid(NoiseReactionComponent) || !IsValid(HeistGameState))
	{
		Message(PlayerController, TEXT("Guard alert modifiers dump: Result=FAIL Reason=MissingGuardContext"), EHeistDebugLevel::Warning, true);
		return;
	}

	AActor* ExitTarget = nullptr;
	float ExitAcceptanceRadius = 0.0f;
	const bool bExitTargetResolved = GuardController->TryGetAlertExitSurveillanceTarget(ExitTarget, ExitAcceptanceRadius);
	const bool bExitSurveillanceValid = !GuardController->IsAlertExitSurveillanceActive() || bExitTargetResolved;
	const bool bValuesValid = GuardCharacter->GetAlertPatrolSpeedMultiplier() > 0.0f && GuardCharacter->GetEffectivePatrolSpeed() > 0.0f &&
		NoiseReactionComponent->GetAlertNoiseRadiusMultiplier() > 0.0f && GuardController->GetAlertSightRadiusMultiplier() > 0.0f && GuardController->GetActiveSightRadius() > 0.0f &&
		GuardStateComponent->GetAlertSearchDurationMultiplier() > 0.0f && GuardStateComponent->GetSearchDuration() > 0.0f;
	const bool bPassed = PlayerController->HasAuthority() && GuardCharacter->HasAuthority() && GuardCharacter->HasResolvedGuardProfile() &&
		GuardController->GetAppliedAlertLevel() == HeistGameState->GetAlertLevel() && bValuesValid && bExitSurveillanceValid;

	Message(
		PlayerController,
		FString::Printf(
			TEXT("Guard alert modifiers dump: Guard=%s Profile=%s Alert=%s AppliedAlert=%s PatrolMultiplier=%.2f PatrolSpeed=%.1f NoiseMultiplier=%.2f SightMultiplier=%.2f SightRadius=%.1f SearchMultiplier=%.2f SearchDuration=%.2f ExitSurveillance=%s ExitTarget=%s ExitAcceptance=%.1f Authority=%s Result=%s"),
			*GetNameSafe(GuardCharacter), *GuardCharacter->GetGuardProfileId().ToString(), *UEnum::GetValueAsString(HeistGameState->GetAlertLevel()),
			*UEnum::GetValueAsString(GuardController->GetAppliedAlertLevel()), GuardCharacter->GetAlertPatrolSpeedMultiplier(), GuardCharacter->GetEffectivePatrolSpeed(),
			NoiseReactionComponent->GetAlertNoiseRadiusMultiplier(), GuardController->GetAlertSightRadiusMultiplier(), GuardController->GetActiveSightRadius(),
			GuardStateComponent->GetAlertSearchDurationMultiplier(), GuardStateComponent->GetSearchDuration(), GuardController->IsAlertExitSurveillanceActive() ? TEXT("true") : TEXT("false"),
			*GetNameSafe(ExitTarget), ExitAcceptanceRadius, PlayerController->HasAuthority() ? TEXT("true") : TEXT("false"), bPassed ? TEXT("PASS") : TEXT("FAIL")),
		bPassed ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 15.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugAlertRequest(APlayerController* PlayerController, const FString& LevelName)
{
#if UE_BUILD_SHIPPING
	return;
#else
	if (!IsValid(PlayerController) || !PlayerController->HasAuthority() || !IsValid(PlayerController->GetWorld()))
	{
		Message(PlayerController, TEXT("Global alert request: Authority=false Result=FAIL Reason=ServerOnly"), EHeistDebugLevel::Warning, true);
		return;
	}

	EHeistAlertLevel RequestedAlertLevel = EHeistAlertLevel::Quiet;
	bool bValidLevel = true;
	if (LevelName.Equals(TEXT("Quiet"), ESearchCase::IgnoreCase))
	{
		RequestedAlertLevel = EHeistAlertLevel::Quiet;
	}
	else if (LevelName.Equals(TEXT("Suspicious"), ESearchCase::IgnoreCase))
	{
		RequestedAlertLevel = EHeistAlertLevel::Suspicious;
	}
	else if (LevelName.Equals(TEXT("Searching"), ESearchCase::IgnoreCase))
	{
		RequestedAlertLevel = EHeistAlertLevel::Searching;
	}
	else if (LevelName.Equals(TEXT("Alarmed"), ESearchCase::IgnoreCase))
	{
		RequestedAlertLevel = EHeistAlertLevel::Alarmed;
	}
	else if (LevelName.Equals(TEXT("Lockdown"), ESearchCase::IgnoreCase))
	{
		RequestedAlertLevel = EHeistAlertLevel::Lockdown;
	}
	else
	{
		bValidLevel = false;
	}

	AHeistGameMode* HeistGameMode = PlayerController->GetWorld()->GetAuthGameMode<AHeistGameMode>();
	if (!bValidLevel || !IsValid(HeistGameMode))
	{
		Message(PlayerController,
				FString::Printf(TEXT("Global alert request: Level=%s Authority=true Result=FAIL Reason=%s"), *LevelName, bValidLevel ? TEXT("MissingGameMode") : TEXT("UnknownLevel")),
				EHeistDebugLevel::Warning, true);
		return;
	}

	const FName TriggerId(*FString::Printf(TEXT("ManualAlert_%s"), *UEnum::GetValueAsString(RequestedAlertLevel)));
	const bool bAccepted = HeistGameMode->RequestAlertEscalation(RequestedAlertLevel, TriggerId);
	Message(PlayerController,
			FString::Printf(TEXT("Global alert request: Requested=%s Trigger=%s ProcessedTriggers=%d Authority=true Result=%s"), *UEnum::GetValueAsString(RequestedAlertLevel), *TriggerId.ToString(),
							HeistGameMode->GetProcessedAlertTriggerCount(), bAccepted ? TEXT("PASS") : TEXT("FAIL")),
			bAccepted ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 12.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugGuardSetState(APlayerController* PlayerController, const FString& StateName, const float DurationSeconds)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Guard state debug failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	EHeistGuardState RequestedState = EHeistGuardState::Disabled;
	bool bValidStateName = true;
	if (StateName.Equals(TEXT("Disabled"), ESearchCase::IgnoreCase))
	{
		RequestedState = EHeistGuardState::Disabled;
	}
	else if (StateName.Equals(TEXT("Patrol"), ESearchCase::IgnoreCase))
	{
		RequestedState = EHeistGuardState::Patrol;
	}
	else if (StateName.Equals(TEXT("Investigate"), ESearchCase::IgnoreCase) || StateName.Equals(TEXT("InvestigateNoise"), ESearchCase::IgnoreCase))
	{
		RequestedState = EHeistGuardState::InvestigateNoise;
	}
	else if (StateName.Equals(TEXT("Chase"), ESearchCase::IgnoreCase) || StateName.Equals(TEXT("ChasePlayer"), ESearchCase::IgnoreCase))
	{
		RequestedState = EHeistGuardState::ChasePlayer;
	}
	else if (StateName.Equals(TEXT("Search"), ESearchCase::IgnoreCase) || StateName.Equals(TEXT("SearchLastKnownLocation"), ESearchCase::IgnoreCase))
	{
		RequestedState = EHeistGuardState::SearchLastKnownLocation;
	}
	else if (StateName.Equals(TEXT("Return"), ESearchCase::IgnoreCase) || StateName.Equals(TEXT("ReturnToPatrol"), ESearchCase::IgnoreCase))
	{
		RequestedState = EHeistGuardState::ReturnToPatrol;
	}
	else
	{
		bValidStateName = false;
	}

	if (!bValidStateName)
	{
		Message(PlayerController, FString::Printf(TEXT("Guard state debug failed: unknown state '%s'. Run HeistGuardHelp."), *StateName), EHeistDebugLevel::Warning, true);
		return;
	}

	const float SafeDuration = FMath::Max(0.0f, DurationSeconds);
	HeistPlayerController->DebugRequestSetNearestGuardState(RequestedState, SafeDuration);
	Message(PlayerController, FString::Printf(TEXT("Guard state debug requested: State=%s Duration=%.2f"), *UEnum::GetValueAsString(RequestedState), SafeDuration), EHeistDebugLevel::Info, true);
#endif
}

void UHeistDebugFunctionLibrary::DebugGuardSightCheck(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Guard sight debug failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->DebugRequestEvaluateNearestGuardSight();
	Message(PlayerController, TEXT("Guard sight debug requested against nearest Guard."), EHeistDebugLevel::Info, true);
#endif
}

void UHeistDebugFunctionLibrary::DebugGuardAutomaticSight(APlayerController* PlayerController, const bool bEnabled)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Guard automatic sight debug failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->DebugRequestSetNearestGuardAutomaticSight(bEnabled);
	Message(PlayerController, FString::Printf(TEXT("Guard automatic sight debug requested: Enabled=%s"), bEnabled ? TEXT("true") : TEXT("false")), EHeistDebugLevel::Info, true);
#endif
}

void UHeistDebugFunctionLibrary::DebugGuardNoise(APlayerController* PlayerController, const float Distance)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Guard noise debug failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	const float SafeDistance = FMath::Clamp(Distance, 0.0f, 5000.0f);
	HeistPlayerController->DebugRequestReportGuardNoise(SafeDistance);
	Message(PlayerController, FString::Printf(TEXT("Guard CoinImpact noise debug requested: Distance=%.1f"), SafeDistance), EHeistDebugLevel::Info, true);
#endif
}

void UHeistDebugFunctionLibrary::DebugSetPlayerArrested(APlayerController* PlayerController, const bool bArrested)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Arrest debug failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->DebugRequestSetArrested(bArrested);
	Message(PlayerController, FString::Printf(TEXT("Player arrest debug requested: Arrested=%s"), bArrested ? TEXT("true") : TEXT("false")), EHeistDebugLevel::Info, true);
#endif
}

void UHeistDebugFunctionLibrary::DebugArrestDump(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	const AHeistPlayerState* HeistPlayerState = IsValid(HeistPlayerController) ? HeistPlayerController->GetPlayerState<AHeistPlayerState>() : nullptr;
	const AHeistPlayerCharacter* Character = IsValid(HeistPlayerController) ? HeistPlayerController->GetPawn<AHeistPlayerCharacter>() : nullptr;
	if (!IsValid(HeistPlayerController) || !IsValid(HeistPlayerState) || !IsValid(Character))
	{
		Message(PlayerController, TEXT("Arrest dump failed: missing local Heist player state or character."), EHeistDebugLevel::Warning, true);
		return;
	}

	const UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement();
	Message(PlayerController,
			FString::Printf(TEXT("Player arrest dump: PlayerId=%d Arrested=%s Escaped=%s MovementDisabled=%s Visible=%s Collision=%s InputMode=%s Cursor=%s IgnoreMove=%s IgnoreLook=%s"),
							HeistPlayerState->HeistPlayerId, HeistPlayerState->IsArrested() ? TEXT("true") : TEXT("false"), HeistPlayerState->IsEscaped() ? TEXT("true") : TEXT("false"),
							IsValid(MovementComponent) && MovementComponent->MovementMode == MOVE_None ? TEXT("true") : TEXT("false"), Character->IsHidden() ? TEXT("false") : TEXT("true"),
							Character->GetActorEnableCollision() ? TEXT("true") : TEXT("false"),
							HeistPlayerController->GetLocalInputMode() == EHeistInputMode::Gameplay ? TEXT("Gameplay") : TEXT("NonGameplay"),
							HeistPlayerController->bShowMouseCursor ? TEXT("true") : TEXT("false"), HeistPlayerController->IsMoveInputIgnored() ? TEXT("true") : TEXT("false"),
							HeistPlayerController->IsLookInputIgnored() ? TEXT("true") : TEXT("false")),
			EHeistDebugLevel::Info, true, 8.0f);
	HeistPlayerController->DebugRequestDumpArrestState();
#endif
}

void UHeistDebugFunctionLibrary::DebugFootstepWeight(APlayerController* PlayerController, const float TotalLootWeight)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Footstep weight debug failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	const float SafeWeight = FMath::IsFinite(TotalLootWeight) ? FMath::Max(0.0f, TotalLootWeight) : 0.0f;
	HeistPlayerController->DebugRequestSetFootstepWeight(SafeWeight);
	Message(PlayerController, FString::Printf(TEXT("Footstep weight debug requested: TotalLootWeight=%.1f"), SafeWeight), EHeistDebugLevel::Info, true);
#endif
}

#pragma endregion

#pragma region HUDDebug

void UHeistDebugFunctionLibrary::DebugFirstPersonHUDDump(APlayerController* PlayerController)
{
#if !UE_BUILD_SHIPPING
	AHeistHUD* HeistHUD = IsValid(PlayerController) ? Cast<AHeistHUD>(PlayerController->GetHUD()) : nullptr;
	UHeistHUDWidget* HUDWidget = IsValid(HeistHUD) ? HeistHUD->GetMainHUDWidget() : nullptr;
	if (!IsValid(HUDWidget))
	{
		Message(PlayerController, TEXT("First-person HUD dump failed: missing local Heist HUD widget."), EHeistDebugLevel::Error, true);
		return;
	}

	HUDWidget->DebugDumpFirstPersonHUDState();
	Message(PlayerController, TEXT("First-person HUD dump requested."), EHeistDebugLevel::Info, true);
#endif
}

#pragma endregion

#pragma region FirstPersonScaleDebug

void UHeistDebugFunctionLibrary::DebugFirstPersonScaleCheck(APlayerController* PlayerController, const float ForwardDistance)
{
#if !UE_BUILD_SHIPPING
	if (!IsValid(PlayerController))
	{
		Message(nullptr, TEXT("First-person scale check failed: PlayerController is invalid."), EHeistDebugLevel::Error, true);
		return;
	}

	AHeistPlayerCharacter* HeistCharacter = PlayerController->GetPawn<AHeistPlayerCharacter>();
	UCapsuleComponent* CapsuleComponent = IsValid(HeistCharacter) ? HeistCharacter->GetCapsuleComponent() : nullptr;
	UWorld* World = PlayerController->GetWorld();
	if (!IsValid(HeistCharacter) || !IsValid(CapsuleComponent) || !IsValid(World))
	{
		Message(PlayerController, TEXT("First-person scale check failed: character, capsule, or world is invalid."), EHeistDebugLevel::Error, true);
		return;
	}

	FVector CameraLocation = FVector::ZeroVector;
	FRotator CameraRotation = FRotator::ZeroRotator;
	PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);

	const FVector CapsuleLocation = CapsuleComponent->GetComponentLocation();
	const float CapsuleRadius = CapsuleComponent->GetScaledCapsuleRadius();
	const float CapsuleHalfHeight = CapsuleComponent->GetScaledCapsuleHalfHeight();
	const float CheckedForwardDistance = FMath::Max(0.0f, ForwardDistance);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HeistFirstPersonScaleCheck), false, HeistCharacter);

	FHitResult CameraHit;
	const bool bCameraObstructed = World->LineTraceSingleByChannel(CameraHit, CapsuleLocation, CameraLocation, ECC_Visibility, QueryParams);

	constexpr float CeilingProbeDistance = 200.0f;
	constexpr float MinimumCeilingClearance = 10.0f;
	const FVector CapsuleTop = CapsuleLocation + FVector::UpVector * CapsuleHalfHeight;
	FHitResult CeilingHit;
	const bool bCeilingHit = World->LineTraceSingleByChannel(CeilingHit, CapsuleTop + FVector::UpVector, CapsuleTop + FVector::UpVector * CeilingProbeDistance, ECC_Visibility, QueryParams);
	const float CeilingClearance = bCeilingHit ? FVector::Distance(CapsuleTop, CeilingHit.ImpactPoint) : CeilingProbeDistance;

	FVector ForwardDirection = CameraRotation.Vector();
	ForwardDirection.Z = 0.0f;
	ForwardDirection = ForwardDirection.GetSafeNormal();
	const FVector ForwardEnd = CapsuleLocation + ForwardDirection * CheckedForwardDistance;
	FHitResult ForwardHit;
	const bool bForwardBlocked =
		CheckedForwardDistance > 0.0f && !ForwardDirection.IsNearlyZero() &&
		World->SweepSingleByChannel(ForwardHit, CapsuleLocation, ForwardEnd, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight), QueryParams);

	const bool bAutomaticPass = !bCameraObstructed && (!bCeilingHit || CeilingClearance >= MinimumCeilingClearance);
	Message(
		PlayerController,
		FString::Printf(
			TEXT(
				"First-person scale check: Character=%s Location=%s Camera=%s CapsuleRadius=%.1f CapsuleHalfHeight=%.1f CameraObstructed=%s CameraBlocker=%s CeilingHit=%s CeilingClearance=%.1f CeilingBlocker=%s ForwardDistance=%.1f ForwardBlocked=%s ForwardBlocker=%s AutoResult=%s"),
			*GetNameSafe(HeistCharacter), *CapsuleLocation.ToCompactString(), *CameraLocation.ToCompactString(), CapsuleRadius, CapsuleHalfHeight, bCameraObstructed ? TEXT("true") : TEXT("false"),
			bCameraObstructed ? *GetNameSafe(CameraHit.GetActor()) : TEXT("None"), bCeilingHit ? TEXT("true") : TEXT("false"), CeilingClearance,
			bCeilingHit ? *GetNameSafe(CeilingHit.GetActor()) : TEXT("None"), CheckedForwardDistance, bForwardBlocked ? TEXT("true") : TEXT("false"),
			bForwardBlocked ? *GetNameSafe(ForwardHit.GetActor()) : TEXT("None"), bAutomaticPass ? TEXT("PASS") : TEXT("FAIL")),
		bAutomaticPass ? EHeistDebugLevel::Info : EHeistDebugLevel::Error, true, 8.0f);
#endif
}

#pragma endregion

#pragma region Logging

void UHeistDebugFunctionLibrary::LogMessage(const EHeistDebugChannel Channel, const EHeistDebugLevel Level, const FString& Message)
{
#define HEIST_WRITE_TO_CATEGORY(Category)                                                                                                                                      \
	do                                                                                                                                                                           \
	{                                                                                                                                                                            \
		switch (Level)                                                                                                                                                             \
		{                                                                                                                                                                          \
		case EHeistDebugLevel::Verbose:                                                                                                                                            \
			UE_LOG(Category, Verbose, TEXT("%s"), *Message);                                                                                                                         \
			break;                                                                                                                                                                   \
		case EHeistDebugLevel::Warning:                                                                                                                                            \
			UE_LOG(Category, Warning, TEXT("%s"), *Message);                                                                                                                         \
			break;                                                                                                                                                                   \
		case EHeistDebugLevel::Error:                                                                                                                                              \
			UE_LOG(Category, Error, TEXT("%s"), *Message);                                                                                                                           \
			break;                                                                                                                                                                   \
		case EHeistDebugLevel::Info:                                                                                                                                               \
		default:                                                                                                                                                                   \
			UE_LOG(Category, Log, TEXT("%s"), *Message);                                                                                                                             \
			break;                                                                                                                                                                   \
		}                                                                                                                                                                          \
	} while (false)

	switch (Channel)
	{
	case EHeistDebugChannel::Inventory:
		HEIST_WRITE_TO_CATEGORY(LogHeistInventory);
		break;
	case EHeistDebugChannel::Network:
		HEIST_WRITE_TO_CATEGORY(LogHeistNetwork);
		break;
	case EHeistDebugChannel::UI:
		HEIST_WRITE_TO_CATEGORY(LogHeistUI);
		break;
	case EHeistDebugChannel::AI:
		HEIST_WRITE_TO_CATEGORY(LogHeistAI);
		break;
	case EHeistDebugChannel::Core:
	default:
		HEIST_WRITE_TO_CATEGORY(LogHeist);
		break;
	}

#undef HEIST_WRITE_TO_CATEGORY
}

void UHeistDebugFunctionLibrary::Message(const UObject* WorldContextObject, const FString& Message, EHeistDebugLevel Level, bool bPrintToScreen, float Duration)
{
#if UE_BUILD_SHIPPING
	return;
#else
	const FString ContextName = GetNameSafe(WorldContextObject);
	const FString FormattedMessage = FString::Printf(TEXT("[%s] %s"), *ContextName, *Message);

	LogMessage(EHeistDebugChannel::Core, Level, FormattedMessage);

	if (!bPrintToScreen || GEngine == nullptr)
	{
		return;
	}

	FColor MessageColor = FColor::White;
	if (Level == EHeistDebugLevel::Warning)
	{
		MessageColor = FColor::Yellow;
	}
	else if (Level == EHeistDebugLevel::Error)
	{
		MessageColor = FColor::Red;
	}

	GEngine->AddOnScreenDebugMessage(INDEX_NONE, FMath::Max(0.0f, Duration), MessageColor, FormattedMessage);
#endif
}

#pragma endregion

#pragma region RareLootLogging

void UHeistDebugFunctionLibrary::DebugRareLootTimersStarted(const UObject* WorldContextObject, const TArray<float>& EventTimes, const float WarningLeadTime)
{
#if !UE_BUILD_SHIPPING
	TArray<FString> TimeEntries;
	TimeEntries.Reserve(EventTimes.Num());
	for (const float EventTime : EventTimes)
	{
		TimeEntries.Add(FString::Printf(TEXT("%.2f"), EventTime));
	}

	Message(WorldContextObject, FString::Printf(TEXT("Rare Loot timers started: EventTimes=[%s] WarningLeadTime=%.2f"), *FString::Join(TimeEntries, TEXT(",")), WarningLeadTime));
#endif
}

void UHeistDebugFunctionLibrary::DebugRareLootWarningStarted(const UObject* WorldContextObject, const int32 EventIndex, const FName ItemId, const float SpawnServerTime)
{
#if !UE_BUILD_SHIPPING
	Message(WorldContextObject, FString::Printf(TEXT("Rare Loot warning started: EventIndex=%d ItemId=%s SpawnServerTime=%.2f"), EventIndex, *ItemId.ToString(), SpawnServerTime));
#endif
}

void UHeistDebugFunctionLibrary::DebugRareLootSpawned(const UObject* WorldContextObject, const int32 EventIndex, const UObject* LootActor, const UObject* SpawnPoint, const FName ItemId,
													  const FVector& WorldLocation)
{
#if !UE_BUILD_SHIPPING
	Message(WorldContextObject, FString::Printf(TEXT("Rare Loot spawned: EventIndex=%d LootActor=%s SpawnPoint=%s ItemId=%s Location=(%.1f,%.1f,%.1f)"), EventIndex, *GetNameSafe(LootActor),
												*GetNameSafe(SpawnPoint), *ItemId.ToString(), WorldLocation.X, WorldLocation.Y, WorldLocation.Z));
#endif
}

void UHeistDebugFunctionLibrary::DebugRareLootEventFailed(const UObject* WorldContextObject, const int32 EventIndex, const TCHAR* Reason)
{
#if !UE_BUILD_SHIPPING
	Message(WorldContextObject, FString::Printf(TEXT("Rare Loot event failed: EventIndex=%d Reason=%s"), EventIndex, Reason), EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugRareLootPickedUp(const UObject* WorldContextObject, const int32 EventIndex, const UObject* LootActor, const UObject* Requester, const FName ItemId)
{
#if !UE_BUILD_SHIPPING
	Message(WorldContextObject, FString::Printf(TEXT("Rare Loot picked up: EventIndex=%d LootActor=%s Requester=%s ItemId=%s MarkerActive=false"), EventIndex, *GetNameSafe(LootActor),
												*GetNameSafe(Requester), *ItemId.ToString()));
#endif
}

#pragma endregion

#pragma region GameplayDebug

void UHeistDebugFunctionLibrary::DebugMissingInputAsset(const UObject* WorldContextObject, const TCHAR* AssetName)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("%s is not assigned in the PlayerController Blueprint."), AssetName), EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryOpenSkipped(const UObject* WorldContextObject)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, TEXT("Inventory open request skipped: Inventory Widget/ViewModel setup is incomplete."), EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryRequestRejected(const UObject* WorldContextObject, const TCHAR* RequestName, const int32 InstanceId, const TCHAR* Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Inventory request rejected: Request=%s InstanceId=%d Reason=%s"), RequestName, InstanceId, Reason), EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryDropAccepted(const UObject* WorldContextObject, const UObject* Character, const FName ItemId, const int32 InstanceId, const UObject* DroppedLootActor,
															const FVector& DropOrigin)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Inventory drop accepted: Character=%s ItemId=%s InstanceId=%d WorldLoot=%s DropOrigin=%s"), *GetNameSafe(Character), *ItemId.ToString(),
												InstanceId, *GetNameSafe(DroppedLootActor), *DropOrigin.ToCompactString()));
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryItemDefinitionLookupRejected(const FName ItemId, const TCHAR* Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	UE_LOG(LogHeistInventory, Warning, TEXT("Item definition lookup rejected: ItemId=%s Reason=%s"), *ItemId.ToString(), Reason);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryAddRejected(const UObject* OwnerActor, const FName ItemId, const TCHAR* Reason, const int32 GridColumnCount, const int32 GridRowCount)
{
#if UE_BUILD_SHIPPING
	return;
#else
	if (GridColumnCount != INDEX_NONE && GridRowCount != INDEX_NONE)
	{
		UE_LOG(LogHeistInventory, Warning, TEXT("Inventory add rejected: Owner=%s ItemId=%s Reason=%s Grid=%dx%d"), *GetNameSafe(OwnerActor), *ItemId.ToString(), Reason, GridColumnCount,
			   GridRowCount);
		return;
	}

	UE_LOG(LogHeistInventory, Warning, TEXT("Inventory add rejected: Owner=%s ItemId=%s Reason=%s"), *GetNameSafe(OwnerActor), *ItemId.ToString(), Reason);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryItemAdded(const UObject* OwnerActor, const FName ItemId, const int32 InstanceId, const FIntPoint& GridPosition, const FIntPoint& PlacedSize,
														 const bool bRotated, const int32 ItemCount)
{
#if UE_BUILD_SHIPPING
	return;
#else
	UE_LOG(LogHeistInventory, Log, TEXT("Inventory item added: Owner=%s ItemId=%s InstanceId=%d Grid=(%d,%d) Size=%dx%d Rotated=%s ItemCount=%d"), *GetNameSafe(OwnerActor), *ItemId.ToString(),
		   InstanceId, GridPosition.X, GridPosition.Y, PlacedSize.X, PlacedSize.Y, bRotated ? TEXT("true") : TEXT("false"), ItemCount);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryItemMoved(const UObject* OwnerActor, const int32 InstanceId, const FIntPoint& GridPosition)
{
#if UE_BUILD_SHIPPING
	return;
#else
	UE_LOG(LogHeistInventory, Log, TEXT("Inventory item moved: Owner=%s InstanceId=%d Grid=(%d,%d)"), *GetNameSafe(OwnerActor), InstanceId, GridPosition.X, GridPosition.Y);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryItemRotated(const UObject* OwnerActor, const int32 InstanceId, const bool bRotated)
{
#if UE_BUILD_SHIPPING
	return;
#else
	UE_LOG(LogHeistInventory, Log, TEXT("Inventory item rotated: Owner=%s InstanceId=%d Rotated=%s"), *GetNameSafe(OwnerActor), InstanceId, bRotated ? TEXT("true") : TEXT("false"));
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryItemRemoved(const UObject* OwnerActor, const FName ItemId, const int32 InstanceId, const int32 ItemCount)
{
#if UE_BUILD_SHIPPING
	return;
#else
	UE_LOG(LogHeistInventory, Log, TEXT("Inventory item removed: Owner=%s ItemId=%s InstanceId=%d ItemCount=%d"), *GetNameSafe(OwnerActor), *ItemId.ToString(), InstanceId, ItemCount);
#endif
}

void UHeistDebugFunctionLibrary::DebugQuickSlotAssigned(const UObject* OwnerActor, const int32 SlotTypeValue, const int32 InstanceId, const FName ItemId)
{
#if UE_BUILD_SHIPPING
	return;
#else
	UE_LOG(LogHeistInventory, Log, TEXT("QuickSlot assigned: Owner=%s Slot=%d InstanceId=%d ItemId=%s"), *GetNameSafe(OwnerActor), SlotTypeValue, InstanceId, *ItemId.ToString());
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryOccupancyInvalid(const int32 InstanceId, const FName ItemId, const TCHAR* Reason, const FIntPoint& GridPosition, const FIntPoint& ItemSize)
{
#if UE_BUILD_SHIPPING
	return;
#else
	if (ItemSize != FIntPoint::ZeroValue)
	{
		UE_LOG(LogHeistInventory, Error, TEXT("Inventory occupancy invalid: InstanceId=%d ItemId=%s Grid=(%d,%d) Size=%dx%d Reason=%s"), InstanceId, *ItemId.ToString(), GridPosition.X, GridPosition.Y,
			   ItemSize.X, ItemSize.Y, Reason);
		return;
	}

	UE_LOG(LogHeistInventory, Error, TEXT("Inventory occupancy invalid: InstanceId=%d ItemId=%s Reason=%s"), InstanceId, *ItemId.ToString(), Reason);
#endif
}

void UHeistDebugFunctionLibrary::DebugLootPickupRequestReceived(const UObject* WorldContextObject, const UObject* Character, const UObject* TargetLootActor)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Loot pickup request received: Character=%s Target=%s"), *GetNameSafe(Character), *GetNameSafe(TargetLootActor)));
#endif
}

void UHeistDebugFunctionLibrary::DebugLootPickupRequestRejected(const UObject* WorldContextObject, const UObject* TargetLootActor, const TCHAR* Reason, const float Distance)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Loot pickup request rejected: Target=%s Reason=%s%s"), *GetNameSafe(TargetLootActor), Reason, *FormatOptionalDistance(Distance)),
			EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugLootPickupRequestAccepted(const UObject* WorldContextObject, const UObject* TargetLootActor, const FName ItemId, const int32 InstanceId, const float Distance)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Loot pickup request accepted: Target=%s ItemId=%s InstanceId=%d Distance=%.1f InventoryCommitted=true"), *GetNameSafe(TargetLootActor),
												*ItemId.ToString(), InstanceId, Distance));
#endif
}

void UHeistDebugFunctionLibrary::DebugEscapeRequestRejected(const UObject* WorldContextObject, const UObject* TargetVentActor, const TCHAR* Reason, const float Distance)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Escape request rejected: Vent=%s Reason=%s%s"), *GetNameSafe(TargetVentActor), Reason, *FormatOptionalDistance(Distance)),
			EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugEscapeRequestAccepted(const UObject* WorldContextObject, const UObject* Character, const UObject* TargetVentActor, const float Distance)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Escape request accepted: Character=%s Vent=%s Distance=%.1f State=Casting"), *GetNameSafe(Character), *GetNameSafe(TargetVentActor), Distance));
#endif
}

void UHeistDebugFunctionLibrary::DebugEscapeCastStarted(const UObject* WorldContextObject, const UObject* Character, const UObject* TargetVentActor, const float DurationSeconds,
														const float EndServerTime)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Escape cast started: Character=%s Vent=%s Duration=%.2f EndServerTime=%.2f"), *GetNameSafe(Character), *GetNameSafe(TargetVentActor),
												DurationSeconds, EndServerTime));
#endif
}

void UHeistDebugFunctionLibrary::DebugEscapeCastStateReplicated(const UObject* WorldContextObject, const UObject* Character, const bool bIsActive, const float EndServerTime)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject,
			FString::Printf(TEXT("Escape cast state replicated: Character=%s IsActive=%s EndServerTime=%.2f"), *GetNameSafe(Character), bIsActive ? TEXT("true") : TEXT("false"), EndServerTime));
#endif
}

void UHeistDebugFunctionLibrary::DebugEscapeCastCompleted(const UObject* WorldContextObject, const UObject* Character, const UObject* TargetVentActor)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Escape cast completed: Character=%s Vent=%s Result=Escaped"), *GetNameSafe(Character), *GetNameSafe(TargetVentActor)));
#endif
}

void UHeistDebugFunctionLibrary::DebugEscapeCastCancelled(const UObject* WorldContextObject, const FString& CharacterName, const FString& VentName, const TCHAR* Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Escape cast cancelled: Character=%s Vent=%s Reason=%s"), *CharacterName, *VentName, Reason));
#endif
}

void UHeistDebugFunctionLibrary::DebugObservationRequestRejected(const UObject* WorldContextObject, const UObject* TargetDisplayCase, const TCHAR* Reason, const float Distance)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Observation request rejected: Case=%s Reason=%s Distance=%s"), *GetNameSafe(TargetDisplayCase), Reason, *FormatOptionalDistance(Distance)),
			EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugObservationCastStarted(const UObject* WorldContextObject, const UObject* Character, const UObject* TargetDisplayCase, const float DurationSeconds,
															 const float EndServerTime)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Observation cast started: Character=%s Case=%s Duration=%.2f EndServerTime=%.2f ServerApproved=true"), *GetNameSafe(Character),
												*GetNameSafe(TargetDisplayCase), DurationSeconds, EndServerTime));
#endif
}

void UHeistDebugFunctionLibrary::DebugObservationCastStateReplicated(const UObject* WorldContextObject, const UObject* Character, const bool bIsActive, const float EndServerTime)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject,
			FString::Printf(TEXT("Observation cast state replicated: Character=%s IsActive=%s EndServerTime=%.2f"), *GetNameSafe(Character), bIsActive ? TEXT("true") : TEXT("false"), EndServerTime));
#endif
}

void UHeistDebugFunctionLibrary::DebugObservationCastCompleted(const UObject* WorldContextObject, const UObject* Character, const UObject* TargetDisplayCase)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject,
			FString::Printf(TEXT("Observation cast completed: Character=%s Case=%s Result=Observed SessionRetained=true"), *GetNameSafe(Character), *GetNameSafe(TargetDisplayCase)));
#endif
}

void UHeistDebugFunctionLibrary::DebugObservationCastCancelled(const UObject* WorldContextObject, const FString& CharacterName, const FString& DisplayCaseName, const TCHAR* Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Observation cast cancelled: Character=%s Case=%s Reason=%s SessionReleased=true"), *CharacterName, *DisplayCaseName, Reason));
#endif
}

void UHeistDebugFunctionLibrary::DebugForgerySessionBeginRejected(const UHeistForgeryComponent* ForgeryComponent, const AHeistPaintingDisplayCaseActor* TargetDisplayCase,
																  const FName Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	const UObject* Character = IsValid(ForgeryComponent) ? ForgeryComponent->GetOwner() : nullptr;
	FString MessageText;
	if (Reason == FName(TEXT("SessionAlreadyActive")))
	{
		MessageText = FString::Printf(TEXT("Forgery session begin rejected: Character=%s Case=%s ActiveCase=%s Reason=%s"), *GetNameSafe(Character), *GetNameSafe(TargetDisplayCase),
									  *GetNameSafe(IsValid(ForgeryComponent) ? ForgeryComponent->GetActiveDisplayCase() : nullptr), *Reason.ToString());
	}
	else if (Reason == FName(TEXT("CaseOwnershipMismatch")))
	{
		MessageText = FString::Printf(TEXT("Forgery session begin rejected: Character=%s Case=%s CaseOwner=%s Reason=%s"), *GetNameSafe(Character), *GetNameSafe(TargetDisplayCase),
									  *GetNameSafe(IsValid(TargetDisplayCase) ? TargetDisplayCase->GetSessionOwner() : nullptr), *Reason.ToString());
	}
	else if (Reason == FName(TEXT("CaseNotObserved")))
	{
		const FString CaseState = IsValid(TargetDisplayCase) ? UEnum::GetValueAsString(TargetDisplayCase->GetDisplayCaseState()) : TEXT("Invalid");
		MessageText = FString::Printf(TEXT("Forgery session begin rejected: Character=%s Case=%s CaseState=%s Reason=%s"), *GetNameSafe(Character), *GetNameSafe(TargetDisplayCase),
									  *CaseState, *Reason.ToString());
	}
	else
	{
		MessageText = FString::Printf(TEXT("Forgery session begin rejected: Character=%s Case=%s Reason=%s"), *GetNameSafe(Character), *GetNameSafe(TargetDisplayCase), *Reason.ToString());
	}

	LogMessage(EHeistDebugChannel::Network, EHeistDebugLevel::Warning, MessageText);
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryTemplatePreparationRejected(const UHeistForgeryComponent* ForgeryComponent,
																		  const AHeistPaintingDisplayCaseActor* TargetDisplayCase, const FName ArtifactId,
																		  const FHeistForgeryTemplateRow* TemplateDefinition, const FName Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	const UObject* Character = IsValid(ForgeryComponent) ? ForgeryComponent->GetOwner() : nullptr;
	FString MessageText;
	if (Reason == FName(TEXT("InvalidPaletteScoreContract")) && TemplateDefinition != nullptr)
	{
		MessageText = FString::Printf(TEXT("Forgery template preparation rejected: Character=%s Case=%s Artifact=%s Template=%s PaletteColors=%d Reason=%s"), *GetNameSafe(Character),
									  *GetNameSafe(TargetDisplayCase), *ArtifactId.ToString(), *TemplateDefinition->TemplateId.ToString(), TemplateDefinition->AllowedPalette.Num(),
									  *Reason.ToString());
	}
	else if (Reason == FName(TEXT("ReferenceAssetLoadFailed")) && TemplateDefinition != nullptr)
	{
		MessageText = FString::Printf(TEXT("Forgery template preparation rejected: Character=%s Case=%s Artifact=%s Template=%s ReferenceImage=%s ReferenceMask=%s Reason=%s"),
									  *GetNameSafe(Character), *GetNameSafe(TargetDisplayCase), *ArtifactId.ToString(), *TemplateDefinition->TemplateId.ToString(),
									  *TemplateDefinition->ReferenceImage.ToSoftObjectPath().ToString(), *TemplateDefinition->ReferenceMask.ToSoftObjectPath().ToString(), *Reason.ToString());
	}
	else
	{
		MessageText = FString::Printf(TEXT("Forgery template preparation rejected: Character=%s Case=%s Artifact=%s Reason=%s"), *GetNameSafe(Character), *GetNameSafe(TargetDisplayCase),
									  *ArtifactId.ToString(), *Reason.ToString());
	}

	LogMessage(EHeistDebugChannel::Core, EHeistDebugLevel::Error, MessageText);
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryTemplatePrepared(const UHeistForgeryComponent* ForgeryComponent, const AHeistPaintingDisplayCaseActor* TargetDisplayCase,
															   const FHeistForgeryTemplateRow& TemplateDefinition)
{
#if UE_BUILD_SHIPPING
	return;
#else
	LogMessage(
		EHeistDebugChannel::Core, EHeistDebugLevel::Info,
		FString::Printf(
			TEXT(
				"Forgery template prepared: Character=%s Case=%s Artifact=%s Template=%s ReferenceImage=%s ReferenceMask=%s BackgroundFilter=%s BackgroundTolerance=%.3f PaletteColors=%d Observation=%.2f Forgery=%.2f StrokeLimit=%d Brush=%.4f CoverageWeight=%.3f MajorShapeWeight=%.3f ExtraStrokePenaltyWeight=%.3f TimeoutPenalty=%.3f ShapeWeight=%.3f ColorWeight=%.3f MaxPaintRatio=%.2f OverpaintCap=%.2f Result=PASS"),
			*GetNameSafe(IsValid(ForgeryComponent) ? ForgeryComponent->GetOwner() : nullptr), *GetNameSafe(TargetDisplayCase),
			*(IsValid(ForgeryComponent) ? ForgeryComponent->GetActiveArtifactId() : NAME_None).ToString(), *TemplateDefinition.TemplateId.ToString(),
			*TemplateDefinition.ReferenceImage.ToSoftObjectPath().ToString(),
			*TemplateDefinition.ReferenceMask.ToSoftObjectPath().ToString(),
			*StaticEnum<EHeistForgeryBackgroundFilter>()->GetNameStringByValue(static_cast<int64>(TemplateDefinition.BackgroundFilterMode)),
			TemplateDefinition.BackgroundColorTolerance, TemplateDefinition.AllowedPalette.Num(), TemplateDefinition.ObservationDuration, TemplateDefinition.ForgeryDuration,
			TemplateDefinition.StrokeLimit, TemplateDefinition.BrushSize, TemplateDefinition.CoverageWeight, TemplateDefinition.MajorShapeWeight,
			TemplateDefinition.ExtraStrokePenaltyWeight, TemplateDefinition.TimeoutPenalty, TemplateDefinition.ShapeAccuracyWeight, TemplateDefinition.ColorAccuracyWeight,
			TemplateDefinition.MaximumPaintToReferenceRatio, TemplateDefinition.OverpaintScoreCap));
#endif
}

void UHeistDebugFunctionLibrary::DebugForgerySubmitRejected(const UHeistForgeryComponent* ForgeryComponent, const FName Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	LogMessage(EHeistDebugChannel::Network, EHeistDebugLevel::Warning,
			   FString::Printf(TEXT("Forgery submit rejected: Character=%s Case=%s SubmitPending=%s Reason=%s"),
							   *GetNameSafe(IsValid(ForgeryComponent) ? ForgeryComponent->GetOwner() : nullptr),
							   *GetNameSafe(IsValid(ForgeryComponent) ? ForgeryComponent->GetActiveDisplayCase() : nullptr),
							   IsValid(ForgeryComponent) && ForgeryComponent->IsSubmitPending() ? TEXT("true") : TEXT("false"), *Reason.ToString()));
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryStrokePayloadRejected(const UHeistForgeryComponent* ForgeryComponent, const int32 StrokeCount, const int32 PointCount,
																   const int32 PayloadBytes, const float ClientBrushSize, const int32 ClientSessionRevision,
																   const FName Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	LogMessage(
		EHeistDebugChannel::Network, EHeistDebugLevel::Warning,
		FString::Printf(
			TEXT(
				"Forgery stroke payload rejected: Character=%s Case=%s Strokes=%d Points=%d PayloadBytes=%d ClientBrush=%.4f ServerBrush=%.4f ClientSessionRevision=%d ServerSessionRevision=%d SubmitPending=%s Reason=%s"),
			*GetNameSafe(IsValid(ForgeryComponent) ? ForgeryComponent->GetOwner() : nullptr),
			*GetNameSafe(IsValid(ForgeryComponent) ? ForgeryComponent->GetActiveDisplayCase() : nullptr), StrokeCount, PointCount, PayloadBytes, ClientBrushSize,
			IsValid(ForgeryComponent) ? ForgeryComponent->GetTemplateBrushSize() : 0.0f, ClientSessionRevision,
			IsValid(ForgeryComponent) ? ForgeryComponent->GetSessionRevision() : INDEX_NONE,
			IsValid(ForgeryComponent) && ForgeryComponent->IsSubmitPending() ? TEXT("true") : TEXT("false"), *Reason.ToString()));
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryStrokePayloadAccepted(const UHeistForgeryComponent* ForgeryComponent,
																   const AHeistPaintingDisplayCaseActor* SubmittedDisplayCase, const int32 ValidatedSessionRevision)
{
#if UE_BUILD_SHIPPING
	return;
#else
	LogMessage(
		EHeistDebugChannel::Network, EHeistDebugLevel::Info,
		FString::Printf(
			TEXT(
				"Forgery stroke payload accepted: Character=%s Case=%s Strokes=%d Points=%d PayloadBytes=%d Brush=%.4f ValidatedForSessionRevision=%d SubmitPending=%s RenderTargetReplicated=false Authority=true Result=PASS"),
			*GetNameSafe(IsValid(ForgeryComponent) ? ForgeryComponent->GetOwner() : nullptr), *GetNameSafe(SubmittedDisplayCase),
			IsValid(ForgeryComponent) ? ForgeryComponent->GetValidatedStrokeCount() : 0, IsValid(ForgeryComponent) ? ForgeryComponent->GetValidatedPointCount() : 0,
			IsValid(ForgeryComponent) ? ForgeryComponent->GetValidatedPayloadBytes() : 0, IsValid(ForgeryComponent) ? ForgeryComponent->GetValidatedBrushSize() : 0.0f,
			ValidatedSessionRevision, IsValid(ForgeryComponent) && ForgeryComponent->IsSubmitPending() ? TEXT("true") : TEXT("false")));
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryStrokeValidationResult(const UHeistForgeryComponent* ForgeryComponent, const bool bAccepted, const FName Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	const AActor* Character = IsValid(ForgeryComponent) ? ForgeryComponent->GetOwner() : nullptr;
	LogMessage(
		EHeistDebugChannel::Network, EHeistDebugLevel::Info,
		FString::Printf(
			TEXT(
				"Forgery stroke validation result: Character=%s Accepted=%s HasValidatedPayload=%s Strokes=%d Points=%d PayloadBytes=%d Brush=%.4f Reason=%s ValidationRevision=%d Authority=%s"),
			*GetNameSafe(Character), bAccepted ? TEXT("true") : TEXT("false"),
			IsValid(ForgeryComponent) && ForgeryComponent->HasValidatedStrokePayload() ? TEXT("true") : TEXT("false"),
			IsValid(ForgeryComponent) ? ForgeryComponent->GetValidatedStrokeCount() : 0, IsValid(ForgeryComponent) ? ForgeryComponent->GetValidatedPointCount() : 0,
			IsValid(ForgeryComponent) ? ForgeryComponent->GetValidatedPayloadBytes() : 0, IsValid(ForgeryComponent) ? ForgeryComponent->GetValidatedBrushSize() : 0.0f,
			Reason.IsNone() ? TEXT("None") : *Reason.ToString(), IsValid(ForgeryComponent) ? ForgeryComponent->GetStrokeValidationRevision() : INDEX_NONE,
			IsValid(Character) && Character->HasAuthority() ? TEXT("true") : TEXT("false")));
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryScoreCalculationRejected(const UHeistForgeryComponent* ForgeryComponent, const FName Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	LogMessage(EHeistDebugChannel::Network, EHeistDebugLevel::Error,
			   FString::Printf(TEXT("Forgery score calculation rejected: Character=%s Case=%s Artifact=%s Template=%s ReferenceMask=%s Reason=%s"),
							   *GetNameSafe(IsValid(ForgeryComponent) ? ForgeryComponent->GetOwner() : nullptr),
							   *GetNameSafe(IsValid(ForgeryComponent) ? ForgeryComponent->GetActiveDisplayCase() : nullptr),
							   *(IsValid(ForgeryComponent) ? ForgeryComponent->GetActiveArtifactId() : NAME_None).ToString(),
							   *(IsValid(ForgeryComponent) ? ForgeryComponent->GetActiveTemplateId() : NAME_None).ToString(),
							   *(IsValid(ForgeryComponent) ? ForgeryComponent->GetReferenceMaskAsset().ToSoftObjectPath().ToString() : FString(TEXT("None"))), *Reason.ToString()));
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryPaintingDataBuildRejected(const UHeistForgeryComponent* ForgeryComponent, const FName Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	LogMessage(EHeistDebugChannel::Network, EHeistDebugLevel::Error,
			   FString::Printf(TEXT("Forgery painting data build rejected: Character=%s Case=%s Template=%s Reason=%s"),
							   *GetNameSafe(IsValid(ForgeryComponent) ? ForgeryComponent->GetOwner() : nullptr),
							   *GetNameSafe(IsValid(ForgeryComponent) ? ForgeryComponent->GetActiveDisplayCase() : nullptr),
							   *(IsValid(ForgeryComponent) ? ForgeryComponent->GetActiveTemplateId() : NAME_None).ToString(), *Reason.ToString()));
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryScoreCommitRejected(const UHeistForgeryComponent* ForgeryComponent,
																 const AHeistPaintingDisplayCaseActor* TargetDisplayCase, const FName Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	LogMessage(EHeistDebugChannel::Network, EHeistDebugLevel::Error,
			   FString::Printf(TEXT("Forgery score commit rejected: Character=%s Case=%s Reason=%s"),
							   *GetNameSafe(IsValid(ForgeryComponent) ? ForgeryComponent->GetOwner() : nullptr), *GetNameSafe(TargetDisplayCase), *Reason.ToString()));
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryReferenceDecodeFailed(const UHeistForgeryComponent* ForgeryComponent, const UObject* Texture, const FName SourceKind,
																   const int32 PixelFormat, const int32 SourceWidth, const int32 SourceHeight)
{
#if UE_BUILD_SHIPPING
	return;
#else
	LogMessage(EHeistDebugChannel::Network, EHeistDebugLevel::Error,
			   FString::Printf(TEXT("Forgery score %s decode failed: Character=%s Texture=%s PixelFormat=%d Source=%dx%d"), *SourceKind.ToString(),
							   *GetNameSafe(IsValid(ForgeryComponent) ? ForgeryComponent->GetOwner() : nullptr), *GetNameSafe(Texture), PixelFormat, SourceWidth, SourceHeight));
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryOpenCVMetrics(const UHeistForgeryComponent* ForgeryComponent, const float DistanceRecall, const float DistancePrecision,
														   const float BidirectionalDistance, const float MaskPrecision, const float MaskRecall, const float MaskIoU,
														   const float MaskDice, const float LabSSIM, const float PaletteHistogram, const float ColorGeometric,
														   const float PaletteBonus)
{
#if UE_BUILD_SHIPPING
	return;
#else
	LogMessage(
		EHeistDebugChannel::Network, EHeistDebugLevel::Info,
		FString::Printf(
			TEXT(
				"Forgery OpenCV metrics: Character=%s Template=%s DistanceRecall=%.4f DistancePrecision=%.4f BidirectionalDistance=%.4f MaskPrecision=%.4f MaskRecall=%.4f MaskIoU=%.4f MaskDice=%.4f LabSSIM=%.4f PaletteHistogram=%.4f ColorGeometric=%.4f PaletteBonus=%.4f Result=PASS"),
			*GetNameSafe(IsValid(ForgeryComponent) ? ForgeryComponent->GetOwner() : nullptr),
			*(IsValid(ForgeryComponent) ? ForgeryComponent->GetActiveTemplateId() : NAME_None).ToString(), DistanceRecall, DistancePrecision, BidirectionalDistance,
			MaskPrecision, MaskRecall, MaskIoU, MaskDice, LabSSIM, PaletteHistogram, ColorGeometric, PaletteBonus));
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryReferenceMaskRejected(const UHeistForgeryComponent* ForgeryComponent, const bool bUseReferenceMask,
																   const int32 ReferencePixels, const int32 TotalPixels)
{
#if UE_BUILD_SHIPPING
	return;
#else
	const FString TexturePath = !IsValid(ForgeryComponent)
									? TEXT("None")
									: bUseReferenceMask ? ForgeryComponent->GetReferenceMaskAsset().ToSoftObjectPath().ToString()
														: ForgeryComponent->GetReferenceImageAsset().ToSoftObjectPath().ToString();
	LogMessage(EHeistDebugChannel::Network, EHeistDebugLevel::Error,
			   FString::Printf(TEXT("Forgery score mask rejected: Character=%s Texture=%s ReferencePixels=%d TotalPixels=%d Reason=DegenerateReferenceMask"),
							   *GetNameSafe(IsValid(ForgeryComponent) ? ForgeryComponent->GetOwner() : nullptr), *TexturePath, ReferencePixels, TotalPixels));
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryScoreCommitted(const UHeistForgeryComponent* ForgeryComponent,
															const AHeistPaintingDisplayCaseActor* TargetDisplayCase, const int32 TotalScorePixels,
															const float PaintCompletenessExponent, const bool bReferenceCacheHit, const double ReferenceMilliseconds,
															const double OpenCVMilliseconds, const double TotalMilliseconds)
{
#if UE_BUILD_SHIPPING
	return;
#else
	const FHeistForgeryResult Result = IsValid(ForgeryComponent) ? ForgeryComponent->GetAuthoritativeForgeryResult() : FHeistForgeryResult();
	const int32 Resolution = IsValid(ForgeryComponent) ? ForgeryComponent->GetForgeryScoreResolution() : 0;
	const int32 ReferencePixels = IsValid(ForgeryComponent) ? ForgeryComponent->GetReferenceMaskPixelCount() : 0;
	const int32 SubmittedPixels = IsValid(ForgeryComponent) ? ForgeryComponent->GetSubmittedMaskPixelCount() : 0;
	const float SafeTotalPixels = FMath::Max(1, TotalScorePixels);
	LogMessage(
		EHeistDebugChannel::Network, EHeistDebugLevel::Info,
		FString::Printf(
			TEXT(
				"Forgery score committed: Character=%s Case=%s Artifact=%s Template=%s Backend=OpenCV ShapeMetric=DistanceDiceGeometric ColorMetric=LabSSIMHistogramGeometric Fusion=WeightedGeometricBottleneck Calibration=OpenCVHistogramBonus0.75x3 ScoreCurve=Shape1.15Color1.10 PaintCompletenessExponent=%.2f Score=%.2f Coverage=%.2f MajorShape=%.2f ColorAccuracy=%.2f MissingPenalty=%.2f ExtraPenalty=%.2f TimeoutPenalty=%.2f CompletionTime=%.2f PaintToReference=%.2f PaintCompleteness=%.3f AntiFill=%s Resolution=%dx%d ReferencePixels=%d SubmittedPixels=%d ReferenceRatio=%.4f SubmittedRatio=%.4f Cache=%s ReferenceMs=%.3f OpenCVMs=%.3f TotalMs=%.3f ScoreRevision=%d Authority=true Deterministic=true Result=PASS"),
			*GetNameSafe(IsValid(ForgeryComponent) ? ForgeryComponent->GetOwner() : nullptr), *GetNameSafe(TargetDisplayCase), *Result.ArtifactId.ToString(),
			*Result.TemplateId.ToString(), PaintCompletenessExponent, Result.SimilarityScore, Result.CoverageScore, Result.MajorShapeScore, Result.ColorAccuracyScore,
			Result.MissingShapePenalty, Result.ExtraStrokePenalty, Result.TimeoutPenalty, Result.CompletionTime, Result.PaintToReferenceRatio,
			FMath::Pow(FMath::Clamp(Result.PaintToReferenceRatio, 0.0f, 1.0f), PaintCompletenessExponent), Result.bAntiFillTriggered ? TEXT("true") : TEXT("false"),
			Resolution, Resolution, ReferencePixels, SubmittedPixels, static_cast<float>(ReferencePixels) / SafeTotalPixels, static_cast<float>(SubmittedPixels) / SafeTotalPixels,
			bReferenceCacheHit ? TEXT("Hit") : TEXT("Miss"), ReferenceMilliseconds, OpenCVMilliseconds, TotalMilliseconds,
			IsValid(ForgeryComponent) ? ForgeryComponent->GetForgeryScoreRevision() : INDEX_NONE));
#endif
}

void UHeistDebugFunctionLibrary::DebugForgerySessionCompleted(const UHeistForgeryComponent* ForgeryComponent,
															  const AHeistPaintingDisplayCaseActor* PreviousDisplayCase)
{
#if UE_BUILD_SHIPPING
	return;
#else
	const bool bHasScore = IsValid(ForgeryComponent) && ForgeryComponent->HasAuthoritativeForgeryResult();
	const FHeistForgeryResult Result = bHasScore ? ForgeryComponent->GetAuthoritativeForgeryResult() : FHeistForgeryResult();
	LogMessage(EHeistDebugChannel::Network, EHeistDebugLevel::Info,
			   FString::Printf(TEXT("Forgery session completed: Character=%s PreviousCase=%s HasScore=%s Score=%.2f ReplicaPlaced=%s Revision=%d Authority=true Result=%s"),
							   *GetNameSafe(IsValid(ForgeryComponent) ? ForgeryComponent->GetOwner() : nullptr), *GetNameSafe(PreviousDisplayCase),
							   bHasScore ? TEXT("true") : TEXT("false"), Result.SimilarityScore, Result.bReplicaPlaced ? TEXT("true") : TEXT("false"),
							   IsValid(ForgeryComponent) ? ForgeryComponent->GetSessionRevision() : INDEX_NONE,
							   bHasScore && Result.bReplicaPlaced ? TEXT("PASS") : TEXT("FAIL")));
#endif
}

void UHeistDebugFunctionLibrary::DebugForgerySessionCleared(const UHeistForgeryComponent* ForgeryComponent,
															const AHeistPaintingDisplayCaseActor* PreviousDisplayCase, const FName Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	LogMessage(EHeistDebugChannel::Network, EHeistDebugLevel::Info,
			   FString::Printf(TEXT("Forgery session cleared: Character=%s PreviousCase=%s Reason=%s Revision=%d"),
							   *GetNameSafe(IsValid(ForgeryComponent) ? ForgeryComponent->GetOwner() : nullptr), *GetNameSafe(PreviousDisplayCase), *Reason.ToString(),
							   IsValid(ForgeryComponent) ? ForgeryComponent->GetSessionRevision() : INDEX_NONE));
#endif
}

void UHeistDebugFunctionLibrary::DebugForgerySessionSnapshot(const UHeistForgeryComponent* ForgeryComponent, const TCHAR* ChangeSource, const FName Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	const AActor* Character = IsValid(ForgeryComponent) ? ForgeryComponent->GetOwner() : nullptr;
	const bool bHasScore = IsValid(ForgeryComponent) && ForgeryComponent->HasAuthoritativeForgeryResult();
	const FHeistForgeryResult Result = bHasScore ? ForgeryComponent->GetAuthoritativeForgeryResult() : FHeistForgeryResult();
	const FName LastCleanupReason = IsValid(ForgeryComponent) ? ForgeryComponent->GetLastCleanupReason() : NAME_None;
	LogMessage(
		EHeistDebugChannel::Network, EHeistDebugLevel::Info,
		FString::Printf(
			TEXT(
				"Forgery session %s: Character=%s Case=%s Active=%s SubmitPending=%s HasScore=%s Score=%.2f TemplatePrepared=%s Artifact=%s Template=%s EndServerTime=%.2f Revision=%d LastCleanup=%s Reason=%s Authority=%s"),
			ChangeSource, *GetNameSafe(Character), *GetNameSafe(IsValid(ForgeryComponent) ? ForgeryComponent->GetActiveDisplayCase() : nullptr),
			IsValid(ForgeryComponent) && ForgeryComponent->IsSessionActive() ? TEXT("true") : TEXT("false"),
			IsValid(ForgeryComponent) && ForgeryComponent->IsSubmitPending() ? TEXT("true") : TEXT("false"), bHasScore ? TEXT("true") : TEXT("false"),
			Result.SimilarityScore, IsValid(ForgeryComponent) && ForgeryComponent->HasPreparedForgeryTemplate() ? TEXT("true") : TEXT("false"),
			*(IsValid(ForgeryComponent) ? ForgeryComponent->GetActiveArtifactId() : NAME_None).ToString(),
			*(IsValid(ForgeryComponent) ? ForgeryComponent->GetActiveTemplateId() : NAME_None).ToString(),
			IsValid(ForgeryComponent) ? ForgeryComponent->GetSessionEndServerTime() : 0.0f,
			IsValid(ForgeryComponent) ? ForgeryComponent->GetSessionRevision() : INDEX_NONE, LastCleanupReason.IsNone() ? TEXT("None") : *LastCleanupReason.ToString(),
			Reason.IsNone() ? TEXT("None") : *Reason.ToString(), IsValid(Character) && Character->HasAuthority() ? TEXT("true") : TEXT("false")));
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryStrokeValidationReplicated(const UHeistForgeryComponent* ForgeryComponent)
{
#if UE_BUILD_SHIPPING
	return;
#else
	const FName Reason = IsValid(ForgeryComponent) ? ForgeryComponent->GetLastStrokeValidationReason() : NAME_None;
	const bool bAccepted = IsValid(ForgeryComponent) && ForgeryComponent->WasLastStrokeValidationAccepted();
	LogMessage(
		EHeistDebugChannel::Network, EHeistDebugLevel::Info,
		FString::Printf(
			TEXT(
				"Forgery stroke validation replicated: Character=%s Accepted=%s HasValidatedPayload=%s Strokes=%d Points=%d PayloadBytes=%d Brush=%.4f Reason=%s ValidationRevision=%d Authority=false Result=%s"),
			*GetNameSafe(IsValid(ForgeryComponent) ? ForgeryComponent->GetOwner() : nullptr), bAccepted ? TEXT("true") : TEXT("false"),
			IsValid(ForgeryComponent) && ForgeryComponent->HasValidatedStrokePayload() ? TEXT("true") : TEXT("false"),
			IsValid(ForgeryComponent) ? ForgeryComponent->GetValidatedStrokeCount() : 0, IsValid(ForgeryComponent) ? ForgeryComponent->GetValidatedPointCount() : 0,
			IsValid(ForgeryComponent) ? ForgeryComponent->GetValidatedPayloadBytes() : 0, IsValid(ForgeryComponent) ? ForgeryComponent->GetValidatedBrushSize() : 0.0f,
			Reason.IsNone() ? TEXT("None") : *Reason.ToString(), IsValid(ForgeryComponent) ? ForgeryComponent->GetStrokeValidationRevision() : INDEX_NONE,
			bAccepted ? TEXT("PASS") : TEXT("REJECTED")));
#endif
}

void UHeistDebugFunctionLibrary::DebugForgeryScoreReplicated(const UHeistForgeryComponent* ForgeryComponent)
{
#if UE_BUILD_SHIPPING
	return;
#else
	const bool bHasScore = IsValid(ForgeryComponent) && ForgeryComponent->HasAuthoritativeForgeryResult();
	const FHeistForgeryResult Result = bHasScore ? ForgeryComponent->GetAuthoritativeForgeryResult() : FHeistForgeryResult();
	const int32 Resolution = IsValid(ForgeryComponent) ? ForgeryComponent->GetForgeryScoreResolution() : 0;
	LogMessage(
		EHeistDebugChannel::Network, EHeistDebugLevel::Info,
		FString::Printf(
			TEXT(
				"Forgery score replicated: Character=%s HasScore=%s Artifact=%s Template=%s Score=%.2f Coverage=%.2f MajorShape=%.2f ColorAccuracy=%.2f MissingPenalty=%.2f ExtraPenalty=%.2f TimeoutPenalty=%.2f CompletionTime=%.2f PaintToReference=%.2f AntiFill=%s Resolution=%dx%d ReferencePixels=%d SubmittedPixels=%d ScoreRevision=%d Authority=false Result=%s"),
			*GetNameSafe(IsValid(ForgeryComponent) ? ForgeryComponent->GetOwner() : nullptr), bHasScore ? TEXT("true") : TEXT("false"), *Result.ArtifactId.ToString(),
			*Result.TemplateId.ToString(), Result.SimilarityScore, Result.CoverageScore, Result.MajorShapeScore, Result.ColorAccuracyScore, Result.MissingShapePenalty,
			Result.ExtraStrokePenalty, Result.TimeoutPenalty, Result.CompletionTime, Result.PaintToReferenceRatio, Result.bAntiFillTriggered ? TEXT("true") : TEXT("false"),
			Resolution, Resolution, IsValid(ForgeryComponent) ? ForgeryComponent->GetReferenceMaskPixelCount() : 0,
			IsValid(ForgeryComponent) ? ForgeryComponent->GetSubmittedMaskPixelCount() : 0,
			IsValid(ForgeryComponent) ? ForgeryComponent->GetForgeryScoreRevision() : INDEX_NONE, bHasScore ? TEXT("PASS") : TEXT("CLEARED")));
#endif
}

void UHeistDebugFunctionLibrary::DebugLootScoreWeightRejected(const UObject* WorldContextObject, const TCHAR* Reason, const int32 ScoreDelta, const float WeightDelta)
{
#if UE_BUILD_SHIPPING
	return;
#else
	const bool bHasDeltaContext = ScoreDelta != INDEX_NONE || WeightDelta >= 0.0f;
	Message(WorldContextObject,
			bHasDeltaContext
				? FString::Printf(TEXT("Loot score/weight rejected: PlayerState=%s Reason=%s ScoreDelta=%d WeightDelta=%.2f"), *GetNameSafe(WorldContextObject), Reason, ScoreDelta, WeightDelta)
				: FString::Printf(TEXT("Loot score/weight rejected: PlayerState=%s Reason=%s"), *GetNameSafe(WorldContextObject), Reason),
			EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugLootScoreWeightApplied(const UObject* WorldContextObject, const int32 ScoreDelta, const float WeightDelta, const int32 TotalScore, const float TotalWeight)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Loot score/weight applied: PlayerState=%s ScoreDelta=%d WeightDelta=%.2f TotalScore=%d TotalWeight=%.2f"), *GetNameSafe(WorldContextObject),
												ScoreDelta, WeightDelta, TotalScore, TotalWeight));
#endif
}

void UHeistDebugFunctionLibrary::DebugLootScoreWeightRemoved(const UObject* WorldContextObject, const int32 ScoreDelta, const float WeightDelta, const int32 TotalScore, const float TotalWeight)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Loot score/weight removed: PlayerState=%s ScoreDelta=%d WeightDelta=%.2f TotalScore=%d TotalWeight=%.2f"), *GetNameSafe(WorldContextObject),
												ScoreDelta, WeightDelta, TotalScore, TotalWeight));
#endif
}

void UHeistDebugFunctionLibrary::DebugPlayerEscapeStateRejected(const UObject* WorldContextObject, const TCHAR* Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Player escape state rejected: PlayerState=%s Reason=%s"), *GetNameSafe(WorldContextObject), Reason), EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugPlayerEscapeStateCommitted(const UObject* WorldContextObject, const int32 HeistPlayerId, const int32 FinalScore, const float EscapeTimeSeconds)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Player escape state committed: PlayerState=%s HeistPlayerId=%d IsEscaped=true FinalScore=%d EscapeTime=%.2f ScoreFrozen=true"),
												*GetNameSafe(WorldContextObject), HeistPlayerId, FinalScore, EscapeTimeSeconds));
#endif
}

void UHeistDebugFunctionLibrary::DebugPlayerEscapeStateReplicated(const UObject* WorldContextObject, const int32 HeistPlayerId, const bool bEscaped)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Player escape state replicated: PlayerState=%s HeistPlayerId=%d IsEscaped=%s"), *GetNameSafe(WorldContextObject), HeistPlayerId,
												bEscaped ? TEXT("true") : TEXT("false")));
#endif
}

void UHeistDebugFunctionLibrary::DebugPlayerStateScoreReplicated(const UObject* WorldContextObject, const int32 TotalLootScore)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("PlayerState score replicated: PlayerState=%s TotalLootScore=%d"), *GetNameSafe(WorldContextObject), TotalLootScore));
#endif
}

void UHeistDebugFunctionLibrary::DebugPlayerStateWeightReplicated(const UObject* WorldContextObject, const float TotalLootWeight)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("PlayerState weight replicated: PlayerState=%s TotalLootWeight=%.2f"), *GetNameSafe(WorldContextObject), TotalLootWeight));
#endif
}

void UHeistDebugFunctionLibrary::DebugWeightMovementSkipped(const UObject* WorldContextObject, const TCHAR* Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject,
			FString::Printf(TEXT("Weight movement speed skipped: %s=%s Reason=%s"), WorldContextObject && WorldContextObject->IsA<APlayerState>() ? TEXT("PlayerState") : TEXT("Character"),
							*GetNameSafe(WorldContextObject), Reason),
			EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugWeightMovementSpeedApplied(const UObject* WorldContextObject, const float TotalWeight, const float BaseSpeed, const float FinalSpeed)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject,
			FString::Printf(TEXT("Weight movement speed applied: Character=%s TotalWeight=%.2f BaseSpeed=%.2f FinalSpeed=%.2f"), *GetNameSafe(WorldContextObject), TotalWeight, BaseSpeed, FinalSpeed));
#endif
}

void UHeistDebugFunctionLibrary::DebugThrowableUseRejected(const UObject* WorldContextObject, const EHeistQuickSlotType SlotType, const FName ItemId, const TCHAR* Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Throwable use rejected: Slot=%s ItemId=%s Reason=%s"), ToQuickSlotText(SlotType), *ItemId.ToString(), Reason), EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugThrowableProjectileSpawned(const UObject* WorldContextObject, const UObject* Character, const UObject* Projectile, const FName ItemId,
																 const FVector& TargetWorldLocation, const FVector& LaunchDirection, const float ProjectileSpeed, const bool bDebugBypassInventory)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(TEXT("Throwable projectile spawned: Character=%s Projectile=%s ItemId=%s CrosshairTarget=(%.1f,%.1f,%.1f) LaunchDirection=(%.3f,%.3f,%.3f) Speed=%.1f DebugBypassInventory=%s"),
						*GetNameSafe(Character), *GetNameSafe(Projectile), *ItemId.ToString(), TargetWorldLocation.X, TargetWorldLocation.Y, TargetWorldLocation.Z, LaunchDirection.X,
						LaunchDirection.Y, LaunchDirection.Z, ProjectileSpeed, bDebugBypassInventory ? TEXT("true") : TEXT("false")));
#endif
}

void UHeistDebugFunctionLibrary::DebugThrowableProjectileImpact(const UObject* WorldContextObject, const UObject* Projectile, const UObject* OtherActor, const FName ItemId,
																const FVector& ImpactLocation)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Throwable projectile impact: Projectile=%s OtherActor=%s ItemId=%s Location=(%.1f,%.1f,%.1f)"), *GetNameSafe(Projectile),
												*GetNameSafe(OtherActor), *ItemId.ToString(), ImpactLocation.X, ImpactLocation.Y, ImpactLocation.Z));
#endif
}

void UHeistDebugFunctionLibrary::DebugGuardStunApplied(const UObject* WorldContextObject, const UObject* GuardActor, const float DurationSeconds)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Guard stun applied: Guard=%s Duration=%.2f"), *GetNameSafe(GuardActor), DurationSeconds));
#endif
}

void UHeistDebugFunctionLibrary::DebugGuardStunCleared(const UObject* WorldContextObject, const UObject* GuardActor, const EHeistGuardState NewState)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Guard stun cleared: Guard=%s NewState=%s"), *GetNameSafe(GuardActor), *UEnum::GetValueAsString(NewState)));
#endif
}

void UHeistDebugFunctionLibrary::DebugGuardStateChanged(const UObject* WorldContextObject, const UObject* GuardActor, const EHeistGuardState PreviousState, const EHeistGuardState NewState,
														const float StateEndServerTime)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Guard state changed: Guard=%s Previous=%s New=%s EndServerTime=%.2f"), *GetNameSafe(GuardActor), *UEnum::GetValueAsString(PreviousState),
												*UEnum::GetValueAsString(NewState), StateEndServerTime));
#endif
}

void UHeistDebugFunctionLibrary::DebugGuardStateRequestRejected(const UObject* WorldContextObject, const UObject* GuardActor, const EHeistGuardState RequestedState, const TCHAR* Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Guard state request rejected: Guard=%s Requested=%s Reason=%s"), *GetNameSafe(GuardActor), *UEnum::GetValueAsString(RequestedState), Reason),
			EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugGuardStateReplicated(const UObject* WorldContextObject, const UObject* GuardActor, const EHeistGuardState NewState)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Guard state replicated: Guard=%s State=%s"), *GetNameSafe(GuardActor), *UEnum::GetValueAsString(NewState)));
#endif
}

void UHeistDebugFunctionLibrary::DebugDrawGuardSpawnMarker(const UObject* WorldContextObject, UObject* GuardActor)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GuardActor);
	const UCapsuleComponent* CapsuleComponent = IsValid(GuardCharacter) ? GuardCharacter->GetCapsuleComponent() : nullptr;
	UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	if (!IsValid(World) || !IsValid(CapsuleComponent))
	{
		return;
	}

	const FVector CapsuleLocation = CapsuleComponent->GetComponentLocation();
	const float CapsuleHalfHeight = CapsuleComponent->GetScaledCapsuleHalfHeight();
	DrawDebugCapsule(World, CapsuleLocation, CapsuleHalfHeight, CapsuleComponent->GetScaledCapsuleRadius(), CapsuleComponent->GetComponentQuat(), FColor::Green, true, -1.0f, 0, 3.0f);

	const FVector ArrowStart = CapsuleLocation + FVector::UpVector * CapsuleHalfHeight;
	DrawDebugDirectionalArrow(World, ArrowStart, ArrowStart + GuardCharacter->GetActorForwardVector() * 150.0f, 40.0f, FColor::Yellow, true, -1.0f, 0, 3.0f);
	DrawDebugString(World, FVector::UpVector * 30.0f, TEXT("DEBUG GUARD"), GuardCharacter, FColor::Green, 0.0f, true, 1.2f);
#endif
}

void UHeistDebugFunctionLibrary::DebugGuardStateTreeEvent(const UObject* WorldContextObject, const UObject* GuardActor, const FGameplayTag& StateEventTag)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Guard StateTree event sent: Guard=%s Event=%s"), *GetNameSafe(GuardActor), *StateEventTag.ToString()));
#endif
}

void UHeistDebugFunctionLibrary::DebugGuardPerceptionConfigured(const UObject* WorldContextObject, const UObject* GuardActor, const float SightRadius, const float AggroResetDistance,
																const float SightAngle, const float InvestigateSightAngle, const float EyeHeight, const float DetectionGrace,
																const bool bDoorsBlockSight, const bool bDisplayCasesBlockSight, const FName DoorOccluderTag, const float UpdateInterval)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT(
				"Guard perception configured: Guard=%s SightRadius=%.1f AggroReset=%.1f SightAngle=%.1f InvestigateAngle=%.1f EyeHeight=%.1f DetectionGrace=%.2f DoorsBlockSight=%s DisplayCasesBlockSight=%s DoorTag=%s UpdateInterval=%.2f"),
			*GetNameSafe(GuardActor), SightRadius, AggroResetDistance, SightAngle, InvestigateSightAngle, EyeHeight, DetectionGrace, bDoorsBlockSight ? TEXT("true") : TEXT("false"),
			bDisplayCasesBlockSight ? TEXT("true") : TEXT("false"), *DoorOccluderTag.ToString(), UpdateInterval));
#endif
}

void UHeistDebugFunctionLibrary::DebugGuardSightEvaluated(const UObject* WorldContextObject, const UObject* GuardActor, const UObject* TargetActor, const bool bCanSeeTarget, const TCHAR* Reason,
														  const UObject* BlockingActor)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject,
			FString::Printf(TEXT("Guard sight evaluated: Guard=%s Target=%s Visible=%s Reason=%s BlockingActor=%s"), *GetNameSafe(GuardActor), *GetNameSafe(TargetActor),
							bCanSeeTarget ? TEXT("true") : TEXT("false"), Reason ? Reason : TEXT("None"), *GetNameSafe(BlockingActor)),
			bCanSeeTarget ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugGuardDetectionGraceStarted(const UObject* WorldContextObject, const UObject* GuardActor, const UObject* TargetActor, const float DurationSeconds)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Guard detection grace started: Guard=%s Target=%s Duration=%.2f"), *GetNameSafe(GuardActor), *GetNameSafe(TargetActor), DurationSeconds));
#endif
}

void UHeistDebugFunctionLibrary::DebugGuardDetectionGraceCancelled(const UObject* WorldContextObject, const UObject* GuardActor, const UObject* TargetActor, const TCHAR* Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject,
			FString::Printf(TEXT("Guard detection grace cancelled: Guard=%s Target=%s Reason=%s"), *GetNameSafe(GuardActor), *GetNameSafe(TargetActor), Reason ? Reason : TEXT("None")));
#endif
}

void UHeistDebugFunctionLibrary::DebugGuardSightTargetAcquired(const UObject* WorldContextObject, const UObject* GuardActor, const UObject* TargetActor)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Guard sight target acquired: Guard=%s Target=%s"), *GetNameSafe(GuardActor), *GetNameSafe(TargetActor)));
#endif
}

void UHeistDebugFunctionLibrary::DebugGuardSightTargetLost(const UObject* WorldContextObject, const UObject* GuardActor, const UObject* TargetActor, const FVector& LastKnownLocation,
														   const TCHAR* Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Guard sight target lost: Guard=%s Target=%s LastKnown=(%.1f,%.1f,%.1f) Reason=%s"), *GetNameSafe(GuardActor), *GetNameSafe(TargetActor),
												LastKnownLocation.X, LastKnownLocation.Y, LastKnownLocation.Z, Reason ? Reason : TEXT("None")));
#endif
}

void UHeistDebugFunctionLibrary::DebugGuardNoiseReactionAccepted(const UObject* WorldContextObject, const UObject* GuardActor, const FHeistSoundPingEvent& SoundPingEvent, const float Distance,
																 const float InvestigateDuration)
{
#if UE_BUILD_SHIPPING
	return;
#else
	const FString MessageText =
		FString::Printf(TEXT("Guard noise reaction accepted: Candidate=Selected Guard=%s SequenceId=%d Type=%d Distance=%.1f Radius=%.1f InvestigateDuration=%.2f Location=(%.1f,%.1f,%.1f)"),
						*GetNameSafe(GuardActor), SoundPingEvent.SequenceId, static_cast<int32>(SoundPingEvent.PingType), Distance, SoundPingEvent.Radius, InvestigateDuration,
						SoundPingEvent.WorldLocation.X, SoundPingEvent.WorldLocation.Y, SoundPingEvent.WorldLocation.Z);
	if (SoundPingEvent.PingType == EHeistSoundPingType::Footstep)
	{
		UE_LOG(LogHeistAI, Verbose, TEXT("[%s] %s"), *GetNameSafe(WorldContextObject), *MessageText);
		return;
	}

	Message(WorldContextObject, MessageText);
#endif
}

void UHeistDebugFunctionLibrary::DebugGuardNoiseReactionRejected(const UObject* WorldContextObject, const UObject* GuardActor, const FHeistSoundPingEvent& SoundPingEvent, const TCHAR* Reason,
																 const float Distance)
{
#if UE_BUILD_SHIPPING
	return;
#else
	const FString MessageText = FString::Printf(TEXT("Guard noise reaction rejected: Guard=%s SequenceId=%d Type=%d Distance=%.1f Radius=%.1f Reason=%s"), *GetNameSafe(GuardActor),
												SoundPingEvent.SequenceId, static_cast<int32>(SoundPingEvent.PingType), Distance, SoundPingEvent.Radius, Reason ? Reason : TEXT("None"));
	if (SoundPingEvent.PingType == EHeistSoundPingType::Footstep)
	{
		UE_LOG(LogHeistAI, Verbose, TEXT("[%s] %s"), *GetNameSafe(WorldContextObject), *MessageText);
		return;
	}

	Message(WorldContextObject, MessageText, EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugCoinDistractionDecision(const UObject* WorldContextObject, const UObject* GuardActor, const FHeistSoundPingEvent& SoundPingEvent,
															  const EHeistGuardState GuardState, const TCHAR* Decision, const TCHAR* Rule, const int32 CoinPriority,
															  const EHeistSoundPingType PreviousCandidateType, const int32 PreviousCandidatePriority)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject,
			FString::Printf(
				TEXT("Coin distraction decision: Guard=%s Decision=%s Rule=%s GuardState=%d SequenceId=%d CoinPriority=%d PreviousType=%d PreviousPriority=%d Focus=(%.1f,%.1f,%.1f) Radius=%.1f"),
				*GetNameSafe(GuardActor), Decision ? Decision : TEXT("Unknown"), Rule ? Rule : TEXT("None"), static_cast<int32>(GuardState), SoundPingEvent.SequenceId, CoinPriority,
				static_cast<int32>(PreviousCandidateType), PreviousCandidatePriority, SoundPingEvent.WorldLocation.X, SoundPingEvent.WorldLocation.Y, SoundPingEvent.WorldLocation.Z,
				SoundPingEvent.Radius),
			Decision && FCString::Stricmp(Decision, TEXT("ACCEPT")) == 0 ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugGuardPatrolPathResolved(const UObject* WorldContextObject, const UObject* GuardActor, const FName RouteId, const int32 WaypointCount)
{
#if UE_BUILD_SHIPPING
	return;
#else
	const AActor* GuardActorAsActor = IsValid(GuardActor) && GuardActor->IsA<AActor>() ? static_cast<const AActor*>(GuardActor) : nullptr;
	Message(WorldContextObject,
			FString::Printf(TEXT("Guard patrol path resolved: Guard=%s RouteId=%s Waypoints=%d Authority=%s"), *GetNameSafe(GuardActor), *RouteId.ToString(), WaypointCount,
							IsValid(GuardActorAsActor) && GuardActorAsActor->HasAuthority() ? TEXT("true") : TEXT("false")),
			WaypointCount > 0 ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugGuardMoveStalled(const UObject* WorldContextObject, const UObject* GuardActor, const EHeistGuardState GuardState, const FVector& Destination,
													   const float NoProgressSeconds, const uint8 RetryCount)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject,
			FString::Printf(TEXT("Guard move stalled: Guard=%s State=%s Destination=(%.1f,%.1f,%.1f) NoProgress=%.2f Retry=%d Recovery=StopAndRepath"),
							*GetNameSafe(GuardActor), *UEnum::GetValueAsString(GuardState), Destination.X, Destination.Y, Destination.Z, NoProgressSeconds, RetryCount),
			EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugGuardInvestigateConfirmationStarted(const UObject* WorldContextObject, const UObject* GuardActor, const FVector& InvestigateLocation, const float DurationSeconds)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Guard investigate confirmation started: Guard=%s Location=(%.1f,%.1f,%.1f) Duration=%.2f"), *GetNameSafe(GuardActor), InvestigateLocation.X,
												InvestigateLocation.Y, InvestigateLocation.Z, DurationSeconds));
#endif
}

void UHeistDebugFunctionLibrary::DebugGuardSearchTimerStarted(const UObject* WorldContextObject, const UObject* GuardActor, const FVector& SearchLocation, const float DurationSeconds)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Guard search timer started: Guard=%s LastKnown=(%.1f,%.1f,%.1f) Duration=%.2f"), *GetNameSafe(GuardActor), SearchLocation.X, SearchLocation.Y,
												SearchLocation.Z, DurationSeconds));
#endif
}

void UHeistDebugFunctionLibrary::DebugSoundPingDefinitionRejected(const UObject* WorldContextObject, const FName SoundPingId, const TCHAR* Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Sound Ping definition rejected: SoundPingId=%s Reason=%s"), *SoundPingId.ToString(), Reason), EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugEscapedPlayerRestrictionsApplied(const UObject* WorldContextObject)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(WorldContextObject, FString::Printf(TEXT("Escaped player restrictions applied: Character=%s MovementDisabled=true InteractionDisabled=true CollisionDisabled=true Hidden=true"),
												*GetNameSafe(WorldContextObject)));
#endif
}

#pragma endregion

#pragma region LobbyDebug

void UHeistDebugFunctionLibrary::DebugLobbyHelp(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(PlayerController,
			TEXT("Lobby debug commands: HeistLobbyShow | HeistLobbyHide | HeistLobbyDump | HeistSessionHost | HeistSessionJoin <6-character code> | HeistSessionDump"),
			EHeistDebugLevel::Info, true, 8.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugLobbyShow(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	AHeistHUD* HeistHUD = IsValid(HeistPlayerController) ? Cast<AHeistHUD>(HeistPlayerController->GetHUD()) : nullptr;
	if (!IsValid(HeistHUD))
	{
		Message(PlayerController, TEXT("Lobby debug show failed: missing Heist HUD."), EHeistDebugLevel::Warning, true);
		return;
	}

	const bool bShown = HeistHUD->ShowLobbyScreen();
	Message(PlayerController, FString::Printf(TEXT("Lobby debug show requested: Shown=%s"), bShown ? TEXT("true") : TEXT("false")), bShown ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true);
#endif
}

void UHeistDebugFunctionLibrary::DebugLobbyHide(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	AHeistHUD* HeistHUD = IsValid(HeistPlayerController) ? Cast<AHeistHUD>(HeistPlayerController->GetHUD()) : nullptr;
	if (!IsValid(HeistHUD))
	{
		Message(PlayerController, TEXT("Lobby debug hide failed: missing Heist HUD."), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistHUD->HideLobbyScreen();
	Message(PlayerController, TEXT("Lobby debug hide requested."), EHeistDebugLevel::Info, true);
#endif
}

void UHeistDebugFunctionLibrary::DebugLobbyDump(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	AHeistHUD* HeistHUD = IsValid(HeistPlayerController) ? Cast<AHeistHUD>(HeistPlayerController->GetHUD()) : nullptr;
	UHeistLobbyViewModel* LobbyViewModel = IsValid(HeistHUD) ? HeistHUD->GetLobbyViewModel() : nullptr;
	if (!IsValid(LobbyViewModel))
	{
		Message(PlayerController, TEXT("Lobby debug dump failed: missing Lobby ViewModel."), EHeistDebugLevel::Warning, true);
		return;
	}

	LobbyViewModel->RefreshLobbyData();
	Message(PlayerController,
			FString::Printf(TEXT("Lobby dump: Connected=%d LocalPlayerId=%d Phase=%s Countdown=%s Loadout=%s Blocker=%s"), LobbyViewModel->GetConnectedPlayerCount(),
							LobbyViewModel->GetLocalPlayerId(), *LobbyViewModel->GetPhaseText().ToString(), *LobbyViewModel->GetReadyCountdownText().ToString(),
							*LobbyViewModel->GetDefaultLoadoutText().ToString(), *LobbyViewModel->GetAuthorityBlockerText().ToString()),
			EHeistDebugLevel::Info, true, 8.0f);
	Message(PlayerController, FString::Printf(TEXT("Lobby slot: %s"), *LobbyViewModel->GetPlayerSlot1Text().ToString()), EHeistDebugLevel::Info, false);
	Message(PlayerController, FString::Printf(TEXT("Lobby slot: %s"), *LobbyViewModel->GetPlayerSlot2Text().ToString()), EHeistDebugLevel::Info, false);
	Message(PlayerController, FString::Printf(TEXT("Lobby slot: %s"), *LobbyViewModel->GetPlayerSlot3Text().ToString()), EHeistDebugLevel::Info, false);
	Message(PlayerController, FString::Printf(TEXT("Lobby slot: %s"), *LobbyViewModel->GetPlayerSlot4Text().ToString()), EHeistDebugLevel::Info, false);
#endif
}

void UHeistDebugFunctionLibrary::DebugOnlineSessionHost(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	UHeistGameInstance* HeistGameInstance = IsValid(HeistPlayerController) ? Cast<UHeistGameInstance>(HeistPlayerController->GetGameInstance()) : nullptr;
	AHeistHUD* HeistHUD = IsValid(HeistPlayerController) ? Cast<AHeistHUD>(HeistPlayerController->GetHUD()) : nullptr;
	if (!IsValid(HeistGameInstance))
	{
		Message(PlayerController, TEXT("Session host failed: missing Heist GameInstance."), EHeistDebugLevel::Warning, true);
		return;
	}

	if (IsValid(HeistHUD))
	{
		HeistHUD->HideMainHUD();
		HeistHUD->ShowLobbyScreen();
	}
	const bool bAccepted = HeistGameInstance->RequestHostSession();
	Message(PlayerController, FString::Printf(TEXT("Session host request: Accepted=%s"), bAccepted ? TEXT("true") : TEXT("false")),
			bAccepted ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true);
#endif
}

void UHeistDebugFunctionLibrary::DebugOnlineSessionJoin(APlayerController* PlayerController, const FString& JoinCode)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	UHeistGameInstance* HeistGameInstance = IsValid(HeistPlayerController) ? Cast<UHeistGameInstance>(HeistPlayerController->GetGameInstance()) : nullptr;
	AHeistHUD* HeistHUD = IsValid(HeistPlayerController) ? Cast<AHeistHUD>(HeistPlayerController->GetHUD()) : nullptr;
	if (!IsValid(HeistGameInstance))
	{
		Message(PlayerController, TEXT("Session join failed: missing Heist GameInstance."), EHeistDebugLevel::Warning, true);
		return;
	}

	if (IsValid(HeistHUD))
	{
		HeistHUD->HideMainHUD();
		HeistHUD->ShowLobbyScreen();
	}
	const bool bAccepted = HeistGameInstance->RequestJoinSessionByCode(JoinCode);
	Message(PlayerController, FString::Printf(TEXT("Session join request: Code=%s Accepted=%s"), *JoinCode, bAccepted ? TEXT("true") : TEXT("false")),
			bAccepted ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true);
#endif
}

void UHeistDebugFunctionLibrary::DebugOnlineSessionDump(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	UHeistGameInstance* HeistGameInstance = IsValid(HeistPlayerController) ? Cast<UHeistGameInstance>(HeistPlayerController->GetGameInstance()) : nullptr;
	if (!IsValid(HeistGameInstance))
	{
		Message(PlayerController, TEXT("Online session dump failed: missing Heist GameInstance."), EHeistDebugLevel::Warning, true);
		return;
	}

	const bool bConfigurationValid = !HeistGameInstance->GetActiveOnlineSubsystemName().IsNone() && HeistGameInstance->GetSelectedMapId() == FName(TEXT("M01"))
									 && !HeistGameInstance->GetLobbyMapPath().IsEmpty();
	Message(PlayerController,
			FString::Printf(TEXT("Online session dump: Subsystem=%s State=%s Failure=%s ActiveCode=%s PendingCode=%s MaxPublic=%d Map=%s MapPath=%s Build=%d Pending=%s Hosting=%s Joined=%s Result=%s"),
							*HeistGameInstance->GetActiveOnlineSubsystemName().ToString(), *HeistGameInstance->GetOnlineSessionState().ToString(),
							HeistGameInstance->GetLastOnlineSessionFailure().IsNone() ? TEXT("None") : *HeistGameInstance->GetLastOnlineSessionFailure().ToString(),
							HeistGameInstance->GetActiveJoinCode().IsEmpty() ? TEXT("None") : *HeistGameInstance->GetActiveJoinCode(),
							HeistGameInstance->GetPendingJoinCode().IsEmpty() ? TEXT("None") : *HeistGameInstance->GetPendingJoinCode(),
							HeistGameInstance->GetMaxPublicConnections(), *HeistGameInstance->GetSelectedMapId().ToString(), *HeistGameInstance->GetLobbyMapPath(),
							HeistGameInstance->GetSessionBuildUniqueId(),
							HeistGameInstance->IsOnlineSessionOperationPending() ? TEXT("true") : TEXT("false"),
							HeistGameInstance->IsHostingOnlineSession() ? TEXT("true") : TEXT("false"),
							HeistGameInstance->IsJoinedOnlineSession() ? TEXT("true") : TEXT("false"), bConfigurationValid ? TEXT("PASS") : TEXT("FAIL")),
			bConfigurationValid ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true, 10.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(const UHeistGameInstance* GameInstance, const TCHAR* Request, const FName SubsystemName, const FString& JoinCode,
														   const FName State, const bool bAccepted, const TCHAR* Reason)
{
#if !UE_BUILD_SHIPPING
	LogMessage(EHeistDebugChannel::Network, bAccepted ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning,
			   FString::Printf(TEXT("Online session request: GameInstance=%s Request=%s Subsystem=%s Code=%s State=%s Accepted=%s Reason=%s"), *GetNameSafe(GameInstance),
							   Request != nullptr ? Request : TEXT("Unknown"), *SubsystemName.ToString(), JoinCode.IsEmpty() ? TEXT("None") : *JoinCode, *State.ToString(),
							   bAccepted ? TEXT("true") : TEXT("false"), Reason != nullptr ? Reason : TEXT("None")));
#endif
}

void UHeistDebugFunctionLibrary::DebugOnlineSessionCreateComplete(const UHeistGameInstance* GameInstance, const FName SessionName, const FName SubsystemName,
																  const FString& JoinCode, const bool bCreated, const bool bTravelAccepted, const FName FailureReason)
{
#if !UE_BUILD_SHIPPING
	const bool bPass = bCreated && bTravelAccepted && !JoinCode.IsEmpty();
	LogMessage(EHeistDebugChannel::Network, bPass ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning,
			   FString::Printf(TEXT("Online session create complete: GameInstance=%s Session=%s Subsystem=%s Code=%s MaxPublic=%d Map=%s Build=%d Created=%s TravelAccepted=%s Failure=%s Result=%s"),
							   *GetNameSafe(GameInstance), *SessionName.ToString(), *SubsystemName.ToString(), JoinCode.IsEmpty() ? TEXT("None") : *JoinCode,
							   IsValid(GameInstance) ? GameInstance->GetMaxPublicConnections() : 0,
							   IsValid(GameInstance) ? *GameInstance->GetSelectedMapId().ToString() : TEXT("None"),
							   IsValid(GameInstance) ? GameInstance->GetSessionBuildUniqueId() : 0, bCreated ? TEXT("true") : TEXT("false"),
							   bTravelAccepted ? TEXT("true") : TEXT("false"),
							   FailureReason.IsNone() ? TEXT("None") : *FailureReason.ToString(), bPass ? TEXT("PASS") : TEXT("FAIL")));
#endif
}

void UHeistDebugFunctionLibrary::DebugOnlineSessionFindComplete(const UHeistGameInstance* GameInstance, const FString& JoinCode, const int32 ResultCount,
																const int32 MatchingCodeCount, const int32 FullMatchCount, const int32 VersionMismatchCount,
																const FName SelectedSessionId, const bool bJoinRequestAccepted, const FName FailureReason)
{
#if !UE_BUILD_SHIPPING
	LogMessage(
		EHeistDebugChannel::Network, bJoinRequestAccepted ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning,
		FString::Printf(TEXT("Online session find complete: GameInstance=%s Code=%s Results=%d CodeMatches=%d Full=%d VersionMismatch=%d Selected=%s JoinRequested=%s Failure=%s"),
						*GetNameSafe(GameInstance), JoinCode.IsEmpty() ? TEXT("None") : *JoinCode, ResultCount, MatchingCodeCount, FullMatchCount, VersionMismatchCount,
						SelectedSessionId.IsNone() ? TEXT("None") : *SelectedSessionId.ToString(), bJoinRequestAccepted ? TEXT("true") : TEXT("false"),
						FailureReason.IsNone() ? TEXT("None") : *FailureReason.ToString()));
#endif
}

void UHeistDebugFunctionLibrary::DebugOnlineSessionJoinComplete(const UHeistGameInstance* GameInstance, const FName SessionName, const FString& JoinCode,
																const int32 JoinResult, const bool bAddressResolved, const bool bTravelRequested, const FName FailureReason)
{
#if !UE_BUILD_SHIPPING
	const bool bPass = JoinResult == 0 && bAddressResolved && bTravelRequested;
	LogMessage(EHeistDebugChannel::Network, bPass ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning,
			   FString::Printf(TEXT("Online session join complete: GameInstance=%s Session=%s Code=%s JoinResult=%d AddressResolved=%s TravelRequested=%s Failure=%s Result=%s"),
							   *GetNameSafe(GameInstance), *SessionName.ToString(), JoinCode.IsEmpty() ? TEXT("None") : *JoinCode, JoinResult,
							   bAddressResolved ? TEXT("true") : TEXT("false"), bTravelRequested ? TEXT("true") : TEXT("false"),
							   FailureReason.IsNone() ? TEXT("None") : *FailureReason.ToString(), bPass ? TEXT("PASS") : TEXT("FAIL")));
#endif
}

#pragma endregion

#pragma region ResultDebug

void UHeistDebugFunctionLibrary::DebugResultHelp(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(PlayerController, TEXT("Result debug commands: HeistResultShow | HeistResultHide | HeistResultDump | HeistResultRebuild | HeistResultSeed <Score> <Escaped 1/0> <EscapeTime>"),
			EHeistDebugLevel::Info, true, 8.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugResultShow(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	AHeistHUD* HeistHUD = IsValid(HeistPlayerController) ? Cast<AHeistHUD>(HeistPlayerController->GetHUD()) : nullptr;
	if (!IsValid(HeistHUD))
	{
		Message(PlayerController, TEXT("Result debug show failed: missing Heist HUD."), EHeistDebugLevel::Warning, true);
		return;
	}

	const bool bShown = HeistHUD->ShowResultScreen();
	Message(PlayerController, FString::Printf(TEXT("Result debug show requested: Shown=%s"), bShown ? TEXT("true") : TEXT("false")), bShown ? EHeistDebugLevel::Info : EHeistDebugLevel::Warning, true);
#endif
}

void UHeistDebugFunctionLibrary::DebugResultHide(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	AHeistHUD* HeistHUD = IsValid(HeistPlayerController) ? Cast<AHeistHUD>(HeistPlayerController->GetHUD()) : nullptr;
	if (!IsValid(HeistHUD))
	{
		Message(PlayerController, TEXT("Result debug hide failed: missing Heist HUD."), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistHUD->HideResultScreen();
	Message(PlayerController, TEXT("Result debug hide requested."), EHeistDebugLevel::Info, true);
#endif
}

void UHeistDebugFunctionLibrary::DebugResultDump(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	const AHeistGameState* HeistGameState = IsValid(PlayerController) && IsValid(PlayerController->GetWorld()) ? PlayerController->GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistGameState))
	{
		Message(PlayerController, TEXT("Result debug dump failed: missing Heist GameState."), EHeistDebugLevel::Warning, true);
		return;
	}

	const TArray<FHeistPlayerResult>& PlayerResults = HeistGameState->GetPlayerResults();
	Message(PlayerController, FString::Printf(TEXT("Contribution result dump: PlayerCount=%d"), PlayerResults.Num()), EHeistDebugLevel::Info, true, 6.0f);

	for (const FHeistPlayerResult& PlayerResult : PlayerResults)
	{
		Message(PlayerController, FString::Printf(TEXT("Result entry: %s"), *FormatResultEntry(PlayerResult)), EHeistDebugLevel::Info, false);
	}
#endif
}

void UHeistDebugFunctionLibrary::DebugResultRebuild(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Result debug rebuild failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->DebugRequestRebuildResults();
	Message(PlayerController, TEXT("Result debug rebuild requested."), EHeistDebugLevel::Info, true);
#endif
}

void UHeistDebugFunctionLibrary::DebugResultSeed(APlayerController* PlayerController, const int32 Score, const bool bEscaped, const float EscapeTimeSeconds)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Result debug seed failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	const int32 SafeScore = FMath::Max(0, Score);
	const float SafeEscapeTimeSeconds = FMath::Max(0.0f, EscapeTimeSeconds);
	HeistPlayerController->DebugRequestSeedResult(SafeScore, bEscaped, SafeEscapeTimeSeconds);
	Message(PlayerController, FString::Printf(TEXT("Result debug seed requested: Score=%d Escaped=%s EscapeTime=%.2f"), SafeScore, bEscaped ? TEXT("true") : TEXT("false"), SafeEscapeTimeSeconds),
			EHeistDebugLevel::Info, true);
#endif
}

#pragma endregion

#pragma region InventoryDebug

void UHeistDebugFunctionLibrary::DebugInventoryHelp(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		PlayerController,
		TEXT(
			"Inventory debug commands: HeistInvDump | HeistInvOpen 1/0 | HeistInvAdd <ItemId> | HeistInvMove <InstanceId> <X> <Y> | HeistInvRotate <InstanceId> | HeistInvDrop <InstanceId> | HeistInvAssign <Q|Coin> <InstanceId> | HeistInvClear <Q|Coin> | HeistInvInvalidMove <InstanceId>"),
		EHeistDebugLevel::Info, true, 8.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryDump(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	const UHeistInventoryComponent* InventoryComponent = ResolveInventoryComponent(PlayerController);
	if (!IsValid(InventoryComponent))
	{
		Message(PlayerController, TEXT("Inventory debug dump failed: missing local Heist inventory component."), EHeistDebugLevel::Warning, true);
		return;
	}

	const FHeistReplicatedInventory& ReplicatedInventory = InventoryComponent->GetReplicatedInventory();
	const AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	Message(PlayerController,
			FString::Printf(TEXT("Inventory dump: Open=%s InputMode=%s Cursor=%s IgnoreMove=%s IgnoreLook=%s Grid=%dx%d Items=%d QuickSlots=%d"),
							InventoryComponent->IsInventoryOpen() ? TEXT("true") : TEXT("false"),
							IsValid(HeistPlayerController) ? ToInputModeText(HeistPlayerController->GetLocalInputMode()) : TEXT("Unknown"),
							IsValid(HeistPlayerController) && HeistPlayerController->bShowMouseCursor ? TEXT("true") : TEXT("false"),
							IsValid(HeistPlayerController) && HeistPlayerController->IsMoveInputIgnored() ? TEXT("true") : TEXT("false"),
							IsValid(HeistPlayerController) && HeistPlayerController->IsLookInputIgnored() ? TEXT("true") : TEXT("false"), InventoryComponent->GetGridColumnCount(),
							InventoryComponent->GetGridRowCount(), ReplicatedInventory.Items.Num(), InventoryComponent->GetQuickSlots().Num()),
			EHeistDebugLevel::Info, true, 6.0f);

	for (const FHeistInventoryFastArrayItem& ItemEntry : ReplicatedInventory.Items)
	{
		const FHeistInventoryItem& Item = ItemEntry.InventoryItem;
		Message(PlayerController,
				FString::Printf(TEXT("Inventory item: InstanceId=%d ItemId=%s Grid=(%d,%d) Quantity=%d Rotated=%s"), Item.InstanceId, *Item.ItemId.ToString(), Item.GridPosition.X, Item.GridPosition.Y,
								Item.Quantity, Item.bRotated ? TEXT("true") : TEXT("false")),
				EHeistDebugLevel::Info, false);
	}

	for (const FHeistQuickSlotState& QuickSlot : InventoryComponent->GetQuickSlots())
	{
		Message(PlayerController, FString::Printf(TEXT("QuickSlot: Slot=%s InstanceId=%d"), ToQuickSlotText(QuickSlot.SlotType), QuickSlot.ItemInstanceId), EHeistDebugLevel::Info, false);
	}
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryOpen(APlayerController* PlayerController, const bool bOpen)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Inventory debug open failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->RequestSetInventoryOpen(bOpen);
	Message(PlayerController, FString::Printf(TEXT("Inventory debug open requested: Open=%s"), bOpen ? TEXT("true") : TEXT("false")), EHeistDebugLevel::Info, true);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryAdd(APlayerController* PlayerController, const FName ItemId)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController) || ItemId.IsNone())
	{
		Message(PlayerController, TEXT("Inventory debug add failed: invalid Heist player controller or ItemId."), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->DebugRequestAddInventoryItem(ItemId);
	Message(PlayerController, FString::Printf(TEXT("Inventory debug add requested: ItemId=%s"), *ItemId.ToString()), EHeistDebugLevel::Info, true);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryMove(APlayerController* PlayerController, const int32 InstanceId, const int32 GridX, const int32 GridY)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Inventory debug move failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->RequestMoveInventoryItem(InstanceId, FIntPoint(GridX, GridY));
	Message(PlayerController, FString::Printf(TEXT("Inventory debug move requested: InstanceId=%d Grid=(%d,%d)"), InstanceId, GridX, GridY), EHeistDebugLevel::Info, true);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryRotate(APlayerController* PlayerController, const int32 InstanceId)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Inventory debug rotate failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->RequestRotateInventoryItem(InstanceId);
	Message(PlayerController, FString::Printf(TEXT("Inventory debug rotate requested: InstanceId=%d"), InstanceId), EHeistDebugLevel::Info, true);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryDrop(APlayerController* PlayerController, const int32 InstanceId)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Inventory debug drop failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->RequestDropInventoryItem(InstanceId);
	Message(PlayerController, FString::Printf(TEXT("Inventory debug drop requested: InstanceId=%d"), InstanceId), EHeistDebugLevel::Info, true);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryAssignQuickSlot(APlayerController* PlayerController, const FString& SlotName, const int32 InstanceId)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Inventory debug assign failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	EHeistQuickSlotType SlotType = EHeistQuickSlotType::None;
	if (!TryParseQuickSlotName(SlotName, SlotType))
	{
		Message(PlayerController, FString::Printf(TEXT("Inventory debug assign failed: invalid slot '%s'. Use Q or Coin."), *SlotName), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->RequestAssignQuickSlot(SlotType, InstanceId);
	Message(PlayerController, FString::Printf(TEXT("Inventory debug assign requested: Slot=%s InstanceId=%d"), ToQuickSlotText(SlotType), InstanceId), EHeistDebugLevel::Info, true);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryClearQuickSlot(APlayerController* PlayerController, const FString& SlotName)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Inventory debug clear failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	EHeistQuickSlotType SlotType = EHeistQuickSlotType::None;
	if (!TryParseQuickSlotName(SlotName, SlotType))
	{
		Message(PlayerController, FString::Printf(TEXT("Inventory debug clear failed: invalid slot '%s'. Use Q or Coin."), *SlotName), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->RequestClearQuickSlot(SlotType);
	Message(PlayerController, FString::Printf(TEXT("Inventory debug clear requested: Slot=%s"), ToQuickSlotText(SlotType)), EHeistDebugLevel::Info, true);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryInvalidMove(APlayerController* PlayerController, const int32 InstanceId)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Inventory debug invalid move failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->RequestMoveInventoryItem(InstanceId, FIntPoint(-1, -1));
	Message(PlayerController, FString::Printf(TEXT("Inventory debug invalid move requested: InstanceId=%d Grid=(-1,-1)"), InstanceId), EHeistDebugLevel::Info, true);
#endif
}

#pragma endregion

#pragma region StatusDebug

void UHeistDebugFunctionLibrary::DebugStatusHelp(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(PlayerController, TEXT("Status debug commands: HeistStatusDump"), EHeistDebugLevel::Info, true, 8.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugStatusDump(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	const UHeistStatusComponent* StatusComponent = ResolveStatusComponent(PlayerController);
	if (!IsValid(StatusComponent))
	{
		Message(PlayerController, TEXT("Status debug dump failed: missing local Heist status component."), EHeistDebugLevel::Warning, true);
		return;
	}

	Message(PlayerController, FString::Printf(TEXT("Status dump: Tags=[%s]"), *FormatStatusTags(StatusComponent->GetStatusTags())), EHeistDebugLevel::Info, true, 6.0f);
#endif
}

#pragma endregion

#pragma region FeedbackDebug

void UHeistDebugFunctionLibrary::DebugFeedbackHelp(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(PlayerController, TEXT("Feedback debug commands: HeistFeedbackTest | HeistFeedbackBagFull | HeistFeedbackDump"), EHeistDebugLevel::Info, true, 8.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugFeedbackTest(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Feedback test failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->DebugRequestFeedbackTest();
	Message(PlayerController, TEXT("Popup feedback ownership test requested."), EHeistDebugLevel::Info, true);
#endif
}

void UHeistDebugFunctionLibrary::DebugFeedbackBagFull(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Bag Full feedback test failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->DebugRequestFillInventoryForFeedback(TEXT("Throwable_coin"));
	Message(PlayerController, TEXT("Bag Full feedback test requested."), EHeistDebugLevel::Info, true);
#endif
}

void UHeistDebugFunctionLibrary::DebugFeedbackDump(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistHUD* HeistHUD = IsValid(PlayerController) ? Cast<AHeistHUD>(PlayerController->GetHUD()) : nullptr;
	UHeistHUDWidget* HUDWidget = IsValid(HeistHUD) ? HeistHUD->GetMainHUDWidget() : nullptr;
	if (!IsValid(HUDWidget))
	{
		Message(PlayerController, TEXT("Feedback dump failed: missing local Heist HUD widget."), EHeistDebugLevel::Warning, true);
		return;
	}

	HUDWidget->DebugDumpFeedbackState();
	Message(PlayerController, TEXT("Feedback presentation dump requested."), EHeistDebugLevel::Info, true);
#endif
}

#pragma endregion

#pragma region ThrowableDebug

void UHeistDebugFunctionLibrary::DebugThrowableHelp(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(PlayerController, TEXT("Throwable debug commands: HeistCoinThrow <Distance> | HeistCoinThrowAt <X> <Y> <Z>"), EHeistDebugLevel::Info, true, 8.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugCoinThrow(APlayerController* PlayerController, const float Distance)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Coin debug throw failed: invalid Heist player controller or pawn."), EHeistDebugLevel::Warning, true);
		return;
	}

	const float ClampedDistance = FMath::Clamp(Distance, 100.0f, 5000.0f);
	FVector ViewLocation;
	FVector CameraForward;
	FVector TargetWorldLocation;
	if (!HeistPlayerController->TryBuildCameraForwardAim(ClampedDistance, ViewLocation, CameraForward, TargetWorldLocation))
	{
		Message(PlayerController, TEXT("Coin debug throw failed: invalid camera forward."), EHeistDebugLevel::Warning, true);
		return;
	}
	HeistPlayerController->DebugRequestThrowCoinAtWorldLocation(TargetWorldLocation);
	Message(PlayerController, FString::Printf(TEXT("Coin debug throw requested: Distance=%.1f CameraForward=(%.3f,%.3f,%.3f)"), ClampedDistance, CameraForward.X, CameraForward.Y, CameraForward.Z),
			EHeistDebugLevel::Info, true);
#endif
}

void UHeistDebugFunctionLibrary::DebugCoinThrowAt(APlayerController* PlayerController, const float TargetX, const float TargetY, const float TargetZ)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Coin debug throw-at failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	const FVector TargetWorldLocation(TargetX, TargetY, TargetZ);
	HeistPlayerController->DebugRequestThrowCoinAtWorldLocation(TargetWorldLocation);
	Message(PlayerController, FString::Printf(TEXT("Coin debug throw-at requested: Target=(%.1f,%.1f,%.1f)"), TargetX, TargetY, TargetZ), EHeistDebugLevel::Info, true);
#endif
}

#pragma endregion
