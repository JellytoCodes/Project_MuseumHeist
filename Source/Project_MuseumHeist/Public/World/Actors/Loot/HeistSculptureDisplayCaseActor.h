#pragma once

#include "CoreMinimal.h"
#include "World/Interaction/HeistInteractableActor.h"

#include "HeistSculptureDisplayCaseActor.generated.h"

/**
 * Dedicated shell for sculpture exhibits.
 *
 * Sculpture assembly is outside the active v1.0 scope, so this actor only
 * owns sculpture-case identity and presentation assembly. It intentionally
 * does not inherit painting state, palette raster, texture projection, or
 * replica placement behavior.
 */
UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistSculptureDisplayCaseActor : public AHeistInteractableActor
{
	GENERATED_BODY()

  public:
	AHeistSculptureDisplayCaseActor();

	UFUNCTION(BlueprintPure, Category = "Heist|SculptureCase")
	FName GetSculptureCaseId() const;

	UFUNCTION(BlueprintPure, Category = "Heist|SculptureCase")
	FName GetTargetArtifactId() const;

  protected:
	virtual bool CanInteract(const AActor* Interactor) const override;

  private:
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Heist|SculptureCase", meta = (AllowPrivateAccess = "true"))
	FName SculptureCaseId = TEXT("SculptureCase_Unassigned");

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Heist|SculptureCase", meta = (AllowPrivateAccess = "true"))
	FName TargetArtifactId = NAME_None;
};
