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

#pragma region FirstPersonScaleDebug

public:
	UFUNCTION(Exec)
	void HeistFirstPersonScaleCheck(float ForwardDistance = 200.0f);

#pragma endregion

#pragma region HUDDebug

public:
	UFUNCTION(Exec)
	void HeistHUDDump();

#pragma endregion

#pragma region InventoryDebug

public:
	UFUNCTION(Exec)
	void HeistInvHelp();

	UFUNCTION(Exec)
	void HeistInvDump();

	UFUNCTION(Exec)
	void HeistInvOpen(bool bOpen);

	UFUNCTION(Exec)
	void HeistInvAdd(const FString& ItemId);

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

#pragma endregion

#pragma region FeedbackDebug

public:
	UFUNCTION(Exec)
	void HeistFeedbackHelp();

	UFUNCTION(Exec)
	void HeistFeedbackTest();

	UFUNCTION(Exec)
	void HeistFeedbackBagFull();

	UFUNCTION(Exec)
	void HeistFeedbackDump();

#pragma endregion

#pragma region ThrowableDebug

public:
	UFUNCTION(Exec)
	void HeistThrowHelp();

	UFUNCTION(Exec)
	void HeistCoinThrow(float Distance = 1000.0f);

	UFUNCTION(Exec)
	void HeistCoinThrowAt(float TargetX, float TargetY, float TargetZ);

#pragma endregion

#pragma region SoundPingDebug

public:
	UFUNCTION(Exec)
	void HeistSoundPingHelp();

	UFUNCTION(Exec)
	void HeistSoundPingDump();

	UFUNCTION(Exec)
	void HeistSoundPingTest();

	UFUNCTION(Exec)
	void HeistFootstepWeight(float TotalLootWeight = 0.0f);

#pragma endregion

#pragma region LobbyDebug

public:
	UFUNCTION(Exec)
	void HeistLobbyHelp();

	UFUNCTION(Exec)
	void HeistLobbyShow();

	UFUNCTION(Exec)
	void HeistLobbyHide();

	UFUNCTION(Exec)
	void HeistLobbyDump();

#pragma endregion

#pragma region ResultDebug

public:
	UFUNCTION(Exec)
	void HeistResultHelp();

	UFUNCTION(Exec)
	void HeistResultShow();

	UFUNCTION(Exec)
	void HeistResultHide();

	UFUNCTION(Exec)
	void HeistResultDump();

	UFUNCTION(Exec)
	void HeistResultRebuild();

	UFUNCTION(Exec)
	void HeistResultSeed(int32 Score = 1000, bool bEscaped = true, float EscapeTimeSeconds = 10.0f);

#pragma endregion

#pragma region ObjectiveDebug

public:
	UFUNCTION(Exec)
	void HeistObjectiveHelp();

	UFUNCTION(Exec)
	void HeistObjectiveDump();

	UFUNCTION(Exec)
	void HeistM01ObjectiveDump();

	UFUNCTION(Exec)
	void HeistGrayboxDump(const FString& MapId);

	UFUNCTION(Exec)
	void HeistObjectiveSet(
		const FString& ArtifactId,
		const FString& CaseId,
		const FString& StateName,
		int32 UseLocalPlayerAsCarrier = 0);

#pragma endregion

#pragma region DisplayCaseDebug

public:
	UFUNCTION(Exec)
	void HeistCaseHelp();

	UFUNCTION(Exec)
	void HeistCaseSpawn(float Distance = 250.0f);

	UFUNCTION(Exec)
	void HeistCaseSpawnFor(int32 PlayerId, float Distance = 150.0f);

	UFUNCTION(Exec)
	void HeistCaseDump();

	UFUNCTION(Exec)
	void HeistCaseAdvance();

	UFUNCTION(Exec)
	void HeistCaseSet(const FString& StateName);

	UFUNCTION(Exec)
	void HeistCaseBegin(int32 PlayerId = -1);

	UFUNCTION(Exec)
	void HeistCaseCancel(int32 PlayerId = -1);

	UFUNCTION(Exec)
	void HeistCasePhase(const FString& PhaseName);

	UFUNCTION(Exec)
	void HeistOriginalHelp();

	UFUNCTION(Exec)
	void HeistOriginalDump();

	UFUNCTION(Exec)
	void HeistOriginalTake();

	UFUNCTION(Exec)
	void HeistOriginalDrop();

#pragma endregion

#pragma region ForgeryDebug

public:
	UFUNCTION(Exec)
	void HeistForgeryHelp();

	UFUNCTION(Exec)
	void HeistForgeryDump();

	UFUNCTION(Exec)
	void HeistForgeryInputDump();

	UFUNCTION(Exec)
	void HeistForgeryTemplateDump();

	UFUNCTION(Exec)
	void HeistForgeryStrokeDump();

	UFUNCTION(Exec)
	void HeistForgeryBegin(float DurationSeconds = 60.0f);

	UFUNCTION(Exec)
	void HeistForgerySubmit();

	UFUNCTION(Exec)
	void HeistForgeryCancel();

	UFUNCTION(Exec)
	void HeistForgeryTimeout();

	UFUNCTION(Exec)
	void HeistForgeryUIDump();

	UFUNCTION(Exec)
	void HeistForgeryUIPreview(FString State);

#pragma endregion

#pragma region GuardDebug

public:
	UFUNCTION(Exec)
	void HeistGuardHelp();

	UFUNCTION(Exec)
	void HeistDifficultyDump();

	UFUNCTION(Exec)
	void HeistGuardSpawn(float Distance = 300.0f);

	UFUNCTION(Exec)
	void HeistGuardDump();

	UFUNCTION(Exec)
	void HeistGuardState(const FString& StateName, float DurationSeconds = 5.0f);

	UFUNCTION(Exec)
	void HeistGuardSightCheck();

	UFUNCTION(Exec)
	void HeistGuardSightAuto(int32 Enabled = 0);

	UFUNCTION(Exec)
	void HeistGuardNoise(float Distance = 500.0f);

	UFUNCTION(Exec)
	void HeistArrest();

	UFUNCTION(Exec)
	void HeistRelease();

	UFUNCTION(Exec)
	void HeistArrestDump();

#pragma endregion
};
