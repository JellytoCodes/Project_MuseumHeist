#include "UI/Widgets/HeistQuickSlotWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

#pragma region Presentation

void UHeistQuickSlotWidget::SetupHUDQuickSlot(const FHeistQuickSlotPresentation& InConfirmedPresentation, UObject* InIcon)
{
	ConfirmedPresentation = InConfirmedPresentation;
	if (IsValid(PlaceholderIcon))
	{
		if (IsValid(InIcon))
		{
			FSlateBrush IconBrush = PlaceholderIcon->GetBrush();
			IconBrush.SetResourceObject(InIcon);
			PlaceholderIcon->SetBrush(IconBrush);
		}
		PlaceholderIcon->SetOpacity(ConfirmedPresentation.bAssigned ? 1.0f : 0.22f);
	}
	RefreshPresentation();
}

void UHeistQuickSlotWidget::RefreshPresentation()
{
	if (IsValid(KeyLabelText))
	{
		KeyLabelText->SetText(ConfirmedPresentation.KeyLabel);
	}
	if (IsValid(CountText))
	{
		CountText->SetText(FText::AsNumber(ConfirmedPresentation.Quantity));
		CountText->SetVisibility(ConfirmedPresentation.bAssigned ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

#pragma endregion
