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

	AHeistGuardCharacter* ResolveGuardCharacter(
		const AHeistGuardAIController* Controller)
	{
		return IsValid(Controller)
			? Cast<AHeistGuardCharacter>(Controller->GetPawn())
			: nullptr;
	}

	UHeistGuardStateComponent* ResolveGuardStateComponent(
		const AHeistGuardAIController* Controller)
	{
		AHeistGuardCharacter* GuardCharacter = ResolveGuardCharacter(Controller);
		return IsValid(GuardCharacter)
			? GuardCharacter->GetGuardStateComponent()
			: nullptr;
	}

	UHeistPatrolPathComponent* ResolvePatrolPath(
		const AHeistGuardAIController* Controller)
	{
		AHeistGuardCharacter* GuardCharacter = ResolveGuardCharacter(Controller);
		return IsValid(GuardCharacter)
			? GuardCharacter->GetPatrolPathComponent()
			: nullptr;
	}

	void ResetMove(FHeistGuardStateTreeTaskInstanceData& InstanceData)
	{
		InstanceData.MoveRequestId = MAX_uint32;
		InstanceData.RequestResult =
			static_cast<uint8>(EPathFollowingRequestResult::Failed);
		InstanceData.bMoveFinished = false;
		InstanceData.bMoveSucceeded = false;
	}

	const TCHAR* GetMoveRequestResultText(
		const FHeistGuardStateTreeTaskInstanceData& InstanceData)
	{
		switch (static_cast<EPathFollowingRequestResult::Type>(
			InstanceData.RequestResult))
		{
		case EPathFollowingRequestResult::RequestSuccessful:
			return TEXT("RequestSuccessful");
		case EPathFollowingRequestResult::AlreadyAtGoal:
			return TEXT("AlreadyAtGoal");
		case EPathFollowingRequestResult::Failed:
		default:
			return TEXT("Failed");
		}
	}

	bool StartMove(
		FHeistGuardStateTreeTaskInstanceData& InstanceData,
		AHeistGuardAIController& Controller,
		AActor* TargetActor,
		const FVector& Destination,
		const float AcceptanceRadius)
	{
		Controller.StopMovement();
		ResetMove(InstanceData);

		FAIMoveRequest MoveRequest;
		MoveRequest
			.SetNavigationFilter(Controller.GetDefaultNavigationFilterClass())
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

		const FPathFollowingRequestResult MoveResult =
			Controller.MoveTo(MoveRequest);
		InstanceData.MoveRequestId = MoveResult.MoveId.GetID();
		InstanceData.RequestResult =
			static_cast<uint8>(MoveResult.Code.GetValue());
		InstanceData.bMoveFinished =
			MoveResult.Code != EPathFollowingRequestResult::RequestSuccessful;
		InstanceData.bMoveSucceeded =
			MoveResult.Code == EPathFollowingRequestResult::AlreadyAtGoal;
		InstanceData.Phase =
			static_cast<uint8>(EHeistGuardTaskPhase::Moving);
		return MoveResult.Code != EPathFollowingRequestResult::Failed;
	}

	void DebugMoveRequest(
		AHeistGuardAIController& Controller,
		const EHeistGuardState GuardState,
		const FVector& Destination,
		const FHeistGuardStateTreeTaskInstanceData& InstanceData,
		const int32 WaypointIndex = INDEX_NONE,
		const int32 WaypointCount = 0)
	{
		UHeistDebugFunctionLibrary::DebugGuardMovement(
			&Controller,
			ResolveGuardCharacter(&Controller),
			GuardState,
			TEXT("StateTreeRequested"),
			Destination,
			WaypointIndex,
			WaypointCount,
			GetMoveRequestResultText(InstanceData));
	}

	bool StartPatrolMove(
		FHeistGuardStateTreeTaskInstanceData& InstanceData,
		AHeistGuardAIController& Controller)
	{
		AHeistGuardCharacter* GuardCharacter =
			ResolveGuardCharacter(&Controller);
		UHeistPatrolPathComponent* PatrolPath =
			ResolvePatrolPath(&Controller);
		if (!IsValid(GuardCharacter) || !IsValid(PatrolPath))
		{
			return false;
		}

		if (PatrolPath->GetWaypointCount() == 0)
		{
			PatrolPath->ResolvePatrolPath();
			UHeistDebugFunctionLibrary::DebugGuardPatrolPathResolved(
				&Controller,
				GuardCharacter,
				PatrolPath->GetPatrolRouteId(),
				PatrolPath->GetWaypointCount());
		}

		AHeistGuardWaypoint* Waypoint = PatrolPath->GetCurrentWaypoint();
		const bool bMoveStarted =
			IsValid(Waypoint)
			&& StartMove(
				InstanceData,
				Controller,
				Waypoint,
				Waypoint->GetActorLocation(),
				PatrolPath->GetAcceptanceRadius());
		DebugMoveRequest(
			Controller,
			EHeistGuardState::Patrol,
			IsValid(Waypoint)
				? Waypoint->GetActorLocation()
				: GuardCharacter->GetActorLocation(),
			InstanceData,
			PatrolPath->GetCurrentWaypointIndex(),
			PatrolPath->GetWaypointCount());
		return bMoveStarted;
	}

	bool StartFocusMove(
		FHeistGuardStateTreeTaskInstanceData& InstanceData,
		AHeistGuardAIController& Controller,
		const EHeistGuardState GuardState)
	{
		UHeistGuardStateComponent* GuardStateComponent =
			ResolveGuardStateComponent(&Controller);
		const UHeistPatrolPathComponent* PatrolPath =
			ResolvePatrolPath(&Controller);
		if (!IsValid(GuardStateComponent))
		{
			return false;
		}

		const FVector Destination =
			GuardStateComponent->GetStateFocusLocation();
		const float AcceptanceRadius =
			IsValid(PatrolPath)
				? PatrolPath->GetAcceptanceRadius()
				: 75.0f;
		const bool bMoveStarted = StartMove(
			InstanceData,
			Controller,
			nullptr,
			Destination,
			AcceptanceRadius);
		DebugMoveRequest(
			Controller,
			GuardState,
			Destination,
			InstanceData);
		return bMoveStarted;
	}

	bool StartChaseMove(
		FHeistGuardStateTreeTaskInstanceData& InstanceData,
		AHeistGuardAIController& Controller)
	{
		UHeistGuardStateComponent* GuardStateComponent =
			ResolveGuardStateComponent(&Controller);
		const UHeistPatrolPathComponent* PatrolPath =
			ResolvePatrolPath(&Controller);
		AActor* ChaseTarget = IsValid(GuardStateComponent)
			? GuardStateComponent->GetChaseTarget()
			: nullptr;
		if (!IsValid(GuardStateComponent) || !IsValid(ChaseTarget))
		{
			return false;
		}

		const float AcceptanceRadius =
			IsValid(PatrolPath)
				? PatrolPath->GetAcceptanceRadius()
				: 75.0f;
		const bool bMoveStarted = StartMove(
			InstanceData,
			Controller,
			ChaseTarget,
			ChaseTarget->GetActorLocation(),
			AcceptanceRadius);
		DebugMoveRequest(
			Controller,
			EHeistGuardState::ChasePlayer,
			ChaseTarget->GetActorLocation(),
			InstanceData);
		return bMoveStarted;
	}

	bool StartReturnMove(
		FHeistGuardStateTreeTaskInstanceData& InstanceData,
		AHeistGuardAIController& Controller)
	{
		AHeistGuardCharacter* GuardCharacter =
			ResolveGuardCharacter(&Controller);
		UHeistPatrolPathComponent* PatrolPath =
			ResolvePatrolPath(&Controller);
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
			DebugMoveRequest(
				Controller,
				EHeistGuardState::ReturnToPatrol,
				GuardCharacter->GetActorLocation(),
				InstanceData,
				INDEX_NONE,
				PatrolPath->GetWaypointCount());
			return false;
		}

		const bool bMoveStarted = StartMove(
			InstanceData,
			Controller,
			Waypoint,
			Waypoint->GetActorLocation(),
			PatrolPath->GetAcceptanceRadius());
		DebugMoveRequest(
			Controller,
			EHeistGuardState::ReturnToPatrol,
			Waypoint->GetActorLocation(),
			InstanceData,
			PatrolPath->GetCurrentWaypointIndex(),
			PatrolPath->GetWaypointCount());
		return bMoveStarted;
	}

	void AwaitAuthoritativeStateChange(
		FHeistGuardStateTreeTaskInstanceData& InstanceData)
	{
		InstanceData.Phase =
			static_cast<uint8>(EHeistGuardTaskPhase::AwaitingStateChange);
	}

	void BeginPatrolWait(
		FHeistGuardStateTreeTaskInstanceData& InstanceData,
		const UHeistPatrolPathComponent* PatrolPath)
	{
		InstanceData.WaitRemaining = IsValid(PatrolPath)
			? PatrolPath->GetWaypointWaitDuration()
			: 0.0f;
		InstanceData.Phase =
			static_cast<uint8>(EHeistGuardTaskPhase::Waiting);
	}
}

FHeistGuardStateTreeTask::FHeistGuardStateTreeTask()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FHeistGuardStateTreeTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult&) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ResetMove(InstanceData);
	InstanceData.WaitRemaining = 0.0f;
	InstanceData.Phase =
		static_cast<uint8>(EHeistGuardTaskPhase::Idle);

	AHeistGuardAIController* Controller = ResolveController(Context);
	UHeistGuardStateComponent* GuardStateComponent =
		ResolveGuardStateComponent(Controller);
	if (!IsValid(Controller)
		|| !Controller->HasAuthority()
		|| !IsValid(GuardStateComponent))
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
		if (!StartPatrolMove(InstanceData, *Controller))
		{
			BeginPatrolWait(
				InstanceData,
				ResolvePatrolPath(Controller));
		}
		break;
	case EHeistGuardState::InvestigateNoise:
		if (!StartFocusMove(
			InstanceData,
			*Controller,
			EHeistGuardState::InvestigateNoise))
		{
			GuardStateComponent->EnterPatrol();
			AwaitAuthoritativeStateChange(InstanceData);
		}
		break;
	case EHeistGuardState::ChasePlayer:
		if (!StartChaseMove(InstanceData, *Controller))
		{
			GuardStateComponent->EnterSearchLastKnownLocation(
				GuardStateComponent->GetStateFocusLocation());
			AwaitAuthoritativeStateChange(InstanceData);
		}
		break;
	case EHeistGuardState::SearchLastKnownLocation:
		if (!StartFocusMove(
			InstanceData,
			*Controller,
			EHeistGuardState::SearchLastKnownLocation))
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

EStateTreeRunStatus FHeistGuardStateTreeTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AHeistGuardAIController* Controller = ResolveController(Context);
	AHeistGuardCharacter* GuardCharacter =
		ResolveGuardCharacter(Controller);
	UHeistGuardStateComponent* GuardStateComponent =
		ResolveGuardStateComponent(Controller);
	UHeistPatrolPathComponent* PatrolPath =
		ResolvePatrolPath(Controller);
	if (!IsValid(Controller)
		|| !Controller->HasAuthority()
		|| !IsValid(GuardCharacter)
		|| !IsValid(GuardStateComponent))
	{
		return EStateTreeRunStatus::Failed;
	}

	if (GuardStateComponent->GetGuardState() != GuardState)
	{
		return EStateTreeRunStatus::Running;
	}

	const EHeistGuardTaskPhase Phase =
		static_cast<EHeistGuardTaskPhase>(InstanceData.Phase);
	if (Phase == EHeistGuardTaskPhase::AwaitingStateChange)
	{
		return EStateTreeRunStatus::Running;
	}

	if (Phase == EHeistGuardTaskPhase::Waiting)
	{
		InstanceData.WaitRemaining =
			FMath::Max(0.0f, InstanceData.WaitRemaining - DeltaTime);
		if (InstanceData.WaitRemaining <= 0.0f)
		{
			if (GuardState == EHeistGuardState::Patrol
				&& IsValid(PatrolPath)
				&& IsValid(PatrolPath->AdvanceWaypoint()))
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

	if (GuardState == EHeistGuardState::ChasePlayer
		&& Controller->TryArrestChaseTarget())
	{
		AwaitAuthoritativeStateChange(InstanceData);
		return EStateTreeRunStatus::Running;
	}

	if (!InstanceData.bMoveFinished)
	{
		const UPathFollowingComponent* PathFollowingComponent =
			Controller->GetPathFollowingComponent();
		if (IsValid(PathFollowingComponent)
			&& PathFollowingComponent->GetStatus() != EPathFollowingStatus::Idle
			&& FAIRequestID(InstanceData.MoveRequestId).IsEquivalent(
				PathFollowingComponent->GetCurrentRequestId()))
		{
			return EStateTreeRunStatus::Running;
		}

		InstanceData.bMoveFinished = true;
		InstanceData.bMoveSucceeded =
			IsValid(PathFollowingComponent)
			&& PathFollowingComponent->DidMoveReachGoal();
	}

	const bool bMoveSucceeded = InstanceData.bMoveSucceeded;
	ResetMove(InstanceData);
	UHeistDebugFunctionLibrary::DebugGuardMovement(
		Controller,
		GuardCharacter,
		GuardState,
		TEXT("StateTreeCompleted"),
		GuardStateComponent->GetStateFocusLocation(),
		IsValid(PatrolPath)
			? PatrolPath->GetCurrentWaypointIndex()
			: INDEX_NONE,
		IsValid(PatrolPath)
			? PatrolPath->GetWaypointCount()
			: 0,
		bMoveSucceeded ? TEXT("Success") : TEXT("Failed"));

	switch (GuardState)
	{
	case EHeistGuardState::Patrol:
		BeginPatrolWait(InstanceData, PatrolPath);
		break;
	case EHeistGuardState::InvestigateNoise:
		if (bMoveSucceeded
			&& GuardStateComponent->StartInvestigateConfirmationTimer())
		{
			UHeistDebugFunctionLibrary::DebugGuardInvestigateConfirmationStarted(
				Controller,
				GuardCharacter,
				GuardStateComponent->GetStateFocusLocation(),
				GuardStateComponent->GetInvestigateConfirmationDuration());
		}
		else
		{
			GuardStateComponent->EnterPatrol();
		}
		AwaitAuthoritativeStateChange(InstanceData);
		break;
	case EHeistGuardState::ChasePlayer:
		if (!bMoveSucceeded
			|| !StartChaseMove(InstanceData, *Controller))
		{
			GuardStateComponent->EnterSearchLastKnownLocation(
				GuardStateComponent->GetStateFocusLocation());
			AwaitAuthoritativeStateChange(InstanceData);
		}
		break;
	case EHeistGuardState::SearchLastKnownLocation:
		if (bMoveSucceeded
			&& GuardStateComponent->StartSearchTimer())
		{
			UHeistDebugFunctionLibrary::DebugGuardSearchTimerStarted(
				Controller,
				GuardCharacter,
				GuardStateComponent->GetStateFocusLocation(),
				GuardStateComponent->GetSearchDuration());
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

void FHeistGuardStateTreeTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult&) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (AHeistGuardAIController* Controller = ResolveController(Context))
	{
		Controller->StopMovement();
	}
	ResetMove(InstanceData);
	InstanceData.WaitRemaining = 0.0f;
	InstanceData.Phase =
		static_cast<uint8>(EHeistGuardTaskPhase::Idle);
}
