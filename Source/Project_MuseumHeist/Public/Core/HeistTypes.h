#pragma once

#include "CoreMinimal.h"

#include "HeistTypes.generated.h"

#pragma region MatchFlow

UENUM(BlueprintType)
enum class EHeistMatchPhase : uint8
{
	None,
	Lobby,
	InProgress,
	Results
};

#pragma endregion

#pragma region Inventory

UENUM(BlueprintType)
enum class EHeistItemType : uint8
{
	None,
	Loot,
	Usable
};

UENUM(BlueprintType)
enum class EHeistLootGrade : uint8
{
	None,
	Common,
	Rare
};

#pragma endregion

#pragma region Interaction

UENUM(BlueprintType)
enum class EHeistUseType : uint8
{
	None,
	Instant,
	Throwable,
	Placeable
};

UENUM(BlueprintType)
enum class EHeistTargetType : uint8
{
	None,
	Self,
	Actor,
	World
};

#pragma endregion

#pragma region AI

UENUM(BlueprintType)
enum class EHeistGuardState : uint8
{
	Patrol,
	Chase,
	Stunned,
	Investigate
};

#pragma endregion

#pragma region World

UENUM(BlueprintType)
enum class EHeistZoneId : uint8
{
	None
};

#pragma endregion

#pragma region QuickSlots

UENUM(BlueprintType)
enum class EHeistQuickSlotType : uint8
{
	None,
	Coin,
	SmokeGrenade,
	GlueTrap
};

#pragma endregion
