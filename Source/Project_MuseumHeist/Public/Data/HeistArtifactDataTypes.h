#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "Engine/DataTable.h"

#include "HeistArtifactDataTypes.generated.h"

class AActor;

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
