#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/NetSerialization.h"

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
	Drawing,
	Assembly
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

UENUM(BlueprintType)
enum class EHeistObjectAssemblyState : uint8
{
	Secured,
	Observed,
	AssemblyInProgress,
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

UENUM(BlueprintType)
enum class EHeistAlertLevel : uint8
{
	Quiet,
	Suspicious,
	Searching,
	Alarmed,
	Lockdown
};

UENUM(BlueprintType)
enum class EHeistContractOutcome : uint8
{
	None,
	Success,
	PartialHaul,
	Failed
};

namespace HeistContractOutcomeReasons
{
	PROJECT_MUSEUMHEIST_API FName ContractComplete();
	PROJECT_MUSEUMHEIST_API FName RequiredTargetSecuredQuotaShort();
	PROJECT_MUSEUMHEIST_API FName LockdownBeforeContractComplete();
	PROJECT_MUSEUMHEIST_API FName MatchTimerExpired();
	PROJECT_MUSEUMHEIST_API FName AllRemainingCrewArrested();
	PROJECT_MUSEUMHEIST_API FName AllCrewDisconnected();
	PROJECT_MUSEUMHEIST_API FName NoCrewEscaped();
	PROJECT_MUSEUMHEIST_API FName RequiredTargetMissing();
	PROJECT_MUSEUMHEIST_API bool IsFailureReason(FName ReasonId);
	PROJECT_MUSEUMHEIST_API FName Resolve(EHeistContractOutcome Outcome, bool bRequiredTargetSecured, bool bAtLeastOneCrewEscaped,
		bool bAllRemainingCrewArrested, bool bAllCrewDisconnected, FName TerminalTrigger);
	PROJECT_MUSEUMHEIST_API FText ToDisplayText(FName ReasonId);
}

/**
 * Replicated server-authoritative Contract Run state.
 * CarriedValue is still at risk; only SecuredValue contributes to outcome.
 */
USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistContractSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Contract")
	FName ContractId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Contract")
	FName MapId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Contract")
	int32 AssignmentSeed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Contract")
	FName RequiredTargetArtifactId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Contract")
	FName RequiredTargetCaseId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Contract")
	int32 LootValueQuota = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Contract")
	int32 CarriedValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Contract")
	int32 SecuredValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Contract")
	bool bRequiredTargetSecured = false;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Contract")
	EHeistContractOutcome Outcome = EHeistContractOutcome::None;

	/** Stable replicated reason id. Clients resolve the same localized player-facing text from this id. */
	UPROPERTY(BlueprintReadOnly, Category = "Heist|Contract")
	FName OutcomeReasonId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Contract")
	int32 Revision = 0;

	bool IsInitialized() const
	{
		return Revision > 0 && !ContractId.IsNone() && !MapId.IsNone() && !RequiredTargetArtifactId.IsNone() && !RequiredTargetCaseId.IsNone() && LootValueQuota > 0;
	}

	bool IsProgressValid() const
	{
		return CarriedValue >= 0 && SecuredValue >= 0;
	}

	bool IsSuccessConditionMet() const
	{
		return IsInitialized() && bRequiredTargetSecured && SecuredValue >= LootValueQuota;
	}

	EHeistContractOutcome ResolveTerminalOutcome(const bool bAtLeastOneCrewEscaped) const
	{
		if (!bAtLeastOneCrewEscaped || !bRequiredTargetSecured)
		{
			return EHeistContractOutcome::Failed;
		}
		return SecuredValue >= LootValueQuota ? EHeistContractOutcome::Success : EHeistContractOutcome::PartialHaul;
	}

	FText GetOutcomeReasonText() const
	{
		return HeistContractOutcomeReasons::ToDisplayText(OutcomeReasonId);
	}

	bool IsOutcomeConsistent() const
	{
		switch (Outcome)
		{
		case EHeistContractOutcome::None:
			return OutcomeReasonId.IsNone();
		case EHeistContractOutcome::Success:
			return IsSuccessConditionMet() && OutcomeReasonId == HeistContractOutcomeReasons::ContractComplete();
		case EHeistContractOutcome::PartialHaul:
			return IsInitialized() && bRequiredTargetSecured && SecuredValue < LootValueQuota &&
				OutcomeReasonId == HeistContractOutcomeReasons::RequiredTargetSecuredQuotaShort();
		case EHeistContractOutcome::Failed:
			return IsInitialized() && HeistContractOutcomeReasons::IsFailureReason(OutcomeReasonId);
		default:
			return false;
		}
	}

	bool operator==(const FHeistContractSnapshot& Other) const
	{
		return ContractId == Other.ContractId && MapId == Other.MapId && AssignmentSeed == Other.AssignmentSeed && RequiredTargetArtifactId == Other.RequiredTargetArtifactId &&
			   RequiredTargetCaseId == Other.RequiredTargetCaseId && LootValueQuota == Other.LootValueQuota && CarriedValue == Other.CarriedValue && SecuredValue == Other.SecuredValue &&
			   bRequiredTargetSecured == Other.bRequiredTargetSecured && Outcome == Other.Outcome && OutcomeReasonId == Other.OutcomeReasonId && Revision == Other.Revision;
	}
};

#pragma endregion

#pragma region ResultData

/**
 * Compact Object Assembly submission unit.
 * Clients submit stable data identifiers only; meshes, arbitrary transforms,
 * preview actors, and physics state are never part of the network contract.
 */
USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistObjectAssemblyEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heist|Object Assembly")
	FName PartId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heist|Object Assembly")
	FName SocketId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heist|Object Assembly")
	uint8 QuantizedOrientation = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heist|Object Assembly")
	FName MaterialId = NAME_None;

	bool operator==(const FHeistObjectAssemblyEntry& Other) const
	{
		return PartId == Other.PartId && SocketId == Other.SocketId && QuantizedOrientation == Other.QuantizedOrientation && MaterialId == Other.MaterialId;
	}
};

/** Server-authoritative deterministic score for Object Assembly Forgery. */
USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistObjectAssemblyResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Object Assembly")
	FName ArtifactId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Object Assembly")
	FName TemplateId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Object Assembly")
	float QualityScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Object Assembly")
	float RequiredPartScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Object Assembly")
	float SocketTopologyScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Object Assembly")
	float OrientationScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Object Assembly")
	float MaterialScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Object Assembly")
	float Completeness = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Object Assembly")
	bool bExtraPartCapTriggered = false;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Object Assembly")
	float CompletionTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Object Assembly")
	bool bReplicaPlaced = false;

	bool operator==(const FHeistObjectAssemblyResult& Other) const
	{
		return ArtifactId == Other.ArtifactId && TemplateId == Other.TemplateId && FMath::IsNearlyEqual(QualityScore, Other.QualityScore, 0.001f) &&
			   FMath::IsNearlyEqual(RequiredPartScore, Other.RequiredPartScore, 0.001f) && FMath::IsNearlyEqual(SocketTopologyScore, Other.SocketTopologyScore, 0.001f) &&
			   FMath::IsNearlyEqual(OrientationScore, Other.OrientationScore, 0.001f) && FMath::IsNearlyEqual(MaterialScore, Other.MaterialScore, 0.001f) &&
			   FMath::IsNearlyEqual(Completeness, Other.Completeness, 0.001f) && bExtraPartCapTriggered == Other.bExtraPartCapTriggered &&
			   FMath::IsNearlyEqual(CompletionTime, Other.CompletionTime, 0.001f) && bReplicaPlaced == Other.bReplicaPlaced;
	}
};

/** Replicated final assembly state used by clients to rebuild the replica locally. */
USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistObjectAssemblyReplicaData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Object Assembly")
	TArray<FHeistObjectAssemblyEntry> Entries;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Object Assembly")
	int32 Revision = 0;

	bool operator==(const FHeistObjectAssemblyReplicaData& Other) const
	{
		return Entries == Other.Entries && Revision == Other.Revision;
	}
};

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
		return ArtifactId == Other.ArtifactId && TemplateId == Other.TemplateId && ForgeryType == Other.ForgeryType && FMath::IsNearlyEqual(SimilarityScore, Other.SimilarityScore, 0.001f) &&
			   FMath::IsNearlyEqual(CoverageScore, Other.CoverageScore, 0.001f) && FMath::IsNearlyEqual(MajorShapeScore, Other.MajorShapeScore, 0.001f) &&
			   FMath::IsNearlyEqual(ColorAccuracyScore, Other.ColorAccuracyScore, 0.001f) && FMath::IsNearlyEqual(PaintToReferenceRatio, Other.PaintToReferenceRatio, 0.001f) &&
			   bAntiFillTriggered == Other.bAntiFillTriggered && FMath::IsNearlyEqual(MissingShapePenalty, Other.MissingShapePenalty, 0.001f) &&
			   FMath::IsNearlyEqual(ExtraStrokePenalty, Other.ExtraStrokePenalty, 0.001f) && FMath::IsNearlyEqual(TimeoutPenalty, Other.TimeoutPenalty, 0.001f) &&
			   FMath::IsNearlyEqual(CompletionTime, Other.CompletionTime, 0.001f) && bReplicaPlaced == Other.bReplicaPlaced;
	}
};

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistPlayerContribution
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Contribution")
	int32 SurfaceForgeries = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Contribution")
	float BestSurfaceQuality = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Contribution")
	int32 Assemblies = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Contribution")
	float BestAssemblyQuality = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Contribution")
	int32 ArtifactsRecovered = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Contribution")
	float CarryTimeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Contribution")
	int32 SecuredLootValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Contribution")
	int32 GuardsDistracted = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Contribution")
	int32 TeammatesRescued = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Contribution")
	int32 AlarmsTriggered = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Contribution")
	bool bEscaped = false;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Contribution")
	bool bArrested = false;

	bool IsValid() const
	{
		return SurfaceForgeries >= 0 && FMath::IsFinite(BestSurfaceQuality) && FMath::IsWithinInclusive(BestSurfaceQuality, 0.0f, 100.0f) &&
			Assemblies >= 0 && FMath::IsFinite(BestAssemblyQuality) && FMath::IsWithinInclusive(BestAssemblyQuality, 0.0f, 100.0f) &&
			ArtifactsRecovered >= 0 && FMath::IsFinite(CarryTimeSeconds) && CarryTimeSeconds >= 0.0f && SecuredLootValue >= 0 &&
			GuardsDistracted >= 0 && TeammatesRescued >= 0 && AlarmsTriggered >= 0 && !(bEscaped && bArrested);
	}

	bool operator==(const FHeistPlayerContribution& Other) const
	{
		return SurfaceForgeries == Other.SurfaceForgeries && FMath::IsNearlyEqual(BestSurfaceQuality, Other.BestSurfaceQuality, 0.001f) &&
			Assemblies == Other.Assemblies && FMath::IsNearlyEqual(BestAssemblyQuality, Other.BestAssemblyQuality, 0.001f) &&
			ArtifactsRecovered == Other.ArtifactsRecovered && FMath::IsNearlyEqual(CarryTimeSeconds, Other.CarryTimeSeconds, 0.001f) &&
			SecuredLootValue == Other.SecuredLootValue && GuardsDistracted == Other.GuardsDistracted && TeammatesRescued == Other.TeammatesRescued &&
			AlarmsTriggered == Other.AlarmsTriggered && bEscaped == Other.bEscaped && bArrested == Other.bArrested;
	}
};

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistReplicaRecapEntry
{
	GENERATED_BODY()

	static constexpr int32 PaintingThumbnailResolution = 64;
	static constexpr int32 MaximumPaintingPaletteColors = 8;
	static constexpr int32 MaximumAssemblyEntries = 32;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result|Replica")
	FName CaseId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result|Replica")
	FName ArtifactId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result|Replica")
	FName TemplateId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result|Replica")
	EHeistForgeryType ForgeryType = EHeistForgeryType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result|Replica")
	float QualityScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result|Replica")
	bool bRequiredTarget = false;

	/** Result-screen thumbnail sampled from the actual submitted Surface Forgery image. Two palette indices per byte. */
	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result|Replica")
	int32 PaintingResolution = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result|Replica")
	TArray<FColor> PaintingPalette;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result|Replica")
	TArray<uint8> PaintingPackedPaletteIndices;

	/** Actual submitted Object Assembly recipe. Clients rebuild presentation from stable ids. */
	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result|Replica")
	TArray<FHeistObjectAssemblyEntry> AssemblyEntries;

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);

	bool operator==(const FHeistReplicaRecapEntry& Other) const
	{
		return CaseId == Other.CaseId && ArtifactId == Other.ArtifactId && TemplateId == Other.TemplateId && ForgeryType == Other.ForgeryType &&
			FMath::IsNearlyEqual(QualityScore, Other.QualityScore, 0.001f) && bRequiredTarget == Other.bRequiredTarget && PaintingResolution == Other.PaintingResolution &&
			PaintingPalette == Other.PaintingPalette && PaintingPackedPaletteIndices == Other.PaintingPackedPaletteIndices && AssemblyEntries == Other.AssemblyEntries;
	}
};

template <>
struct TStructOpsTypeTraits<FHeistReplicaRecapEntry> : public TStructOpsTypeTraitsBase2<FHeistReplicaRecapEntry>
{
	enum
	{
		WithNetSerializer = true
	};
};

/** Immutable, server-authored end-of-contract summary. This never mutates quota progress. */
USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistTeamResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result")
	EHeistContractOutcome Outcome = EHeistContractOutcome::None;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result")
	FName OutcomeReasonId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result")
	FName RequiredTargetArtifactId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result")
	bool bRequiredTargetSecured = false;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result")
	int32 LootValueQuota = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result")
	int32 SecuredValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result")
	int32 ExtraValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result|Reward")
	int32 RequiredTargetValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result|Reward")
	int32 SecuredLooseLootValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result|Reward")
	float RequiredTargetQuality = 50.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result|Reward")
	float ForgeryRewardMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result|Reward")
	float StealthRewardMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result|Reward")
	int32 ArrestPenalty = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result|Reward")
	int32 TeamReward = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result")
	int32 CrewCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result")
	int32 EscapedCrewCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result")
	int32 ArrestedCrewCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result|Replica")
	TArray<FHeistReplicaRecapEntry> ReplicaRecap;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result")
	int32 Revision = 0;

	bool IsValid() const
	{
		return Revision > 0 && Outcome != EHeistContractOutcome::None && !OutcomeReasonId.IsNone() && LootValueQuota > 0 && SecuredValue >= 0 && TeamReward >= 0;
	}

	bool operator==(const FHeistTeamResult& Other) const
	{
		return Outcome == Other.Outcome && OutcomeReasonId == Other.OutcomeReasonId && RequiredTargetArtifactId == Other.RequiredTargetArtifactId &&
			bRequiredTargetSecured == Other.bRequiredTargetSecured && LootValueQuota == Other.LootValueQuota && SecuredValue == Other.SecuredValue && ExtraValue == Other.ExtraValue &&
			RequiredTargetValue == Other.RequiredTargetValue && SecuredLooseLootValue == Other.SecuredLooseLootValue &&
			FMath::IsNearlyEqual(RequiredTargetQuality, Other.RequiredTargetQuality, 0.001f) && FMath::IsNearlyEqual(ForgeryRewardMultiplier, Other.ForgeryRewardMultiplier, 0.001f) &&
			FMath::IsNearlyEqual(StealthRewardMultiplier, Other.StealthRewardMultiplier, 0.001f) && ArrestPenalty == Other.ArrestPenalty && TeamReward == Other.TeamReward &&
			CrewCount == Other.CrewCount && EscapedCrewCount == Other.EscapedCrewCount && ArrestedCrewCount == Other.ArrestedCrewCount && ReplicaRecap == Other.ReplicaRecap && Revision == Other.Revision;
	}
};

namespace HeistTeamReward
{
	/** Pure deterministic reward calculation. Secured/Quota values are inputs only and are never mutated. */
	PROJECT_MUSEUMHEIST_API bool Calculate(int32 RequiredTargetValue, int32 SecuredLooseLootValue, float RequiredTargetQuality,
		float MinimumForgeryMultiplier, float MaximumForgeryMultiplier, int32 AlertLevelIndex, float AlertLevelPenalty, float MinimumStealthMultiplier,
		int32 ArrestedCrewCount, float ArrestPenaltyPerPlayer, float& OutForgeryMultiplier, float& OutStealthMultiplier, int32& OutArrestPenalty, int32& OutTeamReward);
}

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

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Result")
	FHeistPlayerContribution Contribution;

	bool operator==(const FHeistPlayerResult& Other) const
	{
		return PlayerId == Other.PlayerId && LootScore == Other.LootScore && FinalScore == Other.FinalScore && LootWeight == Other.LootWeight && EscapeTimeSeconds == Other.EscapeTimeSeconds &&
			   bEscaped == Other.bEscaped && bArrested == Other.bArrested && Contribution == Other.Contribution;
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
		return StateTag == Other.StateTag && FMath::IsNearlyEqual(EndServerTime, Other.EndServerTime);
	}
};

#pragma endregion

#pragma region Inventory

UENUM(BlueprintType)
enum class EHeistItemType : uint8
{
	None,
	Loot,
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
	StunHit,
	ReplicaSwap
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
		return EventIndex == Other.EventIndex && ItemId == Other.ItemId && WorldLocation.Equals(Other.WorldLocation) && FMath::IsNearlyEqual(SpawnServerTime, Other.SpawnServerTime) &&
			   bIncomingWarningActive == Other.bIncomingWarningActive && bDirectionMarkerActive == Other.bDirectionMarkerActive;
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
	float ServerTimeSeconds = 0.0f;

	bool operator==(const FHeistSoundPingEvent& Other) const
	{
		return SequenceId == Other.SequenceId && SoundPingTag == Other.SoundPingTag && PingType == Other.PingType && WorldLocation.Equals(Other.WorldLocation) &&
			   FMath::IsNearlyEqual(Radius, Other.Radius) && FMath::IsNearlyEqual(Duration, Other.Duration) && bAffectsGuards == Other.bAffectsGuards &&
			   FMath::IsNearlyEqual(ServerTimeSeconds, Other.ServerTimeSeconds);
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

	Coin
};

#pragma endregion
