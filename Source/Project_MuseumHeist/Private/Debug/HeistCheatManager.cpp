#include "Debug/HeistCheatManager.h"

#include "Debug/HeistDebugFunctionLibrary.h"
#include "GameFramework/PlayerController.h"

#pragma region Construction

UHeistCheatManager::UHeistCheatManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
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

#pragma region FirstPersonScaleDebug

void UHeistCheatManager::HeistFirstPersonScaleCheck(const float ForwardDistance)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugFirstPersonScaleCheck(
		GetOuterAPlayerController(),
		ForwardDistance);
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

void UHeistCheatManager::HeistStatusSmoke(const float DurationSeconds)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugStatusSmoke(GetOuterAPlayerController(), DurationSeconds);
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

void UHeistCheatManager::HeistResultSeed(
	const int32 Score,
	const bool bEscaped,
	const float EscapeTimeSeconds)
{
#if !UE_BUILD_SHIPPING
	UHeistDebugFunctionLibrary::DebugResultSeed(
		GetOuterAPlayerController(),
		Score,
		bEscaped,
		EscapeTimeSeconds);
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
