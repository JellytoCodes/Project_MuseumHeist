#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"

#include "HeistCheatManager.generated.h"

UCLASS()
class PROJECT_MUSEUMHEIST_API UHeistCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	UHeistCheatManager(const FObjectInitializer& ObjectInitializer);

#pragma region InventoryDebug

public:
	UFUNCTION(Exec)
	void HeistInvHelp();

	UFUNCTION(Exec)
	void HeistInvDump();

	UFUNCTION(Exec)
	void HeistInvOpen(bool bOpen);

	UFUNCTION(Exec)
	void HeistInvMove(int32 InstanceId, int32 GridX, int32 GridY);

	UFUNCTION(Exec)
	void HeistInvRotate(int32 InstanceId);

	UFUNCTION(Exec)
	void HeistInvDrop(int32 InstanceId);

	UFUNCTION(Exec)
	void HeistInvAssign(const FString& SlotName, int32 InstanceId);

	UFUNCTION(Exec)
	void HeistInvClear(const FString& SlotName);

	UFUNCTION(Exec)
	void HeistInvInvalidMove(int32 InstanceId);

#pragma endregion
};
