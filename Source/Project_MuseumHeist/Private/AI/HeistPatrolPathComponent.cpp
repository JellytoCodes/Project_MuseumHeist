#include "AI/HeistPatrolPathComponent.h"

#include "EngineUtils.h"
#include "World/AI/HeistGuardWaypoint.h"

UHeistPatrolPathComponent::UHeistPatrolPathComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UHeistPatrolPathComponent::ResolvePatrolPath()
{
	ResolvedWaypoints.Reset();
	CurrentWaypointIndex = 0;
	PatrolDirection = 1;

	UWorld* World = GetWorld();
	if (!IsValid(World) || PatrolRouteId.IsNone())
	{
		return false;
	}

	for (TActorIterator<AHeistGuardWaypoint> It(World); It; ++It)
	{
		AHeistGuardWaypoint* Waypoint = *It;
		if (IsValid(Waypoint) && Waypoint->GetPatrolRouteId() == PatrolRouteId)
		{
			ResolvedWaypoints.Add(Waypoint);
		}
	}

	ResolvedWaypoints.Sort(
		[](const AHeistGuardWaypoint& Left, const AHeistGuardWaypoint& Right)
		{
			if (Left.GetPatrolOrder() != Right.GetPatrolOrder())
			{
				return Left.GetPatrolOrder() < Right.GetPatrolOrder();
			}
			return Left.GetName() < Right.GetName();
		});
	return !ResolvedWaypoints.IsEmpty();
}

AHeistGuardWaypoint* UHeistPatrolPathComponent::GetCurrentWaypoint() const
{
	return ResolvedWaypoints.IsValidIndex(CurrentWaypointIndex)
		? ResolvedWaypoints[CurrentWaypointIndex].Get()
		: nullptr;
}

AHeistGuardWaypoint* UHeistPatrolPathComponent::AdvanceWaypoint()
{
	if (ResolvedWaypoints.IsEmpty())
	{
		return nullptr;
	}

	const int32 NextWaypointIndex = CurrentWaypointIndex + PatrolDirection;
	if (ResolvedWaypoints.IsValidIndex(NextWaypointIndex))
	{
		CurrentWaypointIndex = NextWaypointIndex;
	}
	else if (bLoopPatrol && ResolvedWaypoints.Num() > 1)
	{
		PatrolDirection *= -1;
		CurrentWaypointIndex += PatrolDirection;
	}
	else
	{
		return nullptr;
	}
	return GetCurrentWaypoint();
}

int32 UHeistPatrolPathComponent::GetCurrentWaypointIndex() const
{
	return CurrentWaypointIndex;
}

int32 UHeistPatrolPathComponent::GetWaypointCount() const
{
	return ResolvedWaypoints.Num();
}

FName UHeistPatrolPathComponent::GetPatrolRouteId() const
{
	return PatrolRouteId;
}

float UHeistPatrolPathComponent::GetAcceptanceRadius() const
{
	return FMath::Max(0.0f, AcceptanceRadius);
}

float UHeistPatrolPathComponent::GetWaypointWaitDuration() const
{
	return FMath::Max(0.0f, WaypointWaitDuration);
}
