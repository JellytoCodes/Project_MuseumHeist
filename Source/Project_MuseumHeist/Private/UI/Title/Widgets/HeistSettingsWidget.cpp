#include "UI/Title/Widgets/HeistSettingsWidget.h"

#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Core/HeistGameUserSettings.h"
#include "UI/Title/ViewModels/HeistSettingsViewModel.h"
#include "View/MVVMView.h"

#pragma region Lifecycle

void UHeistSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(SettingsCloseButton))
	{
		SettingsCloseButton->OnClicked.AddUniqueDynamic(this, &UHeistSettingsWidget::HandleSettingsCloseClicked);
	}
	if (IsValid(ApplySettingsButton))
	{
		ApplySettingsButton->OnClicked.AddUniqueDynamic(this, &UHeistSettingsWidget::HandleApplySettingsClicked);
	}
	if (IsValid(RestoreDefaultSettingsButton))
	{
		RestoreDefaultSettingsButton->OnClicked.AddUniqueDynamic(this, &UHeistSettingsWidget::HandleRestoreDefaultSettingsClicked);
	}
	if (IsValid(FOVSlider))
	{
		FOVSlider->OnValueChanged.AddUniqueDynamic(this, &UHeistSettingsWidget::HandleFieldOfViewChanged);
	}
	if (IsValid(MouseSensitivitySlider))
	{
		MouseSensitivitySlider->OnValueChanged.AddUniqueDynamic(this, &UHeistSettingsWidget::HandleMouseSensitivityChanged);
	}
	if (IsValid(MasterVolumeSlider))
	{
		MasterVolumeSlider->OnValueChanged.AddUniqueDynamic(this, &UHeistSettingsWidget::HandleMasterVolumeChanged);
	}

	SetVisibility(ESlateVisibility::Collapsed);
}

void UHeistSettingsWidget::NativeDestruct()
{
	if (IsValid(SettingsViewModel))
	{
		SettingsViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	}
	if (IsValid(SettingsCloseButton))
	{
		SettingsCloseButton->OnClicked.RemoveDynamic(this, &UHeistSettingsWidget::HandleSettingsCloseClicked);
	}
	if (IsValid(ApplySettingsButton))
	{
		ApplySettingsButton->OnClicked.RemoveDynamic(this, &UHeistSettingsWidget::HandleApplySettingsClicked);
	}
	if (IsValid(RestoreDefaultSettingsButton))
	{
		RestoreDefaultSettingsButton->OnClicked.RemoveDynamic(this, &UHeistSettingsWidget::HandleRestoreDefaultSettingsClicked);
	}
	if (IsValid(FOVSlider))
	{
		FOVSlider->OnValueChanged.RemoveDynamic(this, &UHeistSettingsWidget::HandleFieldOfViewChanged);
	}
	if (IsValid(MouseSensitivitySlider))
	{
		MouseSensitivitySlider->OnValueChanged.RemoveDynamic(this, &UHeistSettingsWidget::HandleMouseSensitivityChanged);
	}
	if (IsValid(MasterVolumeSlider))
	{
		MasterVolumeSlider->OnValueChanged.RemoveDynamic(this, &UHeistSettingsWidget::HandleMasterVolumeChanged);
	}

	Super::NativeDestruct();
}

#pragma endregion

#pragma region ViewModel

void UHeistSettingsWidget::SetupSettingsWidget(UHeistSettingsViewModel* InSettingsViewModel)
{
	checkf(IsValid(InSettingsViewModel), TEXT("HeistSettingsWidget requires a valid HeistSettingsViewModel"));

	if (IsValid(SettingsViewModel))
	{
		SettingsViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	}

	SettingsViewModel = InSettingsViewModel;
	SettingsViewModel->GetSnapshotChangedDelegate().AddUObject(this, &UHeistSettingsWidget::RefreshSettingsControls);

	TScriptInterface<INotifyFieldValueChanged> ViewModelInterface;
	ViewModelInterface.SetObject(SettingsViewModel);
	ViewModelInterface.SetInterface(SettingsViewModel);
	if (UMVVMView* MVVMView = GetExtension<UMVVMView>())
	{
		MVVMView->SetViewModelByClass(ViewModelInterface);
	}

	PopulateSettingsOptions();
	SettingsViewModel->RefreshSettingsData();
}

void UHeistSettingsWidget::OpenSettings()
{
	PopulateSettingsOptions();
	if (IsValid(SettingsViewModel))
	{
		SettingsViewModel->RefreshSettingsData();
	}
	SetVisibility(ESlateVisibility::Visible);
	RefreshSettingsControls();
}

void UHeistSettingsWidget::CloseSettings()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

#pragma endregion

#pragma region Presentation

void UHeistSettingsWidget::HandleSettingsCloseClicked()
{
	CloseSettings();
}

void UHeistSettingsWidget::HandleApplySettingsClicked()
{
	if (!IsValid(SettingsViewModel))
	{
		return;
	}

	const int32 ResolutionIndex = IsValid(ResolutionComboBox) ? ResolutionComboBox->GetSelectedIndex() : INDEX_NONE;
	const FIntPoint Resolution = SupportedSettingsResolutions.IsValidIndex(ResolutionIndex) ? SupportedSettingsResolutions[ResolutionIndex]
		: SettingsViewModel->GetScreenResolution();
	const int32 WindowModeValue = IsValid(WindowModeComboBox) ? WindowModeComboBox->GetSelectedIndex() : SettingsViewModel->GetWindowModeValue();
	const float FieldOfView = IsValid(FOVSlider) ? FOVSlider->GetValue() : SettingsViewModel->GetFieldOfView();
	const float MouseSensitivity = IsValid(MouseSensitivitySlider) ? MouseSensitivitySlider->GetValue() : SettingsViewModel->GetMouseSensitivity();
	const float MasterVolume = IsValid(MasterVolumeSlider) ? MasterVolumeSlider->GetValue() : SettingsViewModel->GetMasterVolume();

	SettingsViewModel->RequestApplySettings(FieldOfView, MouseSensitivity, MasterVolume, Resolution.X, Resolution.Y, WindowModeValue);
}

void UHeistSettingsWidget::HandleRestoreDefaultSettingsClicked()
{
	if (IsValid(SettingsViewModel))
	{
		SettingsViewModel->RequestRestoreDefaultSettings();
	}
}

void UHeistSettingsWidget::HandleFieldOfViewChanged(const float)
{
	RefreshSettingsValueTexts();
}

void UHeistSettingsWidget::HandleMouseSensitivityChanged(const float)
{
	RefreshSettingsValueTexts();
}

void UHeistSettingsWidget::HandleMasterVolumeChanged(const float)
{
	RefreshSettingsValueTexts();
}

void UHeistSettingsWidget::PopulateSettingsOptions()
{
	if (IsValid(SettingsViewModel))
	{
		SettingsViewModel->GetSupportedSettingsResolutions(SupportedSettingsResolutions);
	}
	else
	{
		UHeistGameUserSettings::GetSupportedScreenResolutions(SupportedSettingsResolutions);
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
		WindowModeComboBox->AddOption(TEXT("전체 화면"));
		WindowModeComboBox->AddOption(TEXT("테두리 없는 창"));
		WindowModeComboBox->AddOption(TEXT("창 모드"));
	}
}

void UHeistSettingsWidget::RefreshSettingsControls()
{
	if (!IsValid(SettingsViewModel))
	{
		return;
	}

	if (IsValid(FOVSlider))
	{
		FOVSlider->SetMinValue(UHeistGameUserSettings::MinimumFieldOfView);
		FOVSlider->SetMaxValue(UHeistGameUserSettings::MaximumFieldOfView);
		FOVSlider->SetValue(SettingsViewModel->GetFieldOfView());
	}
	if (IsValid(MouseSensitivitySlider))
	{
		MouseSensitivitySlider->SetMinValue(UHeistGameUserSettings::MinimumMouseSensitivity);
		MouseSensitivitySlider->SetMaxValue(UHeistGameUserSettings::MaximumMouseSensitivity);
		MouseSensitivitySlider->SetValue(SettingsViewModel->GetMouseSensitivity());
	}
	if (IsValid(MasterVolumeSlider))
	{
		MasterVolumeSlider->SetMinValue(UHeistGameUserSettings::MinimumMasterVolume);
		MasterVolumeSlider->SetMaxValue(UHeistGameUserSettings::MaximumMasterVolume);
		MasterVolumeSlider->SetValue(SettingsViewModel->GetMasterVolume());
	}
	if (IsValid(ResolutionComboBox))
	{
		const int32 ResolutionIndex = SupportedSettingsResolutions.IndexOfByKey(SettingsViewModel->GetScreenResolution());
		ResolutionComboBox->SetSelectedIndex(ResolutionIndex != INDEX_NONE ? ResolutionIndex : 0);
	}
	if (IsValid(WindowModeComboBox))
	{
		WindowModeComboBox->SetSelectedIndex(FMath::Clamp(SettingsViewModel->GetWindowModeValue(), 0, 2));
	}
	if (IsValid(SettingsStatusText))
	{
		SettingsStatusText->SetText(SettingsViewModel->GetStatusText());
		SettingsStatusText->SetVisibility(SettingsViewModel->GetStatusText().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	RefreshSettingsValueTexts();
}

void UHeistSettingsWidget::RefreshSettingsValueTexts()
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
