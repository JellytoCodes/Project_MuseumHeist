#include "UI/Widgets/HeistInventoryWidget.h"

#include "Character/Components/HeistNoiseEmitterComponent.h"
#include "Character/HeistPlayerCharacter.h"
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
#include "UI/Widgets/HeistInventoryFrameWidget.h"
#include "UI/Widgets/HeistInventoryItemWidget.h"
#include "UI/Widgets/HeistInventorySlotWidget.h"
#include "View/MVVMView.h"
#include "World/Actors/Loot/HeistPaintingDisplayCaseActor.h"

UHeistInventoryWidget::UHeistInventoryWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UHeistInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	InventoryGrid = IsValid(InventoryFrameWidget) ? InventoryFrameWidget->GetInventoryGrid() : nullptr;
	ItemOverlay = IsValid(InventoryFrameWidget) ? InventoryFrameWidget->GetItemOverlay() : nullptr;

	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.RemoveDynamic(this, &UHeistInventoryWidget::HandleCloseButtonClicked);
		CloseButton->OnClicked.AddDynamic(this, &UHeistInventoryWidget::HandleCloseButtonClicked);
	}
}

void UHeistInventoryWidget::NativeDestruct()
{
	InventoryGrid = nullptr;
	ItemOverlay = nullptr;
	bItemOverlayLayoutDirty = true;
	LastGridTopLeftInOverlay = FVector2D::ZeroVector;
	LastGridSizeInOverlay = FVector2D::ZeroVector;

	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.RemoveDynamic(this, &UHeistInventoryWidget::HandleCloseButtonClicked);
	}

	if (IsValid(InventoryViewModel))
	{
		InventoryViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UHeistInventoryWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshItemOverlayLayout();
}

bool UHeistInventoryWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UHeistInventoryDragDropOperation* InventoryOperation = Cast<UHeistInventoryDragDropOperation>(InOperation);
	FIntPoint TargetGridPosition(-1, -1);
	if (IsValid(InventoryOperation))
	{
		if (TryGetDropTargetGridPosition(InDragDropEvent, TargetGridPosition))
		{
			InventoryOperation->SetWorldDropPreview(false);
			UpdateDropPreview(InventoryOperation->InstanceId, TargetGridPosition);
		}
		else
		{
			InventoryOperation->SetWorldDropPreview(true);
			ClearDropPreview();
		}
		return true;
	}

	ClearDropPreview();
	return Super::NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);
}

void UHeistInventoryWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	if (UHeistInventoryDragDropOperation* InventoryOperation = Cast<UHeistInventoryDragDropOperation>(InOperation))
	{
		InventoryOperation->SetWorldDropPreview(true);
	}
	ClearDropPreview();
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}

bool UHeistInventoryWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UHeistInventoryDragDropOperation* InventoryOperation = Cast<UHeistInventoryDragDropOperation>(InOperation);
	if (!IsValid(InventoryOperation))
	{
		ClearDropPreview();
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}

	FIntPoint TargetGridPosition(-1, -1);
	if (TryGetDropTargetGridPosition(InDragDropEvent, TargetGridPosition))
	{
		RequestMoveItem(InventoryOperation->InstanceId, TargetGridPosition);
	}
	else
	{
		InventoryOperation->SetWorldDropPreview(true);
		RequestDropItem(InventoryOperation->InstanceId);
	}

	ClearDropPreview();
	return true;
}

void UHeistInventoryWidget::SetupInventoryWidget(UHeistInventoryViewModel* InInventoryViewModel, AHeistPlayerController* InPlayerController)
{
	checkf(IsValid(InInventoryViewModel), TEXT("HeistInventoryWidget requires a valid InventoryViewModel"));
	checkf(IsValid(InPlayerController), TEXT("HeistInventoryWidget requires a valid HeistPlayerController"));

	InventoryViewModel = InInventoryViewModel;
	PlayerController = InPlayerController;
	InventoryViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	InventoryViewModel->GetSnapshotChangedDelegate().AddUObject(this, &UHeistInventoryWidget::RefreshVisibilityFromConfirmedSnapshot);

	TScriptInterface<INotifyFieldValueChanged> InventoryViewModelInterface;
	InventoryViewModelInterface.SetObject(InventoryViewModel);
	InventoryViewModelInterface.SetInterface(InventoryViewModel);

	if (UMVVMView* MVVMView = GetExtension<UMVVMView>())
	{
		MVVMView->SetViewModelByClass(InventoryViewModelInterface);
	}

	RefreshVisibilityFromConfirmedSnapshot();
}

void UHeistInventoryWidget::RefreshVisibilityFromConfirmedSnapshot()
{
	SetVisibility(IsValid(InventoryViewModel) && InventoryViewModel->IsInventoryOpen() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (IsValid(InventoryViewModel))
	{
		const TArray<FHeistInventoryItem>& ConfirmedItems = InventoryViewModel->GetItems();
		const int32 GridColumns = InventoryViewModel->GetGridColumnCount();
		const int32 GridRows = InventoryViewModel->GetGridRowCount();
		if (IsValid(InventorySummaryText))
		{
			const AHeistPlayerCharacter* OwningCharacter = IsValid(PlayerController) ? Cast<AHeistPlayerCharacter>(PlayerController->GetPawn()) : nullptr;
			const UHeistNoiseEmitterComponent* NoiseEmitter = IsValid(OwningCharacter) ? OwningCharacter->GetNoiseEmitterComponent() : GetDefault<UHeistNoiseEmitterComponent>();
			const float MediumWeightThreshold = IsValid(NoiseEmitter) ? FMath::Max(0.0f, NoiseEmitter->GetMediumWeightThreshold()) : 5.0f;
			const float HeavyWeightThreshold = IsValid(NoiseEmitter) ? FMath::Max(MediumWeightThreshold, NoiseEmitter->GetHeavyWeightThreshold()) : 10.0f;
			const float TotalWeight = FMath::Max(0.0f, InventoryViewModel->GetTotalWeight());

			FText WeightState = NSLOCTEXT("HeistInventory", "WeightStateLight", "가벼움");
			if (TotalWeight >= HeavyWeightThreshold)
			{
				WeightState = NSLOCTEXT("HeistInventory", "WeightStateHeavy", "무거움");
			}
			else if (TotalWeight >= MediumWeightThreshold)
			{
				WeightState = NSLOCTEXT("HeistInventory", "WeightStateMedium", "중간");
			}

			InventorySummaryText->SetText(FText::Format(NSLOCTEXT("HeistInventory", "WeightStateFormat", "배낭 상태: {0}"), WeightState));
		}

		BP_RefreshConfirmedInventory(ConfirmedItems, GridColumns, GridRows);
		RebuildConfirmedInventory(ConfirmedItems, GridColumns, GridRows);
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

bool UHeistInventoryWidget::CanPreviewItemDrop(const int32 InstanceId, const FIntPoint& TargetGridPosition) const
{
	const FHeistInventoryItem* SourceItem = ConfirmedInventoryItems.FindByPredicate([InstanceId](const FHeistInventoryItem& InventoryItem) { return InventoryItem.InstanceId == InstanceId; });
	if (SourceItem == nullptr)
	{
		return false;
	}

	FIntPoint PlacedSize;
	UTexture2D* Icon = nullptr;
	TryResolveItemPresentation(*SourceItem, PlacedSize, Icon);
	if (TargetGridPosition.X < 0 || TargetGridPosition.Y < 0 || TargetGridPosition.X + PlacedSize.X > ConfirmedGridColumns || TargetGridPosition.Y + PlacedSize.Y > ConfirmedGridRows)
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

void UHeistInventoryWidget::RebuildConfirmedInventory(const TArray<FHeistInventoryItem>& ConfirmedItems, const int32 GridColumns, const int32 GridRows)
{
	ConfirmedInventoryItems = ConfirmedItems;
	ConfirmedGridColumns = GridColumns;
	ConfirmedGridRows = GridRows;
	bItemOverlayLayoutDirty = true;

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
					UHeistInventorySlotWidget* SlotWidget = CreateWidget<UHeistInventorySlotWidget>(GetOwningPlayer(), InventorySlotWidgetClass);
					if (!IsValid(SlotWidget))
					{
						continue;
					}

					const FIntPoint GridCoordinate(Column, Row);
					SlotWidget->SetupSlot(GridCoordinate, IsGridCoordinateOccupied(GridCoordinate), this);
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

		UHeistInventoryItemWidget* ItemWidget = CreateWidget<UHeistInventoryItemWidget>(GetOwningPlayer(), InventoryItemWidgetClass);
		if (!IsValid(ItemWidget))
		{
			continue;
		}

		ItemWidget->SetupItem(ConfirmedItem, Icon, this);
		UCanvasPanelSlot* CanvasSlot = ItemOverlay->AddChildToCanvas(ItemWidget);
		const FVector2D CellSize = ResolveInventoryCellSize();
		CanvasSlot->SetPosition(FVector2D(ConfirmedItem.GridPosition.X * CellSize.X, ConfirmedItem.GridPosition.Y * CellSize.Y));
		CanvasSlot->SetSize(FVector2D(PlacedSize.X * CellSize.X, PlacedSize.Y * CellSize.Y));
		CanvasSlot->SetZOrder(10);
		InventoryItemWidgets.Add(ItemWidget);
	}

	RefreshItemOverlayLayout();
}

void UHeistInventoryWidget::RefreshItemOverlayLayout()
{
	if (!IsValid(InventoryGrid) || !IsValid(ItemOverlay) || ConfirmedGridColumns <= 0 || ConfirmedGridRows <= 0)
	{
		return;
	}

	const FGeometry& GridGeometry = InventoryGrid->GetCachedGeometry();
	const FGeometry& OverlayGeometry = ItemOverlay->GetCachedGeometry();
	const FVector2D GridLocalSize = GridGeometry.GetLocalSize();
	const FVector2D OverlayLocalSize = OverlayGeometry.GetLocalSize();
	if (GridLocalSize.X <= 0.0 || GridLocalSize.Y <= 0.0 || OverlayLocalSize.X <= 0.0 || OverlayLocalSize.Y <= 0.0)
	{
		return;
	}

	const FVector2D GridTopLeft = OverlayGeometry.AbsoluteToLocal(GridGeometry.LocalToAbsolute(FVector2D::ZeroVector));
	const FVector2D GridBottomRight = OverlayGeometry.AbsoluteToLocal(GridGeometry.LocalToAbsolute(GridLocalSize));
	const FVector2D GridSizeInOverlay = GridBottomRight - GridTopLeft;
	if (!bItemOverlayLayoutDirty && GridTopLeft.Equals(LastGridTopLeftInOverlay) && GridSizeInOverlay.Equals(LastGridSizeInOverlay))
	{
		return;
	}

	const FVector2D CellSize(GridSizeInOverlay.X / static_cast<double>(ConfirmedGridColumns), GridSizeInOverlay.Y / static_cast<double>(ConfirmedGridRows));
	if (CellSize.X <= 0.0 || CellSize.Y <= 0.0)
	{
		return;
	}

	for (UHeistInventoryItemWidget* ItemWidget : InventoryItemWidgets)
	{
		if (!IsValid(ItemWidget))
		{
			continue;
		}

		const FHeistInventoryItem* ConfirmedItem = ConfirmedInventoryItems.FindByPredicate(
			[ItemWidget](const FHeistInventoryItem& InventoryItem) { return InventoryItem.InstanceId == ItemWidget->GetInstanceId(); });
		UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(ItemWidget->Slot);
		if (ConfirmedItem == nullptr || !IsValid(CanvasSlot))
		{
			continue;
		}

		const FIntPoint PlacedSize = ConfirmedItem->BaseGridSize.X > 0 && ConfirmedItem->BaseGridSize.Y > 0 ? ConfirmedItem->GetPlacedSize() : FIntPoint(1, 1);
		CanvasSlot->SetPosition(GridTopLeft + FVector2D(ConfirmedItem->GridPosition.X * CellSize.X, ConfirmedItem->GridPosition.Y * CellSize.Y));
		CanvasSlot->SetSize(FVector2D(PlacedSize.X * CellSize.X, PlacedSize.Y * CellSize.Y));
	}

	bItemOverlayLayoutDirty = false;
	LastGridTopLeftInOverlay = GridTopLeft;
	LastGridSizeInOverlay = GridSizeInOverlay;
}

FVector2D UHeistInventoryWidget::ResolveInventoryCellSize() const
{
	if (IsValid(InventoryGrid) && ConfirmedGridColumns > 0 && ConfirmedGridRows > 0)
	{
		const FVector2D GridLocalSize = InventoryGrid->GetCachedGeometry().GetLocalSize();
		if (GridLocalSize.X > 0.0 && GridLocalSize.Y > 0.0)
		{
			return FVector2D(GridLocalSize.X / static_cast<double>(ConfirmedGridColumns), GridLocalSize.Y / static_cast<double>(ConfirmedGridRows));
		}
	}

	return InventoryCellSize;
}

bool UHeistInventoryWidget::TryResolveItemPresentation(const FHeistInventoryItem& InventoryItem, FIntPoint& OutPlacedSize, UTexture2D*& OutIcon) const
{
	OutPlacedSize = FIntPoint(1, 1);
	OutIcon = nullptr;
	if (InventoryItem.BaseGridSize.X > 0 && InventoryItem.BaseGridSize.Y > 0)
	{
		OutPlacedSize = InventoryItem.GetPlacedSize();
	}
	if (InventoryItem.IsOriginalArtifact())
	{
		const AHeistPaintingDisplayCaseActor* PaintingSourceCase = Cast<AHeistPaintingDisplayCaseActor>(InventoryItem.SourceDisplayCase.Get());
		if (IsValid(PaintingSourceCase))
		{
			OutIcon = PaintingSourceCase->LoadOriginalReferenceImage();
		}
		return true;
	}
	if (!IsValid(ItemDataTable) || ItemDataTable->GetRowStruct() != FHeistItemDataRow::StaticStruct())
	{
		return false;
	}

	const FHeistItemDataRow* ItemDefinition = ItemDataTable->FindRow<FHeistItemDataRow>(InventoryItem.ItemId, TEXT("UHeistInventoryWidget::TryResolveItemPresentation"), false);
	if (ItemDefinition == nullptr)
	{
		return false;
	}

	OutIcon = ItemDefinition->Icon.LoadSynchronous();
	return true;
}

bool UHeistInventoryWidget::TryGetDropTargetGridPosition(const FDragDropEvent& DragDropEvent, FIntPoint& OutGridPosition) const
{
	OutGridPosition = FIntPoint(-1, -1);
	if (!IsValid(InventoryGrid))
	{
		return false;
	}

	const FVector2D CellSize = ResolveInventoryCellSize();
	if (CellSize.X <= 0.0 || CellSize.Y <= 0.0)
	{
		return false;
	}

	const FVector2D LocalPosition = InventoryGrid->GetCachedGeometry().AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition());
	OutGridPosition = FIntPoint(FMath::FloorToInt(LocalPosition.X / CellSize.X), FMath::FloorToInt(LocalPosition.Y / CellSize.Y));
	return OutGridPosition.X >= 0 && OutGridPosition.Y >= 0 && OutGridPosition.X < ConfirmedGridColumns && OutGridPosition.Y < ConfirmedGridRows;
}

void UHeistInventoryWidget::UpdateDropPreview(const int32 InstanceId, const FIntPoint& TargetGridPosition)
{
	const FHeistInventoryItem* SourceItem = ConfirmedInventoryItems.FindByPredicate([InstanceId](const FHeistInventoryItem& InventoryItem) { return InventoryItem.InstanceId == InstanceId; });
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
			Coordinate.X >= TargetGridPosition.X && Coordinate.Y >= TargetGridPosition.Y && Coordinate.X < TargetGridPosition.X + PlacedSize.X && Coordinate.Y < TargetGridPosition.Y + PlacedSize.Y;
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

bool UHeistInventoryWidget::IsGridCoordinateOccupied(const FIntPoint& GridCoordinate, const int32 ExcludedInstanceId) const
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
		if (GridCoordinate.X >= InventoryItem.GridPosition.X && GridCoordinate.Y >= InventoryItem.GridPosition.Y && GridCoordinate.X < InventoryItem.GridPosition.X + PlacedSize.X &&
			GridCoordinate.Y < InventoryItem.GridPosition.Y + PlacedSize.Y)
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
