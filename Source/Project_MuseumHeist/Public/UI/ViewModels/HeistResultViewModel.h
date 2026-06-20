#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"

#include "HeistResultViewModel.generated.h"

UCLASS(BlueprintType)
class PROJECT_MUSEUMHEIST_API UHeistResultViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UHeistResultViewModel(const FObjectInitializer& ObjectInitializer);

	void SetupViewModel();
};
