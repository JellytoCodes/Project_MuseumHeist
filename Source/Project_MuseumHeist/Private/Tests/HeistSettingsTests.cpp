#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Components/Button.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
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

#endif
