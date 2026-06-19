#include "Core/HeistGameplayTags.h"

FHeistGameplayTags FHeistGameplayTags::GameplayTags;

const FHeistGameplayTags& FHeistGameplayTags::Get()
{
	return GameplayTags;
}

void FHeistGameplayTags::InitializeNativeGameplayTags()
{
	// Native gameplay tags will be registered here when the tag list is defined.
}
