#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistTitleMenuWidget.generated.h"

class UButton;
class UTextBlock;

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistTitleMenuWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

#pragma region Lifecycle

  protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

#pragma endregion

#pragma region ViewModel

  public:
	void SetupTitleMenuWidget(class UHeistTitleMenuViewModel* InTitleMenuViewModel, class UHeistSettingsViewModel* InSettingsViewModel);
	UHeistTitleMenuViewModel* GetTitleMenuViewModel() const;

  private:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|TitleMenu", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistTitleMenuViewModel> TitleMenuViewModel;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|TitleMenu", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistSettingsViewModel> SettingsViewModel;

#pragma endregion

#pragma region Presentation

  private:
	UFUNCTION()
	void HandleHostSessionClicked();

	UFUNCTION()
	void HandleJoinSessionClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleQuitGameClicked();

	void RefreshTitleMenuPresentation();
	void CloseChildPanels();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> HostSessionButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> JoinSessionButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> SettingsButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> QuitGameButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> SessionStatusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> SessionErrorText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<class UHeistSessionJoinWidget> SessionJoinWidget;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<class UHeistSettingsWidget> SettingsWidget;

#pragma endregion
};
