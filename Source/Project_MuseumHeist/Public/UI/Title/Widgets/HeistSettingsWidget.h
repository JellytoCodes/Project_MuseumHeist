#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistSettingsWidget.generated.h"

class UButton;
class UComboBoxString;
class USlider;
class UTextBlock;

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistSettingsWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

#pragma region Lifecycle

  protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

#pragma endregion

#pragma region ViewModel

  public:
	void SetupSettingsWidget(class UHeistSettingsViewModel* InSettingsViewModel);
	void OpenSettings();
	void CloseSettings();

  private:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|Settings", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistSettingsViewModel> SettingsViewModel;

#pragma endregion

#pragma region Presentation

  private:
	UFUNCTION()
	void HandleSettingsCloseClicked();

	UFUNCTION()
	void HandleApplySettingsClicked();

	UFUNCTION()
	void HandleRestoreDefaultSettingsClicked();

	UFUNCTION()
	void HandleFieldOfViewChanged(float NewValue);

	UFUNCTION()
	void HandleMouseSensitivityChanged(float NewValue);

	UFUNCTION()
	void HandleMasterVolumeChanged(float NewValue);

	void PopulateSettingsOptions();
	void RefreshSettingsControls();
	void RefreshSettingsValueTexts();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> SettingsCloseButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> ApplySettingsButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> RestoreDefaultSettingsButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<USlider> FOVSlider;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<USlider> MouseSensitivitySlider;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<USlider> MasterVolumeSlider;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UComboBoxString> ResolutionComboBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UComboBoxString> WindowModeComboBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> FOVValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> MouseSensitivityValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> MasterVolumeValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> SettingsStatusText;

	TArray<FIntPoint> SupportedSettingsResolutions;

#pragma endregion
};
