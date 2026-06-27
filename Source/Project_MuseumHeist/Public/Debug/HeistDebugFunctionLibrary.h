#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "HeistDebugFunctionLibrary.generated.h"

class APlayerController;

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
	UFUNCTION(BlueprintCallable, Category = "Heist|Debug", meta = (DevelopmentOnly, WorldContext = "WorldContextObject", AdvancedDisplay = "bPrintToScreen,Duration"))
	static void Message(const UObject* WorldContextObject, const FString& Message, EHeistDebugLevel Level = EHeistDebugLevel::Info, bool bPrintToScreen = false, float Duration = 3.0f);

#pragma endregion

#pragma region GameplayDebug

public:
	static void DebugMissingInputAsset(const UObject* WorldContextObject, const TCHAR* AssetName);
	static void DebugInventoryOpenSkipped(const UObject* WorldContextObject);
	static void DebugInventoryRequestRejected(const UObject* WorldContextObject, const TCHAR* RequestName, int32 InstanceId, const TCHAR* Reason);
	static void DebugInventoryDropAccepted(const UObject* WorldContextObject, const UObject* Character, FName ItemId, int32 InstanceId, const UObject* DroppedLootActor, const FVector& DropOrigin);
	static void DebugPinataDropAccepted(const UObject* WorldContextObject, const UObject* Character, const UObject* DropInstigator, FName ItemId, int32 InstanceId, const UObject* DroppedLootActor, const FVector& DropOrigin);
	static void DebugPinataDropSkipped(const UObject* WorldContextObject, const TCHAR* Reason);
	static void DebugInventoryItemDefinitionLookupRejected(FName ItemId, const TCHAR* Reason);
	static void DebugInventoryAddRejected(const UObject* OwnerActor, FName ItemId, const TCHAR* Reason, int32 GridColumnCount = INDEX_NONE, int32 GridRowCount = INDEX_NONE);
	static void DebugInventoryItemAdded(const UObject* OwnerActor, FName ItemId, int32 InstanceId, const FIntPoint& GridPosition, const FIntPoint& PlacedSize, bool bRotated, int32 ItemCount);
	static void DebugInventoryItemMoved(const UObject* OwnerActor, int32 InstanceId, const FIntPoint& GridPosition);
	static void DebugInventoryItemRotated(const UObject* OwnerActor, int32 InstanceId, bool bRotated);
	static void DebugInventoryItemRemoved(const UObject* OwnerActor, FName ItemId, int32 InstanceId, int32 ItemCount);
	static void DebugInventoryOccupancyInvalid(int32 InstanceId, FName ItemId, const TCHAR* Reason, const FIntPoint& GridPosition = FIntPoint::ZeroValue, const FIntPoint& ItemSize = FIntPoint::ZeroValue);
	static void DebugQuickSlotAssigned(const UObject* OwnerActor, int32 SlotTypeValue, int32 InstanceId, FName ItemId);

	static void DebugLootPickupRequestReceived(const UObject* WorldContextObject, const UObject* Character, const UObject* TargetLootActor);
	static void DebugLootPickupRequestRejected(const UObject* WorldContextObject, const UObject* TargetLootActor, const TCHAR* Reason, float Distance = -1.0f);
	static void DebugLootPickupRequestAccepted(const UObject* WorldContextObject, const UObject* TargetLootActor, FName ItemId, int32 InstanceId, float Distance);
	static void DebugLootDataFallbackApplied(const UObject* WorldContextObject, FName LootRowName);

	static void DebugEscapeRequestRejected(const UObject* WorldContextObject, const UObject* TargetVentActor, const TCHAR* Reason, float Distance = -1.0f);
	static void DebugEscapeRequestAccepted(const UObject* WorldContextObject, const UObject* Character, const UObject* TargetVentActor, float Distance);
	static void DebugEscapeCastStarted(const UObject* WorldContextObject, const UObject* Character, const UObject* TargetVentActor, float DurationSeconds, float EndServerTime);
	static void DebugEscapeCastStateReplicated(const UObject* WorldContextObject, const UObject* Character, bool bIsActive, float EndServerTime);
	static void DebugEscapeCastCompleted(const UObject* WorldContextObject, const UObject* Character, const UObject* TargetVentActor);
	static void DebugEscapeCastCancelled(const UObject* WorldContextObject, const FString& CharacterName, const FString& VentName, const TCHAR* Reason);

	static void DebugLootScoreWeightRejected(const UObject* WorldContextObject, const TCHAR* Reason, int32 ScoreDelta = INDEX_NONE, float WeightDelta = -1.0f);
	static void DebugLootScoreWeightApplied(const UObject* WorldContextObject, int32 ScoreDelta, float WeightDelta, int32 TotalScore, float TotalWeight);
	static void DebugLootScoreWeightRemoved(const UObject* WorldContextObject, int32 ScoreDelta, float WeightDelta, int32 TotalScore, float TotalWeight);
	static void DebugPlayerEscapeStateRejected(const UObject* WorldContextObject, const TCHAR* Reason);
	static void DebugPlayerEscapeStateCommitted(const UObject* WorldContextObject, int32 HeistPlayerId, int32 FinalScore, float EscapeTimeSeconds);
	static void DebugPlayerEscapeStateReplicated(const UObject* WorldContextObject, int32 HeistPlayerId, bool bEscaped);
	static void DebugPlayerStateScoreReplicated(const UObject* WorldContextObject, int32 TotalLootScore);
	static void DebugPlayerStateWeightReplicated(const UObject* WorldContextObject, float TotalLootWeight);

	static void DebugWeightMovementSkipped(const UObject* WorldContextObject, const TCHAR* Reason);
	static void DebugWeightMovementSpeedApplied(const UObject* WorldContextObject, float TotalWeight, float BaseSpeed, float FinalSpeed);
	static void DebugStatusTagApplied(const UObject* WorldContextObject, const FGameplayTag& StateTag, float EndServerTime);
	static void DebugStatusTagCleared(const UObject* WorldContextObject, const FGameplayTag& StateTag);
	static void DebugStatusTagsReplicated(const UObject* WorldContextObject, const TArray<FHeistTimedTagState>& StatusTags);
	static void DebugThrowableUseRejected(const UObject* WorldContextObject, EHeistQuickSlotType SlotType, FName ItemId, const TCHAR* Reason);
	static void DebugThrowableProjectileSpawned(const UObject* WorldContextObject, const UObject* Character, const UObject* Projectile, FName ItemId, const FVector& TargetWorldLocation, float ProjectileSpeed, bool bDebugBypassInventory);
	static void DebugThrowableProjectileImpact(const UObject* WorldContextObject, const UObject* Projectile, const UObject* OtherActor, FName ItemId, const FVector& ImpactLocation);
	static void DebugCoinProjectileDamageApplied(const UObject* WorldContextObject, const UObject* Projectile, const UObject* HitCharacter, float Damage);
	static void DebugCoinProjectileStunApplied(const UObject* WorldContextObject, const UObject* Projectile, const UObject* HitCharacter, float DurationSeconds);
	static void DebugCoinProjectileStunRejected(const UObject* WorldContextObject, const UObject* Projectile, const UObject* HitCharacter, const TCHAR* Reason);
	static void DebugSmokeCloudSpawned(const UObject* WorldContextObject, const UObject* Projectile, const UObject* SmokeCloud, FName ItemId, const FVector& WorldLocation, float Radius, float DurationSeconds);
	static void DebugSmokeCloudStateReplicated(const UObject* WorldContextObject, const UObject* SmokeCloud, float Radius, float EndServerTime, bool bBlocksAISight);
	static void DebugSmokeCloudOverlapChanged(const UObject* WorldContextObject, const UObject* SmokeCloud, const UObject* Actor, bool bInsideSmoke, float RemainingSeconds);
	static void DebugSmokeSightQuery(const UObject* WorldContextObject, const UObject* SmokeCloud, const FVector& FromLocation, const FVector& ToLocation, bool bBlocked);
	static void DebugRareLootTimersStarted(const UObject* WorldContextObject, const TArray<float>& EventTimes, float WarningLeadTime);
	static void DebugRareLootWarningStarted(const UObject* WorldContextObject, int32 EventIndex, FName ItemId, float SpawnServerTime);
	static void DebugRareLootSpawned(const UObject* WorldContextObject, int32 EventIndex, const UObject* LootActor, const UObject* SpawnPoint, FName ItemId, const FVector& WorldLocation);
	static void DebugRareLootEventFailed(const UObject* WorldContextObject, int32 EventIndex, const TCHAR* Reason);
	static void DebugRareLootPickedUp(const UObject* WorldContextObject, int32 EventIndex, const UObject* LootActor, const UObject* Requester, FName ItemId);
	static void DebugRareLootStateReplicated(const UObject* WorldContextObject, const FHeistRareLootEventState& EventState);
	static void DebugGapTrackerTimerStarted(const UObject* WorldContextObject, float UpdateInterval);
	static void DebugGapTrackerStateChanged(const UObject* WorldContextObject, bool bActive, int32 LeaderPlayerId);
	static void DebugGapTrackerStateReplicated(const UObject* WorldContextObject, bool bActive, int32 LeaderPlayerId);
	static void DebugGapTrackerDirectionUpdated(const UObject* WorldContextObject, int32 PlayerId, const FVector& Direction);
	static void DebugGapTrackerDirectionReplicated(const UObject* WorldContextObject, int32 PlayerId, const FVector& Direction);
	static void DebugGapTrackerOverrideChanged(const UObject* WorldContextObject, bool bOverrideEnabled, bool bForcedActive);
	static void DebugGapTrackerScoreSet(const UObject* WorldContextObject, int32 PlayerId, int32 Score);
	static void DebugTrapPlacementCastStarted(const UObject* WorldContextObject, const UObject* Character, FName ItemId, const FVector& TargetWorldLocation, float DurationSeconds, float EndServerTime);
	static void DebugTrapPlacementCastStateReplicated(const UObject* WorldContextObject, const UObject* Character, bool bIsActive, float EndServerTime);
	static void DebugTrapPlacementCastCancelled(const UObject* WorldContextObject, const FString& CharacterName, FName ItemId, const TCHAR* Reason);
	static void DebugTrapPlaced(const UObject* WorldContextObject, const UObject* Character, const UObject* TrapActor, FName ItemId, const FVector& WorldLocation);
	static void DebugTrapTriggered(const UObject* WorldContextObject, const UObject* TrapActor, const UObject* TriggeringActor, FName ItemId, float DurationSeconds);
	static void DebugTrapTriggerRejected(const UObject* WorldContextObject, const UObject* TrapActor, const UObject* TriggeringActor, const TCHAR* Reason);
	static void DebugGuardStunApplied(const UObject* WorldContextObject, const UObject* GuardActor, float DurationSeconds);
	static void DebugGuardStunCleared(const UObject* WorldContextObject, const UObject* GuardActor, EHeistGuardState NewState);
	static void DebugGuardStateReplicated(const UObject* WorldContextObject, const UObject* GuardActor, EHeistGuardState NewState);
	static void DebugSoundPingReported(const UObject* WorldContextObject, const FHeistSoundPingEvent& SoundPingEvent);
	static void DebugSoundPingReplicated(const UObject* WorldContextObject, const FHeistSoundPingEvent& SoundPingEvent);
	static void DebugEscapedPlayerRestrictionsApplied(const UObject* WorldContextObject);
	static void DebugResultScreenShowSkipped(const UObject* HUD, const UObject* ViewModel, const UClass* WidgetClass);
	static void DebugWidgetMissingMVVMView(const UObject* Widget, const TCHAR* WidgetRole);

#pragma endregion

#pragma region InventoryDebug

public:
	static void DebugInventoryHelp(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Inventory", meta = (DevelopmentOnly))
	static void DebugInventoryDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Inventory", meta = (DevelopmentOnly))
	static void DebugInventoryOpen(APlayerController* PlayerController, bool bOpen);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Inventory", meta = (DevelopmentOnly))
	static void DebugInventoryMove(APlayerController* PlayerController, int32 InstanceId, int32 GridX, int32 GridY);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Inventory", meta = (DevelopmentOnly))
	static void DebugInventoryRotate(APlayerController* PlayerController, int32 InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Inventory", meta = (DevelopmentOnly))
	static void DebugInventoryDrop(APlayerController* PlayerController, int32 InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Inventory", meta = (DevelopmentOnly))
	static void DebugInventoryAssignQuickSlot(APlayerController* PlayerController, const FString& SlotName, int32 InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Inventory", meta = (DevelopmentOnly))
	static void DebugInventoryClearQuickSlot(APlayerController* PlayerController, const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Inventory", meta = (DevelopmentOnly))
	static void DebugInventoryInvalidMove(APlayerController* PlayerController, int32 InstanceId);

#pragma endregion

#pragma region StatusDebug

public:
	static void DebugStatusHelp(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Status", meta = (DevelopmentOnly))
	static void DebugStatusDump(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Status", meta = (DevelopmentOnly, ClampMin = "0.0", Units = "s"))
	static void DebugStatusStun(APlayerController* PlayerController, float DurationSeconds);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Status", meta = (DevelopmentOnly, ClampMin = "0.0", Units = "s"))
	static void DebugStatusImmune(APlayerController* PlayerController, float DurationSeconds);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Status", meta = (DevelopmentOnly))
	static void DebugStatusClear(APlayerController* PlayerController);

#pragma endregion

#pragma region ThrowableDebug

public:
	static void DebugThrowableHelp(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Throwable", meta = (DevelopmentOnly))
	static void DebugCoinThrow(APlayerController* PlayerController, float Distance);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Throwable", meta = (DevelopmentOnly))
	static void DebugCoinThrowAt(APlayerController* PlayerController, float TargetX, float TargetY, float TargetZ);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Throwable", meta = (DevelopmentOnly))
	static void DebugSmokeThrow(APlayerController* PlayerController, float Distance);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Throwable", meta = (DevelopmentOnly))
	static void DebugSmokeThrowAt(APlayerController* PlayerController, float TargetX, float TargetY, float TargetZ);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Throwable", meta = (DevelopmentOnly))
	static void DebugSmokeSightCheck(APlayerController* PlayerController, float Distance);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Throwable", meta = (DevelopmentOnly))
	static void DebugSmokeSightCheckAt(APlayerController* PlayerController, float TargetX, float TargetY, float TargetZ);

#pragma endregion

#pragma region RareLootDebug

public:
	static void DebugRareLootHelp(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|RareLoot", meta = (DevelopmentOnly))
	static void DebugForceRareLootEvent(APlayerController* PlayerController, float WarningDelaySeconds);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|RareLoot", meta = (DevelopmentOnly))
	static void DebugDumpRareLootState(APlayerController* PlayerController);

#pragma endregion

#pragma region GapTrackerDebug

public:
	static void DebugGapTrackerHelp(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|GapTracker", meta = (DevelopmentOnly))
	static void DebugDumpGapTrackerState(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|GapTracker", meta = (DevelopmentOnly))
	static void DebugSetGapTrackerScore(APlayerController* PlayerController, int32 Score);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|GapTracker", meta = (DevelopmentOnly))
	static void DebugForceGapTracker(APlayerController* PlayerController, bool bActive);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|GapTracker", meta = (DevelopmentOnly))
	static void DebugClearGapTrackerOverride(APlayerController* PlayerController);

#pragma endregion

#pragma region TrapDebug

public:
	static void DebugTrapHelp(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Trap", meta = (DevelopmentOnly))
	static void DebugGlueTrapPlace(APlayerController* PlayerController, float Distance);

	UFUNCTION(BlueprintCallable, Category = "Heist|Debug|Trap", meta = (DevelopmentOnly))
	static void DebugGlueTrapPlaceAt(APlayerController* PlayerController, float TargetX, float TargetY, float TargetZ);

#pragma endregion
};
