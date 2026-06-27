#include "UI/Widgets/HeistRareLootAlertWidget.h"

#include "Debug/HeistDebugFunctionLibrary.h"
#include "GameFramework/Pawn.h"
#include "UI/ViewModels/HeistHUDViewModel.h"
#include "View/MVVMView.h"

#pragma region Construction

UHeistRareLootAlertWidget::UHeistRareLootAlertWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#pragma endregion

#pragma region Lifecycle

void UHeistRareLootAlertWidget::NativeDestruct()
{
	if (IsValid(ViewModel))
	{
		ViewModel->GetRareLootPresentationChangedDelegate().RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UHeistRareLootAlertWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!IsValid(ViewModel) || !ViewModel->IsRareLootDirectionMarkerVisible())
	{
		return;
	}

	const APawn* OwningPawn = GetOwningPlayerPawn();
	if (!IsValid(OwningPawn))
	{
		return;
	}

	const FVector Delta = ViewModel->GetRareLootWorldLocation() - OwningPawn->GetActorLocation();
	const FVector2D Direction = FVector2D(Delta.X, Delta.Y).GetSafeNormal();
	const float AngleDegrees = FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
	BP_UpdateRareLootDirectionMarker(Direction, AngleDegrees);
}

#pragma endregion

#pragma region ViewModel

void UHeistRareLootAlertWidget::SetupRareLootAlertWidget(UHeistHUDViewModel* InViewModel)
{
	checkf(IsValid(InViewModel), TEXT("HeistRareLootAlertWidget requires a valid HUD ViewModel."));

	ViewModel = InViewModel;
	ViewModel->GetRareLootPresentationChangedDelegate().RemoveAll(this);
	ViewModel->GetRareLootPresentationChangedDelegate().AddUObject(
		this,
		&UHeistRareLootAlertWidget::RefreshRareLootPresentation);

	TScriptInterface<INotifyFieldValueChanged> ViewModelInterface;
	ViewModelInterface.SetObject(ViewModel);
	ViewModelInterface.SetInterface(ViewModel);
	if (UMVVMView* MVVMView = GetExtension<UMVVMView>())
	{
		MVVMView->SetViewModelByClass(ViewModelInterface);
	}
	else
	{
		UHeistDebugFunctionLibrary::DebugWidgetMissingMVVMView(this, TEXT("RareLootAlert"));
	}

	RefreshRareLootPresentation();
}

void UHeistRareLootAlertWidget::RefreshRareLootPresentation()
{
	const bool bIncoming = IsValid(ViewModel) && ViewModel->IsRareLootIncoming();
	const bool bMarkerActive = IsValid(ViewModel) && ViewModel->IsRareLootDirectionMarkerVisible();
	SetVisibility(bIncoming || bMarkerActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (IsValid(ViewModel))
	{
		BP_RefreshRareLootPresentation(
			bIncoming,
			bMarkerActive,
			ViewModel->GetRareLootEventIndex(),
			ViewModel->GetRareLootItemId(),
			ViewModel->GetRareLootSpawnServerTime());
	}
}

#pragma endregion
