#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistResultWidget.generated.h"

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistResultWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

#pragma region Construction

public:
	UHeistResultWidget(const FObjectInitializer& ObjectInitializer);

#pragma endregion

#pragma region ViewModel

public:
	void SetupResultWidget(class UHeistResultViewModel* InResultViewModel);
	UHeistResultViewModel* GetResultViewModel() const;

private:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistResultViewModel> ResultViewModel;

#pragma endregion
};
