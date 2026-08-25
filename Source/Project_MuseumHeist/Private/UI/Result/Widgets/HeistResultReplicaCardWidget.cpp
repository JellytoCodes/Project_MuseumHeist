#include "UI/Result/Widgets/HeistResultReplicaCardWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"

void UHeistResultReplicaCardWidget::ApplyReplicaEntry(const FHeistReplicaRecapEntry& ReplicaEntry, UTexture2D* ReplicaTexture)
{
	const FText DisplayName = ReplicaEntry.ArtifactDisplayName.IsEmpty()
		? FText::FromName(ReplicaEntry.ArtifactId)
		: ReplicaEntry.ArtifactDisplayName;

	if (IsValid(ArtifactNameText))
	{
		ArtifactNameText->SetText(DisplayName);
	}
	if (IsValid(QualityText))
	{
		QualityText->SetText(FText::Format(NSLOCTEXT("HeistResult", "ReplicaQuality", "품질 {0}"),
			FText::AsNumber(FMath::RoundToInt(ReplicaEntry.QualityScore))));
	}
	if (IsValid(RequiredTargetBadge))
	{
		RequiredTargetBadge->SetVisibility(ReplicaEntry.bRequiredTarget ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (IsValid(ReplicaImage))
	{
		if (IsValid(ReplicaTexture))
		{
			ReplicaImage->SetBrushFromTexture(ReplicaTexture, true);
		}
		ReplicaImage->SetVisibility(IsValid(ReplicaTexture) ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}
