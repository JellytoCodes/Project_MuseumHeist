#include "World/Actors/Loot/HeistSculptureDisplayCaseActor.h"

AHeistSculptureDisplayCaseActor::AHeistSculptureDisplayCaseActor()
{
	SetObjectIdentityForLegacyMigration(SculptureCaseId, TargetArtifactId, TEXT("Sculpture"));
}

void AHeistSculptureDisplayCaseActor::PostLoad()
{
	Super::PostLoad();
	SetObjectIdentityForLegacyMigration(SculptureCaseId, TargetArtifactId, TEXT("Sculpture"));
}

FName AHeistSculptureDisplayCaseActor::GetSculptureCaseId() const
{
	return GetObjectCaseId();
}
