#include "UI/Widgets/HeistInventoryWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Core/HeistPlayerController.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Inventory/HeistItemDataTypes.h"
#include "UI/DragDrop/HeistInventoryDragDropOperation.h"
#include "UI/ViewModels/HeistInventoryViewModel.h"
#include "UI/ViewModels/HeistQuickSlotViewModel.h"
#include "UI/Widgets/HeistInventoryItemWidget.h"
#include "UI/Widgets/HeistQuickSlotWidget.h"
#include "UI/Widgets/HeistInventorySlotWidget.h"
#include "View/MVVMView.h"

UHeistInventoryWidget::UHeistInventoryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UHeistInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.RemoveDynamic(this, &UHeistInventoryWidget::HandleCloseButtonClicked);
		CloseButton->OnClicked.AddDynamic(this, &UHeistInventoryWidget::HandleCloseButtonClicked);
	}
}

void UHeistInventoryWidget::NativeDestruct()
{
	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.RemoveDynamic(this, &UHeistInventoryWidget::HandleCloseButtonClicked);
	}

	if (IsValid(InventoryViewModel))
	{
		InventoryViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	}
	if (IsValid(QuickSlotViewModel))
	{
		QuickSlotViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	}

	Super::NativeDestruct();
}

bool UHeistInventoryWidget::NativeOnDragOver(
	const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	const UHeistInventoryDragDropOperation* InventoryOperation =
		Cast<UHeistInventoryDragDropOperation>(InOperation);
	FIntPoint TargetGridPosition(-1, -1);
	if (IsValid(InventoryOperation)
		&& TryGetDropTargetGridPosition(InDragDropEvent, TargetGridPosition))
	{
		UpdateDropPreview(InventoryOperation->InstanceId, TargetGridPosition);
		return true;
	}

	ClearDropPreview();
	return Super::NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);
}

void UHeistInventoryWidget::NativeOnDragLeave(
	const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	ClearDropPreview();
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}

bool UHeistInventoryWidget::NativeOnDrop(
	const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	const UHeistInventoryDragDropOperation* InventoryOperation =
		Cast<UHeistInventoryDragDropOperation>(InOperation);
	FIntPoint TargetGridPosition(-1, -1);
	const bool bHandled = IsValid(InventoryOperation)
		&& TryGetDropTargetGridPosition(InDragDropEvent, TargetGridPosition);
	if (bHandled)
	{
		RequestMoveItem(InventoryOperation->InstanceId, TargetGridPosition);
	}

	ClearDropPreview();
	return bHandled || Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

void UHeistInventoryWidget::SetupInventoryWidget(
	UHeistInventoryViewModel* InInventoryViewModel,
	UHeistQuickSlotViewModel* InQuickSlotViewModel,
	AHeistPlayerController* InPlayerController)
{
	checkf(IsValid(InInventoryViewModel), TEXT("HeistInventoryWidget requires a valid InventoryViewModel"));
	checkf(IsValid(InQuickSlotViewModel), TEXT("HeistInventoryWidget requires a valid QuickSlotViewModel"));
	checkf(IsValid(InPlayerController), TEXT("HeistInventoryWidget requires a valid HeistPlayerController"));

	InventoryViewModel = InInventoryViewModel;
	QuickSlotViewModel = InQuickSlotViewModel;
	PlayerController = InPlayerController;
	InventoryViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	InventoryViewModel->GetSnapshotChangedDelegate().AddUObject(
		this,
		&UHeistInventoryWidget::RefreshVisibilityFromConfirmedSnapshot);
	QuickSlotViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	QuickSlotViewModel->GetSnapshotChangedDelegate().AddUObject(
		this,
		&UHeistInventoryWidget::RefreshQuickSlotPresentation);

	TScriptInterface<INotifyFieldValueChanged> InventoryViewModelInterface;
	InventoryViewModelInterface.SetObject(InventoryViewModel);
	InventoryViewModelInterface.SetInterface(InventoryViewModel);

	TScriptInterface<INotifyFieldValueChanged> QuickSlotViewModelInterface;
	QuickSlotViewModelInterface.SetObject(QuickSlotViewModel);
	QuickSlotViewModelInterface.SetInterface(QuickSlotViewModel);

	if (UMVVMView* MVVMView = GetExtension<UMVVMView>())
	{
		MVVMView->SetViewModelByClass(InventoryViewModelInterface);
		MVVMView->SetViewModelByClass(QuickSlotViewModelInterface);
	}

	RefreshVisibilityFromConfirmedSnapshot();
	RefreshQuickSlotPresentation();
}

void UHeistInventoryWidget::RefreshVisibilityFromConfirmedSnapshot()
{
	SetVisibility(
		IsValid(InventoryViewModel) && InventoryViewModel->IsInventoryOpen()
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	if (IsValid(InventoryViewModel))
	{
		const TArray<FHeistInventoryItem>& ConfirmedItems = InventoryViewModel->GetItems();
		const int32 GridColumns = InventoryViewModel->GetGridColumnCount();
		const int32 GridRows = InventoryViewModel->GetGridRowCount();
		if (IsValid(InventorySummaryText))
		{
			InventorySummaryText->SetText(FText::Format(
				NSLOCTEXT("HeistInventory", "InventorySummaryFormat", "INVENTORY  {0} ITEMS  |  GRID {1}x{2}"),
				FText::AsNumber(ConfirmedItems.Num()),
				FText::AsNumber(GridColumns),
				FText::AsNumber(GridRows)));
		}

		BP_RefreshConfirmedInventory(
			ConfirmedItems,
			GridColumns,
			GridRows);
		RebuildConfirmedInventory(
			ConfirmedItems,
			GridColumns,
			GridRows);
	}
}

void UHeistInventoryWidget::RefreshQuickSlotPresentation()
{
	if (IsValid(QuickSlotViewModel))
	{
		const TArray<FHeistQuickSlotState>& ConfirmedQuickSlots = QuickSlotViewModel->GetQuickSlots();
		int32 AssignedQuickSlotCount = 0;
		for (const FHeistQuickSlotState& QuickSlotState : ConfirmedQuickSlots)
		{
			if (QuickSlotState.ItemInstanceId != INDEX_NONE)
			{
				++AssignedQuickSlotCount;
			}
		}
		if (IsValid(QuickSlotSummaryText))
		{
			QuickSlotSummaryText->SetText(FText::Format(
				NSLOCTEXT("HeistInventory", "QuickSlotSummaryFormat", "QUICK SLOTS  {0}/3 ASSIGNED"),
				FText::AsNumber(AssignedQuickSlotCount)));
		}

		BP_RefreshConfirmedQuickSlots(ConfirmedQuickSlots);
		RebuildConfirmedQuickSlots(QuickSlotViewModel->GetQuickSlotPresentations());
	}
}

void UHeistInventoryWidget::RequestCloseInventory()
{
	if (IsValid(PlayerController))
	{
		PlayerController->RequestSetInventoryOpen(false);
	}
}

void UHeistInventoryWidget::RequestMoveItem(const int32 InstanceId, const FIntPoint TargetGridPosition)
{
	if (IsValid(PlayerController))
	{
		PlayerController->RequestMoveInventoryItem(InstanceId, TargetGridPosition);
	}
}

void UHeistInventoryWidget::RequestRotateItem(const int32 InstanceId)
{
	if (IsValid(PlayerController))
	{
		PlayerController->RequestRotateInventoryItem(InstanceId);
	}
}

void UHeistInventoryWidget::RequestDropItem(const int32 InstanceId)
{
	if (IsValid(PlayerController))
	{
		PlayerController->RequestDropInventoryItem(InstanceId);
	}
}

void UHeistInventoryWidget::RequestAssignQuickSlot(
	const EHeistQuickSlotType SlotType,
	const int32 InstanceId)
{
	if (IsValid(PlayerController))
	{
		PlayerController->RequestAssignQuickSlot(SlotType, InstanceId);
	}
}

void UHeistInventoryWidget::RequestClearQuickSlot(const EHeistQuickSlotType SlotType)
{
	if (IsValid(PlayerController))
	{
		PlayerController->RequestClearQuickSlot(SlotType);
	}
}

bool UHeistInventoryWidget::CanPreviewItemDrop(
	const int32 InstanceId,
	const FIntPoint& TargetGridPosition) const
{
	const FHeistInventoryItem* SourceItem = ConfirmedInventoryItems.FindByPredicate(
		[InstanceId](const FHeistInventoryItem& InventoryItem)
		{
			return InventoryItem.InstanceId == InstanceId;
		});
	if (SourceItem == nullptr)
	{
		return false;
	}

	FIntPoint PlacedSize;
	UTexture2D* Icon = nullptr;
	TryResolveItemPresentation(*SourceItem, PlacedSize, Icon);
	if (TargetGridPosition.X < 0
		|| TargetGridPosition.Y < 0
		|| TargetGridPosition.X + PlacedSize.X > ConfirmedGridColumns
		|| TargetGridPosition.Y + PlacedSize.Y > ConfirmedGridRows)
	{
		return false;
	}

	for (int32 Row = TargetGridPosition.Y; Row < TargetGridPosition.Y + PlacedSize.Y; ++Row)
	{
		for (int32 Column = TargetGridPosition.X; Column < TargetGridPosition.X + PlacedSize.X; ++Column)
		{
			if (IsGridCoordinateOccupied(FIntPoint(Column, Row), InstanceId))
			{
				return false;
			}
		}
	}

	return true;
}

void UHeistInventoryWidget::RebuildConfirmedInventory(
	const TArray<FHeistInventoryItem>& ConfirmedItems,
	const int32 GridColumns,
	const int32 GridRows)
{
	ConfirmedInventoryItems = ConfirmedItems;
	ConfirmedGridColumns = GridColumns;
	ConfirmedGridRows = GridRows;

	InventorySlotWidgets.Reset();
	if (IsValid(InventoryGrid))
	{
		InventoryGrid->ClearChildren();
		if (InventorySlotWidgetClass)
		{
			for (int32 Row = 0; Row < GridRows; ++Row)
			{
				for (int32 Column = 0; Column < GridColumns; ++Column)
				{
					UHeistInventorySlotWidget* SlotWidget =
						CreateWidget<UHeistInventorySlotWidget>(GetOwningPlayer(), InventorySlotWidgetClass);
					if (!IsValid(SlotWidget))
					{
						continue;
					}

					const FIntPoint GridCoordinate(Column, Row);
					SlotWidget->SetupSlot(
						GridCoordinate,	
						IsGridCoordinateOccupied(GridCoordinate),
						this);
					UUniformGridSlot* GridSlot = InventoryGrid->AddChildToUniformGrid(SlotWidget, Row, Column);
					if (IsValid(GridSlot))
					{
						GridSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
						GridSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);
					}
					InventorySlotWidgets.Add(SlotWidget);
				}
			}
		}
	}

	InventoryItemWidgets.Reset();
	if (!IsValid(ItemOverlay))
	{
		return;
	}

	ItemOverlay->ClearChildren();
	if (!InventoryItemWidgetClass)
	{
		return;
	}

	for (const FHeistInventoryItem& ConfirmedItem : ConfirmedItems)
	{
		FIntPoint PlacedSize;
		UTexture2D* Icon = nullptr;
		TryResolveItemPresentation(ConfirmedItem, PlacedSize, Icon);

		UHeistInventoryItemWidget* ItemWidget =
			CreateWidget<UHeistInventoryItemWidget>(GetOwningPlayer(), InventoryItemWidgetClass);
		if (!IsValid(ItemWidget))
		{
			continue;
		}

		ItemWidget->SetupItem(ConfirmedItem, PlacedSize, Icon, this);
		UCanvasPanelSlot* CanvasSlot = ItemOverlay->AddChildToCanvas(ItemWidget);
		CanvasSlot->SetPosition(FVector2D(
			ConfirmedItem.GridPosition.X * InventoryCellSize.X,
			ConfirmedItem.GridPosition.Y * InventoryCellSize.Y));
		CanvasSlot->SetSize(FVector2D(
			PlacedSize.X * InventoryCellSize.X,
			PlacedSize.Y * InventoryCellSize.Y));
		CanvasSlot->SetZOrder(10);
		InventoryItemWidgets.Add(ItemWidget);
	}
}

void UHeistInventoryWidget::RebuildConfirmedQuickSlots(
	const TArray<FHeistQuickSlotPresentation>& ConfirmedQuickSlots)
{
	QuickSlotWidgets.Reset();
	if (!IsValid(QuickSlotPanel))
	{
		return;
	}

	QuickSlotPanel->ClearChildren();
	if (!QuickSlotWidgetClass)
	{
		return;
	}

	for (int32 SlotIndex = 0; SlotIndex < ConfirmedQuickSlots.Num(); ++SlotIndex)
	{
		const FHeistQuickSlotPresentation& ConfirmedQuickSlot = ConfirmedQuickSlots[SlotIndex];
		UHeistQuickSlotWidget* QuickSlotWidget =
			CreateWidget<UHeistQuickSlotWidget>(GetOwningPlayer(), QuickSlotWidgetClass);
		if (!IsValid(QuickSlotWidget))
		{
			continue;
		}

		QuickSlotWidget->SetupQuickSlot(
			ConfirmedQuickSlot,
			ResolveQuickSlotIcon(ConfirmedQuickSlot.ItemId),
			this);
		UUniformGridSlot* GridSlot = QuickSlotPanel->AddChildToUniformGrid(QuickSlotWidget, 0, SlotIndex);
		GridSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
		GridSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);
		QuickSlotWidgets.Add(QuickSlotWidget);
	}
}

bool UHeistInventoryWidget::TryResolveItemPresentation(
	const FHeistInventoryItem& InventoryItem,
	FIntPoint& OutPlacedSize,
	UTexture2D*& OutIcon) const
{
	OutPlacedSize = FIntPoint(1, 1);
	OutIcon = nullptr;
	if (!IsValid(ItemDataTable)
		|| ItemDataTable->GetRowStruct() != FHeistItemDataRow::StaticStruct())
	{
		return false;
	}

	const FHeistItemDataRow* ItemDefinition = ItemDataTable->FindRow<FHeistItemDataRow>(
		InventoryItem.ItemId,
		TEXT("UHeistInventoryWidget::TryResolveItemPresentation"),
		false);
	if (ItemDefinition == nullptr)
	{
		return false;
	}

	OutPlacedSize = InventoryItem.bRotated
		? FIntPoint(ItemDefinition->GridSize.Y, ItemDefinition->GridSize.X)
		: ItemDefinition->GridSize;
	OutIcon = ItemDefinition->Icon.LoadSynchronous();
	return true;
}

UTexture2D* UHeistInventoryWidget::ResolveQuickSlotIcon(const FName ItemId) const
{
	if (ItemId.IsNone()
		|| !IsValid(ItemDataTable)
		|| ItemDataTable->GetRowStruct() != FHeistItemDataRow::StaticStruct())
	{
		return nullptr;
	}

	const FHeistItemDataRow* ItemDefinition = ItemDataTable->FindRow<FHeistItemDataRow>(
		ItemId,
		TEXT("UHeistInventoryWidget::ResolveQuickSlotIcon"),
		false);
	return ItemDefinition ? ItemDefinition->Icon.LoadSynchronous() : nullptr;
}

bool UHeistInventoryWidget::TryGetDropTargetGridPosition(
	const FDragDropEvent& DragDropEvent,
	FIntPoint& OutGridPosition) const
{
	OutGridPosition = FIntPoint(-1, -1);
	if (!IsValid(InventoryGrid)
		|| InventoryCellSize.X <= 0.0
		|| InventoryCellSize.Y <= 0.0)
	{
		return false;
	}

	const FVector2D LocalPosition =
		InventoryGrid->GetCachedGeometry().AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition());
	OutGridPosition = FIntPoint(
		FMath::FloorToInt(LocalPosition.X / InventoryCellSize.X),
		FMath::FloorToInt(LocalPosition.Y / InventoryCellSize.Y));
	return OutGridPosition.X >= 0
		&& OutGridPosition.Y >= 0
		&& OutGridPosition.X < ConfirmedGridColumns
		&& OutGridPosition.Y < ConfirmedGridRows;
}

void UHeistInventoryWidget::UpdateDropPreview(
	const int32 InstanceId,
	const FIntPoint& TargetGridPosition)
{
	const FHeistInventoryItem* SourceItem = ConfirmedInventoryItems.FindByPredicate(
		[InstanceId](const FHeistInventoryItem& InventoryItem)
		{
			return InventoryItem.InstanceId == InstanceId;
		});
	if (SourceItem == nullptr)
	{
		ClearDropPreview();
		return;
	}

	FIntPoint PlacedSize;
	UTexture2D* Icon = nullptr;
	TryResolveItemPresentation(*SourceItem, PlacedSize, Icon);
	const bool bValid = CanPreviewItemDrop(InstanceId, TargetGridPosition);
	for (UHeistInventorySlotWidget* SlotWidget : InventorySlotWidgets)
	{
		if (!IsValid(SlotWidget))
		{
			continue;
		}

		const FIntPoint Coordinate = SlotWidget->GetGridCoordinate();
		const bool bInsidePreview =
			Coordinate.X >= TargetGridPosition.X
			&& Coordinate.Y >= TargetGridPosition.Y
			&& Coordinate.X < TargetGridPosition.X + PlacedSize.X
			&& Coordinate.Y < TargetGridPosition.Y + PlacedSize.Y;
		SlotWidget->SetDropPreview(bInsidePreview, bValid);
	}
}

void UHeistInventoryWidget::ClearDropPreview()
{
	for (UHeistInventorySlotWidget* SlotWidget : InventorySlotWidgets)
	{
		if (IsValid(SlotWidget))
		{
			SlotWidget->SetDropPreview(false, false);
		}
	}
}

bool UHeistInventoryWidget::IsGridCoordinateOccupied(
	const FIntPoint& GridCoordinate,
	const int32 ExcludedInstanceId) const
{
	for (const FHeistInventoryItem& InventoryItem : ConfirmedInventoryItems)
	{
		if (InventoryItem.InstanceId == ExcludedInstanceId)
		{
			continue;
		}

		FIntPoint PlacedSize;
		UTexture2D* Icon = nullptr;
		TryResolveItemPresentation(InventoryItem, PlacedSize, Icon);
		if (GridCoordinate.X >= InventoryItem.GridPosition.X
			&& GridCoordinate.Y >= InventoryItem.GridPosition.Y
			&& GridCoordinate.X < InventoryItem.GridPosition.X + PlacedSize.X
			&& GridCoordinate.Y < InventoryItem.GridPosition.Y + PlacedSize.Y)
		{
			return true;
		}
	}

	return false;
}

void UHeistInventoryWidget::HandleCloseButtonClicked()
{
	RequestCloseInventory();
}
