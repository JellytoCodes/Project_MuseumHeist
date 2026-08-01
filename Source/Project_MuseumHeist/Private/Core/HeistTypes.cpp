#include "Core/HeistTypes.h"

#define LOCTEXT_NAMESPACE "HeistContractOutcomeReasons"

namespace HeistContractOutcomeReasons
{
FName ContractComplete()
{
	static const FName Value(TEXT("ContractComplete"));
	return Value;
}

FName RequiredTargetSecuredQuotaShort()
{
	static const FName Value(TEXT("RequiredTargetSecuredQuotaShort"));
	return Value;
}

FName LockdownBeforeContractComplete()
{
	static const FName Value(TEXT("LockdownBeforeContractComplete"));
	return Value;
}

FName MatchTimerExpired()
{
	static const FName Value(TEXT("MatchTimerExpired"));
	return Value;
}

FName AllRemainingCrewArrested()
{
	static const FName Value(TEXT("AllRemainingCrewArrested"));
	return Value;
}

FName AllCrewDisconnected()
{
	static const FName Value(TEXT("AllCrewDisconnected"));
	return Value;
}

FName NoCrewEscaped()
{
	static const FName Value(TEXT("NoCrewEscaped"));
	return Value;
}

FName RequiredTargetMissing()
{
	static const FName Value(TEXT("RequiredTargetMissing"));
	return Value;
}

bool IsFailureReason(const FName ReasonId)
{
	return ReasonId == LockdownBeforeContractComplete() || ReasonId == MatchTimerExpired() || ReasonId == AllRemainingCrewArrested() ||
		ReasonId == AllCrewDisconnected() || ReasonId == NoCrewEscaped() || ReasonId == RequiredTargetMissing();
}

FName Resolve(const EHeistContractOutcome Outcome, const bool bRequiredTargetSecured, const bool bAtLeastOneCrewEscaped,
	const bool bAllRemainingCrewArrested, const bool bAllCrewDisconnected, const FName TerminalTrigger)
{
	if (Outcome == EHeistContractOutcome::Success)
	{
		return ContractComplete();
	}
	if (Outcome == EHeistContractOutcome::PartialHaul)
	{
		return RequiredTargetSecuredQuotaShort();
	}
	if (Outcome != EHeistContractOutcome::Failed)
	{
		return NAME_None;
	}

	// A committed successful/partial deposit always wins. For failed runs, the
	// explicit terminal cause is more useful to the player than a secondary
	// missing-condition message.
	if (TerminalTrigger == FName(TEXT("Lockdown")))
	{
		return LockdownBeforeContractComplete();
	}
	if (TerminalTrigger == FName(TEXT("MatchTimerExpired")))
	{
		return MatchTimerExpired();
	}
	if (bAllRemainingCrewArrested)
	{
		return AllRemainingCrewArrested();
	}
	if (bAllCrewDisconnected)
	{
		return AllCrewDisconnected();
	}
	if (!bAtLeastOneCrewEscaped)
	{
		return NoCrewEscaped();
	}
	return bRequiredTargetSecured ? NoCrewEscaped() : RequiredTargetMissing();
}

FText ToDisplayText(const FName ReasonId)
{
	if (ReasonId == ContractComplete())
	{
		return LOCTEXT("ContractComplete", "The required target was secured and the loot quota was reached.");
	}
	if (ReasonId == RequiredTargetSecuredQuotaShort())
	{
		return LOCTEXT("RequiredTargetSecuredQuotaShort", "The required target was secured, but the loot quota was not reached.");
	}
	if (ReasonId == LockdownBeforeContractComplete())
	{
		return LOCTEXT("LockdownBeforeContractComplete", "The museum entered lockdown before the contract conditions were completed.");
	}
	if (ReasonId == MatchTimerExpired())
	{
		return LOCTEXT("MatchTimerExpired", "The contract timer expired before the contract conditions were completed.");
	}
	if (ReasonId == AllRemainingCrewArrested())
	{
		return LOCTEXT("AllRemainingCrewArrested", "All remaining crew members were arrested before the contract conditions were completed.");
	}
	if (ReasonId == AllCrewDisconnected())
	{
		return LOCTEXT("AllCrewDisconnected", "No crew members remained connected to finish the contract.");
	}
	if (ReasonId == NoCrewEscaped())
	{
		return LOCTEXT("NoCrewEscaped", "No crew member escaped with a secured haul.");
	}
	if (ReasonId == RequiredTargetMissing())
	{
		return LOCTEXT("RequiredTargetMissing", "The required target was not secured.");
	}
	return FText::GetEmpty();
}
}

namespace HeistTeamReward
{
bool Calculate(const int32 RequiredTargetValue, const int32 SecuredLooseLootValue, const float RequiredTargetQuality,
	const float MinimumForgeryMultiplier, const float MaximumForgeryMultiplier, const int32 AlertLevelIndex, const float AlertLevelPenalty,
	const float MinimumStealthMultiplier, const int32 ArrestedCrewCount, const float ArrestPenaltyPerPlayer, float& OutForgeryMultiplier,
	float& OutStealthMultiplier, int32& OutArrestPenalty, int32& OutTeamReward)
{
	OutForgeryMultiplier = 1.0f;
	OutStealthMultiplier = 1.0f;
	OutArrestPenalty = 0;
	OutTeamReward = 0;
	if (RequiredTargetValue < 0 || SecuredLooseLootValue < 0 || ArrestedCrewCount < 0 || AlertLevelIndex < 0 ||
		!FMath::IsFinite(RequiredTargetQuality) || !FMath::IsFinite(MinimumForgeryMultiplier) || !FMath::IsFinite(MaximumForgeryMultiplier) ||
		!FMath::IsFinite(AlertLevelPenalty) || !FMath::IsFinite(MinimumStealthMultiplier) || !FMath::IsFinite(ArrestPenaltyPerPlayer) ||
		MinimumForgeryMultiplier < 0.0f || MaximumForgeryMultiplier < MinimumForgeryMultiplier || AlertLevelPenalty < 0.0f ||
		MinimumStealthMultiplier < 0.0f || MinimumStealthMultiplier > 1.0f || ArrestPenaltyPerPlayer < 0.0f)
	{
		return false;
	}

	OutForgeryMultiplier = FMath::Lerp(MinimumForgeryMultiplier, MaximumForgeryMultiplier, FMath::Clamp(RequiredTargetQuality / 100.0f, 0.0f, 1.0f));
	OutStealthMultiplier = FMath::Clamp(1.0f - AlertLevelPenalty * AlertLevelIndex, MinimumStealthMultiplier, 1.0f);
	const int64 RewardedTargetValue = FMath::Clamp<int64>(FMath::RoundToInt64(static_cast<double>(RequiredTargetValue) * OutForgeryMultiplier * OutStealthMultiplier), 0, MAX_int32);
	const int64 RewardSubtotal = FMath::Clamp<int64>(RewardedTargetValue + static_cast<int64>(SecuredLooseLootValue), 0, MAX_int32);
	const int64 ArrestPenalty = FMath::Clamp<int64>(FMath::RoundToInt64(static_cast<double>(RewardSubtotal) * ArrestPenaltyPerPlayer * ArrestedCrewCount), 0, RewardSubtotal);
	OutArrestPenalty = static_cast<int32>(ArrestPenalty);
	OutTeamReward = static_cast<int32>(RewardSubtotal - ArrestPenalty);
	return true;
}
}

#undef LOCTEXT_NAMESPACE
