#include "UI/Widgets/HeistResultWidget.h"

#include "Core/HeistLogChannels.h"
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
		UE_LOG(
			LogHeistUI,
			Warning,
			TEXT("Result widget has no MVVMView extension; MVVM binding injection skipped. Widget=%s"),
			*GetNameSafe(this));
	}
}

UHeistResultViewModel* UHeistResultWidget::GetResultViewModel() const
{
	return ResultViewModel;
}

#pragma endregion
