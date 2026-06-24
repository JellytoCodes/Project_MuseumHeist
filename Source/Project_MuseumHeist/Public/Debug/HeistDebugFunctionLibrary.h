#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "GameplayTagContainer.h"
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

#pragma region GameplayDebug

public:
	static void DebugMissingInputAsset(const UObject* WorldContextObject, const TCHAR* AssetName);
	static void DebugInventoryOpenSkipped(const UObject* WorldContextObject);
	static void DebugInventoryRequestRejected(
		const UObject* WorldContextObject,
		const TCHAR* RequestName,
		int32 InstanceId,
		const TCHAR* Reason);
	static void DebugInventoryDropAccepted(
		const UObject* WorldContextObject,
		const UObject* Character,
		FName ItemId,
		int32 InstanceId,
		const UObject* DroppedLootActor,
		const FVector& DropOrigin);
	static void DebugInventoryItemDefinitionLookupRejected(FName ItemId, const TCHAR* Reason);
	static void DebugInventoryAddRejected(
		const UObject* OwnerActor,
		FName ItemId,
		const TCHAR* Reason,
		int32 GridColumnCount = INDEX_NONE,
		int32 GridRowCount = INDEX_NONE);
	static void DebugInventoryItemAdded(
		const UObject* OwnerActor,
		FName ItemId,
		int32 InstanceId,
		const FIntPoint& GridPosition,
		const FIntPoint& PlacedSize,
		bool bRotated,
		int32 ItemCount);
	static void DebugInventoryItemMoved(const UObject* OwnerActor, int32 InstanceId, const FIntPoint& GridPosition);
	static void DebugInventoryItemRotated(const UObject* OwnerActor, int32 InstanceId, bool bRotated);
	static void DebugInventoryItemRemoved(
		const UObject* OwnerActor,
		FName ItemId,
		int32 InstanceId,
		int32 ItemCount);
	static void DebugQuickSlotAssigned(
		const UObject* OwnerActor,
		int32 SlotTypeValue,
		int32 InstanceId,
		FName ItemId);
	static void DebugInventoryOccupancyInvalid(
		int32 InstanceId,
		FName ItemId,
		const TCHAR* Reason,
		const FIntPoint& GridPosition = FIntPoint::ZeroValue,
		const FIntPoint& ItemSize = FIntPoint::ZeroValue);

	static void DebugLootPickupRequestReceived(
		const UObject* WorldContextObject,
		const UObject* Character,
		const UObject* TargetLootActor);
	static void DebugLootPickupRequestRejected(
		const UObject* WorldContextObject,
		const UObject* TargetLootActor,
		const TCHAR* Reason,
		float Distance = -1.0f);
	static void DebugLootPickupRequestAccepted(
		const UObject* WorldContextObject,
		const UObject* TargetLootActor,
		FName ItemId,
		int32 InstanceId,
		float Distance);
	static void DebugLootDataFallbackApplied(const UObject* WorldContextObject, FName LootRowName);

	static void DebugEscapeRequestRejected(
		const UObject* WorldContextObject,
		const UObject* TargetVentActor,
		const TCHAR* Reason,
		float Distance = -1.0f);
	static void DebugEscapeRequestAccepted(
		const UObject* WorldContextObject,
		const UObject* Character,
		const UObject* TargetVentActor,
		float Distance);
	static void DebugEscapeCastStarted(
		const UObject* WorldContextObject,
		const UObject* Character,
		const UObject* TargetVentActor,
		float DurationSeconds,
		float EndServerTime);
	static void DebugEscapeCastStateReplicated(
		const UObject* WorldContextObject,
		const UObject* Character,
		bool bIsActive,
		float EndServerTime);
	static void DebugEscapeCastCompleted(
		const UObject* WorldContextObject,
		const UObject* Character,
		const UObject* TargetVentActor);
	static void DebugEscapeCastCancelled(
		const UObject* WorldContextObject,
		const FString& CharacterName,
		const FString& VentName,
		const TCHAR* Reason);

	static void DebugLootScoreWeightRejected(
		const UObject* WorldContextObject,
		const TCHAR* Reason,
		int32 ScoreDelta = INDEX_NONE,
		float WeightDelta = -1.0f);
	static void DebugLootScoreWeightApplied(
		const UObject* WorldContextObject,
		int32 ScoreDelta,
		float WeightDelta,
		int32 TotalScore,
		float TotalWeight);
	static void DebugLootScoreWeightRemoved(
		const UObject* WorldContextObject,
		int32 ScoreDelta,
		float WeightDelta,
		int32 TotalScore,
		float TotalWeight);
	static void DebugPlayerEscapeStateRejected(const UObject* WorldContextObject, const TCHAR* Reason);
	static void DebugPlayerEscapeStateCommitted(
		const UObject* WorldContextObject,
		int32 HeistPlayerId,
		int32 FinalScore,
		float EscapeTimeSeconds);
	static void DebugPlayerEscapeStateReplicated(
		const UObject* WorldContextObject,
		int32 HeistPlayerId,
		bool bEscaped);
	static void DebugPlayerStateScoreReplicated(const UObject* WorldContextObject, int32 TotalLootScore);
	static void DebugPlayerStateWeightReplicated(const UObject* WorldContextObject, float TotalLootWeight);

	static void DebugWeightMovementSkipped(const UObject* WorldContextObject, const TCHAR* Reason);
	static void DebugWeightMovementSpeedApplied(
		const UObject* WorldContextObject,
		float TotalWeight,
		float BaseSpeed,
		float FinalSpeed);
	static void DebugStatusTagApplied(const UObject* WorldContextObject, const FGameplayTag& StateTag, float EndServerTime);
	static void DebugStatusTagCleared(const UObject* WorldContextObject, const FGameplayTag& StateTag);
	static void DebugStatusTagsReplicated(const UObject* WorldContextObject, const TArray<FHeistTimedTagState>& StatusTags);
	static void DebugThrowableUseRejected(
		const UObject* WorldContextObject,
		EHeistQuickSlotType SlotType,
		FName ItemId,
		const TCHAR* Reason);
	static void DebugThrowableProjectileSpawned(
		const UObject* WorldContextObject,
		const UObject* Character,
		const UObject* Projectile,
		FName ItemId,
		const FVector& TargetWorldLocation,
		float ProjectileSpeed,
		bool bDebugBypassInventory);
	static void DebugThrowableProjectileImpact(
		const UObject* WorldContextObject,
		const UObject* Projectile,
		const UObject* OtherActor,
		FName ItemId,
		const FVector& ImpactLocation);
	static void DebugCoinProjectileDamageApplied(
		const UObject* WorldContextObject,
		const UObject* Projectile,
		const UObject* HitCharacter,
		float Damage);
	static void DebugCoinProjectileStunApplied(
		const UObject* WorldContextObject,
		const UObject* Projectile,
		const UObject* HitCharacter,
		float DurationSeconds);
	static void DebugCoinProjectileStunRejected(
		const UObject* WorldContextObject,
		const UObject* Projectile,
		const UObject* HitCharacter,
		const TCHAR* Reason);
	static void DebugSoundPingReported(const UObject* WorldContextObject, const FHeistSoundPingEvent& SoundPingEvent);
	static void DebugSoundPingReplicated(const UObject* WorldContextObject, const FHeistSoundPingEvent& SoundPingEvent);
	static void DebugEscapedPlayerRestrictionsApplied(const UObject* WorldContextObject);
	static void DebugResultScreenShowSkipped(
		const UObject* HUD,
		const UObject* ViewModel,
		const UClass* WidgetClass);
	static void DebugWidgetMissingMVVMView(const UObject* Widget, const TCHAR* WidgetRole);

#pragma endregion

#pragma region InventoryDebug

public:
	static void DebugInventoryHelp(class APlayerController* PlayerController);

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

#pragma region StatusDebug

public:
	static void DebugStatusHelp(class APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Status", meta = (DevelopmentOnly))
	static void DebugStatusDump(class APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Status", meta = (DevelopmentOnly, ClampMin = "0.0", Units = "s"))
	static void DebugStatusStun(class APlayerController* PlayerController, float DurationSeconds);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Status", meta = (DevelopmentOnly, ClampMin = "0.0", Units = "s"))
	static void DebugStatusImmune(class APlayerController* PlayerController, float DurationSeconds);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Status", meta = (DevelopmentOnly))
	static void DebugStatusClear(class APlayerController* PlayerController);

#pragma endregion

#pragma region ThrowableDebug

public:
	static void DebugThrowableHelp(class APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Throwable", meta = (DevelopmentOnly))
	static void DebugCoinThrow(class APlayerController* PlayerController, float Distance);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Throwable", meta = (DevelopmentOnly))
	static void DebugCoinThrowAt(class APlayerController* PlayerController, float TargetX, float TargetY, float TargetZ);

#pragma endregion
};
