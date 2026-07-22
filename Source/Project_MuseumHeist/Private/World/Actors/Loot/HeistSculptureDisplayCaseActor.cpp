#include "World/Actors/Loot/HeistSculptureDisplayCaseActor.h"

AHeistSculptureDisplayCaseActor::AHeistSculptureDisplayCaseActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

FName AHeistSculptureDisplayCaseActor::GetSculptureCaseId() const
{
	return SculptureCaseId;
}

FName AHeistSculptureDisplayCaseActor::GetTargetArtifactId() const
{
	return TargetArtifactId;
}

bool AHeistSculptureDisplayCaseActor::CanInteract(
	const AActor* /*Interactor*/) const
{
	// Sculpture assembly is explicitly deferred. Keeping this false prevents
	// the painting observation/forgery flow from reaching sculpture exhibits.
	return false;
}
