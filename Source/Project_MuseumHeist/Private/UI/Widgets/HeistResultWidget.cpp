#include "UI/Widgets/HeistResultWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "UI/ViewModels/HeistResultViewModel.h"
#include "View/MVVMView.h"

#pragma region Construction

UHeistResultWidget::UHeistResultWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#pragma endregion

#pragma region Lifecycle

void UHeistResultWidget::NativeDestruct()
{
	if (IsValid(ResultViewModel))
	{
		ResultViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	}

	Super::NativeDestruct();
}

#pragma endregion

#pragma region ViewModel

void UHeistResultWidget::SetupResultWidget(UHeistResultViewModel* InResultViewModel)
{
	checkf(IsValid(InResultViewModel), TEXT("HeistResultWidget requires a valid HeistResultViewModel"));

	if (IsValid(ResultViewModel))
	{
		ResultViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	}

	ResultViewModel = InResultViewModel;
	ResultViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	ResultViewModel->GetSnapshotChangedDelegate().AddUObject(
		this,
		&UHeistResultWidget::RefreshResultPresentation);

	TScriptInterface<INotifyFieldValueChanged> ViewModelInterface;
	ViewModelInterface.SetObject(ResultViewModel);
	ViewModelInterface.SetInterface(ResultViewModel);

	if (UMVVMView* MVVMView = GetExtension<UMVVMView>())
	{
		MVVMView->SetViewModelByClass(ViewModelInterface);
	}

	RefreshResultPresentation();
}

UHeistResultViewModel* UHeistResultWidget::GetResultViewModel() const
{
	return ResultViewModel;
}

#pragma endregion

#pragma region Presentation

void UHeistResultWidget::RefreshResultPresentation()
{
	if (!IsValid(ResultViewModel))
	{
		return;
	}

	if (IsValid(WinnerIdText))
	{
		WinnerIdText->SetText(ResultViewModel->GetWinnerIdText());
	}

	if (IsValid(MyRankText))
	{
		MyRankText->SetText(ResultViewModel->GetMyRankText());
	}

	if (IsValid(MyFinalScoreText))
	{
		MyFinalScoreText->SetText(ResultViewModel->GetMyFinalScoreText());
	}

	if (IsValid(EscapedBadge))
	{
		EscapedBadge->SetVisibility(ResultViewModel->GetEscapedVisibility());
	}

	if (IsValid(ResultRow1Container))
	{
		ResultRow1Container->SetVisibility(ResultViewModel->GetResultRow1Visibility());
	}
	if (IsValid(ResultRow2Container))
	{
		ResultRow2Container->SetVisibility(ResultViewModel->GetResultRow2Visibility());
	}
	if (IsValid(ResultRow3Container))
	{
		ResultRow3Container->SetVisibility(ResultViewModel->GetResultRow3Visibility());
	}
	if (IsValid(ResultRow4Container))
	{
		ResultRow4Container->SetVisibility(ResultViewModel->GetResultRow4Visibility());
	}

	if (IsValid(ResultRow1TextBlock))
	{
		ResultRow1TextBlock->SetText(ResultViewModel->GetResultRow1Text());
	}
	if (IsValid(ResultRow2TextBlock))
	{
		ResultRow2TextBlock->SetText(ResultViewModel->GetResultRow2Text());
	}
	if (IsValid(ResultRow3TextBlock))
	{
		ResultRow3TextBlock->SetText(ResultViewModel->GetResultRow3Text());
	}
	if (IsValid(ResultRow4TextBlock))
	{
		ResultRow4TextBlock->SetText(ResultViewModel->GetResultRow4Text());
	}
}

#pragma endregion
