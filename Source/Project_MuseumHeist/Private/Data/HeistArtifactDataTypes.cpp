#include "Data/HeistArtifactDataTypes.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "HeistArtifactDataValidation"

EDataValidationResult FHeistArtifactDataRow::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	auto AddError = [&Context, &Result](const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	};

	if (ArtifactId.IsNone())
	{
		AddError(LOCTEXT("MissingArtifactId", "ArtifactId must not be None."));
	}

	if (DisplayName.IsEmpty())
	{
		AddError(LOCTEXT("MissingDisplayName", "DisplayName must not be empty."));
	}

	if (ArtifactValue <= 0)
	{
		AddError(LOCTEXT("InvalidArtifactValue", "ArtifactValue must be greater than zero."));
	}

	if (Weight < 0.0f)
	{
		AddError(LOCTEXT("InvalidWeight", "Weight must be zero or greater."));
	}

	if (GridWidth <= 0 || GridHeight <= 0)
	{
		AddError(LOCTEXT("InvalidGridSize", "GridWidth and GridHeight must both be greater than zero."));
	}

	if (ForgeryType == EHeistForgeryType::None)
	{
		AddError(LOCTEXT("MissingForgeryType", "ForgeryType must select a supported forgery method."));
	}

	if (ForgeryTemplateId.IsNone())
	{
		AddError(LOCTEXT("MissingForgeryTemplateId", "ForgeryTemplateId must not be None."));
	}

	if (!FMath::IsWithinInclusive(MinimumForgeryScore, 0.0f, 1.0f))
	{
		AddError(LOCTEXT("InvalidMinimumForgeryScore", "MinimumForgeryScore must be between 0.0 and 1.0."));
	}

	if (BaseInspectionDelay < 0.0f)
	{
		AddError(LOCTEXT("InvalidBaseInspectionDelay", "BaseInspectionDelay must be zero or greater."));
	}

	if (VisualActorClass.IsNull())
	{
		AddError(LOCTEXT("MissingVisualActorClass", "VisualActorClass must reference an artifact presentation actor."));
	}

	return Result;
}

EDataValidationResult FHeistForgeryTemplateRow::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	auto AddError = [&Context, &Result](const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	};

	if (TemplateId.IsNone())
	{
		AddError(LOCTEXT("MissingTemplateId", "TemplateId must not be None."));
	}
	if (SurfacePoolId != FName(TEXT("M01")) && SurfacePoolId != FName(TEXT("M02")) && SurfacePoolId != FName(TEXT("M03")))
	{
		AddError(LOCTEXT("InvalidSurfacePoolId", "SurfacePoolId must be M01, M02, or M03."));
	}
	if (ReferenceImage.IsNull())
	{
		AddError(LOCTEXT("MissingReferenceImage", "ReferenceImage must reference a Texture2D."));
	}
	if (BackgroundFilterMode == EHeistForgeryBackgroundFilter::None && ReferenceMask.IsNull())
	{
		AddError(LOCTEXT("MissingReferenceMask", "ReferenceMask must reference a Texture2D when BackgroundFilterMode is None."));
	}
	if (!FMath::IsWithinInclusive(BackgroundColorTolerance, 0.0f, 0.49f))
	{
		AddError(LOCTEXT("InvalidBackgroundColorTolerance", "BackgroundColorTolerance must be between 0.0 and 0.49."));
	}
	if (!FMath::IsWithinInclusive(AllowedPalette.Num(), 2, 8))
	{
		AddError(LOCTEXT("InvalidAllowedPaletteCount", "AllowedPalette must contain between 2 and 8 colors."));
	}
	for (const FLinearColor& PaletteColor : AllowedPalette)
	{
		if (!FMath::IsFinite(PaletteColor.R) || !FMath::IsFinite(PaletteColor.G) || !FMath::IsFinite(PaletteColor.B) || !FMath::IsFinite(PaletteColor.A))
		{
			AddError(LOCTEXT("InvalidAllowedPaletteColor", "AllowedPalette colors must contain finite RGBA values."));
			break;
		}
	}
	if (ObservationDuration < 0.0f)
	{
		AddError(LOCTEXT("InvalidObservationDuration", "ObservationDuration must be zero or greater."));
	}
	if (!FMath::IsFinite(ForgeryDuration) || !FMath::IsWithinInclusive(ForgeryDuration, 20.0f, 45.0f))
	{
		AddError(LOCTEXT("InvalidForgeryDuration", "ForgeryDuration must be finite and between 20 and 45 seconds."));
	}
	if (StrokeLimit <= 0)
	{
		AddError(LOCTEXT("InvalidStrokeLimit", "StrokeLimit must be greater than zero."));
	}
	if (!FMath::IsWithinInclusive(BrushSize, 0.001f, 0.25f))
	{
		AddError(LOCTEXT("InvalidBrushSize", "BrushSize must be between 0.001 and 0.25."));
	}
	if (!FMath::IsWithinInclusive(CoverageWeight, 0.0f, 1.0f) || !FMath::IsWithinInclusive(MajorShapeWeight, 0.0f, 1.0f) || !FMath::IsWithinInclusive(ExtraStrokePenaltyWeight, 0.0f, 1.0f) ||
		!FMath::IsWithinInclusive(TimeoutPenalty, 0.0f, 1.0f) || !FMath::IsWithinInclusive(ShapeAccuracyWeight, 0.0f, 1.0f) || !FMath::IsWithinInclusive(ColorAccuracyWeight, 0.0f, 1.0f))
	{
		AddError(LOCTEXT("InvalidForgeryWeights", "Forgery weights and penalties must be between 0.0 and 1.0."));
	}
	if (CoverageWeight + MajorShapeWeight <= 0.0f)
	{
		AddError(LOCTEXT("MissingPositiveForgeryScoreWeight", "CoverageWeight and MajorShapeWeight cannot both be zero."));
	}
	if (ShapeAccuracyWeight + ColorAccuracyWeight <= 0.0f)
	{
		AddError(LOCTEXT("MissingPositiveAccuracyWeight", "ShapeAccuracyWeight and ColorAccuracyWeight cannot both be zero."));
	}
	if (MaximumPaintToReferenceRatio < 1.0f)
	{
		AddError(LOCTEXT("InvalidMaximumPaintRatio", "MaximumPaintToReferenceRatio must be 1.0 or greater."));
	}
	if (!FMath::IsWithinInclusive(OverpaintScoreCap, 0.0f, 100.0f))
	{
		AddError(LOCTEXT("InvalidOverpaintScoreCap", "OverpaintScoreCap must be between 0.0 and 100.0."));
	}

	return Result;
}

EDataValidationResult FHeistObjectAssemblyPartRow::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	auto AddError = [&Context, &Result](const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	};

	if (PartId.IsNone())
	{
		AddError(LOCTEXT("MissingObjectAssemblyPartId", "Object Assembly PartId must not be None."));
	}
	if (FamilyId.IsNone())
	{
		AddError(LOCTEXT("MissingObjectAssemblyPartFamilyId", "Object Assembly part FamilyId must not be None."));
	}
	if (StaticMesh.IsNull())
	{
		AddError(LOCTEXT("MissingObjectAssemblyStaticMesh", "Object Assembly StaticMesh must reference a Static Mesh."));
	}
	if (CompatibleSocketIds.IsEmpty())
	{
		AddError(LOCTEXT("MissingObjectAssemblyCompatibleSockets", "CompatibleSocketIds must contain at least one socket."));
	}

	TSet<FName> UniqueSocketIds;
	for (const FName SocketId : CompatibleSocketIds)
	{
		if (SocketId.IsNone())
		{
			AddError(LOCTEXT("InvalidObjectAssemblyCompatibleSocket", "CompatibleSocketIds must not contain None."));
			continue;
		}
		if (UniqueSocketIds.Contains(SocketId))
		{
			AddError(LOCTEXT("DuplicateObjectAssemblyCompatibleSocket", "CompatibleSocketIds must not contain duplicates."));
		}
		UniqueSocketIds.Add(SocketId);
	}

	TSet<FName> UniqueMaterialIds;
	for (const FName MaterialId : AllowedMaterialIds)
	{
		if (MaterialId.IsNone())
		{
			AddError(LOCTEXT("InvalidObjectAssemblyMaterialId", "AllowedMaterialIds must not contain None."));
			continue;
		}
		if (UniqueMaterialIds.Contains(MaterialId))
		{
			AddError(LOCTEXT("DuplicateObjectAssemblyMaterialId", "AllowedMaterialIds must not contain duplicates."));
		}
		UniqueMaterialIds.Add(MaterialId);
	}

	if (AllowedOrientationSteps.IsEmpty())
	{
		AddError(LOCTEXT("MissingObjectAssemblyOrientation", "AllowedOrientationSteps must contain at least one orientation index."));
	}

	TSet<uint8> UniqueOrientationSteps;
	for (const uint8 OrientationStep : AllowedOrientationSteps)
	{
		if (OrientationStep > 15)
		{
			AddError(LOCTEXT("InvalidObjectAssemblyOrientation", "AllowedOrientationSteps must use indices from 0 through 15."));
		}
		if (UniqueOrientationSteps.Contains(OrientationStep))
		{
			AddError(LOCTEXT("DuplicateObjectAssemblyOrientation", "AllowedOrientationSteps must not contain duplicates."));
		}
		UniqueOrientationSteps.Add(OrientationStep);
	}

	return Result;
}

EDataValidationResult FHeistObjectAssemblyTemplateRow::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	auto AddError = [&Context, &Result](const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	};

	if (TemplateId.IsNone())
	{
		AddError(LOCTEXT("MissingObjectAssemblyTemplateId", "Object Assembly TemplateId must not be None."));
	}
	if (FamilyId.IsNone())
	{
		AddError(LOCTEXT("MissingObjectAssemblyTemplateFamilyId", "Object Assembly template FamilyId must not be None."));
	}
	if (DisplayName.IsEmpty())
	{
		AddError(LOCTEXT("MissingObjectAssemblyDisplayName", "Object Assembly DisplayName must not be empty."));
	}
	if (CorePartId.IsNone())
	{
		AddError(LOCTEXT("MissingObjectAssemblyCorePartId", "Object Assembly CorePartId must not be None."));
	}
	if (!FMath::IsWithinInclusive(RequiredParts.Num(), 3, 5))
	{
		AddError(LOCTEXT("InvalidObjectAssemblyRequiredPartCount", "RequiredParts must contain between 3 and 5 manipulable parts."));
	}

	TSet<FName> UniqueRequiredPartIds;
	TSet<FName> UniqueRequiredSocketIds;
	for (const FHeistObjectAssemblyEntry& RequiredPart : RequiredParts)
	{
		if (RequiredPart.PartId.IsNone() || RequiredPart.SocketId.IsNone())
		{
			AddError(LOCTEXT("InvalidObjectAssemblyRequiredPartIdentity", "Every RequiredParts entry must provide a PartId and SocketId."));
		}
		if (RequiredPart.PartId == CorePartId)
		{
			AddError(LOCTEXT("ObjectAssemblyCoreRepeatedAsRequiredPart", "CorePartId must not also appear in RequiredParts."));
		}
		if (!RequiredPart.PartId.IsNone() && UniqueRequiredPartIds.Contains(RequiredPart.PartId))
		{
			AddError(LOCTEXT("DuplicateObjectAssemblyRequiredPart", "RequiredParts must not contain duplicate PartId values."));
		}
		if (!RequiredPart.PartId.IsNone())
		{
			UniqueRequiredPartIds.Add(RequiredPart.PartId);
		}
		if (!RequiredPart.SocketId.IsNone() && UniqueRequiredSocketIds.Contains(RequiredPart.SocketId))
		{
			AddError(LOCTEXT("DuplicateObjectAssemblyRequiredSocket", "RequiredParts must not contain duplicate SocketId values."));
		}
		if (!RequiredPart.SocketId.IsNone())
		{
			UniqueRequiredSocketIds.Add(RequiredPart.SocketId);
		}
		if (RequiredPart.QuantizedOrientation > 15)
		{
			AddError(LOCTEXT("InvalidObjectAssemblyRequiredOrientation", "RequiredParts QuantizedOrientation must use an index from 0 through 15."));
		}
	}

	TSet<FName> UniqueDecoyPartIds;
	for (const FName DecoyPartId : DecoyPartIds)
	{
		if (DecoyPartId.IsNone())
		{
			AddError(LOCTEXT("InvalidObjectAssemblyDecoyPart", "DecoyPartIds must not contain None."));
			continue;
		}
		if (DecoyPartId == CorePartId || UniqueRequiredPartIds.Contains(DecoyPartId))
		{
			AddError(LOCTEXT("ConflictingObjectAssemblyDecoyPart", "A decoy PartId must not be the core or a required part."));
		}
		if (UniqueDecoyPartIds.Contains(DecoyPartId))
		{
			AddError(LOCTEXT("DuplicateObjectAssemblyDecoyPart", "DecoyPartIds must not contain duplicates."));
		}
		UniqueDecoyPartIds.Add(DecoyPartId);
	}

	if (!FMath::IsFinite(AssemblyDuration) || !FMath::IsWithinInclusive(AssemblyDuration, 25.0f, 35.0f))
	{
		AddError(LOCTEXT("InvalidObjectAssemblyDuration", "AssemblyDuration must be finite and between 25 and 35 seconds."));
	}

	const bool bWeightsValid = FMath::IsWithinInclusive(RequiredPartWeight, 0.0f, 1.0f) && FMath::IsWithinInclusive(SocketTopologyWeight, 0.0f, 1.0f) &&
							   FMath::IsWithinInclusive(OrientationWeight, 0.0f, 1.0f) && FMath::IsWithinInclusive(MaterialWeight, 0.0f, 1.0f);
	if (!bWeightsValid)
	{
		AddError(LOCTEXT("InvalidObjectAssemblyWeights", "Object Assembly score weights must be finite values between 0.0 and 1.0."));
	}
	if (!FMath::IsFinite(RequiredPartWeight + SocketTopologyWeight + OrientationWeight + MaterialWeight) ||
		RequiredPartWeight + SocketTopologyWeight + OrientationWeight + MaterialWeight <= 0.0f)
	{
		AddError(LOCTEXT("MissingObjectAssemblyScoreWeight", "At least one Object Assembly score weight must be greater than zero."));
	}
	if (!FMath::IsWithinInclusive(ExtraPartScoreCap, 0.0f, 100.0f))
	{
		AddError(LOCTEXT("InvalidObjectAssemblyExtraPartScoreCap", "ExtraPartScoreCap must be a finite value between 0.0 and 100.0."));
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE
#endif
