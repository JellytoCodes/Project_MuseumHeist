#pragma once

#include "CoreMinimal.h"
#include "World/Actors/Loot/HeistObjectDisplayCaseActor.h"

#include "HeistSculptureDisplayCaseActor.generated.h"

/**
 * Deprecated compatibility alias for existing BP_SculptureDisplayCase assets.
 * New Sculpture and Ceramic assets must derive from AHeistObjectDisplayCaseActor.
 */
UCLASS(Deprecated, meta = (DeprecationMessage = "Use AHeistObjectDisplayCaseActor for new Sculpture and Ceramic assets."))
class PROJECT_MUSEUMHEIST_API AHeistSculptureDisplayCaseActor : public AHeistObjectDisplayCaseActor
{
	GENERATED_BODY()

  public:
	AHeistSculptureDisplayCaseActor();

	virtual void PostLoad() override;

	UFUNCTION(BlueprintPure, Category = "Heist|SculptureCase")
	FName GetSculptureCaseId() const;

  private:
	/** Serialized identity retained only for existing BP_SculptureDisplayCase assets. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Heist|SculptureCase", meta = (AllowPrivateAccess = "true"))
	FName SculptureCaseId = TEXT("SculptureCase_Unassigned");

	/** Serialized identity retained only for existing BP_SculptureDisplayCase assets. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Heist|SculptureCase", meta = (AllowPrivateAccess = "true"))
	FName TargetArtifactId = NAME_None;
};
