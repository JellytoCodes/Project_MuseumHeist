#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"

#include "HeistMapPresentationDataTypes.generated.h"

class UTexture2D;

/** World axis shown at the top of a Floor Plan texture. */
UENUM(BlueprintType)
enum class EHeistMapNorthAxis : uint8
{
	PositiveX,
	PositiveY,
	NegativeX,
	NegativeY,
	MAX UMETA(Hidden)
};

/** Marker categories intentionally allowed by the v1 Floor Plan information policy. */
UENUM(BlueprintType)
enum class EHeistFloorPlanMarkerType : uint8
{
	LocalPlayer,
	Teammate,
	Exit,
	Zone,
	TargetGallery,
	DiscoveredTarget,
	DroppedOriginal,
	EscapedTeammate,
	ArrestedTeammate,
	MAX UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistMapZoneAnchor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Map")
	FName ZoneId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Map")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Map")
	FVector2D WorldLocation = FVector2D::ZeroVector;
};

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistMapExitAnchor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Map")
	FName ExitId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Map")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Map")
	FVector2D WorldLocation = FVector2D::ZeroVector;
};

/**
 * Presentation-only data for one fixed museum map.
 * This row never changes Gameplay state or exposes Guard/SoundPing/hidden-loot data.
 */
USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistMapPresentationRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Map")
	FName MapId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Map")
	FText MapDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Map")
	TSoftObjectPtr<UTexture2D> FloorPlanTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Map")
	FVector2D WorldMin = FVector2D(-3000.0, -3000.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Map")
	FVector2D WorldMax = FVector2D(3000.0, 3000.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Map")
	EHeistMapNorthAxis MapNorthAxis = EHeistMapNorthAxis::PositiveY;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Map")
	TArray<FHeistMapZoneAnchor> ZoneAnchors;

	/** Zone shown as the Contract Target Gallery before the exact Required Target is observed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Map")
	FName ContractTargetGalleryZoneId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Map")
	TArray<FHeistMapExitAnchor> DefaultExitAnchors;

	bool IsRuntimeDefinitionValid(FString* OutFailureReason = nullptr) const;
	FVector2D ProjectWorldLocationToMapUV(const FVector2D& WorldLocation) const;
	bool ContainsWorldLocation(const FVector2D& WorldLocation) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
