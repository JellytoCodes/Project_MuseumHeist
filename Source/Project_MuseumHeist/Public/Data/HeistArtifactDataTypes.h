#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "Engine/DataTable.h"

#include "HeistArtifactDataTypes.generated.h"

class AActor;
class UTexture2D;

UENUM(BlueprintType)
enum class EHeistForgeryBackgroundFilter : uint8
{
	None UMETA(DisplayName = "Use Reference Mask"),
	Black UMETA(DisplayName = "Filter Black Background"),
	White UMETA(DisplayName = "Filter White Background")
};

/** Static definition for an objective artifact shared by objective, display-case, and inventory flows. */
USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistArtifactDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Artifact")
	FName ArtifactId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Artifact")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Artifact", meta = (ClampMin = "0"))
	int32 ArtifactValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Artifact", meta = (ClampMin = "0.0"))
	float Weight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Artifact", meta = (ClampMin = "1"))
	int32 GridWidth = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Artifact", meta = (ClampMin = "1"))
	int32 GridHeight = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Artifact")
	EHeistForgeryType ForgeryType = EHeistForgeryType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Artifact")
	FName ForgeryTemplateId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Artifact", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumForgeryScore = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Artifact", meta = (ClampMin = "0.0", Units = "s"))
	float BaseInspectionDelay = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Artifact")
	TSoftClassPtr<AActor> VisualActorClass;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

/** Static drawing-reference definition selected by an artifact's ForgeryTemplateId. */
USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistForgeryTemplateRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Forgery")
	FName TemplateId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Forgery")
	TSoftObjectPtr<UTexture2D> ReferenceImage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Forgery")
	TSoftObjectPtr<UTexture2D> ReferenceMask;

	/**
	 * None uses ReferenceMask. Black/White derive the foreground directly
	 * from ReferenceImage by excluding the selected background color.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Forgery")
	EHeistForgeryBackgroundFilter BackgroundFilterMode = EHeistForgeryBackgroundFilter::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Forgery", meta = (ClampMin = "0.0", ClampMax = "0.49"))
	float BackgroundColorTolerance = 0.08f;

	/**
	 * Deliberately limited colors available to the player. The server
	 * quantizes ReferenceImage pixels to these indices before scoring.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Forgery", meta = (EditFixedOrder))
	TArray<FLinearColor> AllowedPalette = {FLinearColor(0.04f, 0.03f, 0.02f, 1.0f), FLinearColor(0.28f, 0.16f, 0.10f, 1.0f), FLinearColor(0.62f, 0.42f, 0.24f, 1.0f),
										   FLinearColor(0.90f, 0.84f, 0.68f, 1.0f)};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Forgery", meta = (ClampMin = "0.0", Units = "s"))
	float ObservationDuration = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Forgery", meta = (ClampMin = "1.0", Units = "s"))
	float ForgeryDuration = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Forgery", meta = (ClampMin = "1"))
	int32 StrokeLimit = 2048;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Forgery", meta = (ClampMin = "0.001", ClampMax = "0.25"))
	float BrushSize = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Forgery", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CoverageWeight = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Forgery", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MajorShapeWeight = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Forgery", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ExtraStrokePenaltyWeight = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Forgery", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TimeoutPenalty = 0.25f;

	/** Portion awarded by OpenCV distance-based shape similarity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Forgery", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ShapeAccuracyWeight = 0.65f;

	/** Portion awarded by OpenCV Lab SSIM and palette-distribution similarity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Forgery", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ColorAccuracyWeight = 0.35f;

	/**
	 * Submitted painted pixels divided by reference foreground pixels.
	 * Values above this threshold trigger the anti-fill score cap.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Forgery", meta = (ClampMin = "1.0"))
	float MaximumPaintToReferenceRatio = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Forgery", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float OverpaintScoreCap = 20.0f;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
