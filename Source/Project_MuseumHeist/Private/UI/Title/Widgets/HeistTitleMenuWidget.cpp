#include "UI/Title/Widgets/HeistTitleMenuWidget.h"

#include "Components/Button.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/Title/ViewModels/HeistSettingsViewModel.h"
#include "UI/Title/ViewModels/HeistTitleMenuViewModel.h"
#include "UI/Title/Widgets/HeistSessionJoinWidget.h"
#include "UI/Title/Widgets/HeistSettingsWidget.h"
#include "View/MVVMView.h"

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
	if (IsValid(SettingsButton))
	{
		SettingsButton->OnClicked.AddUniqueDynamic(this, &UHeistTitleMenuWidget::HandleSettingsClicked);
	}
	if (IsValid(QuitGameButton))
	{
		QuitGameButton->OnClicked.AddUniqueDynamic(this, &UHeistTitleMenuWidget::HandleQuitGameClicked);
	}

	CloseChildPanels();
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
	if (IsValid(SettingsButton))
	{
		SettingsButton->OnClicked.RemoveDynamic(this, &UHeistTitleMenuWidget::HandleSettingsClicked);
	}
	if (IsValid(QuitGameButton))
	{
		QuitGameButton->OnClicked.RemoveDynamic(this, &UHeistTitleMenuWidget::HandleQuitGameClicked);
	}

	Super::NativeDestruct();
}

#pragma endregion

#pragma region ViewModel

void UHeistTitleMenuWidget::SetupTitleMenuWidget(UHeistTitleMenuViewModel* InTitleMenuViewModel, UHeistSettingsViewModel* InSettingsViewModel)
{
	checkf(IsValid(InTitleMenuViewModel), TEXT("HeistTitleMenuWidget requires a valid HeistTitleMenuViewModel"));
	checkf(IsValid(InSettingsViewModel), TEXT("HeistTitleMenuWidget requires a valid HeistSettingsViewModel"));

	if (IsValid(TitleMenuViewModel))
	{
		TitleMenuViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	}

	TitleMenuViewModel = InTitleMenuViewModel;
	SettingsViewModel = InSettingsViewModel;
	TitleMenuViewModel->GetSnapshotChangedDelegate().AddUObject(this, &UHeistTitleMenuWidget::RefreshTitleMenuPresentation);

	TScriptInterface<INotifyFieldValueChanged> ViewModelInterface;
	ViewModelInterface.SetObject(TitleMenuViewModel);
	ViewModelInterface.SetInterface(TitleMenuViewModel);
	if (UMVVMView* MVVMView = GetExtension<UMVVMView>())
	{
		MVVMView->SetViewModelByClass(ViewModelInterface);
	}

	if (IsValid(SessionJoinWidget))
	{
		SessionJoinWidget->SetupSessionJoinWidget(TitleMenuViewModel);
	}
	if (IsValid(SettingsWidget))
	{
		SettingsWidget->SetupSettingsWidget(SettingsViewModel);
	}

	CloseChildPanels();
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
	CloseChildPanels();
	if (IsValid(TitleMenuViewModel))
	{
		TitleMenuViewModel->RequestHostSession();
	}
}

void UHeistTitleMenuWidget::HandleJoinSessionClicked()
{
	if (!IsValid(TitleMenuViewModel) || !TitleMenuViewModel->CanRequestJoinSession())
	{
		return;
	}

	if (IsValid(SettingsWidget))
	{
		SettingsWidget->CloseSettings();
	}
	if (IsValid(SessionJoinWidget))
	{
		SessionJoinWidget->OpenSessionJoin();
	}
}

void UHeistTitleMenuWidget::HandleSettingsClicked()
{
	if (IsValid(TitleMenuViewModel) && TitleMenuViewModel->IsSessionOperationPending())
	{
		return;
	}

	if (IsValid(SessionJoinWidget))
	{
		SessionJoinWidget->CloseSessionJoin();
	}
	if (IsValid(SettingsWidget))
	{
		SettingsWidget->OpenSettings();
	}
}

void UHeistTitleMenuWidget::HandleQuitGameClicked()
{
	if (APlayerController* OwningPlayerController = GetOwningPlayer())
	{
		UKismetSystemLibrary::QuitGame(this, OwningPlayerController, EQuitPreference::Quit, false);
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
	if (IsValid(SettingsButton))
	{
		SettingsButton->SetIsEnabled(!TitleMenuViewModel->IsSessionOperationPending());
	}
}

void UHeistTitleMenuWidget::CloseChildPanels()
{
	if (IsValid(SessionJoinWidget))
	{
		SessionJoinWidget->CloseSessionJoin();
	}
	if (IsValid(SettingsWidget))
	{
		SettingsWidget->CloseSettings();
	}
}

#pragma endregion
