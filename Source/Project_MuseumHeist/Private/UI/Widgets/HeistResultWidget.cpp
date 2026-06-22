#include "UI/Widgets/HeistResultWidget.h"

#include "MVVMBlueprintLibrary.h"
#include "UI/ViewModels/HeistResultViewModel.h"

#pragma region Construction

UHeistResultWidget::UHeistResultWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#pragma endregion

#pragma region ViewModel

void UHeistResultWidget::SetupResultWidget(UHeistResultViewModel* InResultViewModel)
{
	checkf(IsValid(InResultViewModel), TEXT("HeistResultWidget requires a valid HeistResultViewModel"));

	ResultViewModel = InResultViewModel;

	TScriptInterface<INotifyFieldValueChanged> ViewModelInterface;
	ViewModelInterface.SetObject(ResultViewModel);
	ViewModelInterface.SetInterface(ResultViewModel);
	UMVVMBlueprintLibrary::SetViewModelByClass(this, ViewModelInterface);
}

UHeistResultViewModel* UHeistResultWidget::GetResultViewModel() const
{
	return ResultViewModel;
}

#pragma endregion
