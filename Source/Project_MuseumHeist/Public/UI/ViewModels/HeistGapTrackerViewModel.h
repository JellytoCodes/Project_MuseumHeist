#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"

#include "HeistGapTrackerViewModel.generated.h"

UCLASS(BlueprintType)
class PROJECT_MUSEUMHEIST_API UHeistGapTrackerViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UHeistGapTrackerViewModel(const FObjectInitializer& ObjectInitializer);

	void SetupViewModel();
};
