#pragma once

#include "CoreMinimal.h"

#include "HeistTypes.generated.h"

UENUM(BlueprintType)
enum class EHeistMatchPhase : uint8
{
	None,
	Lobby,
	InProgress,
	Results
};

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

UENUM(BlueprintType)
enum class EHeistGuardState : uint8
{
	Patrol,
	Chase,
	Stunned,
	Investigate
};

UENUM(BlueprintType)
enum class EHeistZoneId : uint8
{
	None
};

UENUM(BlueprintType)
enum class EHeistQuickSlotType : uint8
{
	None,
	Coin,
	SmokeGrenade,
	GlueTrap
};
