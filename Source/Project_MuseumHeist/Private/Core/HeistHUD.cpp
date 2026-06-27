#include "Core/HeistHUD.h"

#include "Character/Components/HeistInventoryComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistPlayerState.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "UI/ViewModels/HeistGapTrackerViewModel.h"
#include "UI/ViewModels/HeistHUDViewModel.h"
#include "UI/ViewModels/HeistInventoryViewModel.h"
#include "UI/ViewModels/HeistQuickSlotViewModel.h"
#include "UI/ViewModels/HeistResultViewModel.h"
#include "UI/Widgets/HeistInventoryWidget.h"
#include "UI/Widgets/HeistRareLootAlertWidget.h"
#include "UI/Widgets/HeistResultWidget.h"

#pragma region Construction

AHeistHUD::AHeistHUD()
{
}

#pragma endregion

#pragma region Lifecycle

void AHeistHUD::BeginPlay()
{
	Super::BeginPlay();
	InitializeInventoryPresentation();
	InitializeRareLootPresentation();
	InitializeGapTrackerPresentation();
	InitializeResultPresentation();
}

#pragma endregion

#pragma region InventoryPresentation

bool AHeistHUD::ShowInventoryScreen()
{
	InitializeInventoryPresentation();
	AHeistPlayerController* HeistPlayerController = Cast<AHeistPlayerController>(GetOwningPlayerController());
	if (!IsValid(HeistPlayerController)
		|| !IsValid(InventoryViewModel)
		|| !IsValid(QuickSlotViewModel)
		|| !InventoryWidgetClass)
	{
		return false;
	}

	if (!IsValid(InventoryWidget))
	{
		InventoryWidget = CreateWidget<UHeistInventoryWidget>(HeistPlayerController, InventoryWidgetClass);
		if (!IsValid(InventoryWidget))
		{
			return false;
		}

		InventoryWidget->SetupInventoryWidget(InventoryViewModel, QuickSlotViewModel, HeistPlayerController);
		InventoryWidget->AddToViewport();
	}

	return true;
}

UHeistInventoryViewModel* AHeistHUD::GetInventoryViewModel() const
{
	return InventoryViewModel;
}

UHeistQuickSlotViewModel* AHeistHUD::GetQuickSlotViewModel() const
{
	return QuickSlotViewModel;
}

void AHeistHUD::InitializeInventoryPresentation()
{
	AHeistPlayerController* HeistPlayerController = Cast<AHeistPlayerController>(GetOwningPlayerController());
	if (!IsValid(HeistPlayerController) || !HeistPlayerController->IsLocalController())
	{
		return;
	}

	if (!IsValid(InventoryViewModel))
	{
		InventoryViewModel = NewObject<UHeistInventoryViewModel>(this);
	}

	if (!IsValid(QuickSlotViewModel))
	{
		QuickSlotViewModel = NewObject<UHeistQuickSlotViewModel>(this);
	}

	AHeistPlayerCharacter* HeistPlayerCharacter = HeistPlayerController->GetPawn<AHeistPlayerCharacter>();
	UHeistInventoryComponent* InventoryComponent = IsValid(HeistPlayerCharacter)
		? HeistPlayerCharacter->GetInventoryComponent()
		: nullptr;
	InventoryViewModel->SetupViewModel(InventoryComponent);
	QuickSlotViewModel->SetupViewModel(InventoryComponent);
}

#pragma endregion

#pragma region RareLootPresentation

UHeistHUDViewModel* AHeistHUD::GetHUDViewModel() const
{
	return HUDViewModel;
}

void AHeistHUD::InitializeRareLootPresentation()
{
	APlayerController* OwningPlayerController = GetOwningPlayerController();
	if (!IsValid(OwningPlayerController) || !OwningPlayerController->IsLocalController())
	{
		return;
	}

	if (!IsValid(HUDViewModel))
	{
		HUDViewModel = NewObject<UHeistHUDViewModel>(this);
	}

	AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	HUDViewModel->SetupViewModel(HeistGameState);

	if (!RareLootAlertWidgetClass || IsValid(RareLootAlertWidget))
	{
		return;
	}

	RareLootAlertWidget = CreateWidget<UHeistRareLootAlertWidget>(
		OwningPlayerController,
		RareLootAlertWidgetClass);
	if (IsValid(RareLootAlertWidget))
	{
		RareLootAlertWidget->SetupRareLootAlertWidget(HUDViewModel);
		RareLootAlertWidget->AddToViewport();
	}
}

#pragma endregion

#pragma region GapTrackerPresentation

UHeistGapTrackerViewModel* AHeistHUD::GetGapTrackerViewModel() const
{
	return GapTrackerViewModel;
}

void AHeistHUD::InitializeGapTrackerPresentation()
{
	APlayerController* OwningPlayerController = GetOwningPlayerController();
	if (!IsValid(OwningPlayerController) || !OwningPlayerController->IsLocalController())
	{
		return;
	}

	if (!IsValid(GapTrackerViewModel))
	{
		GapTrackerViewModel = NewObject<UHeistGapTrackerViewModel>(this);
	}

	AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	AHeistPlayerState* LocalPlayerState = OwningPlayerController->GetPlayerState<AHeistPlayerState>();
	GapTrackerViewModel->SetupViewModel(HeistGameState, LocalPlayerState);
}

#pragma endregion

#pragma region ResultPresentation

bool AHeistHUD::ShowResultScreen()
{
	InitializeResultPresentation();

	if (!IsValid(ResultViewModel) || !ResultWidgetClass)
	{
		UHeistDebugFunctionLibrary::DebugResultScreenShowSkipped(this, ResultViewModel, ResultWidgetClass);
		return false;
	}

	if (!IsValid(ResultWidget))
	{
		APlayerController* OwningPlayerController = GetOwningPlayerController();
		if (!IsValid(OwningPlayerController))
		{
			return false;
		}

		ResultWidget = CreateWidget<UHeistResultWidget>(OwningPlayerController, ResultWidgetClass);
		if (!IsValid(ResultWidget))
		{
			return false;
		}

		ResultWidget->SetupResultWidget(ResultViewModel);
		ResultWidget->AddToViewport();
	}
	else
	{
		ResultWidget->SetVisibility(ESlateVisibility::Visible);
	}

	ResultViewModel->RefreshResultData();
	return true;
}

void AHeistHUD::HideResultScreen()
{
	if (IsValid(ResultWidget))
	{
		ResultWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

UHeistResultViewModel* AHeistHUD::GetResultViewModel() const
{
	return ResultViewModel;
}

void AHeistHUD::InitializeResultPresentation()
{
	APlayerController* OwningPlayerController = GetOwningPlayerController();
	if (!IsValid(OwningPlayerController) || !OwningPlayerController->IsLocalController())
	{
		return;
	}

	if (!IsValid(ResultViewModel))
	{
		ResultViewModel = NewObject<UHeistResultViewModel>(this);
	}

	AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	AHeistPlayerState* HeistPlayerState = OwningPlayerController->GetPlayerState<AHeistPlayerState>();
	ResultViewModel->SetupViewModel(HeistGameState, HeistPlayerState);
}

#pragma endregion
