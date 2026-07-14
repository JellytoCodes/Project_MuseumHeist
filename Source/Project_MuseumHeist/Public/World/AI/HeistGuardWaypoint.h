#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "HeistGuardWaypoint.generated.h"

class USceneComponent;

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistGuardWaypoint : public AActor
{
	GENERATED_BODY()

public:
	AHeistGuardWaypoint();

	FName GetPatrolRouteId() const;
	int32 GetPatrolOrder() const;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|AI|Patrol", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|AI|Patrol", meta = (AllowPrivateAccess = "true"))
	FName PatrolRouteId = FName(TEXT("Default"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|AI|Patrol", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 PatrolOrder = 0;
};
