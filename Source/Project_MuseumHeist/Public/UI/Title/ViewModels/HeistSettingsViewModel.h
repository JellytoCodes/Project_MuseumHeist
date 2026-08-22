#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"

#include "HeistSettingsViewModel.generated.h"

DECLARE_MULTICAST_DELEGATE(FHeistSettingsSnapshotChanged);

UCLASS(BlueprintType)
class PROJECT_MUSEUMHEIST_API UHeistSettingsViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

#pragma region Setup

  public:
	void RefreshSettingsData();
	FHeistSettingsSnapshotChanged& GetSnapshotChangedDelegate();

	UFUNCTION(BlueprintCallable, Category = "Heist|Settings")
	bool RequestApplySettings(float FieldOfView, float MouseSensitivity, float MasterVolume, int32 ResolutionWidth, int32 ResolutionHeight, int32 WindowModeValue);

	UFUNCTION(BlueprintCallable, Category = "Heist|Settings")
	bool RequestRestoreDefaultSettings();

	UFUNCTION(BlueprintCallable, Category = "Heist|Settings")
	void GetSupportedSettingsResolutions(TArray<FIntPoint>& OutResolutions) const;

  private:
	void UpdateSnapshotFromSettings(const class UHeistGameUserSettings& Settings);

	FHeistSettingsSnapshotChanged SnapshotChangedDelegate;

#pragma endregion

#pragma region SettingsData

  public:
	float GetFieldOfView() const;
	float GetMouseSensitivity() const;
	float GetMasterVolume() const;
	FIntPoint GetScreenResolution() const;
	int32 GetWindowModeValue() const;
	const FText& GetStatusText() const;

  private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Settings", meta = (AllowPrivateAccess = "true"))
	float FieldOfView = 90.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Settings", meta = (AllowPrivateAccess = "true"))
	float MouseSensitivity = 1.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Settings", meta = (AllowPrivateAccess = "true"))
	float MasterVolume = 1.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Settings", meta = (AllowPrivateAccess = "true"))
	FIntPoint ScreenResolution = FIntPoint(1920, 1080);

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Settings", meta = (AllowPrivateAccess = "true"))
	int32 WindowModeValue = 1;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Settings", meta = (AllowPrivateAccess = "true"))
	FText StatusText;

#pragma endregion
};
