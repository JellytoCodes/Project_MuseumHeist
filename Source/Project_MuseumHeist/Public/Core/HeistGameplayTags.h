#pragma once

#include "CoreMinimal.h"

struct PROJECT_MUSEUMHEIST_API FHeistGameplayTags
{
	static const FHeistGameplayTags& Get();
	static void InitializeNativeGameplayTags();

private:
	static FHeistGameplayTags GameplayTags;
};
