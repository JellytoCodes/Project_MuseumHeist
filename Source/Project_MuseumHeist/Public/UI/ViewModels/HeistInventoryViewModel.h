#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"

#include "HeistInventoryViewModel.generated.h"

UCLASS(BlueprintType)
class PROJECT_MUSEUMHEIST_API UHeistInventoryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UHeistInventoryViewModel(const FObjectInitializer& ObjectInitializer);

	void SetupViewModel();
};
