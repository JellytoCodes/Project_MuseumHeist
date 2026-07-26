#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistTitleMenuWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistTitleMenuWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

#pragma region Construction

  public:
	UHeistTitleMenuWidget(const FObjectInitializer& ObjectInitializer);

#pragma endregion

#pragma region Lifecycle

  protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

#pragma endregion

#pragma region ViewModel

  public:
	void SetupTitleMenuWidget(class UHeistTitleMenuViewModel* InTitleMenuViewModel);
	UHeistTitleMenuViewModel* GetTitleMenuViewModel() const;

  private:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|TitleMenu", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistTitleMenuViewModel> TitleMenuViewModel;

#pragma endregion

#pragma region Presentation

  private:
	UFUNCTION()
	void HandleHostSessionClicked();

	UFUNCTION()
	void HandleJoinSessionClicked();

	void RefreshTitleMenuPresentation();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> HostSessionButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> JoinSessionButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UEditableTextBox> JoinCodeInput;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> SessionStatusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> SessionErrorText;

#pragma endregion
};
