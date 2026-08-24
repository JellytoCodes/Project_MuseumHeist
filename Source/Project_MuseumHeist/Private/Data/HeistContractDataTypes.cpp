#include "Data/HeistContractDataTypes.h"

namespace
{
constexpr int32 SupportedPlayerCount = 4;

int32 ResolvePlayerCountIndex(const int32 PlayerCount)
{
	return FMath::Clamp(PlayerCount, 1, SupportedPlayerCount) - 1;
}
}

int32 FHeistContractDataRow::ResolveLootValueQuota(const int32 PlayerCount) const
{
	const int32 PlayerCountIndex = ResolvePlayerCountIndex(PlayerCount);
	if (!PlayerCountQuotaMultipliers.IsValidIndex(PlayerCountIndex) || !FMath::IsFinite(PlayerCountQuotaMultipliers[PlayerCountIndex]))
	{
		return 0;
	}

	const double ResolvedQuota = static_cast<double>(BaseLootValueQuota) * static_cast<double>(PlayerCountQuotaMultipliers[PlayerCountIndex]);
	return ResolvedQuota > 0.0 && ResolvedQuota <= static_cast<double>(MAX_int32) ? FMath::RoundToInt(ResolvedQuota) : 0;
}

int32 FHeistContractDataRow::ResolveMinimumOptionalExhibitCount(const int32 PlayerCount) const
{
	const int32 PlayerCountIndex = ResolvePlayerCountIndex(PlayerCount);
	return MinimumOptionalExhibits.IsValidIndex(PlayerCountIndex) ? FMath::Max(0, MinimumOptionalExhibits[PlayerCountIndex]) : 0;
}

int32 FHeistContractDataRow::ResolveMaximumOptionalExhibitCount(const int32 PlayerCount) const
{
	const int32 PlayerCountIndex = ResolvePlayerCountIndex(PlayerCount);
	return MaximumOptionalExhibits.IsValidIndex(PlayerCountIndex) ? FMath::Max(0, MaximumOptionalExhibits[PlayerCountIndex]) : 0;
}

bool FHeistContractDataRow::IsRuntimeDefinitionValid(FString* OutFailureReason) const
{
	const auto Fail = [OutFailureReason](const TCHAR* FailureReason)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FailureReason;
		}
		return false;
	};

	if (ContractId.IsNone())
	{
		return Fail(TEXT("MissingContractId"));
	}
	if (RequiredTargetPoolId.IsNone())
	{
		return Fail(TEXT("MissingRequiredTargetPoolId"));
	}
	if (BaseLootValueQuota <= 0)
	{
		return Fail(TEXT("InvalidBaseLootValueQuota"));
	}
	if (PlayerCountQuotaMultipliers.Num() != SupportedPlayerCount || MinimumOptionalExhibits.Num() != SupportedPlayerCount || MaximumOptionalExhibits.Num() != SupportedPlayerCount)
	{
		return Fail(TEXT("PlayerCountArraySizeMismatch"));
	}
	if (SurfaceTemplateCatalogSize <= 0 || MatchPaintingExhibitCount <= 0 || MatchPaintingExhibitCount > SurfaceTemplateCatalogSize)
	{
		return Fail(TEXT("InvalidSurfaceExhibitCatalogRange"));
	}
	if (!FMath::IsWithinInclusive(MatchDurationSeconds, 900.0f, 1500.0f))
	{
		return Fail(TEXT("MatchDurationOutsideTargetRange"));
	}
	if (ExtractionRuleId.IsNone())
	{
		return Fail(TEXT("MissingExtractionRuleId"));
	}

	int32 PreviousQuota = 0;
	for (int32 PlayerCountIndex = 0; PlayerCountIndex < SupportedPlayerCount; ++PlayerCountIndex)
	{
		const float Multiplier = PlayerCountQuotaMultipliers[PlayerCountIndex];
		const int32 MinimumExhibits = MinimumOptionalExhibits[PlayerCountIndex];
		const int32 MaximumExhibits = MaximumOptionalExhibits[PlayerCountIndex];
		if (!FMath::IsFinite(Multiplier) || Multiplier <= 0.0f)
		{
			return Fail(TEXT("InvalidPlayerCountQuotaMultiplier"));
		}
		if (MinimumExhibits < 0 || MaximumExhibits < MinimumExhibits)
		{
			return Fail(TEXT("InvalidOptionalExhibitRange"));
		}
		if (MaximumExhibits >= MatchPaintingExhibitCount)
		{
			return Fail(TEXT("OptionalExhibitRangeExceedsMatchPaintingCount"));
		}

		const int32 ResolvedQuota = ResolveLootValueQuota(PlayerCountIndex + 1);
		if (ResolvedQuota <= 0 || ResolvedQuota < PreviousQuota)
		{
			return Fail(TEXT("NonMonotonicResolvedQuota"));
		}
		PreviousQuota = ResolvedQuota;
	}

	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}
	return true;
}

#if WITH_EDITOR
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "HeistContractDataValidation"

EDataValidationResult FHeistContractDataRow::IsDataValid(FDataValidationContext& Context) const
{
	FString FailureReason;
	if (IsRuntimeDefinitionValid(&FailureReason))
	{
		return EDataValidationResult::Valid;
	}

	Context.AddError(FText::Format(LOCTEXT("InvalidContractDefinition", "계약 정의가 올바르지 않습니다: {0}."), FText::FromString(FailureReason)));
	return EDataValidationResult::Invalid;
}

#undef LOCTEXT_NAMESPACE
#endif
