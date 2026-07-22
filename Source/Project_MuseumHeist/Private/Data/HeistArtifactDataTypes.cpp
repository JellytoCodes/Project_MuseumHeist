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
	if (ForgeryDuration <= 0.0f)
	{
		AddError(LOCTEXT("InvalidForgeryDuration", "ForgeryDuration must be greater than zero."));
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

#undef LOCTEXT_NAMESPACE
#endif
