#include "UI/Title/Widgets/HeistSessionJoinWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "UI/Title/ViewModels/HeistTitleMenuViewModel.h"
#include "View/MVVMView.h"

#pragma region Lifecycle

void UHeistSessionJoinWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(SubmitJoinSessionButton))
	{
		SubmitJoinSessionButton->OnClicked.AddUniqueDynamic(this, &UHeistSessionJoinWidget::HandleSubmitJoinSessionClicked);
	}
	if (IsValid(CancelSessionButton))
	{
		CancelSessionButton->OnClicked.AddUniqueDynamic(this, &UHeistSessionJoinWidget::HandleCancelSessionClicked);
	}
	if (IsValid(RetrySessionButton))
	{
		RetrySessionButton->OnClicked.AddUniqueDynamic(this, &UHeistSessionJoinWidget::HandleRetrySessionClicked);
	}
	if (IsValid(JoinCloseButton))
	{
		JoinCloseButton->OnClicked.AddUniqueDynamic(this, &UHeistSessionJoinWidget::HandleJoinCloseClicked);
	}

	SetVisibility(ESlateVisibility::Collapsed);
}

void UHeistSessionJoinWidget::NativeDestruct()
{
	if (IsValid(TitleMenuViewModel))
	{
		TitleMenuViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	}
	if (IsValid(SubmitJoinSessionButton))
	{
		SubmitJoinSessionButton->OnClicked.RemoveDynamic(this, &UHeistSessionJoinWidget::HandleSubmitJoinSessionClicked);
	}
	if (IsValid(CancelSessionButton))
	{
		CancelSessionButton->OnClicked.RemoveDynamic(this, &UHeistSessionJoinWidget::HandleCancelSessionClicked);
	}
	if (IsValid(RetrySessionButton))
	{
		RetrySessionButton->OnClicked.RemoveDynamic(this, &UHeistSessionJoinWidget::HandleRetrySessionClicked);
	}
	if (IsValid(JoinCloseButton))
	{
		JoinCloseButton->OnClicked.RemoveDynamic(this, &UHeistSessionJoinWidget::HandleJoinCloseClicked);
	}

	Super::NativeDestruct();
}

#pragma endregion

#pragma region ViewModel

void UHeistSessionJoinWidget::SetupSessionJoinWidget(UHeistTitleMenuViewModel* InTitleMenuViewModel)
{
	checkf(IsValid(InTitleMenuViewModel), TEXT("HeistSessionJoinWidget requires a valid HeistTitleMenuViewModel"));

	if (IsValid(TitleMenuViewModel))
	{
		TitleMenuViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	}

	TitleMenuViewModel = InTitleMenuViewModel;
	TitleMenuViewModel->GetSnapshotChangedDelegate().AddUObject(this, &UHeistSessionJoinWidget::RefreshSessionJoinPresentation);

	TScriptInterface<INotifyFieldValueChanged> ViewModelInterface;
	ViewModelInterface.SetObject(TitleMenuViewModel);
	ViewModelInterface.SetInterface(TitleMenuViewModel);
	if (UMVVMView* MVVMView = GetExtension<UMVVMView>())
	{
		MVVMView->SetViewModelByClass(ViewModelInterface);
	}

	RefreshSessionJoinPresentation();
}

void UHeistSessionJoinWidget::OpenSessionJoin()
{
	if (IsValid(TitleMenuViewModel))
	{
		TitleMenuViewModel->RefreshTitleMenuData();
	}
	SetVisibility(ESlateVisibility::Visible);
	RefreshSessionJoinPresentation();
}

void UHeistSessionJoinWidget::CloseSessionJoin()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

#pragma endregion

#pragma region Presentation

void UHeistSessionJoinWidget::HandleSubmitJoinSessionClicked()
{
	if (IsValid(TitleMenuViewModel) && IsValid(JoinCodeInput))
	{
		TitleMenuViewModel->RequestJoinSessionByCode(JoinCodeInput->GetText().ToString());
	}
}

void UHeistSessionJoinWidget::HandleCancelSessionClicked()
{
	if (IsValid(TitleMenuViewModel))
	{
		TitleMenuViewModel->RequestCancelSessionOperation();
	}
}

void UHeistSessionJoinWidget::HandleRetrySessionClicked()
{
	if (IsValid(TitleMenuViewModel))
	{
		TitleMenuViewModel->RequestRetrySessionOperation();
	}
}

void UHeistSessionJoinWidget::HandleJoinCloseClicked()
{
	if (IsValid(TitleMenuViewModel) && TitleMenuViewModel->IsSessionOperationPending())
	{
		if (TitleMenuViewModel->CanCancelSessionOperation())
		{
			TitleMenuViewModel->RequestCancelSessionOperation();
		}
		return;
	}

	CloseSessionJoin();
}

void UHeistSessionJoinWidget::RefreshSessionJoinPresentation()
{
	if (!IsValid(TitleMenuViewModel))
	{
		return;
	}

	if (IsValid(SubmitJoinSessionButton))
	{
		SubmitJoinSessionButton->SetIsEnabled(TitleMenuViewModel->CanRequestJoinSession());
	}
	if (IsValid(JoinCodeInput))
	{
		JoinCodeInput->SetIsEnabled(TitleMenuViewModel->CanRequestJoinSession());
	}
	if (IsValid(CancelSessionButton))
	{
		CancelSessionButton->SetIsEnabled(TitleMenuViewModel->CanCancelSessionOperation());
	}
	if (IsValid(RetrySessionButton))
	{
		RetrySessionButton->SetIsEnabled(TitleMenuViewModel->CanRetrySessionOperation());
	}
	if (IsValid(SessionErrorText))
	{
		SessionErrorText->SetText(TitleMenuViewModel->GetSessionErrorText());
		SessionErrorText->SetVisibility(TitleMenuViewModel->GetSessionErrorVisibility());
	}
}

#pragma endregion
