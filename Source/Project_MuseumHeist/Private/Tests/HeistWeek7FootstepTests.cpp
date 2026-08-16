#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "AI/HeistGuardAIController.h"
#include "AI/HeistGuardCharacter.h"
#include "AI/HeistGuardStateComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistPlayerState.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Inventory/HeistItemDataTypes.h"
#include "Misc/AutomationTest.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

namespace HeistWeek7FootstepTest
{
constexpr int32 FootstepPlayerId = 2;

enum class EFootstepCapturePhase : uint8
{
	None,
	Walk,
	Sprint
};

struct FFootstepAutomationState
{
	bool bAborted = false;
	bool bCapturedPlaySettings = false;
	EPlayNetMode OriginalNetMode = EPlayNetMode::PIE_Standalone;
	bool bOriginalRunUnderOneProcess = true;
	int32 OriginalClientCount = 1;
	EFootstepCapturePhase CapturePhase = EFootstepCapturePhase::None;
	FDelegateHandle SoundPingHandle;
	TWeakObjectPtr<AHeistGameState> BoundGameState;
	TWeakObjectPtr<AHeistGuardCharacter> SelectedGuard;
	FHeistSoundPingEvent WalkEvent;
	FHeistSoundPingEvent SprintEvent;
	int32 WalkReactedGuardCount = 0;
	int32 SprintReactedGuardCount = 0;
	float ExpectedWalkRadius = 0.0f;
	float ExpectedSprintRadius = 0.0f;
	float ExpectedSprintRefreshInterval = 0.0f;
	FVector WalkStartLocation = FVector::ZeroVector;
	FVector SprintStartLocation = FVector::ZeroVector;
	float WalkCaptureStartServerTime = 0.0f;
	float SprintCaptureStartServerTime = 0.0f;
	float WalkServerDistance = 0.0f;
	float SprintServerDistance = 0.0f;
	float WalkEventSegmentDistance = -1.0f;
	float SprintEventSegmentDistance = -1.0f;
	float WalkClientFocusDistance = -1.0f;
	float SprintClientFocusDistance = -1.0f;
	bool bWalkClientStateReplicated = false;
	bool bSprintClientStateReplicated = false;
	FName SelectedGuardStableName = NAME_None;
	EHeistAlertLevel InitialAlertLevel = EHeistAlertLevel::Quiet;
	int32 InitialAlertRevision = 0;
};

float DistanceToSegment2D(const FVector& Point, const FVector& SegmentStart, const FVector& SegmentEnd)
{
	const FVector2D Point2D(Point.X, Point.Y);
	const FVector2D Start2D(SegmentStart.X, SegmentStart.Y);
	const FVector2D Segment = FVector2D(SegmentEnd.X, SegmentEnd.Y) - Start2D;
	const float SegmentLengthSquared = Segment.SizeSquared();
	if (SegmentLengthSquared <= UE_KINDA_SMALL_NUMBER)
	{
		return FVector2D::Distance(Point2D, Start2D);
	}

	const float Alpha = FMath::Clamp(FVector2D::DotProduct(Point2D - Start2D, Segment) / SegmentLengthSquared, 0.0f, 1.0f);
	return FVector2D::Distance(Point2D, Start2D + Segment * Alpha);
}

TArray<UWorld*> GetPIEWorlds()
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

UWorld* GetServerWorld()
{
	for (UWorld* World : GetPIEWorlds())
	{
		if (IsValid(World) && (World->GetNetMode() == NM_ListenServer || World->GetNetMode() == NM_Standalone))
		{
			return World;
		}
	}
	return nullptr;
}

AHeistPlayerController* GetLocalController(UWorld* World)
{
	if (!IsValid(World))
	{
		return nullptr;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		AHeistPlayerController* Controller = Cast<AHeistPlayerController>(It->Get());
		if (IsValid(Controller) && Controller->IsLocalController())
		{
			return Controller;
		}
	}
	return nullptr;
}

AHeistPlayerController* GetOwningControllerById(const int32 PlayerId)
{
	for (UWorld* World : GetPIEWorlds())
	{
		AHeistPlayerController* Controller = GetLocalController(World);
		const AHeistPlayerState* PlayerState = IsValid(Controller) ? Controller->GetPlayerState<AHeistPlayerState>() : nullptr;
		if (IsValid(PlayerState) && PlayerState->HeistPlayerId == PlayerId)
		{
			return Controller;
		}
	}
	return nullptr;
}

AHeistPlayerCharacter* GetServerCharacterById(const int32 PlayerId)
{
	UWorld* ServerWorld = GetServerWorld();
	if (!IsValid(ServerWorld))
	{
		return nullptr;
	}

	for (TActorIterator<AHeistPlayerCharacter> It(ServerWorld); It; ++It)
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

bool AreTwoPlayerGameplayWorldsReady()
{
	const TArray<UWorld*> Worlds = GetPIEWorlds();
	UWorld* ServerWorld = GetServerWorld();
	const AHeistGameMode* GameMode = IsValid(ServerWorld) ? ServerWorld->GetAuthGameMode<AHeistGameMode>() : nullptr;
	if (Worlds.Num() != 2 || !IsValid(ServerWorld) || !IsValid(GameMode) || !GameMode->IsPlayerCountGuardScalingApplied() ||
		GameMode->GetDifficultyAppliedPlayerCount() != 2)
	{
		return false;
	}

	TSet<int32> LocalPlayerIds;
	for (UWorld* World : Worlds)
	{
		const AHeistGameState* GameState = World->GetGameState<AHeistGameState>();
		const AHeistPlayerController* Controller = GetLocalController(World);
		const AHeistPlayerState* PlayerState = IsValid(Controller) ? Controller->GetPlayerState<AHeistPlayerState>() : nullptr;
		if (!IsValid(GameState) || GameState->GetMatchPhase() != EHeistMatchPhase::InGame || GameState->PlayerArray.Num() != 2 || !IsValid(Controller) ||
			!IsValid(Controller->GetPawn()) || !IsValid(PlayerState) || PlayerState->HeistPlayerId < 1 || PlayerState->HeistPlayerId > 2)
		{
			return false;
		}
		LocalPlayerIds.Add(PlayerState->HeistPlayerId);
	}
	return LocalPlayerIds.Num() == 2 && IsValid(GetServerCharacterById(FootstepPlayerId));
}

bool PositionGuardForFootstep(const TSharedRef<FFootstepAutomationState>& State)
{
	AHeistPlayerCharacter* ServerCharacter = GetServerCharacterById(FootstepPlayerId);
	AHeistGuardCharacter* Guard = State->SelectedGuard.Get();
	AHeistGuardAIController* GuardController = IsValid(Guard) ? Cast<AHeistGuardAIController>(Guard->GetController()) : nullptr;
	UHeistGuardStateComponent* GuardState = IsValid(Guard) ? Guard->GetGuardStateComponent() : nullptr;
	if (!IsValid(ServerCharacter) || !IsValid(Guard) || !IsValid(GuardController) || !IsValid(GuardState))
	{
		return false;
	}

	GuardController->SetAutomaticSightEnabled(false);
	GuardController->StopMovement();
	if (!GuardState->EnterPatrol())
	{
		return false;
	}

	const FVector GuardLocation = ServerCharacter->GetActorLocation() + FVector(0.0f, 250.0f, 0.0f);
	Guard->SetActorLocation(GuardLocation, false, nullptr, ETeleportType::TeleportPhysics);
	Guard->ForceNetUpdate();
	return true;
}

bool ConfigureGuardsAndCapture(const TSharedRef<FFootstepAutomationState>& State)
{
	UWorld* ServerWorld = GetServerWorld();
	AHeistGameMode* GameMode = IsValid(ServerWorld) ? ServerWorld->GetAuthGameMode<AHeistGameMode>() : nullptr;
	AHeistGameState* GameState = IsValid(ServerWorld) ? ServerWorld->GetGameState<AHeistGameState>() : nullptr;
	AHeistPlayerCharacter* ServerCharacter = GetServerCharacterById(FootstepPlayerId);
	const AHeistPlayerController* OwningController = GetOwningControllerById(FootstepPlayerId);
	if (!IsValid(GameMode) || !IsValid(GameState) || !IsValid(ServerCharacter) || !ServerCharacter->HasAuthority() || !IsValid(OwningController) ||
		!IsValid(OwningController->GetWorld()) || OwningController->GetWorld()->GetNetMode() != NM_Client)
	{
		return false;
	}

	FHeistSoundPingDataRow WalkDefinition;
	FHeistSoundPingDataRow SprintDefinition;
	if (!GameMode->TryGetSoundPingDefinition(FName(TEXT("Ping_Footstep_Walk")), WalkDefinition) ||
		!GameMode->TryGetSoundPingDefinition(FName(TEXT("Ping_Footstep_Run")), SprintDefinition))
	{
		return false;
	}
	State->ExpectedWalkRadius = WalkDefinition.Radius;
	State->ExpectedSprintRadius = SprintDefinition.Radius;
	State->ExpectedSprintRefreshInterval = FMath::Max(0.0f, SprintDefinition.RefreshInterval);
	if (!FMath::IsNearlyEqual(State->ExpectedWalkRadius, 500.0f) || !FMath::IsNearlyEqual(State->ExpectedSprintRadius, 1000.0f) ||
		!FMath::IsFinite(State->ExpectedSprintRefreshInterval))
	{
		return false;
	}

	TArray<AHeistGuardCharacter*> Guards;
	for (TActorIterator<AHeistGuardCharacter> It(ServerWorld); It; ++It)
	{
		if (IsValid(*It) && It->IsDifficultyActive() && IsValid(It->GetGuardStateComponent()))
		{
			Guards.Add(*It);
		}
	}
	Guards.Sort([](const AHeistGuardCharacter& Left, const AHeistGuardCharacter& Right) { return Left.GetName() < Right.GetName(); });
	if (Guards.Num() == 0)
	{
		return false;
	}

	State->SelectedGuard = Guards[0];
	State->SelectedGuardStableName = Guards[0]->GetFName();
	for (int32 GuardIndex = 0; GuardIndex < Guards.Num(); ++GuardIndex)
	{
		AHeistGuardCharacter* Guard = Guards[GuardIndex];
		if (AHeistGuardAIController* GuardController = Cast<AHeistGuardAIController>(Guard->GetController()))
		{
			GuardController->SetAutomaticSightEnabled(false);
			GuardController->StopMovement();
		}
		if (GuardIndex > 0)
		{
			Guard->GetGuardStateComponent()->SetDisabled(true);
		}
	}

	if (!PositionGuardForFootstep(State))
	{
		return false;
	}

	State->InitialAlertLevel = GameState->GetAlertLevel();
	State->InitialAlertRevision = GameState->GetAlertRevision();
	State->BoundGameState = GameState;
	State->SoundPingHandle = GameState->GetSoundPingEventReportedDelegate().AddLambda(
		[State](const FHeistSoundPingEvent& SoundPingEvent, int32*)
		{
			if (SoundPingEvent.PingType != EHeistSoundPingType::Footstep)
			{
				return;
			}
			const AHeistPlayerCharacter* ServerCharacter = GetServerCharacterById(FootstepPlayerId);
			const float CaptureStartServerTime = State->CapturePhase == EFootstepCapturePhase::Sprint ? State->SprintCaptureStartServerTime : State->WalkCaptureStartServerTime;
			if (!IsValid(ServerCharacter) || State->CapturePhase == EFootstepCapturePhase::None ||
				SoundPingEvent.ServerTimeSeconds + KINDA_SMALL_NUMBER < CaptureStartServerTime ||
				FVector::Dist2D(SoundPingEvent.WorldLocation, ServerCharacter->GetActorLocation()) > 5.0f)
			{
				return;
			}

			if (State->CapturePhase == EFootstepCapturePhase::Walk && State->WalkEvent.SequenceId == 0)
			{
				State->WalkEvent = SoundPingEvent;
			}
			else if (State->CapturePhase == EFootstepCapturePhase::Sprint && State->SprintEvent.SequenceId == 0)
			{
				State->SprintEvent = SoundPingEvent;
			}
		});
	return State->SoundPingHandle.IsValid();
}

bool ArmFootstepCapture(const TSharedRef<FFootstepAutomationState>& State, const EFootstepCapturePhase CapturePhase)
{
	AHeistPlayerCharacter* ServerCharacter = GetServerCharacterById(FootstepPlayerId);
	AHeistGameState* GameState = State->BoundGameState.Get();
	if (!IsValid(ServerCharacter) || !IsValid(GameState) || CapturePhase == EFootstepCapturePhase::None)
	{
		return false;
	}

	if (CapturePhase == EFootstepCapturePhase::Walk)
	{
		State->WalkEvent = FHeistSoundPingEvent();
		State->WalkStartLocation = ServerCharacter->GetActorLocation();
		State->WalkCaptureStartServerTime = GameState->GetServerWorldTimeSeconds();
		State->WalkReactedGuardCount = 0;
		State->WalkEventSegmentDistance = -1.0f;
		State->WalkClientFocusDistance = -1.0f;
		State->bWalkClientStateReplicated = false;
	}
	else
	{
		State->SprintEvent = FHeistSoundPingEvent();
		State->SprintStartLocation = ServerCharacter->GetActorLocation();
		State->SprintCaptureStartServerTime = GameState->GetServerWorldTimeSeconds();
		State->SprintReactedGuardCount = 0;
		State->SprintEventSegmentDistance = -1.0f;
		State->SprintClientFocusDistance = -1.0f;
		State->bSprintClientStateReplicated = false;
	}

	State->CapturePhase = CapturePhase;
	return true;
}

int32 CountAuthoritativeGuardReactions(UWorld* ServerWorld, const FHeistSoundPingEvent& Event, FName& OutReactedGuardName)
{
	OutReactedGuardName = NAME_None;
	int32 ReactedGuardCount = 0;
	for (TActorIterator<AHeistGuardCharacter> It(ServerWorld); It; ++It)
	{
		const AHeistGuardCharacter* Guard = *It;
		const UHeistGuardStateComponent* GuardState = IsValid(Guard) && Guard->IsDifficultyActive() ? Guard->GetGuardStateComponent() : nullptr;
		if (!IsValid(GuardState) || GuardState->GetGuardState() != EHeistGuardState::InvestigateNoise ||
			!GuardState->GetStateFocusLocation().Equals(Event.WorldLocation, 1.0f))
		{
			continue;
		}

		++ReactedGuardCount;
		OutReactedGuardName = Guard->GetFName();
	}
	return ReactedGuardCount;
}

bool IsSelectedGuardStateReplicatedToClient(const TSharedRef<FFootstepAutomationState>& State, const FHeistSoundPingEvent& Event, const bool bSprint)
{
	const AHeistPlayerController* ClientController = GetOwningControllerById(FootstepPlayerId);
	UWorld* ClientWorld = IsValid(ClientController) ? ClientController->GetWorld() : nullptr;
	if (!IsValid(ClientWorld) || ClientWorld->GetNetMode() != NM_Client || State->SelectedGuardStableName.IsNone())
	{
		return false;
	}

	for (TActorIterator<AHeistGuardCharacter> It(ClientWorld); It; ++It)
	{
		const AHeistGuardCharacter* ClientGuard = *It;
		if (!IsValid(ClientGuard) || ClientGuard->GetFName() != State->SelectedGuardStableName)
		{
			continue;
		}

		const UHeistGuardStateComponent* ClientGuardState = ClientGuard->GetGuardStateComponent();
		const float FocusDistance = IsValid(ClientGuardState) ? FVector::Dist(ClientGuardState->GetStateFocusLocation(), Event.WorldLocation) : -1.0f;
		(bSprint ? State->SprintClientFocusDistance : State->WalkClientFocusDistance) = FocusDistance;
		const bool bReplicated = IsValid(ClientGuardState) && ClientGuardState->GetGuardState() == EHeistGuardState::InvestigateNoise && FocusDistance <= 1.0f;
		(bSprint ? State->bSprintClientStateReplicated : State->bWalkClientStateReplicated) = bReplicated;
		return bReplicated;
	}
	return false;
}

bool IsFootstepEvidenceReady(const TSharedRef<FFootstepAutomationState>& State, const bool bSprint)
{
	UWorld* ServerWorld = GetServerWorld();
	AHeistPlayerCharacter* ServerCharacter = GetServerCharacterById(FootstepPlayerId);
	const FHeistSoundPingEvent& Event = bSprint ? State->SprintEvent : State->WalkEvent;
	const float ExpectedRadius = bSprint ? State->ExpectedSprintRadius : State->ExpectedWalkRadius;
	const FVector StartLocation = bSprint ? State->SprintStartLocation : State->WalkStartLocation;
	const float CaptureStartServerTime = bSprint ? State->SprintCaptureStartServerTime : State->WalkCaptureStartServerTime;
	if (!IsValid(ServerWorld) || !IsValid(ServerCharacter) || Event.SequenceId <= 0 || Event.PingType != EHeistSoundPingType::Footstep || !Event.bAffectsGuards ||
		!FMath::IsNearlyEqual(Event.Radius, ExpectedRadius) || Event.ServerTimeSeconds + KINDA_SMALL_NUMBER < CaptureStartServerTime)
	{
		return false;
	}

	const float TravelDistance = FVector::Dist2D(StartLocation, ServerCharacter->GetActorLocation());
	const float EventSegmentDistance = DistanceToSegment2D(Event.WorldLocation, StartLocation, ServerCharacter->GetActorLocation());
	if (bSprint)
	{
		State->SprintServerDistance = FMath::Max(State->SprintServerDistance, TravelDistance);
		State->SprintEventSegmentDistance = EventSegmentDistance;
	}
	else
	{
		State->WalkServerDistance = FMath::Max(State->WalkServerDistance, TravelDistance);
		State->WalkEventSegmentDistance = EventSegmentDistance;
	}
	if (TravelDistance < 10.0f || EventSegmentDistance > 50.0f)
	{
		return false;
	}

	FName ReactedGuardName;
	const int32 ReactedGuardCount = CountAuthoritativeGuardReactions(ServerWorld, Event, ReactedGuardName);
	(bSprint ? State->SprintReactedGuardCount : State->WalkReactedGuardCount) = ReactedGuardCount;
	if (ReactedGuardCount != 1 || ReactedGuardName != State->SelectedGuardStableName)
	{
		return false;
	}

	return IsSelectedGuardStateReplicatedToClient(State, Event, bSprint);
}

bool IsAlertUnchanged(const TSharedRef<FFootstepAutomationState>& State)
{
	const AHeistGameState* GameState = State->BoundGameState.Get();
	return IsValid(GameState) && GameState->GetAlertLevel() == State->InitialAlertLevel && GameState->GetAlertRevision() == State->InitialAlertRevision;
}

FString DescribeFootstepEvidence(const TSharedRef<FFootstepAutomationState>& State, const bool bSprint)
{
	const AHeistPlayerCharacter* ServerCharacter = GetServerCharacterById(FootstepPlayerId);
	const AHeistGuardCharacter* Guard = State->SelectedGuard.Get();
	const UHeistGuardStateComponent* GuardState = IsValid(Guard) ? Guard->GetGuardStateComponent() : nullptr;
	const AHeistGameState* GameState = State->BoundGameState.Get();
	const FHeistSoundPingEvent& Event = bSprint ? State->SprintEvent : State->WalkEvent;
	const int32 ReactedCount = bSprint ? State->SprintReactedGuardCount : State->WalkReactedGuardCount;
	const FVector StartLocation = bSprint ? State->SprintStartLocation : State->WalkStartLocation;
	const float CaptureStartServerTime = bSprint ? State->SprintCaptureStartServerTime : State->WalkCaptureStartServerTime;
	const float EventSegmentDistance = bSprint ? State->SprintEventSegmentDistance : State->WalkEventSegmentDistance;
	const float ClientFocusDistance = bSprint ? State->SprintClientFocusDistance : State->WalkClientFocusDistance;
	const bool bClientStateReplicated = bSprint ? State->bSprintClientStateReplicated : State->bWalkClientStateReplicated;
	const float TravelDistance = IsValid(ServerCharacter) ? FVector::Dist2D(StartLocation, ServerCharacter->GetActorLocation()) : -1.0f;
	return FString::Printf(
		TEXT("Pace=%s Sequence=%d Radius=%.1f CaptureServerTime=%.2f EventServerTime=%.2f EventSegmentDistance=%.1f ReactedGuards=%d ServerDistance=%.1f "
			 "ServerSpeed=%.1f Guard=%s GuardState=%s FocusDistance=%.1f ClientStateReplicated=%s ClientFocusDistance=%.1f Alert=%s Revision=%d InitialRevision=%d"),
		bSprint ? TEXT("Sprint") : TEXT("Walk"), Event.SequenceId, Event.Radius, CaptureStartServerTime, Event.ServerTimeSeconds, EventSegmentDistance, ReactedCount, TravelDistance,
		IsValid(ServerCharacter) ? ServerCharacter->GetVelocity().Size2D() : -1.0f, *GetNameSafe(Guard),
		IsValid(GuardState) ? *UEnum::GetValueAsString(GuardState->GetGuardState()) : TEXT("Invalid"),
		IsValid(GuardState) ? FVector::Dist(GuardState->GetStateFocusLocation(), Event.WorldLocation) : -1.0f,
		bClientStateReplicated ? TEXT("true") : TEXT("false"), ClientFocusDistance,
		IsValid(GameState) ? *UEnum::GetValueAsString(GameState->GetAlertLevel()) : TEXT("Invalid"), IsValid(GameState) ? GameState->GetAlertRevision() : INDEX_NONE,
		State->InitialAlertRevision);
}

class FFootstepWaitCommand final : public IAutomationLatentCommand
{
  public:
	FFootstepWaitCommand(FAutomationTestBase* InTest, const TSharedRef<FFootstepAutomationState>& InState, FString InDescription, TFunction<bool()> InPredicate,
		const double InTimeoutSeconds, TFunction<FString()> InDiagnostic = {})
		: Test(InTest), State(InState), Description(MoveTemp(InDescription)), Predicate(MoveTemp(InPredicate)), TimeoutSeconds(InTimeoutSeconds), Diagnostic(MoveTemp(InDiagnostic))
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
			Test->AddInfo(FString::Printf(TEXT("W7-008 wait passed: %s"), *Description));
			return true;
		}
		if (FPlatformTime::Seconds() - StartTimeSeconds < TimeoutSeconds)
		{
			return false;
		}

		const FString DiagnosticText = Diagnostic ? Diagnostic() : FString();
		Test->AddError(FString::Printf(TEXT("W7-008 wait timed out: %s%s%s"), *Description, DiagnosticText.IsEmpty() ? TEXT("") : TEXT(" | "), *DiagnosticText));
		State->bAborted = true;
		return true;
	}

  private:
	FAutomationTestBase* Test = nullptr;
	TSharedRef<FFootstepAutomationState> State;
	FString Description;
	TFunction<bool()> Predicate;
	double TimeoutSeconds = 0.0;
	double StartTimeSeconds = 0.0;
	TFunction<FString()> Diagnostic;
};

class FFootstepActionCommand final : public IAutomationLatentCommand
{
  public:
	FFootstepActionCommand(FAutomationTestBase* InTest, const TSharedRef<FFootstepAutomationState>& InState, FString InDescription, TFunction<bool()> InAction,
		const bool bInRunAfterAbort = false)
		: Test(InTest), State(InState), Description(MoveTemp(InDescription)), Action(MoveTemp(InAction)), bRunAfterAbort(bInRunAfterAbort)
	{
	}

	virtual bool Update() override
	{
		if (State->bAborted && !bRunAfterAbort)
		{
			return true;
		}
		if (!Action())
		{
			Test->AddError(FString::Printf(TEXT("W7-008 action failed: %s"), *Description));
			State->bAborted = true;
		}
		else
		{
			Test->AddInfo(FString::Printf(TEXT("W7-008 action passed: %s"), *Description));
		}
		return true;
	}

  private:
	FAutomationTestBase* Test = nullptr;
	TSharedRef<FFootstepAutomationState> State;
	FString Description;
	TFunction<bool()> Action;
	bool bRunAfterAbort = false;
};

class FDriveMoveInputCommand final : public IAutomationLatentCommand
{
  public:
	FDriveMoveInputCommand(FAutomationTestBase* InTest, const TSharedRef<FFootstepAutomationState>& InState, const bool bInSprint, const double InDurationSeconds)
		: Test(InTest), State(InState), bSprint(bInSprint), DurationSeconds(InDurationSeconds)
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

		AHeistPlayerController* OwningController = GetOwningControllerById(FootstepPlayerId);
		UEnhancedPlayerInput* EnhancedPlayerInput = IsValid(OwningController) ? Cast<UEnhancedPlayerInput>(OwningController->PlayerInput) : nullptr;
		UInputAction* MoveAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Blueprints/Player/Input/IA_Move.IA_Move"));
		if (!IsValid(EnhancedPlayerInput) || !IsValid(MoveAction))
		{
			Test->AddError(TEXT("W7-008 move injection failed: owning EnhancedPlayerInput or IA_Move is missing."));
			State->bAborted = true;
			return true;
		}

		const FVector2D MovementInput = bSprint ? FVector2D(0.0f, -1.0f) : FVector2D(0.0f, 1.0f);
		EnhancedPlayerInput->InjectInputForAction(MoveAction, FInputActionValue(MovementInput));
		if (FPlatformTime::Seconds() - StartTimeSeconds < DurationSeconds)
		{
			return false;
		}

		Test->AddInfo(FString::Printf(TEXT("W7-008 client movement input injected: PlayerId=%d Pace=%s Duration=%.2f"), FootstepPlayerId,
			bSprint ? TEXT("Sprint") : TEXT("Walk"), DurationSeconds));
		return true;
	}

  private:
	FAutomationTestBase* Test = nullptr;
	TSharedRef<FFootstepAutomationState> State;
	bool bSprint = false;
	double DurationSeconds = 0.0;
	double StartTimeSeconds = 0.0;
};

bool EnqueueTwoPlayerFootstepScenario(FAutomationTestBase* Test)
{
	const TSharedRef<FFootstepAutomationState> State = MakeShared<FFootstepAutomationState>();
	Test->AddCommand(new FEditorLoadMap(TEXT("/Game/Maps/M01_ClassicalPrototype")));
	Test->AddCommand(new FFootstepActionCommand(Test, State, TEXT("configure two-player listen-server PIE"), [State]()
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
	Test->AddCommand(new FFootstepWaitCommand(Test, State, TEXT("M01 two-player gameplay worlds"), []() { return AreTwoPlayerGameplayWorldsReady(); }, 60.0));
	Test->AddCommand(new FFootstepActionCommand(Test, State, TEXT("configure isolated production Guard noise reaction and capture"), [State]()
	{
		return ConfigureGuardsAndCapture(State);
	}));
	Test->AddCommand(new FFootstepWaitCommand(Test, State, TEXT("Walk 300 is authoritative before movement"), []()
	{
		const AHeistPlayerCharacter* ServerCharacter = GetServerCharacterById(FootstepPlayerId);
		return IsValid(ServerCharacter) && !ServerCharacter->IsSprinting() &&
			FMath::IsNearlyEqual(ServerCharacter->GetCharacterMovement()->MaxWalkSpeed, 300.0f);
	}, 10.0));
	Test->AddCommand(new FFootstepActionCommand(Test, State, TEXT("arm Player 2 Walk server-location and server-time capture"), [State]()
	{
		return ArmFootstepCapture(State, EFootstepCapturePhase::Walk);
	}));
	Test->AddCommand(new FDriveMoveInputCommand(Test, State, false, 1.0));
	Test->AddCommand(new FFootstepWaitCommand(Test, State, TEXT("Walk input emits a Player 2-local 500 cm Footstep and one Guard state replicates as InvestigateNoise"), [State]()
	{
		return IsFootstepEvidenceReady(State, false) && IsAlertUnchanged(State);
	}, 10.0, [State]() { return DescribeFootstepEvidence(State, false); }));
	Test->AddCommand(new FFootstepWaitCommand(Test, State, TEXT("Walk movement settles before Sprint phase"), []()
	{
		const AHeistPlayerCharacter* ServerCharacter = GetServerCharacterById(FootstepPlayerId);
		const UCharacterMovementComponent* Movement = IsValid(ServerCharacter) ? ServerCharacter->GetCharacterMovement() : nullptr;
		return IsValid(Movement) && Movement->Velocity.Size2D() < 5.0f;
	}, 5.0));
	Test->AddCommand(new FFootstepWaitCommand(Test, State, TEXT("production Footstep refresh interval opens before Sprint movement"), [State]()
	{
		const AHeistGameState* GameState = State->BoundGameState.Get();
		return IsValid(GameState) && State->WalkEvent.ServerTimeSeconds > 0.0f &&
			GameState->GetServerWorldTimeSeconds() - State->WalkEvent.ServerTimeSeconds >= State->ExpectedSprintRefreshInterval;
	}, 5.0, [State]()
	{
		const AHeistGameState* GameState = State->BoundGameState.Get();
		const float Elapsed = IsValid(GameState) ? GameState->GetServerWorldTimeSeconds() - State->WalkEvent.ServerTimeSeconds : -1.0f;
		return FString::Printf(TEXT("ElapsedSinceWalkFootstep=%.2f RequiredSprintRefresh=%.2f"), Elapsed, State->ExpectedSprintRefreshInterval);
	}));
	Test->AddCommand(new FFootstepActionCommand(Test, State, TEXT("reset Guard candidate and request Sprint through owning client RPC"), [State]()
	{
		AHeistPlayerController* OwningController = GetOwningControllerById(FootstepPlayerId);
		AHeistPlayerCharacter* ServerCharacter = GetServerCharacterById(FootstepPlayerId);
		if (!PositionGuardForFootstep(State) || !IsValid(OwningController) || !IsValid(ServerCharacter))
		{
			return false;
		}
		if (!ArmFootstepCapture(State, EFootstepCapturePhase::Sprint))
		{
			return false;
		}
		OwningController->RequestSetSprintRequested(true);
		return true;
	}));
	Test->AddCommand(new FFootstepWaitCommand(Test, State, TEXT("Sprint 600 is authoritative before movement"), []()
	{
		const AHeistPlayerCharacter* ServerCharacter = GetServerCharacterById(FootstepPlayerId);
		return IsValid(ServerCharacter) && ServerCharacter->IsSprinting() &&
			FMath::IsNearlyEqual(ServerCharacter->GetCharacterMovement()->MaxWalkSpeed, 600.0f);
	}, 10.0));
	Test->AddCommand(new FDriveMoveInputCommand(Test, State, true, 1.0));
	Test->AddCommand(new FFootstepWaitCommand(Test, State, TEXT("Sprint input emits a Player 2-local 1000 cm Footstep and one Guard state replicates as InvestigateNoise"), [State]()
	{
		return IsFootstepEvidenceReady(State, true) && IsAlertUnchanged(State);
	}, 10.0, [State]() { return DescribeFootstepEvidence(State, true); }));
	Test->AddCommand(new FFootstepActionCommand(Test, State, TEXT("release Sprint and record W7-008 evidence"), [Test, State]()
	{
		AHeistPlayerController* OwningController = GetOwningControllerById(FootstepPlayerId);
		const AHeistGameState* GameState = State->BoundGameState.Get();
		if (!IsValid(OwningController) || !IsValid(GameState) || !IsAlertUnchanged(State))
		{
			return false;
		}
		OwningController->RequestSetSprintRequested(false);
		Test->AddInfo(FString::Printf(
			TEXT("W7-008 2P footstep gate: ClientPlayerId=%d Guard=%s WalkRadius=%.0f WalkCaptureTime=%.2f WalkEventTime=%.2f WalkSegmentDistance=%.1f "
				 "WalkReactedGuards=%d WalkClientReplicated=%s WalkServerDistance=%.1f SprintRadius=%.0f SprintCaptureTime=%.2f SprintEventTime=%.2f SprintSegmentDistance=%.1f "
				 "SprintRefreshInterval=%.2f SprintReactedGuards=%d SprintClientReplicated=%s SprintServerDistance=%.1f AlertLevel=%s AlertRevision=%d DirectAlertMutation=false "
				 "Proof=AuthoritativeEventLocation+ServerGuardState+ClientReplicatedGuardState NavMovementAsserted=false Result=PASS"),
			FootstepPlayerId, *State->SelectedGuardStableName.ToString(), State->WalkEvent.Radius, State->WalkCaptureStartServerTime, State->WalkEvent.ServerTimeSeconds,
			State->WalkEventSegmentDistance, State->WalkReactedGuardCount, State->bWalkClientStateReplicated ? TEXT("true") : TEXT("false"), State->WalkServerDistance,
			State->SprintEvent.Radius, State->SprintCaptureStartServerTime, State->SprintEvent.ServerTimeSeconds, State->SprintEventSegmentDistance, State->ExpectedSprintRefreshInterval,
			State->SprintReactedGuardCount, State->bSprintClientStateReplicated ? TEXT("true") : TEXT("false"), State->SprintServerDistance,
			*UEnum::GetValueAsString(GameState->GetAlertLevel()), GameState->GetAlertRevision()));
		Test->AddInfo(TEXT("W7-008 automated scope: authoritative footstep causality and server/client InvestigateNoise replication are verified; NullRHI NavMesh movement and visible presentation are intentionally not asserted."));
		return true;
	}));
	Test->AddCommand(new FFootstepActionCommand(Test, State, TEXT("unbind SoundPing capture"), [State]()
	{
		if (State->BoundGameState.IsValid() && State->SoundPingHandle.IsValid())
		{
			State->BoundGameState->GetSoundPingEventReportedDelegate().Remove(State->SoundPingHandle);
		}
		State->SoundPingHandle.Reset();
		State->BoundGameState.Reset();
		return true;
	}, true));
	Test->AddCommand(new FEndPlayMapCommand());
	Test->AddCommand(new FWaitLatentCommand(1.0f));
	Test->AddCommand(new FFootstepActionCommand(Test, State, TEXT("restore editor play settings"), [State]()
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistWeek7FootstepGuardInvestigationTwoPlayerTest, "ProjectMuseumHeist.W7.FootstepGuardInvestigationTwoPlayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistWeek7FootstepGuardInvestigationTwoPlayerTest::RunTest(const FString& Parameters)
{
	return HeistWeek7FootstepTest::EnqueueTwoPlayerFootstepScenario(this);
}

#endif
