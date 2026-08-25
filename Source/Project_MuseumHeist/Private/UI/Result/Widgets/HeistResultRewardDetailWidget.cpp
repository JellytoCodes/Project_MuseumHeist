#include "UI/Result/Widgets/HeistResultRewardDetailWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void UHeistResultRewardDetailWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (IsValid(RewardDetailsCloseButton))
	{
		RewardDetailsCloseButton->OnClicked.RemoveAll(this);
		RewardDetailsCloseButton->OnClicked.AddDynamic(this, &UHeistResultRewardDetailWidget::HandleCloseClicked);
	}
	HideDetail();
}

void UHeistResultRewardDetailWidget::NativeDestruct()
{
	if (IsValid(RewardDetailsCloseButton))
	{
		RewardDetailsCloseButton->OnClicked.RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UHeistResultRewardDetailWidget::ApplyTeamResult(const FHeistTeamResult& TeamResult)
{
	const FText RequiredTargetName = TeamResult.RequiredTargetDisplayName.IsEmpty()
		? FText::FromName(TeamResult.RequiredTargetArtifactId)
		: TeamResult.RequiredTargetDisplayName;
	if (IsValid(DetailRequiredTargetValueTextBlock))
	{
		DetailRequiredTargetValueTextBlock->SetText(RequiredTargetName);
	}
	if (IsValid(DetailTargetStatusValueTextBlock))
	{
		DetailTargetStatusValueTextBlock->SetText(TeamResult.bRequiredTargetSecured
			? NSLOCTEXT("HeistResult", "DetailTargetSecured", "확보 완료")
			: NSLOCTEXT("HeistResult", "DetailTargetMissing", "미확보"));
	}
	if (IsValid(DetailQuotaValueTextBlock))
	{
		DetailQuotaValueTextBlock->SetText(FText::AsNumber(TeamResult.LootValueQuota));
	}
	if (IsValid(DetailSecuredValueTextBlock))
	{
		DetailSecuredValueTextBlock->SetText(FText::AsNumber(TeamResult.SecuredValue));
	}
	if (IsValid(DetailExtraValueTextBlock))
	{
		DetailExtraValueTextBlock->SetText(FText::AsNumber(TeamResult.ExtraValue));
	}
	if (IsValid(DetailRequiredTargetRewardValueTextBlock))
	{
		DetailRequiredTargetRewardValueTextBlock->SetText(FText::AsNumber(TeamResult.RequiredTargetValue));
	}
	if (IsValid(DetailLooseLootValueTextBlock))
	{
		DetailLooseLootValueTextBlock->SetText(FText::AsNumber(TeamResult.SecuredLooseLootValue));
	}

	FNumberFormattingOptions PercentFormatting;
	PercentFormatting.MinimumFractionalDigits = 0;
	PercentFormatting.MaximumFractionalDigits = 1;
	if (IsValid(DetailForgeryMultiplierValueTextBlock))
	{
		DetailForgeryMultiplierValueTextBlock->SetText(FText::AsPercent(TeamResult.ForgeryRewardMultiplier, &PercentFormatting));
	}
	if (IsValid(DetailStealthMultiplierValueTextBlock))
	{
		DetailStealthMultiplierValueTextBlock->SetText(FText::AsPercent(TeamResult.StealthRewardMultiplier, &PercentFormatting));
	}
	if (IsValid(DetailArrestPenaltyValueTextBlock))
	{
		DetailArrestPenaltyValueTextBlock->SetText(TeamResult.ArrestPenalty > 0
			? FText::Format(NSLOCTEXT("HeistResult", "DetailArrestPenaltyFormat", "-{0}"), FText::AsNumber(TeamResult.ArrestPenalty))
			: FText::AsNumber(0));
	}
}

void UHeistResultRewardDetailWidget::ShowDetail()
{
	SetVisibility(ESlateVisibility::Visible);
}

void UHeistResultRewardDetailWidget::HideDetail()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

bool UHeistResultRewardDetailWidget::IsDetailVisible() const
{
	return GetVisibility() == ESlateVisibility::Visible;
}

void UHeistResultRewardDetailWidget::HandleCloseClicked()
{
	HideDetail();
}
