#include "Debug/HeistCheatManager.h"

#include "Debug/HeistDebugFunctionLibrary.h"
#include "GameFramework/PlayerController.h"

#pragma region Construction

UHeistCheatManager::UHeistCheatManager(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

#pragma endregion

#pragma region HUDDebug

void UHeistCheatManager::HeistHUDDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugFirstPersonHUDDump(GetOuterAPlayerController());
#endif
}

#pragma endregion

#pragma region ObjectiveDebug

void UHeistCheatManager::HeistObjectiveHelp()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugObjectiveHelp(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistObjectiveDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugObjectiveDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistM01ObjectiveDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugM01ObjectivePlacementDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistGrayboxDump(const FString& MapId)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugCoreGrayboxDump(GetOuterAPlayerController(), MapId);
#endif
}

void UHeistCheatManager::HeistObjectiveSet(const FString& ArtifactId, const FString& CaseId, const FString& StateName, const int32 UseLocalPlayerAsCarrier)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugObjectiveSet(GetOuterAPlayerController(), FName(*ArtifactId), FName(*CaseId), StateName, UseLocalPlayerAsCarrier != 0);
#endif
}

#pragma endregion

#pragma region DisplayCaseDebug

void UHeistCheatManager::HeistCaseHelp()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugDisplayCaseHelp(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistCaseSpawn(const float Distance)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugDisplayCaseSpawn(GetOuterAPlayerController(), Distance);
#endif
}

void UHeistCheatManager::HeistCaseSpawnFor(const int32 PlayerId, const float Distance)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugDisplayCaseSpawnFor(GetOuterAPlayerController(), PlayerId, Distance);
#endif
}

void UHeistCheatManager::HeistCaseDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugDisplayCaseDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistCaseAdvance()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugDisplayCaseAdvance(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistCaseSet(const FString& StateName)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugDisplayCaseSet(GetOuterAPlayerController(), StateName);
#endif
}

void UHeistCheatManager::HeistCaseBegin(const int32 PlayerId)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugDisplayCaseBegin(GetOuterAPlayerController(), PlayerId);
#endif
}

void UHeistCheatManager::HeistCaseCancel(const int32 PlayerId)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugDisplayCaseCancel(GetOuterAPlayerController(), PlayerId);
#endif
}

void UHeistCheatManager::HeistCasePhase(const FString& PhaseName)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugDisplayCasePhase(GetOuterAPlayerController(), PhaseName);
#endif
}

void UHeistCheatManager::HeistOriginalHelp()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugOriginalHelp(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistOriginalDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugOriginalDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistOriginalTake()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugOriginalTake(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistOriginalDrop()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugOriginalDrop(GetOuterAPlayerController());
#endif
}

#pragma endregion

#pragma region ForgeryDebug

void UHeistCheatManager::HeistForgeryHelp()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugForgeryHelp(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistForgeryDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugForgeryDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistForgeryInputDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugForgeryInputDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistForgeryTemplateDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugForgeryTemplateDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistForgeryStrokeDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugForgeryStrokeDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistForgeryTransportDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugForgeryTransportDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistForgeryTransportTest(FString Scenario)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugForgeryTransportTest(GetOuterAPlayerController(), Scenario);
#endif
}

void UHeistCheatManager::HeistForgeryScoreDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugForgeryScoreDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistForgeryScoreTest()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugForgeryScoreTest(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistForgerySwapDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugForgerySwapDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistForgeryVisualDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugForgeryVisualDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistForgeryPaintingDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugForgeryPaintingDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistForgeryBegin(const float DurationSeconds)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugForgeryBegin(GetOuterAPlayerController(), DurationSeconds);
#endif
}

void UHeistCheatManager::HeistForgerySubmit()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugForgerySubmit(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistForgeryCancel()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugForgeryCancel(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistForgeryTimeout()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugForgeryTimeout(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistForgeryRecoveryDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugForgeryRecoveryDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistForgeryRecoveryRace(FString Order)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugForgeryRecoveryRace(GetOuterAPlayerController(), Order);
#endif
}

void UHeistCheatManager::HeistForgeryUIDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugForgeryUIDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistForgeryUIPreview(FString State)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugForgeryUIPreview(GetOuterAPlayerController(), State);
#endif
}

#pragma endregion

#pragma region FirstPersonScaleDebug

void UHeistCheatManager::HeistFirstPersonScaleCheck(const float ForwardDistance)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugFirstPersonScaleCheck(GetOuterAPlayerController(), ForwardDistance);
#endif
}

#pragma endregion

#pragma region GuardDebug

void UHeistCheatManager::HeistGuardHelp()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugGuardHelp(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistDifficultyDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugDifficultyDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistGuardSpawn(const float Distance)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugGuardSpawn(GetOuterAPlayerController(), Distance);
#endif
}

void UHeistCheatManager::HeistGuardDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugGuardDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistInspectionTargetSelect()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugInspectionTargetSelect(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistInspectionTargetDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugInspectionTargetDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistInspectionBegin()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugInspectionBegin(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistInspectionStateDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugInspectionStateDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistAlertDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugAlertDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistGuardAlertModifiersDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugGuardAlertModifiersDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistAlertRequest(const FString& LevelName)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugAlertRequest(GetOuterAPlayerController(), LevelName);
#endif
}

void UHeistCheatManager::HeistGuardState(const FString& StateName, const float DurationSeconds)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugGuardSetState(GetOuterAPlayerController(), StateName, DurationSeconds);
#endif
}

void UHeistCheatManager::HeistGuardSightCheck()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugGuardSightCheck(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistGuardSightAuto(const int32 Enabled)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugGuardAutomaticSight(GetOuterAPlayerController(), Enabled != 0);
#endif
}

void UHeistCheatManager::HeistGuardNoise(const float Distance)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugGuardNoise(GetOuterAPlayerController(), Distance);
#endif
}

void UHeistCheatManager::HeistArrest()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugSetPlayerArrested(GetOuterAPlayerController(), true);
#endif
}

void UHeistCheatManager::HeistRelease()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugSetPlayerArrested(GetOuterAPlayerController(), false);
#endif
}

void UHeistCheatManager::HeistArrestDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugArrestDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistFootstepWeight(const float TotalLootWeight)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugFootstepWeight(GetOuterAPlayerController(), TotalLootWeight);
#endif
}

#pragma endregion

#pragma region InventoryDebug

void UHeistCheatManager::HeistInvHelp()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugInventoryHelp(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistInvDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugInventoryDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistInvOpen(const bool bOpen)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugInventoryOpen(GetOuterAPlayerController(), bOpen);
#endif
}

void UHeistCheatManager::HeistInvAdd(const FString& ItemId)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugInventoryAdd(GetOuterAPlayerController(), FName(*ItemId));
#endif
}

void UHeistCheatManager::HeistInvMove(const int32 InstanceId, const int32 GridX, const int32 GridY)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugInventoryMove(GetOuterAPlayerController(), InstanceId, GridX, GridY);
#endif
}

void UHeistCheatManager::HeistInvRotate(const int32 InstanceId)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugInventoryRotate(GetOuterAPlayerController(), InstanceId);
#endif
}

void UHeistCheatManager::HeistInvDrop(const int32 InstanceId)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugInventoryDrop(GetOuterAPlayerController(), InstanceId);
#endif
}

void UHeistCheatManager::HeistInvAssign(const FString& SlotName, const int32 InstanceId)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugInventoryAssignQuickSlot(GetOuterAPlayerController(), SlotName, InstanceId);
#endif
}

void UHeistCheatManager::HeistInvClear(const FString& SlotName)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugInventoryClearQuickSlot(GetOuterAPlayerController(), SlotName);
#endif
}

void UHeistCheatManager::HeistInvInvalidMove(const int32 InstanceId)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugInventoryInvalidMove(GetOuterAPlayerController(), InstanceId);
#endif
}

#pragma endregion

#pragma region StatusDebug

void UHeistCheatManager::HeistStatusHelp()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugStatusHelp(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistStatusDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugStatusDump(GetOuterAPlayerController());
#endif
}

#pragma endregion

#pragma region FeedbackDebug

void UHeistCheatManager::HeistFeedbackHelp()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugFeedbackHelp(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistFeedbackTest()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugFeedbackTest(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistFeedbackBagFull()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugFeedbackBagFull(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistFeedbackDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugFeedbackDump(GetOuterAPlayerController());
#endif
}

#pragma endregion

#pragma region ThrowableDebug

void UHeistCheatManager::HeistThrowHelp()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugThrowableHelp(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistCoinThrow(const float Distance)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugCoinThrow(GetOuterAPlayerController(), Distance);
#endif
}

void UHeistCheatManager::HeistCoinThrowAt(const float TargetX, const float TargetY, const float TargetZ)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugCoinThrowAt(GetOuterAPlayerController(), TargetX, TargetY, TargetZ);
#endif
}

#pragma endregion

#pragma region SoundPingDebug

void UHeistCheatManager::HeistSoundPingHelp()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugSoundPingHelp(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistSoundPingDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugSoundPingPoolDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistSoundPingTest()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugSoundPingPoolTest(GetOuterAPlayerController());
#endif
}

#pragma endregion

#pragma region LobbyDebug

void UHeistCheatManager::HeistLobbyHelp()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugLobbyHelp(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistLobbyShow()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugLobbyShow(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistLobbyHide()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugLobbyHide(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistLobbyDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugLobbyDump(GetOuterAPlayerController());
#endif
}

#pragma endregion

#pragma region ResultDebug

void UHeistCheatManager::HeistResultHelp()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugResultHelp(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistResultShow()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugResultShow(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistResultHide()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugResultHide(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistResultDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugResultDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistResultRebuild()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugResultRebuild(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistResultSeed(const int32 Score, const bool bEscaped, const float EscapeTimeSeconds)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugResultSeed(GetOuterAPlayerController(), Score, bEscaped, EscapeTimeSeconds);
#endif
}

#pragma endregion
