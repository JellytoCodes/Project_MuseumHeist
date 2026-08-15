#include "Core/HeistTypes.h"

namespace
{
bool IsValidReplicaRecapPayload(const FHeistReplicaRecapEntry& Entry)
{
	if (Entry.CaseId.IsNone() || Entry.ArtifactId.IsNone() || Entry.ArtifactDisplayName.IsEmpty() || Entry.TemplateId.IsNone() || !FMath::IsFinite(Entry.QualityScore) ||
		!FMath::IsWithinInclusive(Entry.QualityScore, 0.0f, 100.0f))
	{
		return false;
	}

	if (Entry.ForgeryType == EHeistForgeryType::Drawing)
	{
		const int32 ExpectedPackedByteCount = FMath::DivideAndRoundUp(
			FHeistReplicaRecapEntry::PaintingThumbnailResolution * FHeistReplicaRecapEntry::PaintingThumbnailResolution, 2);
		const bool bClearedPainting = Entry.PaintingResolution == 0 && Entry.PaintingPalette.IsEmpty() && Entry.PaintingPackedPaletteIndices.IsEmpty();
		const bool bCompletePainting = Entry.PaintingResolution == FHeistReplicaRecapEntry::PaintingThumbnailResolution &&
			FMath::IsWithinInclusive(Entry.PaintingPalette.Num(), 2, FHeistReplicaRecapEntry::MaximumPaintingPaletteColors) &&
			Entry.PaintingPackedPaletteIndices.Num() == ExpectedPackedByteCount;
		if ((!bClearedPainting && !bCompletePainting) || !Entry.AssemblyEntries.IsEmpty())
		{
			return false;
		}

		if (bClearedPainting)
		{
			return true;
		}

		for (int32 PixelIndex = 0; PixelIndex < Entry.PaintingResolution * Entry.PaintingResolution; ++PixelIndex)
		{
			const uint8 PackedByte = Entry.PaintingPackedPaletteIndices[PixelIndex / 2];
			const uint8 PaletteIndex = (PixelIndex & 1) == 0 ? PackedByte & 0x0f : PackedByte >> 4;
			if (PaletteIndex > Entry.PaintingPalette.Num())
			{
				return false;
			}
		}
		return true;
	}

	if (Entry.ForgeryType != EHeistForgeryType::Assembly || Entry.PaintingResolution != 0 || !Entry.PaintingPalette.IsEmpty() ||
		!Entry.PaintingPackedPaletteIndices.IsEmpty() || Entry.AssemblyEntries.IsEmpty() || Entry.AssemblyEntries.Num() > FHeistReplicaRecapEntry::MaximumAssemblyEntries)
	{
		return false;
	}

	for (const FHeistObjectAssemblyEntry& AssemblyEntry : Entry.AssemblyEntries)
	{
		if (AssemblyEntry.PartId.IsNone() || AssemblyEntry.SocketId.IsNone())
		{
			return false;
		}
	}
	return true;
}
}

bool FHeistReplicaRecapEntry::NetSerialize(FArchive& Ar, UPackageMap*, bool& bOutSuccess)
{
	static_assert(sizeof(FColor) == 4, "Replica recap palette network serialization requires four-byte FColor values.");

	if (Ar.IsSaving() && !IsValidReplicaRecapPayload(*this))
	{
		Ar.SetError();
		bOutSuccess = false;
		return true;
	}

	Ar << CaseId;
	Ar << ArtifactId;
	Ar << ArtifactDisplayName;
	Ar << TemplateId;
	uint8 SerializedForgeryType = Ar.IsSaving() ? static_cast<uint8>(ForgeryType) : 0;
	Ar.SerializeBits(&SerializedForgeryType, 2);
	Ar << QualityScore;
	uint8 SerializedRequiredTarget = Ar.IsSaving() && bRequiredTarget ? 1 : 0;
	Ar.SerializeBits(&SerializedRequiredTarget, 1);

	uint32 SerializedPaintingResolution = Ar.IsSaving() ? static_cast<uint32>(PaintingResolution) : 0;
	uint32 SerializedPaletteCount = Ar.IsSaving() ? static_cast<uint32>(PaintingPalette.Num()) : 0;
	uint32 SerializedPackedByteCount = Ar.IsSaving() ? static_cast<uint32>(PaintingPackedPaletteIndices.Num()) : 0;
	uint32 SerializedAssemblyEntryCount = Ar.IsSaving() ? static_cast<uint32>(AssemblyEntries.Num()) : 0;
	Ar.SerializeIntPacked(SerializedPaintingResolution);
	Ar.SerializeIntPacked(SerializedPaletteCount);
	Ar.SerializeIntPacked(SerializedPackedByteCount);
	Ar.SerializeIntPacked(SerializedAssemblyEntryCount);

	if (Ar.IsLoading())
	{
		const bool bClearedDrawingHeader = SerializedForgeryType == static_cast<uint8>(EHeistForgeryType::Drawing) && SerializedPaintingResolution == 0 &&
			SerializedPaletteCount == 0 && SerializedPackedByteCount == 0 && SerializedAssemblyEntryCount == 0;
		const bool bCompleteDrawingHeader = SerializedForgeryType == static_cast<uint8>(EHeistForgeryType::Drawing) &&
			SerializedPaintingResolution == static_cast<uint32>(PaintingThumbnailResolution) &&
			FMath::IsWithinInclusive(SerializedPaletteCount, 2U, static_cast<uint32>(MaximumPaintingPaletteColors)) &&
			SerializedPackedByteCount == static_cast<uint32>(FMath::DivideAndRoundUp(PaintingThumbnailResolution * PaintingThumbnailResolution, 2)) &&
			SerializedAssemblyEntryCount == 0;
		const bool bAssemblyHeader = SerializedForgeryType == static_cast<uint8>(EHeistForgeryType::Assembly) && SerializedPaintingResolution == 0 &&
			SerializedPaletteCount == 0 && SerializedPackedByteCount == 0 && FMath::IsWithinInclusive(SerializedAssemblyEntryCount, 1U, static_cast<uint32>(MaximumAssemblyEntries));
		if (!bClearedDrawingHeader && !bCompleteDrawingHeader && !bAssemblyHeader)
		{
			Ar.SetError();
			bOutSuccess = false;
			return true;
		}

		ForgeryType = static_cast<EHeistForgeryType>(SerializedForgeryType);
		bRequiredTarget = SerializedRequiredTarget != 0;
		PaintingResolution = static_cast<int32>(SerializedPaintingResolution);
		PaintingPalette.SetNumUninitialized(static_cast<int32>(SerializedPaletteCount));
		PaintingPackedPaletteIndices.SetNumUninitialized(static_cast<int32>(SerializedPackedByteCount));
		AssemblyEntries.SetNum(static_cast<int32>(SerializedAssemblyEntryCount));
	}

	if (!PaintingPalette.IsEmpty())
	{
		Ar.Serialize(PaintingPalette.GetData(), static_cast<int64>(PaintingPalette.Num()) * sizeof(FColor));
	}
	if (!PaintingPackedPaletteIndices.IsEmpty())
	{
		Ar.Serialize(PaintingPackedPaletteIndices.GetData(), PaintingPackedPaletteIndices.Num());
	}
	for (FHeistObjectAssemblyEntry& AssemblyEntry : AssemblyEntries)
	{
		Ar << AssemblyEntry.PartId;
		Ar << AssemblyEntry.SocketId;
		Ar.Serialize(&AssemblyEntry.QuantizedOrientation, sizeof(AssemblyEntry.QuantizedOrientation));
		Ar << AssemblyEntry.MaterialId;
	}

	if (Ar.IsLoading() && !IsValidReplicaRecapPayload(*this))
	{
		Ar.SetError();
		bOutSuccess = false;
		return true;
	}

	bOutSuccess = !Ar.IsError();
	return true;
}

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

namespace HeistCrewStatus
{
FText ToDisplayText(const EHeistCrewStatus Status)
{
	switch (Status)
	{
	case EHeistCrewStatus::Forging:
		return NSLOCTEXT("HeistCrewStatus", "Forging", "위조 중");
	case EHeistCrewStatus::Assembling:
		return NSLOCTEXT("HeistCrewStatus", "Assembling", "조립 중");
	case EHeistCrewStatus::CarryingOriginal:
		return NSLOCTEXT("HeistCrewStatus", "CarryingOriginal", "원본 운반");
	case EHeistCrewStatus::Heavy:
		return NSLOCTEXT("HeistCrewStatus", "Heavy", "과적");
	case EHeistCrewStatus::Stunned:
		return NSLOCTEXT("HeistCrewStatus", "Stunned", "기절");
	case EHeistCrewStatus::Arrested:
		return NSLOCTEXT("HeistCrewStatus", "Arrested", "체포");
	case EHeistCrewStatus::Escaped:
		return NSLOCTEXT("HeistCrewStatus", "Escaped", "탈출");
	case EHeistCrewStatus::Active:
	default:
		return NSLOCTEXT("HeistCrewStatus", "Active", "활동 중");
	}
}

FText ToCompactText(const EHeistCrewStatus Status)
{
	switch (Status)
	{
	case EHeistCrewStatus::Forging:
		return NSLOCTEXT("HeistCrewStatus", "ForgingCompact", "위조");
	case EHeistCrewStatus::Assembling:
		return NSLOCTEXT("HeistCrewStatus", "AssemblingCompact", "조립");
	case EHeistCrewStatus::CarryingOriginal:
		return NSLOCTEXT("HeistCrewStatus", "CarryingOriginalCompact", "원본");
	case EHeistCrewStatus::Heavy:
		return NSLOCTEXT("HeistCrewStatus", "HeavyCompact", "과적");
	case EHeistCrewStatus::Stunned:
		return NSLOCTEXT("HeistCrewStatus", "StunnedCompact", "기절");
	case EHeistCrewStatus::Arrested:
		return NSLOCTEXT("HeistCrewStatus", "ArrestedCompact", "체포");
	case EHeistCrewStatus::Escaped:
		return NSLOCTEXT("HeistCrewStatus", "EscapedCompact", "탈출");
	case EHeistCrewStatus::Active:
	default:
		return NSLOCTEXT("HeistCrewStatus", "ActiveCompact", "활동");
	}
}

FText ToIconGlyph(const EHeistCrewStatus Status)
{
	switch (Status)
	{
	case EHeistCrewStatus::Forging:
		return NSLOCTEXT("HeistCrewStatus", "ForgingIcon", "위");
	case EHeistCrewStatus::Assembling:
		return NSLOCTEXT("HeistCrewStatus", "AssemblingIcon", "조");
	case EHeistCrewStatus::CarryingOriginal:
		return NSLOCTEXT("HeistCrewStatus", "CarryingOriginalIcon", "원");
	case EHeistCrewStatus::Heavy:
		return NSLOCTEXT("HeistCrewStatus", "HeavyIcon", "과");
	case EHeistCrewStatus::Stunned:
		return NSLOCTEXT("HeistCrewStatus", "StunnedIcon", "기");
	case EHeistCrewStatus::Arrested:
		return NSLOCTEXT("HeistCrewStatus", "ArrestedIcon", "체");
	case EHeistCrewStatus::Escaped:
		return NSLOCTEXT("HeistCrewStatus", "EscapedIcon", "탈");
	case EHeistCrewStatus::Active:
	default:
		return NSLOCTEXT("HeistCrewStatus", "ActiveIcon", "활");
	}
}

FLinearColor GetPresentationColor(const EHeistCrewStatus Status)
{
	switch (Status)
	{
	case EHeistCrewStatus::Forging:
		return FLinearColor(0.90f, 0.38f, 0.08f, 1.0f);
	case EHeistCrewStatus::Assembling:
		return FLinearColor(0.54f, 0.25f, 0.78f, 1.0f);
	case EHeistCrewStatus::CarryingOriginal:
		return FLinearColor(0.85f, 0.60f, 0.08f, 1.0f);
	case EHeistCrewStatus::Heavy:
		return FLinearColor(0.72f, 0.18f, 0.10f, 1.0f);
	case EHeistCrewStatus::Stunned:
		return FLinearColor(0.05f, 0.58f, 0.78f, 1.0f);
	case EHeistCrewStatus::Arrested:
		return FLinearColor(0.68f, 0.06f, 0.12f, 1.0f);
	case EHeistCrewStatus::Escaped:
		return FLinearColor(0.12f, 0.62f, 0.28f, 1.0f);
	case EHeistCrewStatus::Active:
	default:
		return FLinearColor(0.22f, 0.42f, 0.62f, 1.0f);
	}
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
