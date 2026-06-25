#include "Debug/HeistDebugFunctionLibrary.h"

#include "Character/Components/HeistInventoryComponent.h"
#include "Character/Components/HeistStatusComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameplayTags.h"
#include "Core/HeistTypes.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistLogChannels.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagContainer.h"
#include "Inventory/HeistInventoryTypes.h"

#pragma region InternalHelpers

namespace
{
	AHeistPlayerController* ResolveHeistPlayerController(APlayerController* PlayerController)
	{
		return Cast<AHeistPlayerController>(PlayerController);
	}

	UHeistInventoryComponent* ResolveInventoryComponent(APlayerController* PlayerController)
	{
		const AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
		const AHeistPlayerCharacter* HeistCharacter = IsValid(HeistPlayerController)
			? HeistPlayerController->GetPawn<AHeistPlayerCharacter>()
			: nullptr;
		return IsValid(HeistCharacter) ? HeistCharacter->GetInventoryComponent() : nullptr;
	}

	UHeistStatusComponent* ResolveStatusComponent(APlayerController* PlayerController)
	{
		const AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
		const AHeistPlayerCharacter* HeistCharacter = IsValid(HeistPlayerController)
			? HeistPlayerController->GetPawn<AHeistPlayerCharacter>()
			: nullptr;
		return IsValid(HeistCharacter) ? HeistCharacter->GetStatusComponent() : nullptr;
	}

	const TCHAR* ToQuickSlotText(const EHeistQuickSlotType SlotType)
	{
		switch (SlotType)
		{
		case EHeistQuickSlotType::Coin:
			return TEXT("Coin");
		case EHeistQuickSlotType::SmokeGrenade:
			return TEXT("SmokeGrenade");
		case EHeistQuickSlotType::GlueTrap:
			return TEXT("GlueTrap");
		default:
			return TEXT("None");
		}
	}

	bool TryParseQuickSlotName(const FString& SlotName, EHeistQuickSlotType& OutSlotType)
	{
		const FString NormalizedSlotName = SlotName.TrimStartAndEnd().ToLower();
		if (NormalizedSlotName == TEXT("coin") || NormalizedSlotName == TEXT("q"))
		{
			OutSlotType = EHeistQuickSlotType::Coin;
			return true;
		}

		if (NormalizedSlotName == TEXT("smoke")
			|| NormalizedSlotName == TEXT("smokegrenade")
			|| NormalizedSlotName == TEXT("e"))
		{
			OutSlotType = EHeistQuickSlotType::SmokeGrenade;
			return true;
		}

		if (NormalizedSlotName == TEXT("glue")
			|| NormalizedSlotName == TEXT("gluetrap")
			|| NormalizedSlotName == TEXT("r"))
		{
			OutSlotType = EHeistQuickSlotType::GlueTrap;
			return true;
		}

		OutSlotType = EHeistQuickSlotType::None;
		return false;
	}

	FString FormatOptionalDistance(const float Distance)
	{
		return Distance >= 0.0f
			? FString::Printf(TEXT(" Distance=%.1f"), Distance)
			: FString();
	}

	FString FormatStatusTags(const TArray<FHeistTimedTagState>& StatusTags)
	{
		if (StatusTags.IsEmpty())
		{
			return TEXT("None");
		}

		TArray<FString> Entries;
		Entries.Reserve(StatusTags.Num());
		for (const FHeistTimedTagState& StatusTagState : StatusTags)
		{
			Entries.Add(FString::Printf(
				TEXT("%s@%.2f"),
				*StatusTagState.StateTag.ToString(),
				StatusTagState.EndServerTime));
		}

		return FString::Join(Entries, TEXT(", "));
	}
}

#pragma endregion

#pragma region Logging

void UHeistDebugFunctionLibrary::Message(const UObject* WorldContextObject, const FString& Message, EHeistDebugLevel Level, bool bPrintToScreen, float Duration)
{
#if UE_BUILD_SHIPPING
	return;
#else
	const FString ContextName = GetNameSafe(WorldContextObject);
	const FString FormattedMessage = FString::Printf(TEXT("[%s] %s"), *ContextName, *Message);

	switch (Level)
	{
	case EHeistDebugLevel::Warning:
		UE_LOG(LogHeist, Warning, TEXT("%s"), *FormattedMessage);
		break;
	case EHeistDebugLevel::Error:
		UE_LOG(LogHeist, Error, TEXT("%s"), *FormattedMessage);
		break;
	default:
		UE_LOG(LogHeist, Log, TEXT("%s"), *FormattedMessage);
		break;
	}

	if (!bPrintToScreen || GEngine == nullptr)
	{
		return;
	}

	FColor MessageColor = FColor::White;
	if (Level == EHeistDebugLevel::Warning)
	{
		MessageColor = FColor::Yellow;
	}
	else if (Level == EHeistDebugLevel::Error)
	{
		MessageColor = FColor::Red;
	}

	GEngine->AddOnScreenDebugMessage(
		INDEX_NONE,
		FMath::Max(0.0f, Duration),
		MessageColor,
		FormattedMessage);
#endif
}

#pragma endregion

#pragma region GameplayDebug

void UHeistDebugFunctionLibrary::DebugMissingInputAsset(const UObject* WorldContextObject, const TCHAR* AssetName)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(TEXT("%s is not assigned in the PlayerController Blueprint."), AssetName),
		EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryOpenSkipped(const UObject* WorldContextObject)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		TEXT("Inventory open request skipped: Inventory Widget/ViewModel setup is incomplete."),
		EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryRequestRejected(
	const UObject* WorldContextObject,
	const TCHAR* RequestName,
	const int32 InstanceId,
	const TCHAR* Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Inventory request rejected: Request=%s InstanceId=%d Reason=%s"),
			RequestName,
			InstanceId,
			Reason),
		EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryDropAccepted(const UObject* WorldContextObject, const UObject* Character, const FName ItemId, const int32 InstanceId, const UObject* DroppedLootActor, const FVector& DropOrigin)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Inventory drop accepted: Character=%s ItemId=%s InstanceId=%d WorldLoot=%s DropOrigin=%s"),
			*GetNameSafe(Character),
			*ItemId.ToString(),
			InstanceId,
			*GetNameSafe(DroppedLootActor),
			*DropOrigin.ToCompactString()));
#endif
}

void UHeistDebugFunctionLibrary::DebugPinataDropAccepted(
	const UObject* WorldContextObject,
	const UObject* Character,
	const UObject* DropInstigator,
	const FName ItemId,
	const int32 InstanceId,
	const UObject* DroppedLootActor,
	const FVector& DropOrigin)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Pinata drop accepted: Character=%s Instigator=%s ItemId=%s InstanceId=%d WorldLoot=%s DropOrigin=%s"),
			*GetNameSafe(Character),
			*GetNameSafe(DropInstigator),
			*ItemId.ToString(),
			InstanceId,
			*GetNameSafe(DroppedLootActor),
			*DropOrigin.ToCompactString()));
#endif
}

void UHeistDebugFunctionLibrary::DebugPinataDropSkipped(const UObject* WorldContextObject, const TCHAR* Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(TEXT("Pinata drop skipped: Reason=%s"), Reason));
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryItemDefinitionLookupRejected(const FName ItemId, const TCHAR* Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	UE_LOG(
		LogHeistInventory,
		Warning,
		TEXT("Item definition lookup rejected: ItemId=%s Reason=%s"),
		*ItemId.ToString(),
		Reason);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryAddRejected(const UObject* OwnerActor, const FName ItemId, const TCHAR* Reason, const int32 GridColumnCount, const int32 GridRowCount)
{
#if UE_BUILD_SHIPPING
	return;
#else
	if (GridColumnCount != INDEX_NONE && GridRowCount != INDEX_NONE)
	{
		UE_LOG(
			LogHeistInventory,
			Warning,
			TEXT("Inventory add rejected: Owner=%s ItemId=%s Reason=%s Grid=%dx%d"),
			*GetNameSafe(OwnerActor),
			*ItemId.ToString(),
			Reason,
			GridColumnCount,
			GridRowCount);
		return;
	}

	UE_LOG(
		LogHeistInventory,
		Warning,
		TEXT("Inventory add rejected: Owner=%s ItemId=%s Reason=%s"),
		*GetNameSafe(OwnerActor),
		*ItemId.ToString(),
		Reason);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryItemAdded(const UObject* OwnerActor, const FName ItemId, const int32 InstanceId, const FIntPoint& GridPosition, const FIntPoint& PlacedSize, const bool bRotated, const int32 ItemCount)
{
#if UE_BUILD_SHIPPING
	return;
#else
	UE_LOG(
		LogHeistInventory,
		Log,
		TEXT("Inventory item added: Owner=%s ItemId=%s InstanceId=%d Grid=(%d,%d) Size=%dx%d Rotated=%s ItemCount=%d"),
		*GetNameSafe(OwnerActor),
		*ItemId.ToString(),
		InstanceId,
		GridPosition.X,
		GridPosition.Y,
		PlacedSize.X,
		PlacedSize.Y,
		bRotated ? TEXT("true") : TEXT("false"),
		ItemCount);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryItemMoved(const UObject* OwnerActor, const int32 InstanceId, const FIntPoint& GridPosition)
{
#if UE_BUILD_SHIPPING
	return;
#else
	UE_LOG(
		LogHeistInventory,
		Log,
		TEXT("Inventory item moved: Owner=%s InstanceId=%d Grid=(%d,%d)"),
		*GetNameSafe(OwnerActor),
		InstanceId,
		GridPosition.X,
		GridPosition.Y);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryItemRotated(const UObject* OwnerActor, const int32 InstanceId, const bool bRotated)
{
#if UE_BUILD_SHIPPING
	return;
#else
	UE_LOG(
		LogHeistInventory,
		Log,
		TEXT("Inventory item rotated: Owner=%s InstanceId=%d Rotated=%s"),
		*GetNameSafe(OwnerActor),
		InstanceId,
		bRotated ? TEXT("true") : TEXT("false"));
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryItemRemoved(const UObject* OwnerActor, const FName ItemId, const int32 InstanceId, const int32 ItemCount)
{
#if UE_BUILD_SHIPPING
	return;
#else
	UE_LOG(
		LogHeistInventory,
		Log,
		TEXT("Inventory item removed: Owner=%s ItemId=%s InstanceId=%d ItemCount=%d"),
		*GetNameSafe(OwnerActor),
		*ItemId.ToString(),
		InstanceId,
		ItemCount);
#endif
}

void UHeistDebugFunctionLibrary::DebugQuickSlotAssigned(const UObject* OwnerActor, const int32 SlotTypeValue, const int32 InstanceId, const FName ItemId)
{
#if UE_BUILD_SHIPPING
	return;
#else
	UE_LOG(
		LogHeistInventory,
		Log,
		TEXT("QuickSlot assigned: Owner=%s Slot=%d InstanceId=%d ItemId=%s"),
		*GetNameSafe(OwnerActor),
		SlotTypeValue,
		InstanceId,
		*ItemId.ToString());
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryOccupancyInvalid(const int32 InstanceId, const FName ItemId, const TCHAR* Reason, const FIntPoint& GridPosition, const FIntPoint& ItemSize)
{
#if UE_BUILD_SHIPPING
	return;
#else
	if (ItemSize != FIntPoint::ZeroValue)
	{
		UE_LOG(
			LogHeistInventory,
			Error,
			TEXT("Inventory occupancy invalid: InstanceId=%d ItemId=%s Grid=(%d,%d) Size=%dx%d Reason=%s"),
			InstanceId,
			*ItemId.ToString(),
			GridPosition.X,
			GridPosition.Y,
			ItemSize.X,
			ItemSize.Y,
			Reason);
		return;
	}

	UE_LOG(
		LogHeistInventory,
		Error,
		TEXT("Inventory occupancy invalid: InstanceId=%d ItemId=%s Reason=%s"),
		InstanceId,
		*ItemId.ToString(),
		Reason);
#endif
}

void UHeistDebugFunctionLibrary::DebugLootPickupRequestReceived(const UObject* WorldContextObject, const UObject* Character, const UObject* TargetLootActor)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Loot pickup request received: Character=%s Target=%s"),
			*GetNameSafe(Character),
			*GetNameSafe(TargetLootActor)));
#endif
}

void UHeistDebugFunctionLibrary::DebugLootPickupRequestRejected(const UObject* WorldContextObject, const UObject* TargetLootActor, const TCHAR* Reason, const float Distance)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Loot pickup request rejected: Target=%s Reason=%s%s"),
			*GetNameSafe(TargetLootActor),
			Reason,
			*FormatOptionalDistance(Distance)),
		EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugLootPickupRequestAccepted(const UObject* WorldContextObject, const UObject* TargetLootActor, const FName ItemId, const int32 InstanceId, const float Distance)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Loot pickup request accepted: Target=%s ItemId=%s InstanceId=%d Distance=%.1f InventoryCommitted=true"),
			*GetNameSafe(TargetLootActor),
			*ItemId.ToString(),
			InstanceId,
			Distance));
#endif
}

void UHeistDebugFunctionLibrary::DebugLootDataFallbackApplied(const UObject* WorldContextObject, const FName LootRowName)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(TEXT("LootDataRow '%s' was not found. Fallback values are active."), *LootRowName.ToString()),
		EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugEscapeRequestRejected(
	const UObject* WorldContextObject,
	const UObject* TargetVentActor,
	const TCHAR* Reason,
	const float Distance)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Escape request rejected: Vent=%s Reason=%s%s"),
			*GetNameSafe(TargetVentActor),
			Reason,
			*FormatOptionalDistance(Distance)),
		EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugEscapeRequestAccepted(const UObject* WorldContextObject, const UObject* Character, const UObject* TargetVentActor, const float Distance)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Escape request accepted: Character=%s Vent=%s Distance=%.1f State=Casting"),
			*GetNameSafe(Character),
			*GetNameSafe(TargetVentActor),
			Distance));
#endif
}

void UHeistDebugFunctionLibrary::DebugEscapeCastStarted(const UObject* WorldContextObject, const UObject* Character, const UObject* TargetVentActor, const float DurationSeconds, const float EndServerTime)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Escape cast started: Character=%s Vent=%s Duration=%.2f EndServerTime=%.2f"),
			*GetNameSafe(Character),
			*GetNameSafe(TargetVentActor),
			DurationSeconds,
			EndServerTime));
#endif
}

void UHeistDebugFunctionLibrary::DebugEscapeCastStateReplicated(const UObject* WorldContextObject, const UObject* Character, const bool bIsActive, const float EndServerTime)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Escape cast state replicated: Character=%s IsActive=%s EndServerTime=%.2f"),
			*GetNameSafe(Character),
			bIsActive ? TEXT("true") : TEXT("false"),
			EndServerTime));
#endif
}

void UHeistDebugFunctionLibrary::DebugEscapeCastCompleted(const UObject* WorldContextObject, const UObject* Character, const UObject* TargetVentActor)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Escape cast completed: Character=%s Vent=%s Result=Escaped"),
			*GetNameSafe(Character),
			*GetNameSafe(TargetVentActor)));
#endif
}

void UHeistDebugFunctionLibrary::DebugEscapeCastCancelled(const UObject* WorldContextObject, const FString& CharacterName, const FString& VentName, const TCHAR* Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Escape cast cancelled: Character=%s Vent=%s Reason=%s"),
			*CharacterName,
			*VentName,
			Reason));
#endif
}

void UHeistDebugFunctionLibrary::DebugLootScoreWeightRejected(const UObject* WorldContextObject, const TCHAR* Reason, const int32 ScoreDelta, const float WeightDelta)
{
#if UE_BUILD_SHIPPING
	return;
#else
	const bool bHasDeltaContext = ScoreDelta != INDEX_NONE || WeightDelta >= 0.0f;
	Message(
		WorldContextObject,
		bHasDeltaContext
			? FString::Printf(
				TEXT("Loot score/weight rejected: PlayerState=%s Reason=%s ScoreDelta=%d WeightDelta=%.2f"),
				*GetNameSafe(WorldContextObject),
				Reason,
				ScoreDelta,
				WeightDelta)
			: FString::Printf(
				TEXT("Loot score/weight rejected: PlayerState=%s Reason=%s"),
				*GetNameSafe(WorldContextObject),
				Reason),
		EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugLootScoreWeightApplied(const UObject* WorldContextObject, const int32 ScoreDelta, const float WeightDelta, const int32 TotalScore, const float TotalWeight)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Loot score/weight applied: PlayerState=%s ScoreDelta=%d WeightDelta=%.2f TotalScore=%d TotalWeight=%.2f"),
			*GetNameSafe(WorldContextObject),
			ScoreDelta,
			WeightDelta,
			TotalScore,
			TotalWeight));
#endif
}

void UHeistDebugFunctionLibrary::DebugLootScoreWeightRemoved(const UObject* WorldContextObject, const int32 ScoreDelta, const float WeightDelta, const int32 TotalScore, const float TotalWeight)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Loot score/weight removed: PlayerState=%s ScoreDelta=%d WeightDelta=%.2f TotalScore=%d TotalWeight=%.2f"),
			*GetNameSafe(WorldContextObject),
			ScoreDelta,
			WeightDelta,
			TotalScore,
			TotalWeight));
#endif
}

void UHeistDebugFunctionLibrary::DebugPlayerEscapeStateRejected(const UObject* WorldContextObject, const TCHAR* Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Player escape state rejected: PlayerState=%s Reason=%s"),
			*GetNameSafe(WorldContextObject),
			Reason),
		EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugPlayerEscapeStateCommitted(const UObject* WorldContextObject, const int32 HeistPlayerId, const int32 FinalScore, const float EscapeTimeSeconds)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Player escape state committed: PlayerState=%s HeistPlayerId=%d IsEscaped=true FinalScore=%d EscapeTime=%.2f ScoreFrozen=true"),
			*GetNameSafe(WorldContextObject),
			HeistPlayerId,
			FinalScore,
			EscapeTimeSeconds));
#endif
}

void UHeistDebugFunctionLibrary::DebugPlayerEscapeStateReplicated(const UObject* WorldContextObject, const int32 HeistPlayerId, const bool bEscaped)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Player escape state replicated: PlayerState=%s HeistPlayerId=%d IsEscaped=%s"),
			*GetNameSafe(WorldContextObject),
			HeistPlayerId,
			bEscaped ? TEXT("true") : TEXT("false")));
#endif
}

void UHeistDebugFunctionLibrary::DebugPlayerStateScoreReplicated(const UObject* WorldContextObject, const int32 TotalLootScore)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("PlayerState score replicated: PlayerState=%s TotalLootScore=%d"),
			*GetNameSafe(WorldContextObject),
			TotalLootScore));
#endif
}

void UHeistDebugFunctionLibrary::DebugPlayerStateWeightReplicated(const UObject* WorldContextObject, const float TotalLootWeight)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("PlayerState weight replicated: PlayerState=%s TotalLootWeight=%.2f"),
			*GetNameSafe(WorldContextObject),
			TotalLootWeight));
#endif
}

void UHeistDebugFunctionLibrary::DebugWeightMovementSkipped(const UObject* WorldContextObject, const TCHAR* Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Weight movement speed skipped: %s=%s Reason=%s"),
			WorldContextObject && WorldContextObject->IsA<APlayerState>() ? TEXT("PlayerState") : TEXT("Character"),
			*GetNameSafe(WorldContextObject),
			Reason),
		EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugWeightMovementSpeedApplied(const UObject* WorldContextObject, const float TotalWeight, const float BaseSpeed, const float FinalSpeed)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Weight movement speed applied: Character=%s TotalWeight=%.2f BaseSpeed=%.2f FinalSpeed=%.2f"),
			*GetNameSafe(WorldContextObject),
			TotalWeight,
			BaseSpeed,
			FinalSpeed));
#endif
}

void UHeistDebugFunctionLibrary::DebugStatusTagApplied(const UObject* WorldContextObject, const FGameplayTag& StateTag, const float EndServerTime)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Status tag applied: Owner=%s Tag=%s EndServerTime=%.2f"),
			*GetNameSafe(WorldContextObject ? WorldContextObject->GetOuter() : nullptr),
			*StateTag.ToString(),
			EndServerTime));
#endif
}

void UHeistDebugFunctionLibrary::DebugStatusTagCleared(const UObject* WorldContextObject, const FGameplayTag& StateTag)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Status tag cleared: Owner=%s Tag=%s"),
			*GetNameSafe(WorldContextObject ? WorldContextObject->GetOuter() : nullptr),
			*StateTag.ToString()));
#endif
}

void UHeistDebugFunctionLibrary::DebugStatusTagsReplicated(const UObject* WorldContextObject, const TArray<FHeistTimedTagState>& StatusTags)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Status tags replicated: Owner=%s Tags=[%s]"),
			*GetNameSafe(WorldContextObject ? WorldContextObject->GetOuter() : nullptr),
			*FormatStatusTags(StatusTags)));
#endif
}

void UHeistDebugFunctionLibrary::DebugThrowableUseRejected(const UObject* WorldContextObject, const EHeistQuickSlotType SlotType, const FName ItemId, const TCHAR* Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Throwable use rejected: Slot=%s ItemId=%s Reason=%s"),
			ToQuickSlotText(SlotType),
			*ItemId.ToString(),
			Reason),
		EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugThrowableProjectileSpawned(const UObject* WorldContextObject, const UObject* Character, const UObject* Projectile, const FName ItemId, const FVector& TargetWorldLocation, const float ProjectileSpeed, const bool bDebugBypassInventory)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Throwable projectile spawned: Character=%s Projectile=%s ItemId=%s Target=(%.1f,%.1f,%.1f) Speed=%.1f DebugBypassInventory=%s"),
			*GetNameSafe(Character),
			*GetNameSafe(Projectile),
			*ItemId.ToString(),
			TargetWorldLocation.X,
			TargetWorldLocation.Y,
			TargetWorldLocation.Z,
			ProjectileSpeed,
			bDebugBypassInventory ? TEXT("true") : TEXT("false")));
#endif
}

void UHeistDebugFunctionLibrary::DebugThrowableProjectileImpact(const UObject* WorldContextObject, const UObject* Projectile, const UObject* OtherActor, const FName ItemId, const FVector& ImpactLocation)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Throwable projectile impact: Projectile=%s OtherActor=%s ItemId=%s Location=(%.1f,%.1f,%.1f)"),
			*GetNameSafe(Projectile),
			*GetNameSafe(OtherActor),
			*ItemId.ToString(),
			ImpactLocation.X,
			ImpactLocation.Y,
			ImpactLocation.Z));
#endif
}

void UHeistDebugFunctionLibrary::DebugCoinProjectileDamageApplied(const UObject* WorldContextObject, const UObject* Projectile, const UObject* HitCharacter, const float Damage)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Coin projectile damage applied: Projectile=%s HitCharacter=%s Damage=%.1f"),
			*GetNameSafe(Projectile),
			*GetNameSafe(HitCharacter),
			Damage));
#endif
}

void UHeistDebugFunctionLibrary::DebugCoinProjectileStunApplied(const UObject* WorldContextObject, const UObject* Projectile, const UObject* HitCharacter, const float DurationSeconds)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Coin projectile stun applied: Projectile=%s HitCharacter=%s Duration=%.2f"),
			*GetNameSafe(Projectile),
			*GetNameSafe(HitCharacter),
			DurationSeconds));
#endif
}

void UHeistDebugFunctionLibrary::DebugCoinProjectileStunRejected(const UObject* WorldContextObject, const UObject* Projectile, const UObject* HitCharacter, const TCHAR* Reason)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Coin projectile stun rejected: Projectile=%s HitCharacter=%s Reason=%s"),
			*GetNameSafe(Projectile),
			*GetNameSafe(HitCharacter),
			Reason),
		EHeistDebugLevel::Warning);
#endif
}

void UHeistDebugFunctionLibrary::DebugSoundPingReported(const UObject* WorldContextObject, const FHeistSoundPingEvent& SoundPingEvent)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Sound ping reported: SequenceId=%d Type=%d Tag=%s Location=(%.1f,%.1f,%.1f) ServerTime=%.2f"),
			SoundPingEvent.SequenceId,
			static_cast<int32>(SoundPingEvent.PingType),
			*SoundPingEvent.SoundPingTag.ToString(),
			SoundPingEvent.WorldLocation.X,
			SoundPingEvent.WorldLocation.Y,
			SoundPingEvent.WorldLocation.Z,
			SoundPingEvent.ServerTimeSeconds));
#endif
}

void UHeistDebugFunctionLibrary::DebugSoundPingReplicated(const UObject* WorldContextObject, const FHeistSoundPingEvent& SoundPingEvent)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Sound ping replicated: SequenceId=%d Type=%d Tag=%s Location=(%.1f,%.1f,%.1f) ServerTime=%.2f"),
			SoundPingEvent.SequenceId,
			static_cast<int32>(SoundPingEvent.PingType),
			*SoundPingEvent.SoundPingTag.ToString(),
			SoundPingEvent.WorldLocation.X,
			SoundPingEvent.WorldLocation.Y,
			SoundPingEvent.WorldLocation.Z,
			SoundPingEvent.ServerTimeSeconds));
#endif
}

void UHeistDebugFunctionLibrary::DebugEscapedPlayerRestrictionsApplied(const UObject* WorldContextObject)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		WorldContextObject,
		FString::Printf(
			TEXT("Escaped player restrictions applied: Character=%s MovementDisabled=true InteractionDisabled=true CollisionDisabled=true Hidden=true"),
			*GetNameSafe(WorldContextObject)));
#endif
}

void UHeistDebugFunctionLibrary::DebugResultScreenShowSkipped(const UObject* HUD, const UObject* ViewModel, const UClass* WidgetClass)
{
#if UE_BUILD_SHIPPING
	return;
#else
	UE_LOG(
		LogHeistUI,
		Warning,
		TEXT("Result screen show skipped: HUD=%s ViewModel=%s WidgetClass=%s"),
		*GetNameSafe(HUD),
		*GetNameSafe(ViewModel),
		*GetNameSafe(WidgetClass));
#endif
}

void UHeistDebugFunctionLibrary::DebugWidgetMissingMVVMView(const UObject* Widget, const TCHAR* WidgetRole)
{
#if UE_BUILD_SHIPPING
	return;
#else
	UE_LOG(
		LogHeistUI,
		Warning,
		TEXT("%s widget has no MVVMView extension; MVVM binding injection skipped. Widget=%s"),
		WidgetRole,
		*GetNameSafe(Widget));
#endif
}

#pragma endregion

#pragma region InventoryDebug

void UHeistDebugFunctionLibrary::DebugInventoryHelp(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		PlayerController,
		TEXT("Inventory debug commands: HeistInvDump | HeistInvOpen 1/0 | HeistInvMove <InstanceId> <X> <Y> | HeistInvRotate <InstanceId> | HeistInvDrop <InstanceId> | HeistInvAssign <Q|E|R|Coin|Smoke|Glue> <InstanceId> | HeistInvClear <Q|E|R|Coin|Smoke|Glue> | HeistInvInvalidMove <InstanceId>"),
		EHeistDebugLevel::Info,
		true,
		8.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryDump(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	const UHeistInventoryComponent* InventoryComponent = ResolveInventoryComponent(PlayerController);
	if (!IsValid(InventoryComponent))
	{
		Message(
			PlayerController,
			TEXT("Inventory debug dump failed: missing local Heist inventory component."),
			EHeistDebugLevel::Warning,
			true);
		return;
	}

	const FHeistReplicatedInventory& ReplicatedInventory = InventoryComponent->GetReplicatedInventory();
	Message(
		PlayerController,
		FString::Printf(
			TEXT("Inventory dump: Open=%s Grid=%dx%d Items=%d QuickSlots=%d"),
			InventoryComponent->IsInventoryOpen() ? TEXT("true") : TEXT("false"),
			InventoryComponent->GetGridColumnCount(),
			InventoryComponent->GetGridRowCount(),
			ReplicatedInventory.Items.Num(),
			InventoryComponent->GetQuickSlots().Num()),
		EHeistDebugLevel::Info,
		true,
		6.0f);

	for (const FHeistInventoryFastArrayItem& ItemEntry : ReplicatedInventory.Items)
	{
		const FHeistInventoryItem& Item = ItemEntry.InventoryItem;
		Message(
			PlayerController,
			FString::Printf(
				TEXT("Inventory item: InstanceId=%d ItemId=%s Grid=(%d,%d) Quantity=%d Rotated=%s"),
				Item.InstanceId,
				*Item.ItemId.ToString(),
				Item.GridPosition.X,
				Item.GridPosition.Y,
				Item.Quantity,
				Item.bRotated ? TEXT("true") : TEXT("false")),
			EHeistDebugLevel::Info,
			false);
	}

	for (const FHeistQuickSlotState& QuickSlot : InventoryComponent->GetQuickSlots())
	{
		Message(
			PlayerController,
			FString::Printf(
				TEXT("QuickSlot: Slot=%s InstanceId=%d"),
				ToQuickSlotText(QuickSlot.SlotType),
				QuickSlot.ItemInstanceId),
			EHeistDebugLevel::Info,
			false);
	}
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryOpen(APlayerController* PlayerController, const bool bOpen)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Inventory debug open failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->RequestSetInventoryOpen(bOpen);
	Message(
		PlayerController,
		FString::Printf(TEXT("Inventory debug open requested: Open=%s"), bOpen ? TEXT("true") : TEXT("false")),
		EHeistDebugLevel::Info,
		true);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryMove(
	APlayerController* PlayerController,
	const int32 InstanceId,
	const int32 GridX,
	const int32 GridY)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Inventory debug move failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->RequestMoveInventoryItem(InstanceId, FIntPoint(GridX, GridY));
	Message(
		PlayerController,
		FString::Printf(TEXT("Inventory debug move requested: InstanceId=%d Grid=(%d,%d)"), InstanceId, GridX, GridY),
		EHeistDebugLevel::Info,
		true);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryRotate(APlayerController* PlayerController, const int32 InstanceId)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Inventory debug rotate failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->RequestRotateInventoryItem(InstanceId);
	Message(
		PlayerController,
		FString::Printf(TEXT("Inventory debug rotate requested: InstanceId=%d"), InstanceId),
		EHeistDebugLevel::Info,
		true);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryDrop(APlayerController* PlayerController, const int32 InstanceId)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Inventory debug drop failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->RequestDropInventoryItem(InstanceId);
	Message(
		PlayerController,
		FString::Printf(TEXT("Inventory debug drop requested: InstanceId=%d"), InstanceId),
		EHeistDebugLevel::Info,
		true);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryAssignQuickSlot(
	APlayerController* PlayerController,
	const FString& SlotName,
	const int32 InstanceId)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Inventory debug assign failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	EHeistQuickSlotType SlotType = EHeistQuickSlotType::None;
	if (!TryParseQuickSlotName(SlotName, SlotType))
	{
		Message(
			PlayerController,
			FString::Printf(TEXT("Inventory debug assign failed: invalid slot '%s'. Use Q/Coin, E/Smoke, or R/Glue."), *SlotName),
			EHeistDebugLevel::Warning,
			true);
		return;
	}

	HeistPlayerController->RequestAssignQuickSlot(SlotType, InstanceId);
	Message(
		PlayerController,
		FString::Printf(
			TEXT("Inventory debug assign requested: Slot=%s InstanceId=%d"),
			ToQuickSlotText(SlotType),
			InstanceId),
		EHeistDebugLevel::Info,
		true);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryClearQuickSlot(APlayerController* PlayerController, const FString& SlotName)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Inventory debug clear failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	EHeistQuickSlotType SlotType = EHeistQuickSlotType::None;
	if (!TryParseQuickSlotName(SlotName, SlotType))
	{
		Message(
			PlayerController,
			FString::Printf(TEXT("Inventory debug clear failed: invalid slot '%s'. Use Q/Coin, E/Smoke, or R/Glue."), *SlotName),
			EHeistDebugLevel::Warning,
			true);
		return;
	}

	HeistPlayerController->RequestClearQuickSlot(SlotType);
	Message(
		PlayerController,
		FString::Printf(TEXT("Inventory debug clear requested: Slot=%s"), ToQuickSlotText(SlotType)),
		EHeistDebugLevel::Info,
		true);
#endif
}

void UHeistDebugFunctionLibrary::DebugInventoryInvalidMove(APlayerController* PlayerController, const int32 InstanceId)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Inventory debug invalid move failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->RequestMoveInventoryItem(InstanceId, FIntPoint(-1, -1));
	Message(
		PlayerController,
		FString::Printf(TEXT("Inventory debug invalid move requested: InstanceId=%d Grid=(-1,-1)"), InstanceId),
		EHeistDebugLevel::Info,
		true);
#endif
}

#pragma endregion

#pragma region StatusDebug

void UHeistDebugFunctionLibrary::DebugStatusHelp(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		PlayerController,
		TEXT("Status debug commands: HeistStatusDump | HeistStatusStun <Seconds> | HeistStatusImmune <Seconds> | HeistStatusClear"),
		EHeistDebugLevel::Info,
		true,
		8.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugStatusDump(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	const UHeistStatusComponent* StatusComponent = ResolveStatusComponent(PlayerController);
	if (!IsValid(StatusComponent))
	{
		Message(PlayerController, TEXT("Status debug dump failed: missing local Heist status component."), EHeistDebugLevel::Warning, true);
		return;
	}

	Message(
		PlayerController,
		FString::Printf(
			TEXT("Status dump: Stunned=%s StunImmune=%s Tags=[%s]"),
			StatusComponent->IsStunned() ? TEXT("true") : TEXT("false"),
			StatusComponent->IsStunImmune() ? TEXT("true") : TEXT("false"),
			*FormatStatusTags(StatusComponent->GetStatusTags())),
		EHeistDebugLevel::Info,
		true,
		6.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugStatusStun(APlayerController* PlayerController, const float DurationSeconds)
{
#if UE_BUILD_SHIPPING
	return;
#else
	UHeistStatusComponent* StatusComponent = ResolveStatusComponent(PlayerController);
	if (!IsValid(StatusComponent))
	{
		Message(PlayerController, TEXT("Status debug stun failed: missing local Heist status component."), EHeistDebugLevel::Warning, true);
		return;
	}

	if (!StatusComponent->ApplyStun(DurationSeconds))
	{
		Message(PlayerController, TEXT("Status debug stun rejected."), EHeistDebugLevel::Warning, true);
		return;
	}

	Message(
		PlayerController,
		FString::Printf(TEXT("Status debug stun requested: Duration=%.2f"), DurationSeconds),
		EHeistDebugLevel::Info,
		true);
#endif
}

void UHeistDebugFunctionLibrary::DebugStatusImmune(APlayerController* PlayerController, const float DurationSeconds)
{
#if UE_BUILD_SHIPPING
	return;
#else
	UHeistStatusComponent* StatusComponent = ResolveStatusComponent(PlayerController);
	if (!IsValid(StatusComponent))
	{
		Message(PlayerController, TEXT("Status debug immunity failed: missing local Heist status component."), EHeistDebugLevel::Warning, true);
		return;
	}

	if (!StatusComponent->ApplyStunImmunity(DurationSeconds))
	{
		Message(PlayerController, TEXT("Status debug immunity rejected."), EHeistDebugLevel::Warning, true);
		return;
	}

	Message(
		PlayerController,
		FString::Printf(TEXT("Status debug immunity requested: Duration=%.2f"), DurationSeconds),
		EHeistDebugLevel::Info,
		true);
#endif
}

void UHeistDebugFunctionLibrary::DebugStatusClear(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	UHeistStatusComponent* StatusComponent = ResolveStatusComponent(PlayerController);
	if (!IsValid(StatusComponent))
	{
		Message(PlayerController, TEXT("Status debug clear failed: missing local Heist status component."), EHeistDebugLevel::Warning, true);
		return;
	}

	const bool bClearedStunned = StatusComponent->ClearStatusTag(FHeistGameplayTags::Get().State_Stunned);
	const bool bClearedImmune = StatusComponent->ClearStatusTag(FHeistGameplayTags::Get().State_StunImmune);
	Message(
		PlayerController,
		FString::Printf(
			TEXT("Status debug clear requested: ClearedStunned=%s ClearedStunImmune=%s"),
			bClearedStunned ? TEXT("true") : TEXT("false"),
			bClearedImmune ? TEXT("true") : TEXT("false")),
		EHeistDebugLevel::Info,
		true);
#endif
}

#pragma endregion

#pragma region ThrowableDebug

void UHeistDebugFunctionLibrary::DebugThrowableHelp(APlayerController* PlayerController)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Message(
		PlayerController,
		TEXT("Throwable debug commands: HeistCoinThrow <Distance> | HeistCoinThrowAt <X> <Y> <Z>"),
		EHeistDebugLevel::Info,
		true,
		8.0f);
#endif
}

void UHeistDebugFunctionLibrary::DebugCoinThrow(APlayerController* PlayerController, const float Distance)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	AHeistPlayerCharacter* HeistCharacter = IsValid(HeistPlayerController)
		? HeistPlayerController->GetPawn<AHeistPlayerCharacter>()
		: nullptr;
	if (!IsValid(HeistPlayerController) || !IsValid(HeistCharacter))
	{
		Message(PlayerController, TEXT("Coin debug throw failed: invalid Heist player controller or pawn."), EHeistDebugLevel::Warning, true);
		return;
	}

	const float ClampedDistance = FMath::Clamp(Distance, 100.0f, 5000.0f);
	const FVector TargetWorldLocation = HeistCharacter->GetActorLocation()
		+ HeistCharacter->GetActorForwardVector() * ClampedDistance;
	HeistPlayerController->DebugRequestThrowCoinAtWorldLocation(TargetWorldLocation);
	Message(
		PlayerController,
		FString::Printf(TEXT("Coin debug throw requested: Distance=%.1f"), ClampedDistance),
		EHeistDebugLevel::Info,
		true);
#endif
}

void UHeistDebugFunctionLibrary::DebugCoinThrowAt(
	APlayerController* PlayerController,
	const float TargetX,
	const float TargetY,
	const float TargetZ)
{
#if UE_BUILD_SHIPPING
	return;
#else
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Coin debug throw-at failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	const FVector TargetWorldLocation(TargetX, TargetY, TargetZ);
	HeistPlayerController->DebugRequestThrowCoinAtWorldLocation(TargetWorldLocation);
	Message(
		PlayerController,
		FString::Printf(
			TEXT("Coin debug throw-at requested: Target=(%.1f,%.1f,%.1f)"),
			TargetX,
			TargetY,
			TargetZ),
		EHeistDebugLevel::Info,
		true);
#endif
}

#pragma endregion
