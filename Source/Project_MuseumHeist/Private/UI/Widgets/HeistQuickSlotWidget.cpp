#include "UI/Widgets/HeistQuickSlotWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/DragDrop/HeistInventoryDragDropOperation.h"
#include "UI/Widgets/HeistInventoryWidget.h"

#pragma region Construction

UHeistQuickSlotWidget::UHeistQuickSlotWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

#pragma endregion

#pragma region Lifecycle

void UHeistQuickSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(ClearButton))
	{
		ClearButton->OnClicked.RemoveDynamic(this, &UHeistQuickSlotWidget::HandleClearButtonClicked);
		ClearButton->OnClicked.AddDynamic(this, &UHeistQuickSlotWidget::HandleClearButtonClicked);
	}
}

void UHeistQuickSlotWidget::NativeDestruct()
{
	if (IsValid(ClearButton))
	{
		ClearButton->OnClicked.RemoveDynamic(this, &UHeistQuickSlotWidget::HandleClearButtonClicked);
	}

	Super::NativeDestruct();
}

bool UHeistQuickSlotWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	const bool bCanAssign = IsValid(Cast<UHeistInventoryDragDropOperation>(InOperation)) && ConfirmedPresentation.SlotType != EHeistQuickSlotType::None;
	SetDropPreview(bCanAssign);
	return bCanAssign || Super::NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);
}

void UHeistQuickSlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	SetDropPreview(false);
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}

bool UHeistQuickSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	const UHeistInventoryDragDropOperation* InventoryOperation = Cast<UHeistInventoryDragDropOperation>(InOperation);
	const bool bHandled = IsValid(InventoryOperation) && IsValid(InventoryWidget) && ConfirmedPresentation.SlotType != EHeistQuickSlotType::None;
	if (bHandled)
	{
		InventoryWidget->RequestAssignQuickSlot(ConfirmedPresentation.SlotType, InventoryOperation->InstanceId);
	}

	SetDropPreview(false);
	return bHandled || Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

#pragma endregion

#pragma region Presentation

void UHeistQuickSlotWidget::SetupQuickSlot(const FHeistQuickSlotPresentation& InConfirmedPresentation, UTexture2D* InIcon, UHeistInventoryWidget* InInventoryWidget)
{
	ConfirmedPresentation = InConfirmedPresentation;
	InventoryWidget = InInventoryWidget;

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
	if (IsValid(ItemIdText))
	{
		FString ItemDisplayName = ConfirmedPresentation.ItemId.ToString();
		ItemDisplayName.ReplaceInline(TEXT("_"), TEXT(" "));
		ItemIdText->SetText(ConfirmedPresentation.bAssigned
								? FText::FromString(ItemDisplayName)
								: NSLOCTEXT("HeistQuickSlot", "EmptyItem", "비어 있음"));
	}
	if (IsValid(CountText))
	{
		CountText->SetText(FText::Format(NSLOCTEXT("HeistQuickSlot", "CountFormat", "x{0}"), FText::AsNumber(ConfirmedPresentation.Quantity)));
		CountText->SetVisibility(ConfirmedPresentation.bAssigned ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (IsValid(AssignmentStateText))
	{
		AssignmentStateText->SetText(ConfirmedPresentation.bAssigned ? NSLOCTEXT("HeistQuickSlot", "Assigned", "준비")
																	: NSLOCTEXT("HeistQuickSlot", "Empty", "동전을 여기에 놓으세요"));
	}
	if (IsValid(ClearButton))
	{
		ClearButton->SetIsEnabled(ConfirmedPresentation.bAssigned);
	}
	if (IsValid(SlotBackground))
	{
		SlotBackground->SetBrushColor(ConfirmedPresentation.bAssigned ? FLinearColor(0.08f, 0.24f, 0.32f, 0.96f) : FLinearColor(0.05f, 0.07f, 0.09f, 0.88f));
	}
}

void UHeistQuickSlotWidget::SetDropPreview(const bool bInDropPreview)
{
	if (IsValid(SlotBackground))
	{
		SlotBackground->SetBrushColor(bInDropPreview ? FLinearColor(0.15f, 0.70f, 0.78f, 1.0f)
													 : (ConfirmedPresentation.bAssigned ? FLinearColor(0.08f, 0.24f, 0.32f, 0.96f) : FLinearColor(0.05f, 0.07f, 0.09f, 0.88f)));
	}
}

void UHeistQuickSlotWidget::HandleClearButtonClicked()
{
	if (IsValid(InventoryWidget) && ConfirmedPresentation.bAssigned && ConfirmedPresentation.SlotType != EHeistQuickSlotType::None)
	{
		InventoryWidget->RequestClearQuickSlot(ConfirmedPresentation.SlotType);
	}
}

#pragma endregion
