#include "AI/HeistGuardStateTreeTask.h"

#include "AI/HeistGuardAIController.h"
#include "AI/HeistGuardCharacter.h"
#include "AI/HeistGuardStateComponent.h"
#include "AI/HeistPatrolPathComponent.h"
#include "AIController.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTreeExecutionContext.h"
#include "World/AI/HeistGuardWaypoint.h"
#include "World/Actors/Loot/HeistPaintingDisplayCaseActor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HeistGuardStateTreeTask)

namespace
{
enum class EHeistGuardTaskPhase : uint8
{
	Idle,
	Moving,
	Waiting,
	AwaitingStateChange
};

AHeistGuardAIController* ResolveController(FStateTreeExecutionContext& Context)
{
	return Cast<AHeistGuardAIController>(Context.GetOwner());
}

AHeistGuardCharacter* ResolveGuardCharacter(const AHeistGuardAIController* Controller)
{
	return IsValid(Controller) ? Cast<AHeistGuardCharacter>(Controller->GetPawn()) : nullptr;
}

UHeistGuardStateComponent* ResolveGuardStateComponent(const AHeistGuardAIController* Controller)
{
	AHeistGuardCharacter* GuardCharacter = ResolveGuardCharacter(Controller);
	return IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
}

UHeistPatrolPathComponent* ResolvePatrolPath(const AHeistGuardAIController* Controller)
{
	AHeistGuardCharacter* GuardCharacter = ResolveGuardCharacter(Controller);
	return IsValid(GuardCharacter) ? GuardCharacter->GetPatrolPathComponent() : nullptr;
}

void ResetMove(FHeistGuardStateTreeTaskInstanceData& InstanceData)
{
	InstanceData.MoveRequestId = MAX_uint32;
	InstanceData.RequestResult = static_cast<uint8>(EPathFollowingRequestResult::Failed);
	InstanceData.bMoveFinished = false;
	InstanceData.bMoveSucceeded = false;
}

bool StartMove(FHeistGuardStateTreeTaskInstanceData& InstanceData, AHeistGuardAIController& Controller, AActor* TargetActor, const FVector& Destination, const float AcceptanceRadius)
{
	Controller.StopMovement();
	ResetMove(InstanceData);

	FAIMoveRequest MoveRequest;
	MoveRequest.SetNavigationFilter(Controller.GetDefaultNavigationFilterClass())
		.SetAllowPartialPath(true)
		.SetAcceptanceRadius(FMath::Max(0.0f, AcceptanceRadius))
		.SetCanStrafe(false)
		.SetReachTestIncludesAgentRadius(true)
		.SetReachTestIncludesGoalRadius(true)
		.SetRequireNavigableEndLocation(true)
		.SetProjectGoalLocation(true)
		.SetUsePathfinding(true);

	if (IsValid(TargetActor))
	{
		MoveRequest.SetGoalActor(TargetActor);
	}
	else
	{
		MoveRequest.SetGoalLocation(Destination);
	}

	if (!MoveRequest.IsValid())
	{
		return false;
	}

	const FPathFollowingRequestResult MoveResult = Controller.MoveTo(MoveRequest);
	InstanceData.MoveRequestId = MoveResult.MoveId.GetID();
	InstanceData.RequestResult = static_cast<uint8>(MoveResult.Code.GetValue());
	InstanceData.bMoveFinished = MoveResult.Code != EPathFollowingRequestResult::RequestSuccessful;
	InstanceData.bMoveSucceeded = MoveResult.Code == EPathFollowingRequestResult::AlreadyAtGoal;
	InstanceData.Phase = static_cast<uint8>(EHeistGuardTaskPhase::Moving);
	return MoveResult.Code != EPathFollowingRequestResult::Failed;
}

bool StartPatrolMove(FHeistGuardStateTreeTaskInstanceData& InstanceData, AHeistGuardAIController& Controller)
{
	AHeistGuardCharacter* GuardCharacter = ResolveGuardCharacter(&Controller);
	if (!IsValid(GuardCharacter))
	{
		return false;
	}

	AActor* ExitSurveillanceTarget = nullptr;
	float ExitSurveillanceAcceptanceRadius = 0.0f;
	if (Controller.TryGetAlertExitSurveillanceTarget(ExitSurveillanceTarget, ExitSurveillanceAcceptanceRadius))
	{
		return StartMove(InstanceData, Controller, ExitSurveillanceTarget, ExitSurveillanceTarget->GetActorLocation(), ExitSurveillanceAcceptanceRadius);
	}

	UHeistPatrolPathComponent* PatrolPath = ResolvePatrolPath(&Controller);
	if (!IsValid(PatrolPath))
	{
		return false;
	}

	if (PatrolPath->GetWaypointCount() == 0)
	{
		PatrolPath->ResolvePatrolPath();
		UHeistDebugFunctionLibrary::DebugGuardPatrolPathResolved(&Controller, GuardCharacter, PatrolPath->GetPatrolRouteId(), PatrolPath->GetWaypointCount());
	}

	AHeistGuardWaypoint* Waypoint = PatrolPath->GetCurrentWaypoint();
	const bool bMoveStarted = IsValid(Waypoint) && StartMove(InstanceData, Controller, Waypoint, Waypoint->GetActorLocation(), PatrolPath->GetAcceptanceRadius());
	return bMoveStarted;
}

bool StartFocusMove(FHeistGuardStateTreeTaskInstanceData& InstanceData, AHeistGuardAIController& Controller)
{
	UHeistGuardStateComponent* GuardStateComponent = ResolveGuardStateComponent(&Controller);
	const UHeistPatrolPathComponent* PatrolPath = ResolvePatrolPath(&Controller);
	if (!IsValid(GuardStateComponent))
	{
		return false;
	}

	const FVector Destination = GuardStateComponent->GetStateFocusLocation();
	const float AcceptanceRadius = IsValid(PatrolPath) ? PatrolPath->GetAcceptanceRadius() : 75.0f;
	return StartMove(InstanceData, Controller, nullptr, Destination, AcceptanceRadius);
}

bool StartChaseMove(FHeistGuardStateTreeTaskInstanceData& InstanceData, AHeistGuardAIController& Controller)
{
	UHeistGuardStateComponent* GuardStateComponent = ResolveGuardStateComponent(&Controller);
	const UHeistPatrolPathComponent* PatrolPath = ResolvePatrolPath(&Controller);
	AActor* ChaseTarget = IsValid(GuardStateComponent) ? GuardStateComponent->GetChaseTarget() : nullptr;
	if (!IsValid(GuardStateComponent) || !IsValid(ChaseTarget))
	{
		return false;
	}

	const float AcceptanceRadius = IsValid(PatrolPath) ? PatrolPath->GetAcceptanceRadius() : 75.0f;
	return StartMove(InstanceData, Controller, ChaseTarget, ChaseTarget->GetActorLocation(), AcceptanceRadius);
}

bool StartInspectionMove(FHeistGuardStateTreeTaskInstanceData& InstanceData, AHeistGuardAIController& Controller)
{
	AHeistPaintingDisplayCaseActor* Target = Controller.GetInspectionTarget();
	if (!IsValid(Target))
	{
		return false;
	}

	return StartMove(InstanceData, Controller, Target, Target->GetActorLocation(), Controller.GetInspectionAcceptanceRadius());
}

bool StartReturnMove(FHeistGuardStateTreeTaskInstanceData& InstanceData, AHeistGuardAIController& Controller)
{
	AHeistGuardCharacter* GuardCharacter = ResolveGuardCharacter(&Controller);
	UHeistPatrolPathComponent* PatrolPath = ResolvePatrolPath(&Controller);
	if (!IsValid(GuardCharacter) || !IsValid(PatrolPath))
	{
		return false;
	}

	if (PatrolPath->GetWaypointCount() == 0)
	{
		PatrolPath->ResolvePatrolPath();
	}
	AHeistGuardWaypoint* Waypoint = PatrolPath->GetCurrentWaypoint();
	if (!IsValid(Waypoint))
	{
		return false;
	}

	return StartMove(InstanceData, Controller, Waypoint, Waypoint->GetActorLocation(), PatrolPath->GetAcceptanceRadius());
}

void AwaitAuthoritativeStateChange(FHeistGuardStateTreeTaskInstanceData& InstanceData)
{
	InstanceData.Phase = static_cast<uint8>(EHeistGuardTaskPhase::AwaitingStateChange);
}

void BeginPatrolWait(FHeistGuardStateTreeTaskInstanceData& InstanceData, const UHeistPatrolPathComponent* PatrolPath)
{
	InstanceData.WaitRemaining = IsValid(PatrolPath) ? PatrolPath->GetWaypointWaitDuration() : 0.0f;
	InstanceData.Phase = static_cast<uint8>(EHeistGuardTaskPhase::Waiting);
}
}

FHeistGuardStateTreeTask::FHeistGuardStateTreeTask()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FHeistGuardStateTreeTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ResetMove(InstanceData);
	InstanceData.WaitRemaining = 0.0f;
	InstanceData.Phase = static_cast<uint8>(EHeistGuardTaskPhase::Idle);

	AHeistGuardAIController* Controller = ResolveController(Context);
	UHeistGuardStateComponent* GuardStateComponent = ResolveGuardStateComponent(Controller);
	if (!IsValid(Controller) || !Controller->HasAuthority() || !IsValid(GuardStateComponent))
	{
		return EStateTreeRunStatus::Failed;
	}

	if (GuardStateComponent->GetGuardState() != GuardState)
	{
		return EStateTreeRunStatus::Running;
	}

	switch (GuardState)
	{
	case EHeistGuardState::Patrol:
		if (Controller->TryBeginInspection())
		{
			AwaitAuthoritativeStateChange(InstanceData);
			break;
		}
		if (!StartPatrolMove(InstanceData, *Controller))
		{
			BeginPatrolWait(InstanceData, ResolvePatrolPath(Controller));
		}
		break;
	case EHeistGuardState::InvestigateNoise:
		if (!StartFocusMove(InstanceData, *Controller))
		{
			GuardStateComponent->EnterPatrol();
			AwaitAuthoritativeStateChange(InstanceData);
		}
		break;
	case EHeistGuardState::InspectExhibit:
		if (!StartInspectionMove(InstanceData, *Controller))
		{
			Controller->AbortInspection(FName(TEXT("InspectionMoveRejected")));
			GuardStateComponent->EnterPatrol();
			AwaitAuthoritativeStateChange(InstanceData);
		}
		break;
	case EHeistGuardState::ChasePlayer:
		if (!StartChaseMove(InstanceData, *Controller))
		{
			GuardStateComponent->EnterSearchLastKnownLocation(GuardStateComponent->GetStateFocusLocation());
			AwaitAuthoritativeStateChange(InstanceData);
		}
		break;
	case EHeistGuardState::SearchLastKnownLocation:
		if (!StartFocusMove(InstanceData, *Controller))
		{
			GuardStateComponent->EnterReturnToPatrol();
			AwaitAuthoritativeStateChange(InstanceData);
		}
		break;
	case EHeistGuardState::ReturnToPatrol:
		if (!StartReturnMove(InstanceData, *Controller))
		{
			GuardStateComponent->EnterPatrol();
			AwaitAuthoritativeStateChange(InstanceData);
		}
		break;
	case EHeistGuardState::Stunned:
	case EHeistGuardState::Disabled:
	default:
		Controller->StopMovement();
		AwaitAuthoritativeStateChange(InstanceData);
		break;
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FHeistGuardStateTreeTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AHeistGuardAIController* Controller = ResolveController(Context);
	AHeistGuardCharacter* GuardCharacter = ResolveGuardCharacter(Controller);
	UHeistGuardStateComponent* GuardStateComponent = ResolveGuardStateComponent(Controller);
	UHeistPatrolPathComponent* PatrolPath = ResolvePatrolPath(Controller);
	if (!IsValid(Controller) || !Controller->HasAuthority() || !IsValid(GuardCharacter) || !IsValid(GuardStateComponent))
	{
		return EStateTreeRunStatus::Failed;
	}

	if (GuardStateComponent->GetGuardState() != GuardState)
	{
		return EStateTreeRunStatus::Running;
	}

	const EHeistGuardTaskPhase Phase = static_cast<EHeistGuardTaskPhase>(InstanceData.Phase);
	if (Phase == EHeistGuardTaskPhase::AwaitingStateChange)
	{
		return EStateTreeRunStatus::Running;
	}

	if (Phase == EHeistGuardTaskPhase::Waiting)
	{
		InstanceData.WaitRemaining = FMath::Max(0.0f, InstanceData.WaitRemaining - DeltaTime);
		if (InstanceData.WaitRemaining <= 0.0f)
		{
			if (GuardState == EHeistGuardState::Patrol && Controller->TryBeginInspection())
			{
				AwaitAuthoritativeStateChange(InstanceData);
				return EStateTreeRunStatus::Running;
			}
			if (GuardState == EHeistGuardState::Patrol && Controller->IsAlertExitSurveillanceActive())
			{
				if (!StartPatrolMove(InstanceData, *Controller))
				{
					BeginPatrolWait(InstanceData, PatrolPath);
				}
			}
			else if (GuardState == EHeistGuardState::Patrol && IsValid(PatrolPath) && IsValid(PatrolPath->AdvanceWaypoint()))
			{
				if (!StartPatrolMove(InstanceData, *Controller))
				{
					BeginPatrolWait(InstanceData, PatrolPath);
				}
			}
			else if (GuardState == EHeistGuardState::Patrol)
			{
				BeginPatrolWait(InstanceData, PatrolPath);
			}
		}
		return EStateTreeRunStatus::Running;
	}

	if (GuardState == EHeistGuardState::ChasePlayer && Controller->TryArrestChaseTarget())
	{
		AwaitAuthoritativeStateChange(InstanceData);
		return EStateTreeRunStatus::Running;
	}

	if (!InstanceData.bMoveFinished)
	{
		const UPathFollowingComponent* PathFollowingComponent = Controller->GetPathFollowingComponent();
		if (IsValid(PathFollowingComponent) && PathFollowingComponent->GetStatus() != EPathFollowingStatus::Idle &&
			FAIRequestID(InstanceData.MoveRequestId).IsEquivalent(PathFollowingComponent->GetCurrentRequestId()))
		{
			return EStateTreeRunStatus::Running;
		}

		InstanceData.bMoveFinished = true;
		InstanceData.bMoveSucceeded = IsValid(PathFollowingComponent) && PathFollowingComponent->DidMoveReachGoal();
	}

	const bool bMoveSucceeded = InstanceData.bMoveSucceeded;
	ResetMove(InstanceData);

	switch (GuardState)
	{
	case EHeistGuardState::Patrol:
		if (Controller->TryBeginInspection())
		{
			AwaitAuthoritativeStateChange(InstanceData);
		}
		else
		{
			BeginPatrolWait(InstanceData, PatrolPath);
		}
		break;
	case EHeistGuardState::InvestigateNoise:
		if (bMoveSucceeded && GuardStateComponent->StartInvestigateConfirmationTimer())
		{
			UHeistDebugFunctionLibrary::DebugGuardInvestigateConfirmationStarted(Controller, GuardCharacter, GuardStateComponent->GetStateFocusLocation(),
																				 GuardStateComponent->GetInvestigateConfirmationDuration());
		}
		else
		{
			GuardStateComponent->EnterPatrol();
		}
		AwaitAuthoritativeStateChange(InstanceData);
		break;
	case EHeistGuardState::InspectExhibit:
		if (!bMoveSucceeded || !Controller->StartInspectionCast())
		{
			Controller->AbortInspection(FName(TEXT("InspectionMoveFailed")));
			GuardStateComponent->EnterPatrol();
		}
		AwaitAuthoritativeStateChange(InstanceData);
		break;
	case EHeistGuardState::ChasePlayer:
		if (!bMoveSucceeded || !StartChaseMove(InstanceData, *Controller))
		{
			GuardStateComponent->EnterSearchLastKnownLocation(GuardStateComponent->GetStateFocusLocation());
			AwaitAuthoritativeStateChange(InstanceData);
		}
		break;
	case EHeistGuardState::SearchLastKnownLocation:
		if (bMoveSucceeded && GuardStateComponent->StartSearchTimer())
		{
			UHeistDebugFunctionLibrary::DebugGuardSearchTimerStarted(Controller, GuardCharacter, GuardStateComponent->GetStateFocusLocation(), GuardStateComponent->GetSearchDuration());
		}
		else
		{
			GuardStateComponent->EnterReturnToPatrol();
		}
		AwaitAuthoritativeStateChange(InstanceData);
		break;
	case EHeistGuardState::ReturnToPatrol:
		GuardStateComponent->EnterPatrol();
		AwaitAuthoritativeStateChange(InstanceData);
		break;
	case EHeistGuardState::Stunned:
	case EHeistGuardState::Disabled:
	default:
		break;
	}

	return EStateTreeRunStatus::Running;
}

void FHeistGuardStateTreeTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (AHeistGuardAIController* Controller = ResolveController(Context))
	{
		Controller->StopMovement();
	}
	ResetMove(InstanceData);
	InstanceData.WaitRemaining = 0.0f;
	InstanceData.Phase = static_cast<uint8>(EHeistGuardTaskPhase::Idle);
}
