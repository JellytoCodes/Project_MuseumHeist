#pragma once

#include "CoreMinimal.h"
#include "World/Actors/Loot/HeistPaintingDisplayCaseActor.h"

#include "HeistDisplayCaseActor.generated.h"

/**
 * Legacy Blueprint parent retained so existing BP_DisplayCase assets keep
 * loading. New painting cases must derive from AHeistPaintingDisplayCaseActor.
 */
UCLASS(
	meta = (
		DeprecatedNode,
		DeprecationMessage = "Use AHeistPaintingDisplayCaseActor for painting exhibits."))
class PROJECT_MUSEUMHEIST_API AHeistDisplayCaseActor
	: public AHeistPaintingDisplayCaseActor
{
	GENERATED_BODY()

public:
	AHeistDisplayCaseActor();
};
