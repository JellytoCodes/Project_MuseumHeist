#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

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

#pragma region InputMode

UENUM(BlueprintType)
enum class EHeistInputMode : uint8
{
	Gameplay,
	Inventory,
	Forgery
};

#pragma endregion

#pragma region Objective

UENUM(BlueprintType)
enum class EHeistObjectiveState : uint8
{
	Inactive,
	Available,
	InProgress,
	Completed,
	Failed
};

UENUM(BlueprintType)
enum class EHeistForgeryType : uint8
{
	None,
	Drawing
};

UENUM(BlueprintType)
enum class EHeistDisplayCaseState : uint8
{
	Secured,
	Observed,
	ForgeryInProgress,
	ReplicaReady,
	ReplicaPlaced,
	OriginalAvailable,
	OriginalRemoved,
	Inspecting,
	Completed,
	Suspected,
	Alarmed,
	Failed
};

#pragma endregion

#pragma region ResultData

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistForgeryResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Forgery")
	FName ArtifactId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Forgery")
	FName TemplateId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Forgery")
	EHeistForgeryType ForgeryType = EHeistForgeryType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Forgery")
	float SimilarityScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Forgery")
	float CoverageScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Forgery")
	float MajorShapeScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Forgery")
	float ColorAccuracyScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Forgery")
	float PaintToReferenceRatio = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Forgery")
	bool bAntiFillTriggered = false;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Forgery")
	float MissingShapePenalty = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Forgery")
	float ExtraStrokePenalty = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Forgery")
	float TimeoutPenalty = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Forgery")
	float CompletionTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Forgery")
	bool bReplicaPlaced = false;

	bool operator==(const FHeistForgeryResult& Other) const
	{
		return ArtifactId == Other.ArtifactId
			&& TemplateId == Other.TemplateId
			&& ForgeryType == Other.ForgeryType
			&& FMath::IsNearlyEqual(
				SimilarityScore,
				Other.SimilarityScore,
				0.001f)
			&& FMath::IsNearlyEqual(
				CoverageScore,
				Other.CoverageScore,
				0.001f)
			&& FMath::IsNearlyEqual(
				MajorShapeScore,
				Other.MajorShapeScore,
				0.001f)
			&& FMath::IsNearlyEqual(
				ColorAccuracyScore,
				Other.ColorAccuracyScore,
				0.001f)
			&& FMath::IsNearlyEqual(
				PaintToReferenceRatio,
				Other.PaintToReferenceRatio,
				0.001f)
			&& bAntiFillTriggered == Other.bAntiFillTriggered
			&& FMath::IsNearlyEqual(
				MissingShapePenalty,
				Other.MissingShapePenalty,
				0.001f)
			&& FMath::IsNearlyEqual(
				ExtraStrokePenalty,
				Other.ExtraStrokePenalty,
				0.001f)
			&& FMath::IsNearlyEqual(
				TimeoutPenalty,
				Other.TimeoutPenalty,
				0.001f)
			&& FMath::IsNearlyEqual(
				CompletionTime,
				Other.CompletionTime,
				0.001f)
			&& bReplicaPlaced == Other.bReplicaPlaced;
	}
};

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
	bool bEscaped = false;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result")
	bool bArrested = false;

	bool operator==(const FHeistPlayerResult& Other) const
	{
		return PlayerId == Other.PlayerId
			&& LootScore == Other.LootScore
			&& FinalScore == Other.FinalScore
			&& LootWeight == Other.LootWeight
			&& EscapeTimeSeconds == Other.EscapeTimeSeconds
			&& bEscaped == Other.bEscaped
			&& bArrested == Other.bArrested;
	}
};

#pragma endregion

#pragma region Status

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistTimedTagState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Status")
	FGameplayTag StateTag;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Status")
	float EndServerTime = 0.0f;

	bool operator==(const FHeistTimedTagState& Other) const
	{
		return StateTag == Other.StateTag
			&& FMath::IsNearlyEqual(EndServerTime, Other.EndServerTime);
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
	Disabled,
	Stunned,
	Patrol,
	InvestigateNoise,
	ChasePlayer,
	SearchLastKnownLocation,
	ReturnToPatrol,
	InspectExhibit
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

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistRareLootEventState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Heist|RareLoot")
	int32 EventIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|RareLoot")
	FName ItemId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|RareLoot")
	FVector_NetQuantize WorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|RareLoot")
	float SpawnServerTime = -1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|RareLoot")
	bool bIncomingWarningActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|RareLoot")
	bool bDirectionMarkerActive = false;

	bool operator==(const FHeistRareLootEventState& Other) const
	{
		return EventIndex == Other.EventIndex
			&& ItemId == Other.ItemId
			&& WorldLocation.Equals(Other.WorldLocation)
			&& FMath::IsNearlyEqual(SpawnServerTime, Other.SpawnServerTime)
			&& bIncomingWarningActive == Other.bIncomingWarningActive
			&& bDirectionMarkerActive == Other.bDirectionMarkerActive;
	}
};

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistSoundPingEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Heist|SoundPing")
	int32 SequenceId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|SoundPing")
	FGameplayTag SoundPingTag;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|SoundPing")
	EHeistSoundPingType PingType = EHeistSoundPingType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|SoundPing")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|SoundPing")
	float Radius = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|SoundPing")
	float Duration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|SoundPing")
	bool bAffectsGuards = false;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|SoundPing")
	bool bAffectsPlayers = false;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|SoundPing")
	float ServerTimeSeconds = 0.0f;

	bool operator==(const FHeistSoundPingEvent& Other) const
	{
		return SequenceId == Other.SequenceId
			&& SoundPingTag == Other.SoundPingTag
			&& PingType == Other.PingType
			&& WorldLocation.Equals(Other.WorldLocation)
			&& FMath::IsNearlyEqual(Radius, Other.Radius)
			&& FMath::IsNearlyEqual(Duration, Other.Duration)
			&& bAffectsGuards == Other.bAffectsGuards
			&& bAffectsPlayers == Other.bAffectsPlayers
			&& FMath::IsNearlyEqual(ServerTimeSeconds, Other.ServerTimeSeconds);
	}
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
