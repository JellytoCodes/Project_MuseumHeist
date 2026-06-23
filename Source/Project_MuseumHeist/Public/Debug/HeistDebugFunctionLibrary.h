#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "HeistDebugFunctionLibrary.generated.h"

UENUM(BlueprintType)
enum class EHeistDebugLevel : uint8
{
	Info,
	Warning,
	Error
};

UCLASS()
class PROJECT_MUSEUMHEIST_API UHeistDebugFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

#pragma region Logging

public:
	UFUNCTION(
		BlueprintCallable,
		Category = "Heist|Debug",
		meta = (
			DevelopmentOnly,
			WorldContext = "WorldContextObject",
			AdvancedDisplay = "bPrintToScreen,Duration"))
	static void Message(
		const UObject* WorldContextObject,
		const FString& Message,
		EHeistDebugLevel Level = EHeistDebugLevel::Info,
		bool bPrintToScreen = false,
		float Duration = 3.0f);

#pragma endregion

#pragma region InventoryDebug

public:
	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Inventory", meta = (DevelopmentOnly))
	static void DebugInventoryDump(class APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Inventory", meta = (DevelopmentOnly))
	static void DebugInventoryOpen(class APlayerController* PlayerController, bool bOpen);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Inventory", meta = (DevelopmentOnly))
	static void DebugInventoryMove(class APlayerController* PlayerController, int32 InstanceId, int32 GridX, int32 GridY);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Inventory", meta = (DevelopmentOnly))
	static void DebugInventoryRotate(class APlayerController* PlayerController, int32 InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Inventory", meta = (DevelopmentOnly))
	static void DebugInventoryDrop(class APlayerController* PlayerController, int32 InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Inventory", meta = (DevelopmentOnly))
	static void DebugInventoryAssignQuickSlot(
		class APlayerController* PlayerController,
		const FString& SlotName,
		int32 InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Inventory", meta = (DevelopmentOnly))
	static void DebugInventoryClearQuickSlot(class APlayerController* PlayerController, const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Inventory", meta = (DevelopmentOnly))
	static void DebugInventoryInvalidMove(class APlayerController* PlayerController, int32 InstanceId);

#pragma endregion
};
