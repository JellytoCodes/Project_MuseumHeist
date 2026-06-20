#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"

#include "HeistHUDViewModel.generated.h"

UCLASS(BlueprintType)
class PROJECT_MUSEUMHEIST_API UHeistHUDViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UHeistHUDViewModel(const FObjectInitializer& ObjectInitializer);

	void SetupViewModel();
};
