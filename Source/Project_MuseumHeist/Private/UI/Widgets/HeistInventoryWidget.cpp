#include "UI/Widgets/HeistInventoryWidget.h"

#include "Core/HeistPlayerController.h"
#include "MVVMBlueprintLibrary.h"
#include "UI/ViewModels/HeistInventoryViewModel.h"
#include "UI/ViewModels/HeistQuickSlotViewModel.h"

UHeistInventoryWidget::UHeistInventoryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UHeistInventoryWidget::NativeDestruct()
{
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
	UMVVMBlueprintLibrary::SetViewModelByClass(this, InventoryViewModelInterface);

	TScriptInterface<INotifyFieldValueChanged> QuickSlotViewModelInterface;
	QuickSlotViewModelInterface.SetObject(QuickSlotViewModel);
	QuickSlotViewModelInterface.SetInterface(QuickSlotViewModel);
	UMVVMBlueprintLibrary::SetViewModelByClass(this, QuickSlotViewModelInterface);
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
		BP_RefreshConfirmedInventory(
			InventoryViewModel->GetItems(),
			InventoryViewModel->GetGridColumnCount(),
			InventoryViewModel->GetGridRowCount());
	}
}

void UHeistInventoryWidget::RefreshQuickSlotPresentation()
{
	if (IsValid(QuickSlotViewModel))
	{
		BP_RefreshConfirmedQuickSlots(QuickSlotViewModel->GetQuickSlots());
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
