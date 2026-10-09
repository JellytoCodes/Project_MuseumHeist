#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Core/HeistGameUserSettings.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Scalability.h"
#include "UI/Title/Widgets/HeistSettingsWidget.h"
#include "Widgets/SWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistSettingsCloseActionsTest, "ProjectMuseumHeist.Settings.CloseActions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistSettingsCloseActionsTest::RunTest(const FString& Parameters)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Editor world exists"), World)) return false;
	UClass* SettingsClass = LoadClass<UHeistSettingsWidget>(nullptr, TEXT("/Game/Blueprints/UI/Title/WBP_Settings.WBP_Settings_C"));
	if (!TestNotNull(TEXT("Authored Settings Blueprint loads"), SettingsClass)) return false;
	UHeistSettingsWidget* Settings = CreateWidget<UHeistSettingsWidget>(World, SettingsClass);
	if (!TestNotNull(TEXT("Authored Settings widget is created"), Settings)) return false;

	// Exercise the real Slate wrapper and authored button binding without applying saved settings.
	const TSharedRef<SWidget> SlateWidget = Settings->TakeWidget();
	TArray<UWidget*> AuthoredWidgets;
	Settings->WidgetTree->GetAllWidgets(AuthoredWidgets);
	int32 SelectorCount = 0;
	for (UWidget* Widget : AuthoredWidgets)
	{
		if (Cast<UComboBoxString>(Widget) == nullptr) continue;
		++SelectorCount;
		TestTrue(TEXT("Settings only exposes resolution and window mode selectors"),
			Widget->GetFName() == FName(TEXT("ResolutionComboBox")) || Widget->GetFName() == FName(TEXT("WindowModeComboBox")));
	}
	TestEqual(TEXT("Authored Settings has no graphics quality selector"), SelectorCount, 2);
	UButton* CloseButton = Cast<UButton>(Settings->GetWidgetFromName(TEXT("SettingsCloseButton")));
	if (!TestNotNull(TEXT("Authored footer close button retains its native binding name"), CloseButton)) return false;
	TestEqual(TEXT("Settings starts collapsed"), Settings->GetVisibility(), ESlateVisibility::Collapsed);

	const FKeyEvent EscapeKey(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0);
	const FKeyEvent OtherKey(EKeys::F, FModifierKeysState(), 0, false, 0, 0);
	Settings->OpenSettings();
	TestTrue(TEXT("Open Settings accepts UI focus"), Settings->IsFocusable());
	SlateWidget->OnPreviewKeyDown(FGeometry(), OtherKey);
	TestTrue(TEXT("An unrelated key leaves Settings open"), Settings->IsVisible());
	TestTrue(TEXT("Escape is consumed by the Settings Slate preview handler"), SlateWidget->OnPreviewKeyDown(FGeometry(), EscapeKey).IsEventHandled());
	TestEqual(TEXT("Escape closes Settings"), Settings->GetVisibility(), ESlateVisibility::Collapsed);
	TestFalse(TEXT("Collapsed Settings does not consume Escape"), SlateWidget->OnPreviewKeyDown(FGeometry(), EscapeKey).IsEventHandled());

	for (int32 Reopen = 0; Reopen < 2; ++Reopen)
	{
		Settings->OpenSettings();
		TestTrue(TEXT("Settings can reopen after being closed"), Settings->IsVisible());
		CloseButton->OnClicked.Broadcast();
		TestEqual(TEXT("The authored close button uses the same close action"), Settings->GetVisibility(), ESlateVisibility::Collapsed);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistFixedMediumQualityTest, "ProjectMuseumHeist.Settings.FixedMediumQuality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistFixedMediumQualityTest::RunTest(const FString& Parameters)
{
	UHeistGameUserSettings* Settings = UHeistGameUserSettings::GetHeistGameUserSettings();
	if (!TestNotNull(TEXT("The engine uses Heist GameUserSettings"), Settings)) return false;

	const FString AutomationDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"));
	IFileManager::Get().MakeDirectory(*AutomationDirectory, true);
	const FString OriginalIni = FPaths::CreateTempFilename(*AutomationDirectory, TEXT("HeistSettingsOriginal"), TEXT(".ini"));
	const FString TestIni = FPaths::CreateTempFilename(*AutomationDirectory, TEXT("HeistSettingsMedium"), TEXT(".ini"));
	const FString OriginalGameIni = GGameUserSettingsIni;
	const FString OriginalEditorIni = GEditorSettingsIni;
	const Scalability::FQualityLevels OriginalAppliedQuality = Scalability::GetQualityLevels();
	const Scalability::FQualityLevels OriginalStoredQuality = Settings->ScalabilityQuality;
	const FIntPoint OriginalResolution = Settings->GetScreenResolution();
	const EWindowMode::Type OriginalWindowMode = Settings->GetFullscreenMode();
	Settings->SaveConfig(CPF_Config, *OriginalIni);
	Settings->SaveConfig(CPF_Config, *TestIni);
	ON_SCOPE_EXIT
	{
		GGameUserSettingsIni = OriginalGameIni;
		GEditorSettingsIni = OriginalEditorIni;
		Settings->LoadConfig(Settings->GetClass(), *OriginalIni);
		Settings->ApplyNonResolutionSettings();
		Settings->ApplyMasterVolumeToActiveAudioDevices();
		Settings->ApplySettingsToLocalPlayers();
		Settings->ScalabilityQuality = OriginalStoredQuality;
		Scalability::SetQualityLevels(OriginalAppliedQuality);
		GConfig->UnloadFile(OriginalIni);
		GConfig->UnloadFile(TestIni);
		IFileManager::Get().Delete(*OriginalIni);
		IFileManager::Get().Delete(*TestIni);
	};

	// Keep all writes in temporary files, including UE's editor scalability save path.
	GGameUserSettingsIni = TestIni;
	GEditorSettingsIni = TestIni;
	const FString SettingsSection = Settings->GetClass()->GetPathName();
	GConfig->SetInt(*SettingsSection, TEXT("Version"), 5, TestIni);
	GConfig->SetFloat(*SettingsSection, TEXT("FieldOfView"), 104.0f, TestIni);
	GConfig->SetFloat(*SettingsSection, TEXT("MouseSensitivity"), 1.75f, TestIni);
	GConfig->SetFloat(*SettingsSection, TEXT("MasterVolume"), 0.5f, TestIni);
	Scalability::FQualityLevels LegacyQuality;
	LegacyQuality.SetFromSingleQualityLevel(3);
	LegacyQuality.ResolutionQuality = 61.0f;
	Scalability::SetQualityLevels(LegacyQuality);
	Scalability::SaveState(TestIni);
	GConfig->Flush(false, TestIni);

	const auto CheckMedium = [this](const TCHAR* Stage, const Scalability::FQualityLevels& Quality)
	{
		TestEqual(FString::Printf(TEXT("%s: render resolution stays at 100%%"), Stage), Quality.ResolutionQuality, 100.0f);
		const int32 Groups[] = {Quality.ViewDistanceQuality, Quality.AntiAliasingQuality, Quality.ShadowQuality,
			Quality.GlobalIlluminationQuality, Quality.ReflectionQuality, Quality.PostProcessQuality,
			Quality.TextureQuality, Quality.EffectsQuality, Quality.FoliageQuality, Quality.ShadingQuality, Quality.LandscapeQuality};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Groups); ++Index)
		{
			TestEqual(FString::Printf(TEXT("%s: quality group %d is Medium"), Stage, Index), Groups[Index], 1);
		}
	};
	Settings->LoadSettings(false);
	CheckMedium(TEXT("Legacy saved settings load"), Settings->ScalabilityQuality);
	TestEqual(TEXT("Quality migration preserves saved FOV"), Settings->GetFieldOfView(), 104.0f);
	TestEqual(TEXT("Quality migration preserves saved sensitivity"), Settings->GetMouseSensitivity(), 1.75f);
	TestEqual(TEXT("Quality migration preserves saved master volume"), Settings->GetMasterVolume(), 0.5f);
	TestEqual(TEXT("Quality migration preserves output resolution"), Settings->GetScreenResolution(), OriginalResolution);
	TestEqual(TEXT("Quality migration preserves window mode"), Settings->GetFullscreenMode(), OriginalWindowMode);

	{
		// UGameEngine's initial settings apply and GameInstance::Init precede this flag.
		// Exercise that real engine branch synchronously and restore it before the test continues.
		const bool bOriginalEngineInitialized = GEngine->bIsInitialized;
		ON_SCOPE_EXIT { GEngine->bIsInitialized = bOriginalEngineInitialized; };
		GEngine->bIsInitialized = false;
		Scalability::SetQualityLevels(LegacyQuality);
		Settings->UGameUserSettings::ApplyNonResolutionSettings();
		TestTrue(TEXT("The engine base pre-init apply leaves legacy CVars unchanged"), Scalability::GetQualityLevels() == LegacyQuality);
		Settings->ApplyNonResolutionSettings();
		CheckMedium(TEXT("Pre-engine initialization applies actual CVars"), Scalability::GetQualityLevels());
		TestEqual(TEXT("Pre-engine quality apply preserves output resolution"), Settings->GetScreenResolution(), OriginalResolution);
		TestEqual(TEXT("Pre-engine quality apply preserves window mode"), Settings->GetFullscreenMode(), OriginalWindowMode);
	}

	Settings->SetOverallScalabilityLevel(0);
	Settings->ScalabilityQuality.ResolutionQuality = 55.0f;
	Settings->ApplyNonResolutionSettings();
	CheckMedium(TEXT("Apply to engine CVars"), Scalability::GetQualityLevels());
	TestEqual(TEXT("Non-resolution apply preserves output resolution"), Settings->GetScreenResolution(), OriginalResolution);
	TestEqual(TEXT("Non-resolution apply preserves window mode"), Settings->GetFullscreenMode(), OriginalWindowMode);
	Settings->SaveSettings();
	GConfig->Flush(false, TestIni);
	GConfig->UnloadFile(TestIni);
	Scalability::SetQualityLevels(LegacyQuality);
	Scalability::LoadState(TestIni);
	CheckMedium(TEXT("Persisted quality reload"), Scalability::GetQualityLevels());
	Settings->LoadSettings(false);
	CheckMedium(TEXT("Settings reload"), Settings->ScalabilityQuality);
	TestEqual(TEXT("Save and reload preserve FOV"), Settings->GetFieldOfView(), 104.0f);
	TestEqual(TEXT("Save and reload preserve sensitivity"), Settings->GetMouseSensitivity(), 1.75f);
	TestEqual(TEXT("Save and reload preserve master volume"), Settings->GetMasterVolume(), 0.5f);

	Settings->SetOverallScalabilityLevel(3);
	Settings->ScalabilityQuality.ResolutionQuality = 37.0f;
	Settings->SetToDefaults();
	CheckMedium(TEXT("Restore defaults"), Settings->ScalabilityQuality);
	TestEqual(TEXT("Restore defaults retains the existing FOV default"), Settings->GetFieldOfView(), UHeistGameUserSettings::DefaultFieldOfView);
	TestEqual(TEXT("Restore defaults retains the existing sensitivity default"), Settings->GetMouseSensitivity(), UHeistGameUserSettings::DefaultMouseSensitivity);
	TestEqual(TEXT("Restore defaults retains the existing volume default"), Settings->GetMasterVolume(), UHeistGameUserSettings::DefaultMasterVolume);
	return true;
}

#endif
