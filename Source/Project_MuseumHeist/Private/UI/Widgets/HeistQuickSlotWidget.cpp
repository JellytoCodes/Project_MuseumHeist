#include "UI/Widgets/HeistQuickSlotWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

#pragma region Presentation

void UHeistQuickSlotWidget::SetupHUDQuickSlot(const FHeistQuickSlotPresentation& InConfirmedPresentation, UTexture2D* InIcon)
{
	ConfirmedPresentation = InConfirmedPresentation;
	if (IsValid(PlaceholderIcon))
	{
		if (IsValid(InIcon))
		{
			PlaceholderIcon->SetBrushFromTexture(InIcon);
		}
		PlaceholderIcon->SetOpacity(ConfirmedPresentation.bAssigned ? 1.0f : 0.22f);
	}
	RefreshPresentation();
}

void UHeistQuickSlotWidget::RefreshPresentation()
{
	if (IsValid(KeyLabelText))
	{
		KeyLabelText->SetText(ConfirmedPresentation.KeyLabel.IsEmpty() ? FText::GetEmpty()
			: FText::Format(NSLOCTEXT("HeistQuickSlot", "BracketedKeyLabel", "[{0}]"), ConfirmedPresentation.KeyLabel));
	}
	if (IsValid(CountText))
	{
		CountText->SetText(FText::AsNumber(ConfirmedPresentation.Quantity));
		CountText->SetVisibility(ConfirmedPresentation.bAssigned ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

#pragma endregion
