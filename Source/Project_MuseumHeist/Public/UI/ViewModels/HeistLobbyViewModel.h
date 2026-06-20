#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"

#include "HeistLobbyViewModel.generated.h"

UCLASS(BlueprintType)
class PROJECT_MUSEUMHEIST_API UHeistLobbyViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UHeistLobbyViewModel(const FObjectInitializer& ObjectInitializer);

	void SetupViewModel();
};
