#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "Tasks/StateTreeAITask.h"

#include "HeistGuardStateTreeTask.generated.h"

USTRUCT()
struct FHeistGuardStateTreeTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	float WaitRemaining = 0.0f;

	UPROPERTY(Transient)
	uint32 MoveRequestId = MAX_uint32;

	UPROPERTY(Transient)
	uint8 Phase = 0;

	UPROPERTY(Transient)
	uint8 RequestResult = 0;

	UPROPERTY(Transient)
	bool bMoveFinished = false;

	UPROPERTY(Transient)
	bool bMoveSucceeded = false;
};

/**
 * Executes the movement and wait loop for one authoritative guard state.
 * State changes remain validated by UHeistGuardStateComponent and are routed
 * back into the StateTree through gameplay-tag events.
 */
USTRUCT(meta = (DisplayName = "Execute Guard State", Category = "Heist|AI"))
struct PROJECT_MUSEUMHEIST_API FHeistGuardStateTreeTask : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FHeistGuardStateTreeTaskInstanceData;

	FHeistGuardStateTreeTask();

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(
		FStateTreeExecutionContext& Context,
		float DeltaTime) const override;
	virtual void ExitState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

	UPROPERTY(EditAnywhere, Category = "State")
	EHeistGuardState GuardState = EHeistGuardState::Patrol;
};
