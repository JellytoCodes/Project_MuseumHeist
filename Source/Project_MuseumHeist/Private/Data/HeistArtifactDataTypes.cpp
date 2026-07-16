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

#undef LOCTEXT_NAMESPACE
#endif
