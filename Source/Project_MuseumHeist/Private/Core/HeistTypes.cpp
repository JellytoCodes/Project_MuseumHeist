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
		return LOCTEXT("ContractComplete", "필수 목표를 확보하고 전리품 할당량을 달성했습니다.");
	}
	if (ReasonId == RequiredTargetSecuredQuotaShort())
	{
		return LOCTEXT("RequiredTargetSecuredQuotaShort", "필수 목표는 확보했지만 전리품 할당량을 달성하지 못했습니다.");
	}
	if (ReasonId == LockdownBeforeContractComplete())
	{
		return LOCTEXT("LockdownBeforeContractComplete", "계약 조건을 달성하기 전에 박물관이 봉쇄되었습니다.");
	}
	if (ReasonId == MatchTimerExpired())
	{
		return LOCTEXT("MatchTimerExpired", "계약 조건을 달성하기 전에 제한 시간이 종료되었습니다.");
	}
	if (ReasonId == AllRemainingCrewArrested())
	{
		return LOCTEXT("AllRemainingCrewArrested", "계약 조건을 달성하기 전에 남은 팀원이 모두 체포되었습니다.");
	}
	if (ReasonId == AllCrewDisconnected())
	{
		return LOCTEXT("AllCrewDisconnected", "계약을 완료할 접속 중인 팀원이 남아 있지 않습니다.");
	}
	if (ReasonId == NoCrewEscaped())
	{
		return LOCTEXT("NoCrewEscaped", "확보한 전리품을 가지고 탈출한 팀원이 없습니다.");
	}
	if (ReasonId == RequiredTargetMissing())
	{
		return LOCTEXT("RequiredTargetMissing", "필수 목표를 확보하지 못했습니다.");
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
