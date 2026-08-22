#include "UI/Title/ViewModels/HeistSettingsViewModel.h"

#include "Core/HeistGameUserSettings.h"

void UHeistSettingsViewModel::RefreshSettingsData()
{
	const UHeistGameUserSettings* Settings = UHeistGameUserSettings::GetHeistGameUserSettings();
	if (!IsValid(Settings))
	{
		UE_MVVM_SET_PROPERTY_VALUE(StatusText, NSLOCTEXT("HeistSettings", "SettingsUnavailable", "설정을 사용할 수 없습니다."));
		SnapshotChangedDelegate.Broadcast();
		return;
	}

	UpdateSnapshotFromSettings(*Settings);
	SnapshotChangedDelegate.Broadcast();
}

FHeistSettingsSnapshotChanged& UHeistSettingsViewModel::GetSnapshotChangedDelegate()
{
	return SnapshotChangedDelegate;
}

bool UHeistSettingsViewModel::RequestApplySettings(const float InFieldOfView, const float InMouseSensitivity, const float InMasterVolume,
	const int32 ResolutionWidth, const int32 ResolutionHeight, const int32 InWindowModeValue)
{
	UHeistGameUserSettings* Settings = UHeistGameUserSettings::GetHeistGameUserSettings();
	if (!IsValid(Settings))
	{
		UE_MVVM_SET_PROPERTY_VALUE(StatusText, NSLOCTEXT("HeistSettings", "SettingsApplyUnavailable", "설정을 사용할 수 없습니다."));
		SnapshotChangedDelegate.Broadcast();
		return false;
	}
	if (ResolutionWidth <= 0 || ResolutionHeight <= 0 || InWindowModeValue < static_cast<int32>(EWindowMode::Fullscreen)
		|| InWindowModeValue > static_cast<int32>(EWindowMode::Windowed))
	{
		UE_MVVM_SET_PROPERTY_VALUE(StatusText, NSLOCTEXT("HeistSettings", "SettingsInvalidDisplay", "올바른 해상도와 창 모드를 선택하세요."));
		SnapshotChangedDelegate.Broadcast();
		return false;
	}

	Settings->SetFieldOfView(InFieldOfView);
	Settings->SetMouseSensitivity(InMouseSensitivity);
	Settings->SetMasterVolume(InMasterVolume);
	Settings->SetScreenResolution(FIntPoint(ResolutionWidth, ResolutionHeight));
	Settings->SetFullscreenMode(static_cast<EWindowMode::Type>(InWindowModeValue));
	Settings->ApplyHeistSettings(false);

	UpdateSnapshotFromSettings(*Settings);
	UE_MVVM_SET_PROPERTY_VALUE(StatusText, NSLOCTEXT("HeistSettings", "SettingsApplied", "설정을 저장하고 적용했습니다."));
	SnapshotChangedDelegate.Broadcast();
	return true;
}

bool UHeistSettingsViewModel::RequestRestoreDefaultSettings()
{
	UHeistGameUserSettings* Settings = UHeistGameUserSettings::GetHeistGameUserSettings();
	if (!IsValid(Settings))
	{
		UE_MVVM_SET_PROPERTY_VALUE(StatusText, NSLOCTEXT("HeistSettings", "SettingsDefaultsUnavailable", "설정을 사용할 수 없습니다."));
		SnapshotChangedDelegate.Broadcast();
		return false;
	}

	Settings->RestoreHeistDefaults();
	UpdateSnapshotFromSettings(*Settings);
	UE_MVVM_SET_PROPERTY_VALUE(StatusText, NSLOCTEXT("HeistSettings", "SettingsDefaultsApplied", "기본 설정으로 복원했습니다."));
	SnapshotChangedDelegate.Broadcast();
	return true;
}

void UHeistSettingsViewModel::GetSupportedSettingsResolutions(TArray<FIntPoint>& OutResolutions) const
{
	UHeistGameUserSettings::GetSupportedScreenResolutions(OutResolutions);
}

void UHeistSettingsViewModel::UpdateSnapshotFromSettings(const UHeistGameUserSettings& Settings)
{
	UE_MVVM_SET_PROPERTY_VALUE(FieldOfView, Settings.GetFieldOfView());
	UE_MVVM_SET_PROPERTY_VALUE(MouseSensitivity, Settings.GetMouseSensitivity());
	UE_MVVM_SET_PROPERTY_VALUE(MasterVolume, Settings.GetMasterVolume());
	UE_MVVM_SET_PROPERTY_VALUE(ScreenResolution, Settings.GetScreenResolution());
	UE_MVVM_SET_PROPERTY_VALUE(WindowModeValue, static_cast<int32>(Settings.GetFullscreenMode()));
}

float UHeistSettingsViewModel::GetFieldOfView() const
{
	return FieldOfView;
}

float UHeistSettingsViewModel::GetMouseSensitivity() const
{
	return MouseSensitivity;
}

float UHeistSettingsViewModel::GetMasterVolume() const
{
	return MasterVolume;
}

FIntPoint UHeistSettingsViewModel::GetScreenResolution() const
{
	return ScreenResolution;
}

int32 UHeistSettingsViewModel::GetWindowModeValue() const
{
	return WindowModeValue;
}

const FText& UHeistSettingsViewModel::GetStatusText() const
{
	return StatusText;
}
