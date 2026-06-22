#pragma once

#include "CoreMinimal.h"

#include "HeistTypes.generated.h"

#pragma region MatchFlow

UENUM(BlueprintType)
enum class EHeistMatchPhase : uint8
{
	None,
	Lobby,
	Loadout,
	ReadyCountdown,
	InGame,
	End
};

#pragma endregion

#pragma region ResultData

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistPlayerResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result")
	int32 PlayerId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result")
	int32 LootScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result")
	int32 FinalScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result")
	float LootWeight = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result")
	float EscapeTimeSeconds = -1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result")
	int32 Rank = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result")
	bool bEscaped = false;

	bool operator==(const FHeistPlayerResult& Other) const
	{
		return PlayerId == Other.PlayerId
			&& LootScore == Other.LootScore
			&& FinalScore == Other.FinalScore
			&& LootWeight == Other.LootWeight
			&& EscapeTimeSeconds == Other.EscapeTimeSeconds
			&& Rank == Other.Rank
			&& bEscaped == Other.bEscaped;
	}
};

#pragma endregion

#pragma region Inventory

UENUM(BlueprintType)
enum class EHeistItemType : uint8
{
	None,
	Loot,
	Trap,
	Throwable,
	KeyItem
};

UENUM(BlueprintType)
enum class EHeistLootGrade : uint8
{
	OneStar,
	TwoStar,
	ThreeStar,
	FourStar
};

#pragma endregion

#pragma region Interaction

UENUM(BlueprintType)
enum class EHeistUseType : uint8
{
	None,
	Throw,
	PlaceTrap,
	DeployArea,
	Consume
};

UENUM(BlueprintType)
enum class EHeistTargetType : uint8
{
	None,
	Self,
	WorldLocation,
	ActorHit,
	Area
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
enum class EHeistSpawnCategory : uint8
{
	None,
	VaultFixed,
	ExhibitionRoom,
	RareEvent,
	Dropped
};

UENUM(BlueprintType)
enum class EHeistSoundPingType : uint8
{
	None,
	Footstep,
	GlassBreak,
	CoinImpact,
	NoiseTrap,
	StunHit
};

UENUM(BlueprintType)
enum class EHeistCustomizationType : uint8
{
	Hat,
	Cloth,
	SkinColor,
	HatColor,
	ClothColor
};

UENUM(BlueprintType)
enum class EHeistZoneId : uint8
{
	None,
	ZoneA,
	ZoneB,
	ZoneC,
	ZoneD
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
