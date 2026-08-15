#include "Data/HeistMapPresentationDataTypes.h"

namespace
{
bool IsFiniteVector(const FVector2D& Value)
{
	return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y);
}
}

bool FHeistMapPresentationRow::IsRuntimeDefinitionValid(FString* OutFailureReason) const
{
	const auto Fail = [OutFailureReason](const TCHAR* FailureReason)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FailureReason;
		}
		return false;
	};

	if (MapId.IsNone())
	{
		return Fail(TEXT("MissingMapId"));
	}
	if (MapDisplayName.IsEmpty())
	{
		return Fail(TEXT("MissingMapDisplayName"));
	}
	if (FloorPlanTexture.IsNull())
	{
		return Fail(TEXT("MissingFloorPlanTexture"));
	}
	if (!FloorPlanTexture.ToSoftObjectPath().ToString().Contains(MapId.ToString(), ESearchCase::IgnoreCase))
	{
		return Fail(TEXT("FloorPlanTextureMapIdMismatch"));
	}
	if (!IsFiniteVector(WorldMin) || !IsFiniteVector(WorldMax) || WorldMax.X <= WorldMin.X || WorldMax.Y <= WorldMin.Y)
	{
		return Fail(TEXT("InvalidWorldBounds"));
	}
	if (MapNorthAxis >= EHeistMapNorthAxis::MAX)
	{
		return Fail(TEXT("InvalidMapNorthAxis"));
	}
	if (ZoneAnchors.IsEmpty())
	{
		return Fail(TEXT("MissingZoneAnchors"));
	}
	if (ContractTargetGalleryZoneId.IsNone())
	{
		return Fail(TEXT("MissingContractTargetGalleryZoneId"));
	}

	TSet<FName> UniqueZoneIds;
	bool bFoundTargetGallery = false;
	for (const FHeistMapZoneAnchor& ZoneAnchor : ZoneAnchors)
	{
		if (ZoneAnchor.ZoneId.IsNone() || ZoneAnchor.DisplayName.IsEmpty() || !IsFiniteVector(ZoneAnchor.WorldLocation) ||
			!ContainsWorldLocation(ZoneAnchor.WorldLocation) || UniqueZoneIds.Contains(ZoneAnchor.ZoneId))
		{
			return Fail(TEXT("InvalidZoneAnchor"));
		}
		UniqueZoneIds.Add(ZoneAnchor.ZoneId);
		bFoundTargetGallery |= ZoneAnchor.ZoneId == ContractTargetGalleryZoneId;
	}
	if (!bFoundTargetGallery)
	{
		return Fail(TEXT("TargetGalleryZoneNotFound"));
	}
	if (DefaultExitAnchors.IsEmpty())
	{
		return Fail(TEXT("MissingDefaultExitAnchors"));
	}

	TSet<FName> UniqueExitIds;
	for (const FHeistMapExitAnchor& ExitAnchor : DefaultExitAnchors)
	{
		if (ExitAnchor.ExitId.IsNone() || ExitAnchor.DisplayName.IsEmpty() || !IsFiniteVector(ExitAnchor.WorldLocation) ||
			!ContainsWorldLocation(ExitAnchor.WorldLocation) || UniqueExitIds.Contains(ExitAnchor.ExitId))
		{
			return Fail(TEXT("InvalidExitAnchor"));
		}
		UniqueExitIds.Add(ExitAnchor.ExitId);
	}

	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}
	return true;
}

FVector2D FHeistMapPresentationRow::ProjectWorldLocationToMapUV(const FVector2D& WorldLocation) const
{
	const FVector2D Size = WorldMax - WorldMin;
	if (!IsFiniteVector(WorldLocation) || Size.X <= 0.0 || Size.Y <= 0.0)
	{
		return FVector2D(0.5, 0.5);
	}

	const double NormalizedX = FMath::Clamp((WorldLocation.X - WorldMin.X) / Size.X, 0.0, 1.0);
	const double NormalizedY = FMath::Clamp((WorldLocation.Y - WorldMin.Y) / Size.Y, 0.0, 1.0);
	switch (MapNorthAxis)
	{
	case EHeistMapNorthAxis::PositiveX:
		return FVector2D(1.0 - NormalizedY, 1.0 - NormalizedX);
	case EHeistMapNorthAxis::NegativeX:
		return FVector2D(NormalizedY, NormalizedX);
	case EHeistMapNorthAxis::NegativeY:
		return FVector2D(1.0 - NormalizedX, NormalizedY);
	case EHeistMapNorthAxis::PositiveY:
	default:
		return FVector2D(NormalizedX, 1.0 - NormalizedY);
	}
}

bool FHeistMapPresentationRow::ContainsWorldLocation(const FVector2D& WorldLocation) const
{
	return IsFiniteVector(WorldLocation) && WorldLocation.X >= WorldMin.X && WorldLocation.X <= WorldMax.X && WorldLocation.Y >= WorldMin.Y &&
		WorldLocation.Y <= WorldMax.Y;
}

#if WITH_EDITOR
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "HeistMapPresentationDataValidation"

EDataValidationResult FHeistMapPresentationRow::IsDataValid(FDataValidationContext& Context) const
{
	FString FailureReason;
	if (IsRuntimeDefinitionValid(&FailureReason))
	{
		return EDataValidationResult::Valid;
	}

	Context.AddError(FText::Format(LOCTEXT("InvalidMapPresentationDefinition", "Floor Plan presentation row is invalid: {0}."), FText::FromString(FailureReason)));
	return EDataValidationResult::Invalid;
}

#undef LOCTEXT_NAMESPACE
#endif
