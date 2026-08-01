#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistResultWidget.generated.h"

class UTextBlock;
class UWidget;
class UButton;

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
	virtual void NativeConstruct() override;
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

	UFUNCTION()
	void HandleReturnToLobbyClicked();

	// Blueprint events must remain overridable, so presentation hooks cannot be private.
  protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|Result", meta = (DisplayName = "Refresh Replica Recap"))
	void BP_RefreshReplicaRecap(const TArray<FHeistReplicaRecapEntry>& ReplicaRecap);

  private:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> OutcomeTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> OutcomeReasonTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ContractProgressTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TeamRewardTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> RewardBreakdownTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ReplicaRecapTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> ReturnToLobbyButton;

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
