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

#pragma region TutorialDebug

void UHeistCheatManager::HeistTutorialHelp()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugTutorialHelp(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistTutorialDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugTutorialDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistTutorialReset()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugTutorialReset(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistTutorialAdvance()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugTutorialAdvance(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistTutorialSkip()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugTutorialSkip(GetOuterAPlayerController());
#endif
}

#pragma endregion

#pragma region BuildDebug

void UHeistCheatManager::HeistBuildDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugBuildDump(GetOuterAPlayerController());
#endif
}

#pragma endregion

#pragma region SettingsDebug

void UHeistCheatManager::HeistSettingsHelp()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugSettingsHelp(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistSettingsDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugSettingsDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistSettingsApply(const float FieldOfView, const float MouseSensitivity, const float MasterVolume, const int32 ResolutionWidth,
											const int32 ResolutionHeight, const FString WindowMode)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugSettingsApply(GetOuterAPlayerController(), FieldOfView, MouseSensitivity, MasterVolume, ResolutionWidth, ResolutionHeight, WindowMode);
#endif
}

void UHeistCheatManager::HeistSettingsReset()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugSettingsReset(GetOuterAPlayerController());
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

#pragma region DepositDebug

void UHeistCheatManager::HeistDepositHelp()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugDepositHelp(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistDepositOpen()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugDepositOpen(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistDepositSpawnFor(const int32 PlayerId, const float Distance)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugDepositSpawnFor(GetOuterAPlayerController(), PlayerId, Distance);
#endif
}

void UHeistCheatManager::HeistDepositDump(const int32 PlayerId)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugDepositDump(GetOuterAPlayerController(), PlayerId);
#endif
}

#pragma endregion

#pragma region OutcomeDebug

void UHeistCheatManager::HeistOutcomeHelp()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugOutcomeHelp(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistOutcomeSeed(const int32 SecuredValue, const int32 RequiredTargetSecured)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugOutcomeSeed(GetOuterAPlayerController(), SecuredValue, RequiredTargetSecured != 0);
#endif
}

void UHeistCheatManager::HeistOutcomeResolve(const FString TerminalTrigger)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugOutcomeResolve(GetOuterAPlayerController(), TerminalTrigger);
#endif
}

void UHeistCheatManager::HeistOutcomeDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugOutcomeDump(GetOuterAPlayerController());
#endif
}

#pragma endregion

#pragma region ObjectAssemblyDebug

void UHeistCheatManager::HeistObjectAssemblyHelp()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugObjectAssemblyHelp(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistObjectAssemblySpawn(const float Distance)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugObjectAssemblySpawn(GetOuterAPlayerController(), Distance);
#endif
}

void UHeistCheatManager::HeistObjectAssemblySpawnFor(const int32 PlayerId, const float Distance)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugObjectAssemblySpawnFor(GetOuterAPlayerController(), PlayerId, Distance);
#endif
}

void UHeistCheatManager::HeistObjectAssemblyContentSpawn(const FString Family, const int32 Variant, const float Distance)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugObjectAssemblyContentSpawn(GetOuterAPlayerController(), Family, Variant, Distance);
#endif
}

void UHeistCheatManager::HeistObjectAssemblyContentSpawnFor(const int32 PlayerId, const FString Family, const int32 Variant, const float Distance)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugObjectAssemblyContentSpawnFor(GetOuterAPlayerController(), PlayerId, Family, Variant, Distance);
#endif
}

void UHeistCheatManager::HeistObjectAssemblyKickPlayer(const int32 PlayerId)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugObjectAssemblyKickPlayer(GetOuterAPlayerController(), PlayerId);
#endif
}

void UHeistCheatManager::HeistObjectAssemblyBegin(const float DurationSeconds)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugObjectAssemblyBegin(GetOuterAPlayerController(), DurationSeconds);
#endif
}

void UHeistCheatManager::HeistObjectAssemblyTest(const FString Scenario)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugObjectAssemblyTest(GetOuterAPlayerController(), Scenario);
#endif
}

void UHeistCheatManager::HeistObjectAssemblyDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugObjectAssemblyDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistObjectAssemblyUIDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugObjectAssemblyUIDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistObjectAssemblyCancel()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugObjectAssemblyCancel(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistObjectAssemblyTimeout()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugObjectAssemblyTimeout(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistObjectAssemblyReplicaDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugObjectAssemblyReplicaDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistObjectAssemblyPrototypeGate()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugObjectAssemblyPrototypeGate(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistObjectAssemblyContentValidate()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugObjectAssemblyContentValidate(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistObjectAssemblyReplicaRebuild()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugObjectAssemblyReplicaRebuild(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistObjectAssemblyTakeOriginal()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugObjectAssemblyTakeOriginal(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistObjectAssemblyInspectionReady()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugObjectAssemblyInspectionReady(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistObjectAssemblyInspectionGate()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugObjectAssemblyInspectionGate(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistObjectAssemblyTestIsolation(const int32 Enabled)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugObjectAssemblyTestIsolation(GetOuterAPlayerController(), Enabled != 0);
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

void UHeistCheatManager::HeistSurfaceTemplateDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugSurfaceTemplateDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistSurfaceTemplatePoolTest(const int32 PoolSize)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugSurfaceTemplatePoolTest(GetOuterAPlayerController(), PoolSize);
#endif
}

void UHeistCheatManager::HeistSurfaceTemplateContentValidate(FString PoolId)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugSurfaceTemplateContentValidate(GetOuterAPlayerController(), PoolId);
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

void UHeistCheatManager::HeistInspectionProtectionDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugInspectionProtectionDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistInspectionTimerProtectionTest()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugInspectionTimerProtectionTest(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistAlertDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugAlertDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistLockdownDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugLockdownDump(GetOuterAPlayerController());
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

void UHeistCheatManager::HeistSessionHost()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugOnlineSessionHost(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistSessionJoin(const FString& JoinCode)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugOnlineSessionJoin(GetOuterAPlayerController(), JoinCode);
#endif
}

void UHeistCheatManager::HeistSessionLeave()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugOnlineSessionLeave(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistSessionCancel()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugOnlineSessionCancel(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistSessionCancelTest()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugOnlineSessionCancelTest(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistSessionRetry()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugOnlineSessionRetry(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistSessionFailure(const FName FailureReason)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugOnlineSessionFailure(GetOuterAPlayerController(), FailureReason);
#endif
}

void UHeistCheatManager::HeistSessionMap(const FString& MapId)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugOnlineSessionMap(GetOuterAPlayerController(), MapId);
#endif
}

void UHeistCheatManager::HeistSessionStart()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugOnlineSessionStart(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistSessionComplete()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugOnlineSessionComplete(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistSessionReturn()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugOnlineSessionReturn(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistSessionDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugOnlineSessionDump(GetOuterAPlayerController());
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

void UHeistCheatManager::HeistContributionSeed(const int32 SurfaceForgeries, const float BestSurfaceQuality, const int32 Assemblies,
	const float BestAssemblyQuality, const int32 ArtifactsRecovered, const float CarryTimeSeconds, const int32 SecuredLootValue,
	const int32 GuardsDistracted, const int32 TeammatesRescued, const int32 AlarmsTriggered)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugContributionSeed(GetOuterAPlayerController(), SurfaceForgeries, BestSurfaceQuality, Assemblies,
		BestAssemblyQuality, ArtifactsRecovered, CarryTimeSeconds, SecuredLootValue, GuardsDistracted, TeammatesRescued, AlarmsTriggered);
#endif
}

void UHeistCheatManager::HeistContributionDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugContributionDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistExitPlacementDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugExitPlacementDump(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistMissionGateDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugMissionGateDump(GetOuterAPlayerController());
#endif
}

#pragma endregion
