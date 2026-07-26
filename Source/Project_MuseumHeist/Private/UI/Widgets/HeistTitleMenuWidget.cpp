#include "UI/Widgets/HeistTitleMenuWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "UI/ViewModels/HeistTitleMenuViewModel.h"
#include "View/MVVMView.h"

#pragma region Construction

UHeistTitleMenuWidget::UHeistTitleMenuWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

#pragma endregion

#pragma region Lifecycle

void UHeistTitleMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(HostSessionButton))
	{
		HostSessionButton->OnClicked.AddUniqueDynamic(this, &UHeistTitleMenuWidget::HandleHostSessionClicked);
	}
	if (IsValid(JoinSessionButton))
	{
		JoinSessionButton->OnClicked.AddUniqueDynamic(this, &UHeistTitleMenuWidget::HandleJoinSessionClicked);
	}
}

void UHeistTitleMenuWidget::NativeDestruct()
{
	if (IsValid(TitleMenuViewModel))
	{
		TitleMenuViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	}
	if (IsValid(HostSessionButton))
	{
		HostSessionButton->OnClicked.RemoveDynamic(this, &UHeistTitleMenuWidget::HandleHostSessionClicked);
	}
	if (IsValid(JoinSessionButton))
	{
		JoinSessionButton->OnClicked.RemoveDynamic(this, &UHeistTitleMenuWidget::HandleJoinSessionClicked);
	}

	Super::NativeDestruct();
}

#pragma endregion

#pragma region ViewModel

void UHeistTitleMenuWidget::SetupTitleMenuWidget(UHeistTitleMenuViewModel* InTitleMenuViewModel)
{
	checkf(IsValid(InTitleMenuViewModel), TEXT("HeistTitleMenuWidget requires a valid HeistTitleMenuViewModel"));

	if (IsValid(TitleMenuViewModel))
	{
		TitleMenuViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	}

	TitleMenuViewModel = InTitleMenuViewModel;
	TitleMenuViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	TitleMenuViewModel->GetSnapshotChangedDelegate().AddUObject(this, &UHeistTitleMenuWidget::RefreshTitleMenuPresentation);

	TScriptInterface<INotifyFieldValueChanged> ViewModelInterface;
	ViewModelInterface.SetObject(TitleMenuViewModel);
	ViewModelInterface.SetInterface(TitleMenuViewModel);
	if (UMVVMView* MVVMView = GetExtension<UMVVMView>())
	{
		MVVMView->SetViewModelByClass(ViewModelInterface);
	}

	RefreshTitleMenuPresentation();
}

UHeistTitleMenuViewModel* UHeistTitleMenuWidget::GetTitleMenuViewModel() const
{
	return TitleMenuViewModel;
}

#pragma endregion

#pragma region Presentation

void UHeistTitleMenuWidget::HandleHostSessionClicked()
{
	if (IsValid(TitleMenuViewModel))
	{
		TitleMenuViewModel->RequestHostSession();
	}
}

void UHeistTitleMenuWidget::HandleJoinSessionClicked()
{
	if (IsValid(TitleMenuViewModel) && IsValid(JoinCodeInput))
	{
		TitleMenuViewModel->RequestJoinSessionByCode(JoinCodeInput->GetText().ToString());
	}
}

void UHeistTitleMenuWidget::RefreshTitleMenuPresentation()
{
	if (!IsValid(TitleMenuViewModel))
	{
		return;
	}

	if (IsValid(HostSessionButton))
	{
		HostSessionButton->SetIsEnabled(TitleMenuViewModel->CanRequestHostSession());
	}
	if (IsValid(JoinSessionButton))
	{
		JoinSessionButton->SetIsEnabled(TitleMenuViewModel->CanRequestJoinSession());
	}
	if (IsValid(JoinCodeInput))
	{
		JoinCodeInput->SetIsEnabled(TitleMenuViewModel->CanRequestJoinSession());
	}
	if (IsValid(SessionStatusText))
	{
		SessionStatusText->SetText(TitleMenuViewModel->GetSessionStatusText());
	}
	if (IsValid(SessionErrorText))
	{
		SessionErrorText->SetText(TitleMenuViewModel->GetSessionErrorText());
		SessionErrorText->SetVisibility(TitleMenuViewModel->GetSessionErrorVisibility());
	}
}

#pragma endregion
