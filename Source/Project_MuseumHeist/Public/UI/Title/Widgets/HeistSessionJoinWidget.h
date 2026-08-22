#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistSessionJoinWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistSessionJoinWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

#pragma region Lifecycle

  protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

#pragma endregion

#pragma region ViewModel

  public:
	void SetupSessionJoinWidget(class UHeistTitleMenuViewModel* InTitleMenuViewModel);
	void OpenSessionJoin();
	void CloseSessionJoin();

  private:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|TitleMenu", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistTitleMenuViewModel> TitleMenuViewModel;

#pragma endregion

#pragma region Presentation

  private:
	UFUNCTION()
	void HandleSubmitJoinSessionClicked();

	UFUNCTION()
	void HandleCancelSessionClicked();

	UFUNCTION()
	void HandleRetrySessionClicked();

	UFUNCTION()
	void HandleJoinCloseClicked();

	void RefreshSessionJoinPresentation();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UEditableTextBox> JoinCodeInput;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> SubmitJoinSessionButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> CancelSessionButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> RetrySessionButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> JoinCloseButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> SessionStatusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> SessionErrorText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> SessionActionHintText;

#pragma endregion
};
