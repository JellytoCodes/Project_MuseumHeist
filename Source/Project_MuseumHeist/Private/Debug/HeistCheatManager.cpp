#include "Debug/HeistCheatManager.h"

#include "Debug/HeistDebugFunctionLibrary.h"
#include "GameFramework/PlayerController.h"

#pragma region Construction

UHeistCheatManager::UHeistCheatManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#pragma endregion

#pragma region GuardDebug

void UHeistCheatManager::HeistGuardHelp()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugGuardHelp(GetOuterAPlayerController());
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

void UHeistCheatManager::HeistGuardState(
	const FString& StateName,
	const float DurationSeconds)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugGuardSetState(
		GetOuterAPlayerController(),
		StateName,
		DurationSeconds);
#endif
}

void UHeistCheatManager::HeistGuardStun(const float DurationSeconds)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugGuardApplyStun(
		GetOuterAPlayerController(),
		DurationSeconds);
#endif
}

void UHeistCheatManager::HeistGuardSightCheck()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugGuardSightCheck(
		GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistGuardSightAuto(const int32 Enabled)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugGuardAutomaticSight(
		GetOuterAPlayerController(),
		Enabled != 0);
#endif
}

void UHeistCheatManager::HeistGuardNoise(const float Distance)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugGuardNoise(
		GetOuterAPlayerController(),
		Distance);
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

void UHeistCheatManager::HeistStatusStun(const float DurationSeconds)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugStatusStun(GetOuterAPlayerController(), DurationSeconds);
#endif
}

void UHeistCheatManager::HeistStatusImmune(const float DurationSeconds)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugStatusImmune(GetOuterAPlayerController(), DurationSeconds);
#endif
}

void UHeistCheatManager::HeistStatusClear()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugStatusClear(GetOuterAPlayerController());
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

void UHeistCheatManager::HeistSmokeThrow(const float Distance)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugSmokeThrow(GetOuterAPlayerController(), Distance);
#endif
}

void UHeistCheatManager::HeistSmokeThrowAt(const float TargetX, const float TargetY, const float TargetZ)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugSmokeThrowAt(GetOuterAPlayerController(), TargetX, TargetY, TargetZ);
#endif
}

void UHeistCheatManager::HeistSmokeSightCheck(const float Distance)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugSmokeSightCheck(GetOuterAPlayerController(), Distance);
#endif
}

void UHeistCheatManager::HeistSmokeSightCheckAt(const float TargetX, const float TargetY, const float TargetZ)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugSmokeSightCheckAt(GetOuterAPlayerController(), TargetX, TargetY, TargetZ);
#endif
}

#pragma endregion

#pragma region RareLootDebug

void UHeistCheatManager::HeistRareLootHelp()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugRareLootHelp(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistRareLootForce(const float WarningDelaySeconds)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugForceRareLootEvent(GetOuterAPlayerController(), WarningDelaySeconds);
#endif
}

void UHeistCheatManager::HeistRareLootDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugDumpRareLootState(GetOuterAPlayerController());
#endif
}

#pragma endregion

#pragma region GapTrackerDebug

void UHeistCheatManager::HeistGapHelp()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugGapTrackerHelp(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistGapDump()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugDumpGapTrackerState(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistGapScore(const int32 Score)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugSetGapTrackerScore(GetOuterAPlayerController(), Score);
#endif
}

void UHeistCheatManager::HeistGapForce(const bool bActive)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugForceGapTracker(GetOuterAPlayerController(), bActive);
#endif
}

void UHeistCheatManager::HeistGapAuto()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugClearGapTrackerOverride(GetOuterAPlayerController());
#endif
}

#pragma endregion

#pragma region TrapDebug

void UHeistCheatManager::HeistTrapHelp()
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugTrapHelp(GetOuterAPlayerController());
#endif
}

void UHeistCheatManager::HeistGlueTrapPlace(const float Distance)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugGlueTrapPlace(GetOuterAPlayerController(), Distance);
#endif
}

void UHeistCheatManager::HeistGlueTrapPlaceAt(const float TargetX, const float TargetY, const float TargetZ)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugGlueTrapPlaceAt(GetOuterAPlayerController(), TargetX, TargetY, TargetZ);
#endif
}

#pragma endregion
