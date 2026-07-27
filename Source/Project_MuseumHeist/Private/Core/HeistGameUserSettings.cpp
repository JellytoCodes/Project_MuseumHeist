#include "Core/HeistGameUserSettings.h"

#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "Core/HeistPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

#pragma region Construction

UHeistGameUserSettings::UHeistGameUserSettings(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

#pragma endregion

#pragma region Lifecycle

void UHeistGameUserSettings::ApplySettings(const bool bCheckForCommandLineOverrides)
{
	ValidateSettings();
	Super::ApplySettings(bCheckForCommandLineOverrides);
	ApplyMasterVolumeToActiveAudioDevices();
	ApplySettingsToLocalPlayers();
}

void UHeistGameUserSettings::LoadSettings(const bool bForceReload)
{
	Super::LoadSettings(bForceReload);
	ValidateSettings();
	ApplyMasterVolumeToActiveAudioDevices();
	ApplySettingsToLocalPlayers();
}

void UHeistGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	FieldOfView = DefaultFieldOfView;
	MouseSensitivity = DefaultMouseSensitivity;
	MasterVolume = DefaultMasterVolume;
}

void UHeistGameUserSettings::ValidateSettings()
{
	Super::ValidateSettings();

	FieldOfView = FMath::IsFinite(FieldOfView) ? FMath::Clamp(FieldOfView, MinimumFieldOfView, MaximumFieldOfView) : DefaultFieldOfView;
	MouseSensitivity =
		FMath::IsFinite(MouseSensitivity) ? FMath::Clamp(MouseSensitivity, MinimumMouseSensitivity, MaximumMouseSensitivity) : DefaultMouseSensitivity;
	MasterVolume = FMath::IsFinite(MasterVolume) ? FMath::Clamp(MasterVolume, MinimumMasterVolume, MaximumMasterVolume) : DefaultMasterVolume;
}

#pragma endregion

#pragma region Settings

UHeistGameUserSettings* UHeistGameUserSettings::GetHeistGameUserSettings()
{
	return Cast<UHeistGameUserSettings>(UGameUserSettings::GetGameUserSettings());
}

float UHeistGameUserSettings::GetFieldOfView() const
{
	return FieldOfView;
}

void UHeistGameUserSettings::SetFieldOfView(const float NewFieldOfView)
{
	FieldOfView = FMath::IsFinite(NewFieldOfView) ? FMath::Clamp(NewFieldOfView, MinimumFieldOfView, MaximumFieldOfView) : DefaultFieldOfView;
}

float UHeistGameUserSettings::GetMouseSensitivity() const
{
	return MouseSensitivity;
}

void UHeistGameUserSettings::SetMouseSensitivity(const float NewMouseSensitivity)
{
	MouseSensitivity =
		FMath::IsFinite(NewMouseSensitivity) ? FMath::Clamp(NewMouseSensitivity, MinimumMouseSensitivity, MaximumMouseSensitivity) : DefaultMouseSensitivity;
}

float UHeistGameUserSettings::GetMasterVolume() const
{
	return MasterVolume;
}

void UHeistGameUserSettings::SetMasterVolume(const float NewMasterVolume)
{
	MasterVolume = FMath::IsFinite(NewMasterVolume) ? FMath::Clamp(NewMasterVolume, MinimumMasterVolume, MaximumMasterVolume) : DefaultMasterVolume;
}

void UHeistGameUserSettings::ApplyHeistSettings(const bool bCheckForCommandLineOverrides)
{
	ApplySettings(bCheckForCommandLineOverrides);
	ConfirmVideoMode();
	SaveSettings();
}

void UHeistGameUserSettings::RestoreHeistDefaults()
{
	const FIntPoint PreviousResolution = GetScreenResolution();
	const EWindowMode::Type PreviousWindowMode = GetFullscreenMode();

	SetToDefaults();
	if (GetScreenResolution().X <= 0 || GetScreenResolution().Y <= 0)
	{
		SetScreenResolution(PreviousResolution.X > 0 && PreviousResolution.Y > 0 ? PreviousResolution : GetDesktopResolution());
		SetFullscreenMode(PreviousWindowMode);
	}
	ApplyHeistSettings(false);
}

void UHeistGameUserSettings::GetSupportedScreenResolutions(TArray<FIntPoint>& OutResolutions)
{
	OutResolutions.Reset();
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(OutResolutions);

	if (const UHeistGameUserSettings* Settings = GetHeistGameUserSettings())
	{
		OutResolutions.AddUnique(Settings->GetScreenResolution());
		OutResolutions.AddUnique(Settings->GetDesktopResolution());
	}

	OutResolutions.AddUnique(FIntPoint(1280, 720));
	OutResolutions.AddUnique(FIntPoint(1600, 900));
	OutResolutions.AddUnique(FIntPoint(1920, 1080));
	OutResolutions.RemoveAll([](const FIntPoint& Resolution) { return Resolution.X <= 0 || Resolution.Y <= 0; });
	OutResolutions.Sort([](const FIntPoint& Left, const FIntPoint& Right)
		{
			const int64 LeftPixels = static_cast<int64>(Left.X) * Left.Y;
			const int64 RightPixels = static_cast<int64>(Right.X) * Right.Y;
			return LeftPixels == RightPixels ? Left.X < Right.X : LeftPixels < RightPixels;
		});
}

void UHeistGameUserSettings::ApplyMasterVolumeToActiveAudioDevices()
{
	if (!IsValid(GEngine))
	{
		return;
	}

	if (!IsValid(MasterVolumeSoundMix))
	{
		MasterVolumeSoundMix = NewObject<USoundMix>(this, TEXT("HeistMasterVolumeSoundMix"), RF_Transient);
	}

	const UAudioSettings* AudioSettings = GetDefault<UAudioSettings>();
	USoundClass* MasterSoundClass = IsValid(AudioSettings) ? AudioSettings->GetDefaultSoundClass() : nullptr;
	FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
	if (!IsValid(MasterVolumeSoundMix) || !IsValid(MasterSoundClass) || AudioDeviceManager == nullptr)
	{
		return;
	}

	const float AppliedMasterVolume = GetMasterVolume();
	AudioDeviceManager->IterateOverAllDevices(
		[this, MasterSoundClass, AppliedMasterVolume](const Audio::FDeviceId AudioDeviceId, FAudioDevice* AudioDevice)
		{
			if (AudioDevice == nullptr)
			{
				return;
			}

			if (!InitializedAudioDeviceIds.Contains(AudioDeviceId))
			{
				AudioDevice->PushSoundMixModifier(MasterVolumeSoundMix);
				InitializedAudioDeviceIds.Add(AudioDeviceId);
			}
			AudioDevice->SetSoundMixClassOverride(MasterVolumeSoundMix, MasterSoundClass, AppliedMasterVolume, 1.0f, 0.05f, true);
		});
}

void UHeistGameUserSettings::ApplySettingsToLocalPlayers() const
{
	if (!IsValid(GEngine))
	{
		return;
	}

	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		UWorld* World = WorldContext.World();
		if (!IsValid(World) || (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE))
		{
			continue;
		}

		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			AHeistPlayerController* PlayerController = Cast<AHeistPlayerController>(Iterator->Get());
			if (IsValid(PlayerController) && PlayerController->IsLocalController())
			{
				PlayerController->ApplyLocalUserSettings();
			}
		}
	}
}

int32 UHeistGameUserSettings::GetInitializedAudioDeviceCount() const
{
	return InitializedAudioDeviceIds.Num();
}

#pragma endregion
