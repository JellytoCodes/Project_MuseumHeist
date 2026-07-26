#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "HeistPatrolPathComponent.generated.h"

class AHeistGuardWaypoint;

UCLASS(ClassGroup = (Heist), meta = (BlueprintSpawnableComponent))
class PROJECT_MUSEUMHEIST_API UHeistPatrolPathComponent : public UActorComponent
{
	GENERATED_BODY()

  public:
	UHeistPatrolPathComponent();

	bool ResolvePatrolPath();
	AHeistGuardWaypoint* GetCurrentWaypoint() const;
	AHeistGuardWaypoint* AdvanceWaypoint();
	int32 GetCurrentWaypointIndex() const;
	int32 GetWaypointCount() const;
	FName GetPatrolRouteId() const;
	float GetAcceptanceRadius() const;
	float GetWaypointWaitDuration() const;
	bool ShouldLookAroundAtWaypoints() const;
	float GetLookAroundYawAngle() const;
	float GetLookAroundTurnRate() const;

  private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Guard Patrol Route", meta = (AllowPrivateAccess = "true"))
	FName PatrolRouteId = FName(TEXT("Default"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Guard Patrol Route", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float AcceptanceRadius = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Guard Patrol Route", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float WaypointWaitDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Guard Patrol Route", meta = (AllowPrivateAccess = "true"))
	bool bLookAroundAtWaypoints = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Guard Patrol Route",
			  meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
	float LookAroundYawAngle = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Guard Patrol Route",
			  meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "deg/s"))
	float LookAroundTurnRate = 270.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Guard Patrol Route", meta = (AllowPrivateAccess = "true", DisplayName = "Ping Pong Patrol"))
	bool bLoopPatrol = true;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AHeistGuardWaypoint>> ResolvedWaypoints;

	int32 CurrentWaypointIndex = 0;
	int32 PatrolDirection = 1;
};
