#include "Debug/HeistCheatManager.h"

#include "Debug/HeistDebugFunctionLibrary.h"
#include "GameFramework/PlayerController.h"

#pragma region Construction

UHeistCheatManager::UHeistCheatManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#pragma endregion

#pragma region InventoryDebug

void UHeistCheatManager::HeistInvHelp()
{
	APlayerController* PlayerController = GetOuterAPlayerController();
	UHeistDebugFunctionLibrary::Message(
		PlayerController,
		TEXT("Inventory debug commands: HeistInvDump | HeistInvOpen 1/0 | HeistInvMove <InstanceId> <X> <Y> | HeistInvRotate <InstanceId> | HeistInvDrop <InstanceId> | HeistInvAssign <Q|E|R|Coin|Smoke|Glue> <InstanceId> | HeistInvClear <Q|E|R|Coin|Smoke|Glue> | HeistInvInvalidMove <InstanceId>"),
		EHeistDebugLevel::Info,
		true,
		8.0f);
}

void UHeistCheatManager::HeistInvDump()
{
	UHeistDebugFunctionLibrary::DebugInventoryDump(GetOuterAPlayerController());
}

void UHeistCheatManager::HeistInvOpen(const bool bOpen)
{
	UHeistDebugFunctionLibrary::DebugInventoryOpen(GetOuterAPlayerController(), bOpen);
}

void UHeistCheatManager::HeistInvMove(const int32 InstanceId, const int32 GridX, const int32 GridY)
{
	UHeistDebugFunctionLibrary::DebugInventoryMove(GetOuterAPlayerController(), InstanceId, GridX, GridY);
}

void UHeistCheatManager::HeistInvRotate(const int32 InstanceId)
{
	UHeistDebugFunctionLibrary::DebugInventoryRotate(GetOuterAPlayerController(), InstanceId);
}

void UHeistCheatManager::HeistInvDrop(const int32 InstanceId)
{
	UHeistDebugFunctionLibrary::DebugInventoryDrop(GetOuterAPlayerController(), InstanceId);
}

void UHeistCheatManager::HeistInvAssign(const FString& SlotName, const int32 InstanceId)
{
	UHeistDebugFunctionLibrary::DebugInventoryAssignQuickSlot(GetOuterAPlayerController(), SlotName, InstanceId);
}

void UHeistCheatManager::HeistInvClear(const FString& SlotName)
{
	UHeistDebugFunctionLibrary::DebugInventoryClearQuickSlot(GetOuterAPlayerController(), SlotName);
}

void UHeistCheatManager::HeistInvInvalidMove(const int32 InstanceId)
{
	UHeistDebugFunctionLibrary::DebugInventoryInvalidMove(GetOuterAPlayerController(), InstanceId);
}

#pragma endregion
