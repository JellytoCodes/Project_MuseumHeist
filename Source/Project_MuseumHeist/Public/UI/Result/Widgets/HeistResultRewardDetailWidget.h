#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistResultRewardDetailWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE_OneParam(FHeistResultDetailVisibilityChanged, bool);

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistResultRewardDetailWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

  protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

  public:
	void ApplyTeamResult(const FHeistTeamResult& TeamResult);
	void ShowDetail();
	void HideDetail();
	bool IsDetailVisible() const;
	FHeistResultDetailVisibilityChanged& GetDetailVisibilityChangedDelegate();

  private:
	FHeistResultDetailVisibilityChanged DetailVisibilityChangedDelegate;

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DetailRequiredTargetValueTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DetailTargetStatusValueTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DetailQuotaValueTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DetailSecuredValueTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DetailExtraValueTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DetailRequiredTargetRewardValueTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DetailLooseLootValueTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DetailForgeryMultiplierValueTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DetailStealthMultiplierValueTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DetailArrestPenaltyValueTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> RewardDetailsCloseButton;
};
