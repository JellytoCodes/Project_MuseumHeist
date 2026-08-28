#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"

#include "HeistGameUserSettings.generated.h"

class USoundMix;

UCLASS(Config = GameUserSettings)
class PROJECT_MUSEUMHEIST_API UHeistGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

#pragma region Lifecycle

  public:
	virtual void ApplySettings(bool bCheckForCommandLineOverrides) override;
	virtual void LoadSettings(bool bForceReload = false) override;
	virtual void SetToDefaults() override;
	virtual void ValidateSettings() override;

#pragma endregion

#pragma region Settings

  public:
	static constexpr float MinimumFieldOfView = 70.0f;
	static constexpr float MaximumFieldOfView = 110.0f;
	static constexpr float DefaultFieldOfView = 90.0f;
	static constexpr float MinimumMouseSensitivity = 0.10f;
	static constexpr float MaximumMouseSensitivity = 3.00f;
	static constexpr float DefaultMouseSensitivity = 1.00f;
	static constexpr float MinimumMasterVolume = 0.00f;
	static constexpr float MaximumMasterVolume = 1.00f;
	static constexpr float DefaultMasterVolume = 1.00f;

	UFUNCTION(BlueprintPure, Category = "Heist|Settings")
	static UHeistGameUserSettings* GetHeistGameUserSettings();

	UFUNCTION(BlueprintPure, Category = "Heist|Settings")
	float GetFieldOfView() const;

	UFUNCTION(BlueprintCallable, Category = "Heist|Settings")
	void SetFieldOfView(float NewFieldOfView);

	UFUNCTION(BlueprintPure, Category = "Heist|Settings")
	float GetMouseSensitivity() const;

	UFUNCTION(BlueprintCallable, Category = "Heist|Settings")
	void SetMouseSensitivity(float NewMouseSensitivity);

	UFUNCTION(BlueprintPure, Category = "Heist|Settings")
	float GetMasterVolume() const;

	UFUNCTION(BlueprintCallable, Category = "Heist|Settings")
	void SetMasterVolume(float NewMasterVolume);

	bool HasCompletedTutorial() const;
	void SetTutorialCompleted(bool bCompleted);

	UFUNCTION(BlueprintCallable, Category = "Heist|Settings")
	void ApplyHeistSettings(bool bCheckForCommandLineOverrides = false);

	UFUNCTION(BlueprintCallable, Category = "Heist|Settings")
	void RestoreHeistDefaults();

	UFUNCTION(BlueprintCallable, Category = "Heist|Settings")
	static void GetSupportedScreenResolutions(TArray<FIntPoint>& OutResolutions);

	void ApplyMasterVolumeToActiveAudioDevices();
	void ApplySettingsToLocalPlayers() const;
	int32 GetInitializedAudioDeviceCount() const;

  private:
	UPROPERTY(Config)
	float FieldOfView = DefaultFieldOfView;

	UPROPERTY(Config)
	float MouseSensitivity = DefaultMouseSensitivity;

	UPROPERTY(Config)
	float MasterVolume = DefaultMasterVolume;

	UPROPERTY(Config)
	bool bTutorialCompleted = false;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> MasterVolumeSoundMix;

	TSet<uint32> InitializedAudioDeviceIds;

#pragma endregion
};
