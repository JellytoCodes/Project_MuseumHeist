#include "AI/HeistGuardAIController.h"

#include "AI/HeistGuardCharacter.h"
#include "AI/HeistGuardStateComponent.h"
#include "AI/HeistPatrolPathComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Components/StateTreeAIComponent.h"
#include "Core/HeistGameplayTags.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Inventory/HeistItemDataTypes.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"
#include "World/AI/HeistGuardWaypoint.h"
#include "World/Actors/Loot/HeistDisplayCaseActor.h"

#pragma region Construction

AHeistGuardAIController::AHeistGuardAIController()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	GuardPerceptionComponent =
		CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("GuardPerceptionComponent"));
	GuardSightConfig =
		CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("GuardSightConfig"));
	SetPerceptionComponent(*GuardPerceptionComponent);

	GuardStateTreeComponent =
		CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("GuardStateTreeComponent"));
	GuardStateTreeComponent->SetStartLogicAutomatically(false);
	BrainComponent = GuardStateTreeComponent;
}

#pragma endregion

#pragma region Lifecycle

void AHeistGuardAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	AHeistGuardCharacter* GuardCharacter = CastChecked<AHeistGuardCharacter>(InPawn);
	UHeistGuardStateComponent* GuardStateComponent =
		GuardCharacter->GetGuardStateComponent();
	checkf(IsValid(GuardStateComponent), TEXT("HeistGuardAIController requires GuardStateComponent."));
	checkf(IsValid(GuardPerceptionComponent), TEXT("HeistGuardAIController requires GuardPerceptionComponent."));
	checkf(IsValid(GuardSightConfig), TEXT("HeistGuardAIController requires GuardSightConfig."));

	GuardStateComponent->GetGuardStateChangedDelegate().AddUObject(
		this,
		&AHeistGuardAIController::HandleGuardStateChanged);
	GuardPerceptionComponent->OnTargetPerceptionUpdated.AddUniqueDynamic(
		this,
		&AHeistGuardAIController::HandleTargetPerceptionUpdated);

	if (GuardCharacter->HasResolvedGuardProfile())
	{
		ConfigurePerceptionFromGuardProfile(GuardCharacter->GetGuardProfile());
	}

	if (HasAuthority() && bStartStateTreeAutomatically)
	{
		GuardStateTreeComponent->StartLogic();
		SendGuardStateTreeEvent(GuardStateComponent->GetGuardState());
	}

	HandleGuardStateChanged(
		GuardStateComponent->GetGuardState(),
		GuardStateComponent->GetGuardState());
}

void AHeistGuardAIController::OnUnPossess()
{
	ClearGuardMovementTimer();
	bHasActiveGuardMove = false;
	StopMovement();
	ClearDetectionGrace(TEXT("GuardUnpossessed"));
	ClearSightValidationTimer();
	GuardPerceptionComponent->OnTargetPerceptionUpdated.RemoveDynamic(
		this,
		&AHeistGuardAIController::HandleTargetPerceptionUpdated);

	if (AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn()))
	{
		if (UHeistGuardStateComponent* GuardStateComponent =
			GuardCharacter->GetGuardStateComponent())
		{
			GuardStateComponent->GetGuardStateChangedDelegate().RemoveAll(this);
		}
	}

	if (IsValid(GuardStateTreeComponent) && GuardStateTreeComponent->IsRunning())
	{
		GuardStateTreeComponent->StopLogic(TEXT("Guard unpossessed"));
	}

	Super::OnUnPossess();
}

void AHeistGuardAIController::OnMoveCompleted(
	const FAIRequestID RequestID,
	const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	if (!HasAuthority())
	{
		return;
	}

	AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	UHeistGuardStateComponent* GuardStateComponent =
		IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
	if (!IsValid(GuardStateComponent))
	{
		return;
	}

	const EHeistGuardState CurrentState = GuardStateComponent->GetGuardState();
	if (!bHasActiveGuardMove || ActiveMovementState != CurrentState)
	{
		return;
	}
	bHasActiveGuardMove = false;

	if (CurrentState == EHeistGuardState::Patrol)
	{
		UHeistPatrolPathComponent* PatrolPath = GuardCharacter->GetPatrolPathComponent();
		AHeistGuardWaypoint* Waypoint =
			IsValid(PatrolPath) ? PatrolPath->GetCurrentWaypoint() : nullptr;
		UHeistDebugFunctionLibrary::DebugGuardMovement(
			this,
			GuardCharacter,
			CurrentState,
			TEXT("Completed"),
			IsValid(Waypoint) ? Waypoint->GetActorLocation() : GuardCharacter->GetActorLocation(),
			IsValid(PatrolPath) ? PatrolPath->GetCurrentWaypointIndex() : INDEX_NONE,
			IsValid(PatrolPath) ? PatrolPath->GetWaypointCount() : 0,
			Result.IsSuccess() ? TEXT("Success") : TEXT("Failed"));
		if (Result.IsSuccess())
		{
			HandlePatrolWaypointReached();
		}
		return;
	}

	if (CurrentState == EHeistGuardState::InvestigateNoise)
	{
		const FVector FocusLocation = GuardStateComponent->GetStateFocusLocation();
		UHeistDebugFunctionLibrary::DebugGuardMovement(
			this,
			GuardCharacter,
			CurrentState,
			TEXT("Completed"),
			FocusLocation,
			INDEX_NONE,
			0,
			Result.IsSuccess() ? TEXT("Success") : TEXT("Failed"));
		if (Result.IsSuccess())
		{
			StartInvestigateConfirmation();
		}
		else
		{
			GuardStateComponent->EnterPatrol();
		}
		return;
	}

	if (CurrentState == EHeistGuardState::ChasePlayer)
	{
		const FVector LastKnownLocation = GuardStateComponent->GetStateFocusLocation();
		UHeistDebugFunctionLibrary::DebugGuardMovement(
			this,
			GuardCharacter,
			CurrentState,
			TEXT("Completed"),
			LastKnownLocation,
			INDEX_NONE,
			0,
			Result.IsSuccess() ? TEXT("Success") : TEXT("Failed"));
		if (!Result.IsSuccess())
		{
			GuardStateComponent->EnterSearchLastKnownLocation(LastKnownLocation);
		}
		return;
	}

	if (CurrentState == EHeistGuardState::SearchLastKnownLocation)
	{
		const FVector SearchLocation = GuardStateComponent->GetStateFocusLocation();
		UHeistDebugFunctionLibrary::DebugGuardMovement(
			this,
			GuardCharacter,
			CurrentState,
			TEXT("Completed"),
			SearchLocation,
			INDEX_NONE,
			0,
			Result.IsSuccess() ? TEXT("Success") : TEXT("Failed"));
		if (Result.IsSuccess())
		{
			StartSearchTimer();
		}
		else
		{
			GuardStateComponent->EnterReturnToPatrol();
		}
		return;
	}

	if (CurrentState == EHeistGuardState::ReturnToPatrol)
	{
		const UHeistPatrolPathComponent* PatrolPath = GuardCharacter->GetPatrolPathComponent();
		const AHeistGuardWaypoint* Waypoint =
			IsValid(PatrolPath) ? PatrolPath->GetCurrentWaypoint() : nullptr;
		UHeistDebugFunctionLibrary::DebugGuardMovement(
			this,
			GuardCharacter,
			CurrentState,
			TEXT("Completed"),
			IsValid(Waypoint) ? Waypoint->GetActorLocation() : GuardCharacter->GetActorLocation(),
			IsValid(PatrolPath) ? PatrolPath->GetCurrentWaypointIndex() : INDEX_NONE,
			IsValid(PatrolPath) ? PatrolPath->GetWaypointCount() : 0,
			Result.IsSuccess() ? TEXT("Success") : TEXT("Failed"));
		GuardStateComponent->EnterPatrol();
	}
}

#pragma endregion

#pragma region Movement

void AHeistGuardAIController::BeginPatrolMovement()
{
	AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	UHeistPatrolPathComponent* PatrolPath =
		IsValid(GuardCharacter) ? GuardCharacter->GetPatrolPathComponent() : nullptr;
	if (!HasAuthority() || !IsValid(PatrolPath))
	{
		return;
	}

	if (PatrolPath->GetWaypointCount() == 0)
	{
		PatrolPath->ResolvePatrolPath();
		UHeistDebugFunctionLibrary::DebugGuardPatrolPathResolved(
			this,
			GuardCharacter,
			PatrolPath->GetPatrolRouteId(),
			PatrolPath->GetWaypointCount());
	}

	AHeistGuardWaypoint* Waypoint = PatrolPath->GetCurrentWaypoint();
	if (!IsValid(Waypoint))
	{
		return;
	}

	const EPathFollowingRequestResult::Type MoveResult = MoveToActor(
		Waypoint,
		PatrolPath->GetAcceptanceRadius(),
		true,
		true,
		false,
		nullptr,
		true);
	UHeistDebugFunctionLibrary::DebugGuardMovement(
		this,
		GuardCharacter,
		EHeistGuardState::Patrol,
		TEXT("Requested"),
		Waypoint->GetActorLocation(),
		PatrolPath->GetCurrentWaypointIndex(),
		PatrolPath->GetWaypointCount(),
		MoveResult == EPathFollowingRequestResult::RequestSuccessful
			? TEXT("RequestSuccessful")
			: MoveResult == EPathFollowingRequestResult::AlreadyAtGoal
				? TEXT("AlreadyAtGoal")
				: TEXT("Failed"));

	if (MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		HandlePatrolWaypointReached();
	}
	else if (MoveResult == EPathFollowingRequestResult::RequestSuccessful)
	{
		ActiveMovementState = EHeistGuardState::Patrol;
		bHasActiveGuardMove = true;
	}
}

void AHeistGuardAIController::HandlePatrolWaypointReached()
{
	AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	UHeistPatrolPathComponent* PatrolPath =
		IsValid(GuardCharacter) ? GuardCharacter->GetPatrolPathComponent() : nullptr;
	if (!IsValid(PatrolPath))
	{
		return;
	}

	ClearGuardMovementTimer();
	const float WaitDuration = PatrolPath->GetWaypointWaitDuration();
	if (WaitDuration <= 0.0f)
	{
		AdvancePatrolWaypoint();
		return;
	}

	GetWorldTimerManager().SetTimer(
		GuardMovementTimerHandle,
		this,
		&AHeistGuardAIController::AdvancePatrolWaypoint,
		WaitDuration,
		false);
}

void AHeistGuardAIController::AdvancePatrolWaypoint()
{
	AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	UHeistGuardStateComponent* GuardStateComponent =
		IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
	UHeistPatrolPathComponent* PatrolPath =
		IsValid(GuardCharacter) ? GuardCharacter->GetPatrolPathComponent() : nullptr;
	if (!IsValid(GuardStateComponent)
		|| GuardStateComponent->GetGuardState() != EHeistGuardState::Patrol
		|| !IsValid(PatrolPath))
	{
		return;
	}

	if (!IsValid(PatrolPath->AdvanceWaypoint()))
	{
		return;
	}
	BeginPatrolMovement();
}

void AHeistGuardAIController::BeginInvestigateMovement()
{
	AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	UHeistGuardStateComponent* GuardStateComponent =
		IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
	if (!HasAuthority() || !IsValid(GuardStateComponent))
	{
		return;
	}

	const FVector FocusLocation = GuardStateComponent->GetStateFocusLocation();
	const UHeistPatrolPathComponent* PatrolPath = GuardCharacter->GetPatrolPathComponent();
	const float AcceptanceRadius =
		IsValid(PatrolPath) ? PatrolPath->GetAcceptanceRadius() : 75.0f;
	const EPathFollowingRequestResult::Type MoveResult = MoveToLocation(
		FocusLocation,
		AcceptanceRadius,
		true,
		true,
		true,
		false,
		nullptr,
		true);
	UHeistDebugFunctionLibrary::DebugGuardMovement(
		this,
		GuardCharacter,
		EHeistGuardState::InvestigateNoise,
		TEXT("Requested"),
		FocusLocation,
		INDEX_NONE,
		0,
		MoveResult == EPathFollowingRequestResult::RequestSuccessful
			? TEXT("RequestSuccessful")
			: MoveResult == EPathFollowingRequestResult::AlreadyAtGoal
				? TEXT("AlreadyAtGoal")
				: TEXT("Failed"));

	if (MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		StartInvestigateConfirmation();
	}
	else if (MoveResult == EPathFollowingRequestResult::RequestSuccessful)
	{
		ActiveMovementState = EHeistGuardState::InvestigateNoise;
		bHasActiveGuardMove = true;
	}
	else if (MoveResult == EPathFollowingRequestResult::Failed)
	{
		GuardStateComponent->EnterPatrol();
	}
}

void AHeistGuardAIController::StartInvestigateConfirmation()
{
	AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	UHeistGuardStateComponent* GuardStateComponent =
		IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
	if (!IsValid(GuardStateComponent)
		|| !GuardStateComponent->StartInvestigateConfirmationTimer())
	{
		return;
	}

	UHeistDebugFunctionLibrary::DebugGuardInvestigateConfirmationStarted(
		this,
		GuardCharacter,
		GuardStateComponent->GetStateFocusLocation(),
		GuardStateComponent->GetInvestigateConfirmationDuration());
}

void AHeistGuardAIController::BeginChaseMovement()
{
	AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	UHeistGuardStateComponent* GuardStateComponent =
		IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
	AActor* ChaseTarget =
		IsValid(GuardStateComponent) ? GuardStateComponent->GetChaseTarget() : nullptr;
	if (!HasAuthority() || !IsValid(GuardStateComponent) || !IsValid(ChaseTarget))
	{
		if (IsValid(GuardStateComponent))
		{
			GuardStateComponent->EnterSearchLastKnownLocation(
				GuardStateComponent->GetStateFocusLocation());
		}
		return;
	}

	const UHeistPatrolPathComponent* PatrolPath = GuardCharacter->GetPatrolPathComponent();
	const float AcceptanceRadius =
		IsValid(PatrolPath) ? PatrolPath->GetAcceptanceRadius() : 75.0f;
	const EPathFollowingRequestResult::Type MoveResult = MoveToActor(
		ChaseTarget,
		AcceptanceRadius,
		true,
		true,
		false,
		nullptr,
		true);
	UHeistDebugFunctionLibrary::DebugGuardMovement(
		this,
		GuardCharacter,
		EHeistGuardState::ChasePlayer,
		TEXT("Requested"),
		GuardStateComponent->GetStateFocusLocation(),
		INDEX_NONE,
		0,
		MoveResult == EPathFollowingRequestResult::RequestSuccessful
			? TEXT("RequestSuccessful")
			: MoveResult == EPathFollowingRequestResult::AlreadyAtGoal
				? TEXT("AlreadyAtGoal")
				: TEXT("Failed"));

	if (MoveResult == EPathFollowingRequestResult::RequestSuccessful)
	{
		ActiveMovementState = EHeistGuardState::ChasePlayer;
		bHasActiveGuardMove = true;
	}
	else if (MoveResult == EPathFollowingRequestResult::Failed)
	{
		GuardStateComponent->EnterSearchLastKnownLocation(
			GuardStateComponent->GetStateFocusLocation());
	}
}

void AHeistGuardAIController::BeginSearchMovement()
{
	AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	UHeistGuardStateComponent* GuardStateComponent =
		IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
	if (!HasAuthority() || !IsValid(GuardStateComponent))
	{
		return;
	}

	const FVector SearchLocation = GuardStateComponent->GetStateFocusLocation();
	const UHeistPatrolPathComponent* PatrolPath = GuardCharacter->GetPatrolPathComponent();
	const float AcceptanceRadius =
		IsValid(PatrolPath) ? PatrolPath->GetAcceptanceRadius() : 75.0f;
	const EPathFollowingRequestResult::Type MoveResult = MoveToLocation(
		SearchLocation,
		AcceptanceRadius,
		true,
		true,
		true,
		false,
		nullptr,
		true);
	UHeistDebugFunctionLibrary::DebugGuardMovement(
		this,
		GuardCharacter,
		EHeistGuardState::SearchLastKnownLocation,
		TEXT("Requested"),
		SearchLocation,
		INDEX_NONE,
		0,
		MoveResult == EPathFollowingRequestResult::RequestSuccessful
			? TEXT("RequestSuccessful")
			: MoveResult == EPathFollowingRequestResult::AlreadyAtGoal
				? TEXT("AlreadyAtGoal")
				: TEXT("Failed"));

	if (MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		StartSearchTimer();
	}
	else if (MoveResult == EPathFollowingRequestResult::RequestSuccessful)
	{
		ActiveMovementState = EHeistGuardState::SearchLastKnownLocation;
		bHasActiveGuardMove = true;
	}
	else
	{
		GuardStateComponent->EnterReturnToPatrol();
	}
}

void AHeistGuardAIController::StartSearchTimer()
{
	AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	UHeistGuardStateComponent* GuardStateComponent =
		IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
	if (!IsValid(GuardStateComponent) || !GuardStateComponent->StartSearchTimer())
	{
		if (IsValid(GuardStateComponent))
		{
			GuardStateComponent->EnterReturnToPatrol();
		}
		return;
	}

	UHeistDebugFunctionLibrary::DebugGuardSearchTimerStarted(
		this,
		GuardCharacter,
		GuardStateComponent->GetStateFocusLocation(),
		GuardStateComponent->GetSearchDuration());
}

void AHeistGuardAIController::BeginReturnToPatrolMovement()
{
	AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	UHeistGuardStateComponent* GuardStateComponent =
		IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
	UHeistPatrolPathComponent* PatrolPath =
		IsValid(GuardCharacter) ? GuardCharacter->GetPatrolPathComponent() : nullptr;
	if (!HasAuthority() || !IsValid(GuardStateComponent) || !IsValid(PatrolPath))
	{
		return;
	}

	if (PatrolPath->GetWaypointCount() == 0)
	{
		PatrolPath->ResolvePatrolPath();
	}

	AHeistGuardWaypoint* Waypoint = PatrolPath->GetCurrentWaypoint();
	if (!IsValid(Waypoint))
	{
		UHeistDebugFunctionLibrary::DebugGuardMovement(
			this,
			GuardCharacter,
			EHeistGuardState::ReturnToPatrol,
			TEXT("Requested"),
			GuardCharacter->GetActorLocation(),
			INDEX_NONE,
			PatrolPath->GetWaypointCount(),
			TEXT("NoWaypoint"));
		GuardStateComponent->EnterPatrol();
		return;
	}

	const EPathFollowingRequestResult::Type MoveResult = MoveToActor(
		Waypoint,
		PatrolPath->GetAcceptanceRadius(),
		true,
		true,
		false,
		nullptr,
		true);
	UHeistDebugFunctionLibrary::DebugGuardMovement(
		this,
		GuardCharacter,
		EHeistGuardState::ReturnToPatrol,
		TEXT("Requested"),
		Waypoint->GetActorLocation(),
		PatrolPath->GetCurrentWaypointIndex(),
		PatrolPath->GetWaypointCount(),
		MoveResult == EPathFollowingRequestResult::RequestSuccessful
			? TEXT("RequestSuccessful")
			: MoveResult == EPathFollowingRequestResult::AlreadyAtGoal
				? TEXT("AlreadyAtGoal")
				: TEXT("Failed"));

	if (MoveResult == EPathFollowingRequestResult::AlreadyAtGoal
		|| MoveResult == EPathFollowingRequestResult::Failed)
	{
		GuardStateComponent->EnterPatrol();
	}
	else
	{
		ActiveMovementState = EHeistGuardState::ReturnToPatrol;
		bHasActiveGuardMove = true;
	}
}

void AHeistGuardAIController::ClearGuardMovementTimer()
{
	GetWorldTimerManager().ClearTimer(GuardMovementTimerHandle);
}

#pragma endregion

#pragma region Perception

void AHeistGuardAIController::ConfigurePerceptionFromGuardProfile(
	const FHeistGuardDataRow& GuardData)
{
	if (!HasAuthority()
		|| !IsValid(GuardPerceptionComponent)
		|| !IsValid(GuardSightConfig))
	{
		return;
	}

	SightRadius = FMath::Max(0.0f, GuardData.SightRadius);
	AggroResetDistance = FMath::Max(SightRadius, GuardData.AggroResetDistance);
	DefaultSightHalfAngle = FMath::Clamp(GuardData.SightAngle * 0.5f, 0.0f, 180.0f);
	InvestigateSightHalfAngle =
		FMath::Clamp(GuardData.InvestigateSightAngle * 0.5f, 0.0f, 180.0f);
	EyeHeight = FMath::Max(0.0f, GuardData.EyeHeight);
	DetectionGrace = FMath::Max(0.0f, GuardData.DetectionGrace);
	SightUpdateInterval = FMath::Max(0.01f, GuardData.SightUpdateInterval);
	bDoorsBlockSight = GuardData.bDoorsBlockSight;
	bDisplayCasesBlockSight = GuardData.bDisplayCasesBlockSight;
	DoorOccluderTag = GuardData.DoorOccluderTag;

	GuardSightConfig->SightRadius = SightRadius;
	GuardSightConfig->LoseSightRadius = AggroResetDistance;
	GuardSightConfig->PeripheralVisionAngleDegrees = DefaultSightHalfAngle;
	GuardSightConfig->DetectionByAffiliation.bDetectEnemies = true;
	GuardSightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	GuardSightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	GuardPerceptionComponent->ConfigureSense(*GuardSightConfig);
	GuardPerceptionComponent->SetDominantSense(
		GuardSightConfig->GetSenseImplementation());
	bPerceptionConfigured = SightRadius > 0.0f;

	const AHeistGuardCharacter* GuardCharacter =
		Cast<AHeistGuardCharacter>(GetPawn());
	const UHeistGuardStateComponent* GuardStateComponent =
		IsValid(GuardCharacter)
			? GuardCharacter->GetGuardStateComponent()
			: nullptr;
	UpdateSightForGuardState(
		IsValid(GuardStateComponent)
			? GuardStateComponent->GetGuardState()
			: EHeistGuardState::Patrol);
	StartSightValidationTimer();

	UHeistDebugFunctionLibrary::DebugGuardPerceptionConfigured(
		this,
		GuardCharacter,
		SightRadius,
		AggroResetDistance,
		DefaultSightHalfAngle * 2.0f,
		InvestigateSightHalfAngle * 2.0f,
		EyeHeight,
		DetectionGrace,
		bDoorsBlockSight,
		bDisplayCasesBlockSight,
		DoorOccluderTag,
		SightUpdateInterval);
}

bool AHeistGuardAIController::DebugEvaluateSightTarget(AActor* TargetActor)
{
	const TCHAR* RejectReason = nullptr;
	AActor* BlockingActor = nullptr;
	const bool bCanSeeTarget = CanInitiallySeeTarget(
		TargetActor,
		RejectReason,
		BlockingActor);

	AHeistGuardCharacter* GuardCharacter =
		Cast<AHeistGuardCharacter>(GetPawn());

	UHeistDebugFunctionLibrary::DebugGuardSightEvaluated(
		this,
		GuardCharacter,
		TargetActor,
		bCanSeeTarget,
		RejectReason,
		BlockingActor);
	return bCanSeeTarget;
}

void AHeistGuardAIController::SetAutomaticSightEnabled(const bool bEnabled)
{
	if (!HasAuthority() || bAutomaticSightEnabled == bEnabled)
	{
		return;
	}

	bAutomaticSightEnabled = bEnabled;
	if (!bAutomaticSightEnabled)
	{
		ClearDetectionGrace(TEXT("AutomaticSightDisabled"));
		ClearSightValidationTimer();
		if (IsValid(GuardPerceptionComponent))
		{
			GuardPerceptionComponent->SetSenseEnabled(
				UAISense_Sight::StaticClass(),
				false);
			GuardPerceptionComponent->ForgetAll();
		}
		return;
	}

	const AHeistGuardCharacter* GuardCharacter =
		Cast<AHeistGuardCharacter>(GetPawn());
	const UHeistGuardStateComponent* GuardStateComponent =
		IsValid(GuardCharacter)
			? GuardCharacter->GetGuardStateComponent()
			: nullptr;
	UpdateSightForGuardState(
		IsValid(GuardStateComponent)
			? GuardStateComponent->GetGuardState()
			: EHeistGuardState::Patrol);
	StartSightValidationTimer();
}

bool AHeistGuardAIController::IsAutomaticSightEnabled() const
{
	return bAutomaticSightEnabled;
}

void AHeistGuardAIController::HandleTargetPerceptionUpdated(
	AActor* TargetActor,
	FAIStimulus Stimulus)
{
	if (!bAutomaticSightEnabled
		|| !HasAuthority()
		|| !IsValid(Cast<AHeistPlayerCharacter>(TargetActor)))
	{
		return;
	}

	AHeistGuardCharacter* GuardCharacter =
		Cast<AHeistGuardCharacter>(GetPawn());
	UHeistGuardStateComponent* GuardStateComponent =
		IsValid(GuardCharacter)
			? GuardCharacter->GetGuardStateComponent()
			: nullptr;
	if (!IsValid(GuardStateComponent))
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		if (GuardStateComponent->GetGuardState() == EHeistGuardState::ChasePlayer
			&& GuardStateComponent->GetChaseTarget() == TargetActor)
		{
			GuardStateComponent->RefreshChaseTargetLocation();
			return;
		}

		const TCHAR* RejectReason = nullptr;
		AActor* BlockingActor = nullptr;
		if (CanInitiallySeeTarget(
			TargetActor,
			RejectReason,
			BlockingActor))
		{
			BeginDetectionGrace(TargetActor);
			return;
		}

		UHeistDebugFunctionLibrary::DebugGuardSightEvaluated(
			this,
			GuardCharacter,
			TargetActor,
			false,
			RejectReason,
			BlockingActor);
	}
	else if (PendingSightTarget.Get() == TargetActor)
	{
		ClearDetectionGrace(TEXT("PerceptionLost"));
	}

	if (GuardStateComponent->GetGuardState() == EHeistGuardState::ChasePlayer
		&& GuardStateComponent->GetChaseTarget() == TargetActor)
	{
		const TCHAR* RejectReason = nullptr;
		AActor* BlockingActor = nullptr;
		if (!IsChaseTargetOccluded(
			TargetActor,
			RejectReason,
			BlockingActor))
		{
			return;
		}

		const FVector LastKnownLocation =
			GuardStateComponent->GetStateFocusLocation();
		if (GuardStateComponent->EnterSearchLastKnownLocation(LastKnownLocation))
		{
			UHeistDebugFunctionLibrary::DebugGuardSightTargetLost(
				this,
				GuardCharacter,
				TargetActor,
				LastKnownLocation,
				RejectReason);
		}
	}
}

bool AHeistGuardAIController::CanInitiallySeeTarget(
	const AActor* TargetActor,
	const TCHAR*& OutRejectReason,
	AActor*& OutBlockingActor) const
{
	OutRejectReason = nullptr;
	OutBlockingActor = nullptr;

	const AHeistGuardCharacter* GuardCharacter =
		Cast<AHeistGuardCharacter>(GetPawn());
	const UHeistGuardStateComponent* GuardStateComponent =
		IsValid(GuardCharacter)
			? GuardCharacter->GetGuardStateComponent()
			: nullptr;
	if (!HasAuthority()
		|| !bPerceptionConfigured
		|| !IsValid(GuardCharacter)
		|| !IsValid(GuardStateComponent)
		|| !IsValid(Cast<AHeistPlayerCharacter>(TargetActor)))
	{
		OutRejectReason = TEXT("InvalidSightContext");
		return false;
	}

	if (GuardStateComponent->GetGuardState() == EHeistGuardState::Disabled
		|| GuardStateComponent->GetGuardState() == EHeistGuardState::Stunned)
	{
		OutRejectReason = TEXT("GuardCannotSee");
		return false;
	}

	FVector EyeLocation;
	FRotator EyeRotation;
	GuardCharacter->GetActorEyesViewPoint(EyeLocation, EyeRotation);
	const FVector ToTarget = GetTargetSightLocation(TargetActor) - EyeLocation;
	const float Distance = ToTarget.Size();
	if (Distance > SightRadius)
	{
		OutRejectReason = TEXT("OutsideSightRadius");
		return false;
	}

	const float ActiveHalfAngle =
		GuardStateComponent->GetGuardState() == EHeistGuardState::InvestigateNoise
			|| GuardStateComponent->GetGuardState() == EHeistGuardState::SearchLastKnownLocation
			? InvestigateSightHalfAngle
			: DefaultSightHalfAngle;
	const FVector DirectionToTarget = ToTarget.GetSafeNormal();
	const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(ActiveHalfAngle));
	if (FVector::DotProduct(EyeRotation.Vector(), DirectionToTarget) < MinimumDot)
	{
		OutRejectReason = TEXT("OutsideSightAngle");
		return false;
	}

	return !IsChaseTargetOccluded(
		TargetActor,
		OutRejectReason,
		OutBlockingActor);
}

bool AHeistGuardAIController::IsChaseTargetOccluded(
	const AActor* TargetActor,
	const TCHAR*& OutRejectReason,
	AActor*& OutBlockingActor) const
{
	OutRejectReason = nullptr;
	OutBlockingActor = nullptr;

	const AHeistGuardCharacter* GuardCharacter =
		Cast<AHeistGuardCharacter>(GetPawn());
	if (!IsValid(GuardCharacter)
		|| !IsValid(TargetActor)
		|| !IsValid(GetWorld()))
	{
		OutRejectReason = TEXT("InvalidSightContext");
		return true;
	}

	FVector EyeLocation;
	FRotator EyeRotation;
	GuardCharacter->GetActorEyesViewPoint(EyeLocation, EyeRotation);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HeistGuardSight), false);
	QueryParams.AddIgnoredActor(GuardCharacter);
	QueryParams.AddIgnoredActor(TargetActor);
	TArray<FHitResult> HitResults;
	GetWorld()->LineTraceMultiByChannel(
		HitResults,
		EyeLocation,
		GetTargetSightLocation(TargetActor),
		ECC_Visibility,
		QueryParams);

	for (const FHitResult& HitResult : HitResults)
	{
		AActor* HitActor = HitResult.GetActor();
		if (IsValid(HitActor)
			&& HitActor->IsA<AHeistDisplayCaseActor>())
		{
			if (!bDisplayCasesBlockSight)
			{
				continue;
			}

			OutBlockingActor = HitActor;
			OutRejectReason = TEXT("BlockedByDisplayCase");
			return true;
		}

		if (IsDoorOccluder(HitResult))
		{
			if (!bDoorsBlockSight)
			{
				continue;
			}

			OutBlockingActor = HitActor;
			OutRejectReason = TEXT("BlockedByDoor");
			return true;
		}

		OutBlockingActor = HitActor;
		OutRejectReason = TEXT("BlockedByWorld");
		return true;
	}

	return false;
}

FVector AHeistGuardAIController::GetTargetSightLocation(
	const AActor* TargetActor) const
{
	if (!IsValid(TargetActor))
	{
		return FVector::ZeroVector;
	}

	FVector TargetEyeLocation;
	FRotator TargetEyeRotation;
	TargetActor->GetActorEyesViewPoint(TargetEyeLocation, TargetEyeRotation);
	return TargetEyeLocation;
}

bool AHeistGuardAIController::IsDoorOccluder(
	const FHitResult& HitResult) const
{
	if (DoorOccluderTag.IsNone())
	{
		return false;
	}

	const AActor* HitActor = HitResult.GetActor();
	const UPrimitiveComponent* HitComponent = HitResult.GetComponent();
	return (IsValid(HitActor) && HitActor->ActorHasTag(DoorOccluderTag))
		|| (IsValid(HitComponent) && HitComponent->ComponentHasTag(DoorOccluderTag));
}

void AHeistGuardAIController::TryAcquireSightTarget()
{
	AHeistGuardCharacter* GuardCharacter =
		Cast<AHeistGuardCharacter>(GetPawn());
	UHeistGuardStateComponent* GuardStateComponent =
		IsValid(GuardCharacter)
			? GuardCharacter->GetGuardStateComponent()
			: nullptr;
	if (!IsValid(GuardStateComponent)
		|| GuardStateComponent->GetGuardState() == EHeistGuardState::Disabled
		|| GuardStateComponent->GetGuardState() == EHeistGuardState::Stunned)
	{
		ClearDetectionGrace(TEXT("GuardCannotSee"));
		return;
	}

	AHeistPlayerCharacter* BestTarget = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<AHeistPlayerCharacter> It(GetWorld()); It; ++It)
	{
		AHeistPlayerCharacter* CandidateTarget = *It;
		const TCHAR* RejectReason = nullptr;
		AActor* BlockingActor = nullptr;
		if (!IsValid(CandidateTarget)
			|| !CanInitiallySeeTarget(
				CandidateTarget,
				RejectReason,
				BlockingActor))
		{
			continue;
		}

		const float CandidateDistanceSquared = FVector::DistSquared(
			GuardCharacter->GetActorLocation(),
			CandidateTarget->GetActorLocation());
		if (CandidateDistanceSquared < BestDistanceSquared)
		{
			BestTarget = CandidateTarget;
			BestDistanceSquared = CandidateDistanceSquared;
		}
	}

	if (!IsValid(BestTarget))
	{
		ClearDetectionGrace(TEXT("NoVisibleTarget"));
		return;
	}

	BeginDetectionGrace(BestTarget);
}

void AHeistGuardAIController::BeginDetectionGrace(AActor* TargetActor)
{
	if (!HasAuthority() || !IsValid(TargetActor))
	{
		return;
	}

	if (PendingSightTarget.Get() == TargetActor
		&& GetWorld()
		&& GetWorld()->GetTimerManager().IsTimerActive(DetectionGraceTimerHandle))
	{
		return;
	}

	ClearDetectionGrace(TEXT("TargetReplaced"));
	PendingSightTarget = TargetActor;
	if (DetectionGrace <= 0.0f)
	{
		CompleteDetectionGrace();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DetectionGraceTimerHandle,
			this,
			&AHeistGuardAIController::CompleteDetectionGrace,
			DetectionGrace,
			false);
		UHeistDebugFunctionLibrary::DebugGuardDetectionGraceStarted(
			this,
			GetPawn(),
			TargetActor,
			DetectionGrace);
	}
}

void AHeistGuardAIController::CompleteDetectionGrace()
{
	AActor* TargetActor = PendingSightTarget.Get();
	PendingSightTarget.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DetectionGraceTimerHandle);
	}

	const TCHAR* RejectReason = nullptr;
	AActor* BlockingActor = nullptr;
	if (!IsValid(TargetActor)
		|| !CanInitiallySeeTarget(
			TargetActor,
			RejectReason,
			BlockingActor))
	{
		UHeistDebugFunctionLibrary::DebugGuardDetectionGraceCancelled(
			this,
			GetPawn(),
			TargetActor,
			RejectReason ? RejectReason : TEXT("InvalidTarget"));
		return;
	}

	AHeistGuardCharacter* GuardCharacter =
		Cast<AHeistGuardCharacter>(GetPawn());
	UHeistGuardStateComponent* GuardStateComponent =
		IsValid(GuardCharacter)
			? GuardCharacter->GetGuardStateComponent()
			: nullptr;
	if (IsValid(GuardStateComponent)
		&& GuardStateComponent->EnterChasePlayer(TargetActor))
	{
		UHeistDebugFunctionLibrary::DebugGuardSightTargetAcquired(
			this,
			GuardCharacter,
			TargetActor);
	}
}

void AHeistGuardAIController::ClearDetectionGrace(const TCHAR* Reason)
{
	AActor* PreviousTarget = PendingSightTarget.Get();
	PendingSightTarget.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DetectionGraceTimerHandle);
	}

	if (IsValid(PreviousTarget))
	{
		UHeistDebugFunctionLibrary::DebugGuardDetectionGraceCancelled(
			this,
			GetPawn(),
			PreviousTarget,
			Reason ? Reason : TEXT("Cancelled"));
	}
}

void AHeistGuardAIController::ValidateCurrentChaseTarget()
{
	if (!bAutomaticSightEnabled || !HasAuthority())
	{
		return;
	}

	AHeistGuardCharacter* GuardCharacter =
		Cast<AHeistGuardCharacter>(GetPawn());
	UHeistGuardStateComponent* GuardStateComponent =
		IsValid(GuardCharacter)
			? GuardCharacter->GetGuardStateComponent()
			: nullptr;
	if (!IsValid(GuardStateComponent))
	{
		return;
	}

	if (GuardStateComponent->GetGuardState() != EHeistGuardState::ChasePlayer)
	{
		TryAcquireSightTarget();
		return;
	}

	AActor* ChaseTarget = GuardStateComponent->GetChaseTarget();
	if (!IsValid(ChaseTarget))
	{
		GuardStateComponent->EnterSearchLastKnownLocation(
			GuardStateComponent->GetStateFocusLocation());
		return;
	}

	const float Distance = FVector::Dist(
		GuardCharacter->GetActorLocation(),
		ChaseTarget->GetActorLocation());
	if (Distance > AggroResetDistance)
	{
		const FVector LastKnownLocation = GuardStateComponent->GetStateFocusLocation();
		GuardStateComponent->EnterSearchLastKnownLocation(LastKnownLocation);
		UHeistDebugFunctionLibrary::DebugGuardSightTargetLost(
			this,
			GuardCharacter,
			ChaseTarget,
			LastKnownLocation,
			TEXT("AggroResetDistance"));
		return;
	}

	const FVector LastKnownLocation =
		GuardStateComponent->GetStateFocusLocation();
	const TCHAR* RejectReason = nullptr;
	AActor* BlockingActor = nullptr;
	if (IsChaseTargetOccluded(
		ChaseTarget,
		RejectReason,
		BlockingActor))
	{
		GuardStateComponent->EnterSearchLastKnownLocation(LastKnownLocation);
		UHeistDebugFunctionLibrary::DebugGuardSightTargetLost(
			this,
			GuardCharacter,
			ChaseTarget,
			LastKnownLocation,
			RejectReason);
		return;
	}

	GuardStateComponent->RefreshChaseTargetLocation();
	const UHeistPatrolPathComponent* PatrolPath = GuardCharacter->GetPatrolPathComponent();
	const float AcceptanceRadius =
		IsValid(PatrolPath) ? PatrolPath->GetAcceptanceRadius() : 75.0f;
	if (!bHasActiveGuardMove
		&& FVector::DistSquared(
			GuardCharacter->GetActorLocation(),
			ChaseTarget->GetActorLocation()) > FMath::Square(AcceptanceRadius))
	{
		BeginChaseMovement();
	}
}

void AHeistGuardAIController::UpdateSightForGuardState(
	const EHeistGuardState NewState)
{
	if (!bPerceptionConfigured || !IsValid(GuardPerceptionComponent))
	{
		return;
	}

	const bool bSightEnabled =
		bAutomaticSightEnabled
		&& NewState != EHeistGuardState::Disabled
		&& NewState != EHeistGuardState::Stunned;
	GuardPerceptionComponent->SetSenseEnabled(
		UAISense_Sight::StaticClass(),
		bSightEnabled);
	if (!bSightEnabled)
	{
		ClearDetectionGrace(TEXT("GuardStateBlocksSight"));
		GuardPerceptionComponent->ForgetAll();
		return;
	}

	GuardSightConfig->PeripheralVisionAngleDegrees =
		NewState == EHeistGuardState::InvestigateNoise
			|| NewState == EHeistGuardState::SearchLastKnownLocation
			? InvestigateSightHalfAngle
			: DefaultSightHalfAngle;
	GuardPerceptionComponent->ConfigureSense(*GuardSightConfig);
	GuardPerceptionComponent->RequestStimuliListenerUpdate();
}

void AHeistGuardAIController::StartSightValidationTimer()
{
	ClearSightValidationTimer();
	if (bAutomaticSightEnabled)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				SightValidationTimerHandle,
				this,
				&AHeistGuardAIController::ValidateCurrentChaseTarget,
				SightUpdateInterval,
				true);
		}
	}
}

void AHeistGuardAIController::ClearSightValidationTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SightValidationTimerHandle);
	}
}

#pragma endregion

#pragma region StateTree

UStateTreeAIComponent* AHeistGuardAIController::GetGuardStateTreeComponent() const
{
	return GuardStateTreeComponent.Get();
}

void AHeistGuardAIController::HandleGuardStateChanged(
	const EHeistGuardState PreviousState,
	const EHeistGuardState NewState)
{
	if (!HasAuthority())
	{
		return;
	}

	ClearGuardMovementTimer();
	bHasActiveGuardMove = false;
	StopMovement();

	if (NewState == EHeistGuardState::Patrol)
	{
		BeginPatrolMovement();
	}
	else if (NewState == EHeistGuardState::InvestigateNoise)
	{
		BeginInvestigateMovement();
	}
	else if (NewState == EHeistGuardState::ChasePlayer)
	{
		BeginChaseMovement();
	}
	else if (NewState == EHeistGuardState::SearchLastKnownLocation)
	{
		BeginSearchMovement();
	}
	else if (NewState == EHeistGuardState::ReturnToPatrol)
	{
		BeginReturnToPatrolMovement();
	}

	UpdateSightForGuardState(NewState);
	SendGuardStateTreeEvent(NewState);
}

void AHeistGuardAIController::SendGuardStateTreeEvent(
	const EHeistGuardState NewState)
{
	if (!IsValid(GuardStateTreeComponent)
		|| !GuardStateTreeComponent->IsRunning())
	{
		return;
	}

	const FHeistGameplayTags& GameplayTags = FHeistGameplayTags::Get();
	FGameplayTag StateEventTag;
	switch (NewState)
	{
	case EHeistGuardState::Disabled:
		StateEventTag = GameplayTags.AI_State_Disabled;
		break;
	case EHeistGuardState::Stunned:
		StateEventTag = GameplayTags.AI_State_Stunned;
		break;
	case EHeistGuardState::Patrol:
		StateEventTag = GameplayTags.AI_State_Patrol;
		break;
	case EHeistGuardState::InvestigateNoise:
		StateEventTag = GameplayTags.AI_State_InvestigateNoise;
		break;
	case EHeistGuardState::ChasePlayer:
		StateEventTag = GameplayTags.AI_State_ChasePlayer;
		break;
	case EHeistGuardState::SearchLastKnownLocation:
		StateEventTag = GameplayTags.AI_State_SearchLastKnownLocation;
		break;
	case EHeistGuardState::ReturnToPatrol:
		StateEventTag = GameplayTags.AI_State_ReturnToPatrol;
		break;
	default:
		break;
	}

	if (StateEventTag.IsValid())
	{
		GuardStateTreeComponent->SendStateTreeEvent(StateEventTag);
		UHeistDebugFunctionLibrary::DebugGuardStateTreeEvent(
			this,
			GetPawn(),
			StateEventTag);
	}
}

#pragma endregion
