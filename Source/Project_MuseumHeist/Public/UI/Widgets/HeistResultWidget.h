#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistResultWidget.generated.h"

class UTextBlock;
class UWidget;

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistResultWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

#pragma region Construction

  public:
	UHeistResultWidget(const FObjectInitializer& ObjectInitializer);

#pragma endregion

#pragma region Lifecycle

  protected:
	virtual void NativeDestruct() override;

#pragma endregion

#pragma region ViewModel

  public:
	void SetupResultWidget(class UHeistResultViewModel* InResultViewModel);
	UHeistResultViewModel* GetResultViewModel() const;

  private:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistResultViewModel> ResultViewModel;

#pragma endregion

#pragma region Presentation

  private:
	void RefreshResultPresentation();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> MyFinalScoreText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> EscapedBadge;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> ResultRow1Container;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> ResultRow2Container;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> ResultRow3Container;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> ResultRow4Container;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ResultRow1TextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ResultRow2TextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ResultRow3TextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ResultRow4TextBlock;

#pragma endregion
};
