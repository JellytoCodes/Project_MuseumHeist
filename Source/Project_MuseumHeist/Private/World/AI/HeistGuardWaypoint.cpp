#include "World/AI/HeistGuardWaypoint.h"

#include "Components/SceneComponent.h"

AHeistGuardWaypoint::AHeistGuardWaypoint()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

FName AHeistGuardWaypoint::GetPatrolRouteId() const
{
	return PatrolRouteId;
}

int32 AHeistGuardWaypoint::GetPatrolOrder() const
{
	return PatrolOrder;
}
