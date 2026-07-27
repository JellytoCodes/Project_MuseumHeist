#include "UI/Widgets/HeistTitleMenuWidget.h"

#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Core/HeistGameUserSettings.h"
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
	if (IsValid(CancelSessionButton))
	{
		CancelSessionButton->OnClicked.AddUniqueDynamic(this, &UHeistTitleMenuWidget::HandleCancelSessionClicked);
	}
	if (IsValid(RetrySessionButton))
	{
		RetrySessionButton->OnClicked.AddUniqueDynamic(this, &UHeistTitleMenuWidget::HandleRetrySessionClicked);
	}
	if (IsValid(SettingsButton))
	{
		SettingsButton->OnClicked.AddUniqueDynamic(this, &UHeistTitleMenuWidget::HandleSettingsClicked);
	}
	if (IsValid(SettingsCloseButton))
	{
		SettingsCloseButton->OnClicked.AddUniqueDynamic(this, &UHeistTitleMenuWidget::HandleSettingsCloseClicked);
	}
	if (IsValid(ApplySettingsButton))
	{
		ApplySettingsButton->OnClicked.AddUniqueDynamic(this, &UHeistTitleMenuWidget::HandleApplySettingsClicked);
	}
	if (IsValid(RestoreDefaultSettingsButton))
	{
		RestoreDefaultSettingsButton->OnClicked.AddUniqueDynamic(this, &UHeistTitleMenuWidget::HandleRestoreDefaultSettingsClicked);
	}
	if (IsValid(FOVSlider))
	{
		FOVSlider->OnValueChanged.AddUniqueDynamic(this, &UHeistTitleMenuWidget::HandleFieldOfViewChanged);
	}
	if (IsValid(MouseSensitivitySlider))
	{
		MouseSensitivitySlider->OnValueChanged.AddUniqueDynamic(this, &UHeistTitleMenuWidget::HandleMouseSensitivityChanged);
	}
	if (IsValid(MasterVolumeSlider))
	{
		MasterVolumeSlider->OnValueChanged.AddUniqueDynamic(this, &UHeistTitleMenuWidget::HandleMasterVolumeChanged);
	}

	PopulateSettingsOptions();
	if (IsValid(SettingsPanel))
	{
		SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
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
	if (IsValid(CancelSessionButton))
	{
		CancelSessionButton->OnClicked.RemoveDynamic(this, &UHeistTitleMenuWidget::HandleCancelSessionClicked);
	}
	if (IsValid(RetrySessionButton))
	{
		RetrySessionButton->OnClicked.RemoveDynamic(this, &UHeistTitleMenuWidget::HandleRetrySessionClicked);
	}
	if (IsValid(SettingsButton))
	{
		SettingsButton->OnClicked.RemoveDynamic(this, &UHeistTitleMenuWidget::HandleSettingsClicked);
	}
	if (IsValid(SettingsCloseButton))
	{
		SettingsCloseButton->OnClicked.RemoveDynamic(this, &UHeistTitleMenuWidget::HandleSettingsCloseClicked);
	}
	if (IsValid(ApplySettingsButton))
	{
		ApplySettingsButton->OnClicked.RemoveDynamic(this, &UHeistTitleMenuWidget::HandleApplySettingsClicked);
	}
	if (IsValid(RestoreDefaultSettingsButton))
	{
		RestoreDefaultSettingsButton->OnClicked.RemoveDynamic(this, &UHeistTitleMenuWidget::HandleRestoreDefaultSettingsClicked);
	}
	if (IsValid(FOVSlider))
	{
		FOVSlider->OnValueChanged.RemoveDynamic(this, &UHeistTitleMenuWidget::HandleFieldOfViewChanged);
	}
	if (IsValid(MouseSensitivitySlider))
	{
		MouseSensitivitySlider->OnValueChanged.RemoveDynamic(this, &UHeistTitleMenuWidget::HandleMouseSensitivityChanged);
	}
	if (IsValid(MasterVolumeSlider))
	{
		MasterVolumeSlider->OnValueChanged.RemoveDynamic(this, &UHeistTitleMenuWidget::HandleMasterVolumeChanged);
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

void UHeistTitleMenuWidget::HandleCancelSessionClicked()
{
	if (IsValid(TitleMenuViewModel))
	{
		TitleMenuViewModel->RequestCancelSessionOperation();
	}
}

void UHeistTitleMenuWidget::HandleRetrySessionClicked()
{
	if (IsValid(TitleMenuViewModel))
	{
		TitleMenuViewModel->RequestRetrySessionOperation();
	}
}

void UHeistTitleMenuWidget::HandleSettingsClicked()
{
	PopulateSettingsOptions();
	RefreshSettingsControls();
	if (IsValid(SettingsPanel))
	{
		SettingsPanel->SetVisibility(ESlateVisibility::Visible);
	}
}

void UHeistTitleMenuWidget::HandleSettingsCloseClicked()
{
	if (IsValid(SettingsPanel))
	{
		SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UHeistTitleMenuWidget::HandleApplySettingsClicked()
{
	if (!IsValid(TitleMenuViewModel))
	{
		return;
	}

	const int32 ResolutionIndex = IsValid(ResolutionComboBox) ? ResolutionComboBox->GetSelectedIndex() : INDEX_NONE;
	const FIntPoint Resolution = SupportedSettingsResolutions.IsValidIndex(ResolutionIndex) ? SupportedSettingsResolutions[ResolutionIndex]
																							: TitleMenuViewModel->GetSettingsScreenResolution();
	const int32 WindowModeValue = IsValid(WindowModeComboBox) ? WindowModeComboBox->GetSelectedIndex() : TitleMenuViewModel->GetSettingsWindowModeValue();
	const float FieldOfView = IsValid(FOVSlider) ? FOVSlider->GetValue() : TitleMenuViewModel->GetSettingsFieldOfView();
	const float MouseSensitivity = IsValid(MouseSensitivitySlider) ? MouseSensitivitySlider->GetValue() : TitleMenuViewModel->GetSettingsMouseSensitivity();
	const float MasterVolume = IsValid(MasterVolumeSlider) ? MasterVolumeSlider->GetValue() : TitleMenuViewModel->GetSettingsMasterVolume();

	TitleMenuViewModel->RequestApplySettings(FieldOfView, MouseSensitivity, MasterVolume, Resolution.X, Resolution.Y, WindowModeValue);
}

void UHeistTitleMenuWidget::HandleRestoreDefaultSettingsClicked()
{
	if (IsValid(TitleMenuViewModel))
	{
		TitleMenuViewModel->RequestRestoreDefaultSettings();
	}
}

void UHeistTitleMenuWidget::HandleFieldOfViewChanged(const float)
{
	RefreshSettingsValueTexts();
}

void UHeistTitleMenuWidget::HandleMouseSensitivityChanged(const float)
{
	RefreshSettingsValueTexts();
}

void UHeistTitleMenuWidget::HandleMasterVolumeChanged(const float)
{
	RefreshSettingsValueTexts();
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
	if (IsValid(CancelSessionButton))
	{
		const bool bCanCancelSessionOperation = TitleMenuViewModel->CanCancelSessionOperation();
		CancelSessionButton->SetIsEnabled(bCanCancelSessionOperation);
		CancelSessionButton->SetVisibility(bCanCancelSessionOperation ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (IsValid(RetrySessionButton))
	{
		const bool bCanRetrySessionOperation = TitleMenuViewModel->CanRetrySessionOperation();
		RetrySessionButton->SetIsEnabled(bCanRetrySessionOperation);
		RetrySessionButton->SetVisibility(bCanRetrySessionOperation ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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
	if (IsValid(SessionActionHintText))
	{
		SessionActionHintText->SetText(TitleMenuViewModel->GetSessionActionHintText());
		SessionActionHintText->SetVisibility(TitleMenuViewModel->GetSessionActionHintVisibility());
	}
	if (IsValid(SettingsStatusText))
	{
		SettingsStatusText->SetText(TitleMenuViewModel->GetSettingsStatusText());
		SettingsStatusText->SetVisibility(TitleMenuViewModel->GetSettingsStatusText().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	RefreshSettingsControls();
}

void UHeistTitleMenuWidget::PopulateSettingsOptions()
{
	if (!IsValid(TitleMenuViewModel))
	{
		UHeistGameUserSettings::GetSupportedScreenResolutions(SupportedSettingsResolutions);
	}
	else
	{
		TitleMenuViewModel->GetSupportedSettingsResolutions(SupportedSettingsResolutions);
	}

	if (IsValid(ResolutionComboBox))
	{
		ResolutionComboBox->ClearOptions();
		for (const FIntPoint& Resolution : SupportedSettingsResolutions)
		{
			ResolutionComboBox->AddOption(FString::Printf(TEXT("%d x %d"), Resolution.X, Resolution.Y));
		}
	}

	if (IsValid(WindowModeComboBox))
	{
		WindowModeComboBox->ClearOptions();
		WindowModeComboBox->AddOption(TEXT("FULLSCREEN"));
		WindowModeComboBox->AddOption(TEXT("BORDERLESS"));
		WindowModeComboBox->AddOption(TEXT("WINDOWED"));
	}
}

void UHeistTitleMenuWidget::RefreshSettingsControls()
{
	if (!IsValid(TitleMenuViewModel))
	{
		return;
	}

	if (IsValid(FOVSlider))
	{
		FOVSlider->SetMinValue(UHeistGameUserSettings::MinimumFieldOfView);
		FOVSlider->SetMaxValue(UHeistGameUserSettings::MaximumFieldOfView);
		FOVSlider->SetValue(TitleMenuViewModel->GetSettingsFieldOfView());
	}
	if (IsValid(MouseSensitivitySlider))
	{
		MouseSensitivitySlider->SetMinValue(UHeistGameUserSettings::MinimumMouseSensitivity);
		MouseSensitivitySlider->SetMaxValue(UHeistGameUserSettings::MaximumMouseSensitivity);
		MouseSensitivitySlider->SetValue(TitleMenuViewModel->GetSettingsMouseSensitivity());
	}
	if (IsValid(MasterVolumeSlider))
	{
		MasterVolumeSlider->SetMinValue(UHeistGameUserSettings::MinimumMasterVolume);
		MasterVolumeSlider->SetMaxValue(UHeistGameUserSettings::MaximumMasterVolume);
		MasterVolumeSlider->SetValue(TitleMenuViewModel->GetSettingsMasterVolume());
	}
	if (IsValid(ResolutionComboBox))
	{
		const int32 ResolutionIndex = SupportedSettingsResolutions.IndexOfByKey(TitleMenuViewModel->GetSettingsScreenResolution());
		ResolutionComboBox->SetSelectedIndex(ResolutionIndex != INDEX_NONE ? ResolutionIndex : 0);
	}
	if (IsValid(WindowModeComboBox))
	{
		WindowModeComboBox->SetSelectedIndex(FMath::Clamp(TitleMenuViewModel->GetSettingsWindowModeValue(), 0, 2));
	}
	RefreshSettingsValueTexts();
}

void UHeistTitleMenuWidget::RefreshSettingsValueTexts()
{
	if (IsValid(FOVValueText) && IsValid(FOVSlider))
	{
		FOVValueText->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), FOVSlider->GetValue())));
	}
	if (IsValid(MouseSensitivityValueText) && IsValid(MouseSensitivitySlider))
	{
		MouseSensitivityValueText->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), MouseSensitivitySlider->GetValue())));
	}
	if (IsValid(MasterVolumeValueText) && IsValid(MasterVolumeSlider))
	{
		MasterVolumeValueText->SetText(FText::FromString(FString::Printf(TEXT("%.0f%%"), MasterVolumeSlider->GetValue() * 100.0f)));
	}
}

#pragma endregion
