#include "UI/Widgets/HeistResultWidget.h"

#include "Debug/HeistDebugFunctionLibrary.h"
#include "UI/ViewModels/HeistResultViewModel.h"
#include "View/MVVMView.h"

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

	if (UMVVMView* MVVMView = GetExtension<UMVVMView>())
	{
		MVVMView->SetViewModelByClass(ViewModelInterface);
	}
	else
	{
		UHeistDebugFunctionLibrary::DebugWidgetMissingMVVMView(this, TEXT("Result"));
	}
}

UHeistResultViewModel* UHeistResultWidget::GetResultViewModel() const
{
	return ResultViewModel;
}

#pragma endregion
