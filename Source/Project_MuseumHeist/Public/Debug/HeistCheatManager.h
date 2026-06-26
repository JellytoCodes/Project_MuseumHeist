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

#pragma region StatusDebug

public:
	UFUNCTION(Exec)
	void HeistStatusHelp();

	UFUNCTION(Exec)
	void HeistStatusDump();

	UFUNCTION(Exec)
	void HeistStatusStun(float DurationSeconds = 3.0f);

	UFUNCTION(Exec)
	void HeistStatusImmune(float DurationSeconds = 2.0f);

	UFUNCTION(Exec)
	void HeistStatusClear();

#pragma endregion

#pragma region ThrowableDebug

public:
	UFUNCTION(Exec)
	void HeistThrowHelp();

	UFUNCTION(Exec)
	void HeistCoinThrow(float Distance = 1000.0f);

	UFUNCTION(Exec)
	void HeistCoinThrowAt(float TargetX, float TargetY, float TargetZ);

	UFUNCTION(Exec)
	void HeistSmokeThrow(float Distance = 1000.0f);

	UFUNCTION(Exec)
	void HeistSmokeThrowAt(float TargetX, float TargetY, float TargetZ);

	UFUNCTION(Exec)
	void HeistSmokeSightCheck(float Distance = 1000.0f);

	UFUNCTION(Exec)
	void HeistSmokeSightCheckAt(float TargetX, float TargetY, float TargetZ);

#pragma endregion

#pragma region TrapDebug

public:
	UFUNCTION(Exec)
	void HeistTrapHelp();

	UFUNCTION(Exec)
	void HeistGlueTrapPlace(float Distance = 200.0f);

	UFUNCTION(Exec)
	void HeistGlueTrapPlaceAt(float TargetX, float TargetY, float TargetZ);

#pragma endregion
};
