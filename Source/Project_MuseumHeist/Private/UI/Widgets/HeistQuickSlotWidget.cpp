#include "UI/Widgets/HeistQuickSlotWidget.h"

#include "Components/Border.h"
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
		KeyLabelText->SetText(ConfirmedPresentation.KeyLabel);
	}
	if (IsValid(CountText))
	{
		CountText->SetText(FText::AsNumber(ConfirmedPresentation.Quantity));
		CountText->SetVisibility(ConfirmedPresentation.bAssigned ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (IsValid(SlotBackground))
	{
		SlotBackground->SetBrushColor(ConfirmedPresentation.bAssigned ? FLinearColor(0.24f, 0.20f, 0.14f, 0.96f) : FLinearColor(0.04f, 0.035f, 0.028f, 0.88f));
	}
}

#pragma endregion
