#include "Core/HeistHUD.h"

#include "Character/Components/HeistActionComponent.h"
#include "Character/Components/HeistForgeryComponent.h"
#include "Character/Components/HeistInventoryComponent.h"
#include "Character/Components/HeistInteractionComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistPlayerState.h"
#include "Core/HeistLogChannels.h"
#include "UI/ViewModels/HeistHUDViewModel.h"
#include "UI/ViewModels/HeistForgeryViewModel.h"
#include "UI/ViewModels/HeistInventoryViewModel.h"
#include "UI/ViewModels/HeistLobbyViewModel.h"
#include "UI/ViewModels/HeistQuickSlotViewModel.h"
#include "UI/ViewModels/HeistResultViewModel.h"
#include "UI/Widgets/HeistHUDWidget.h"
#include "UI/Widgets/HeistForgeryWidget.h"
#include "UI/Widgets/HeistInventoryWidget.h"
#include "UI/Widgets/HeistLobbyWidget.h"
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
	RefreshPresentationSources();
}

#pragma endregion

#pragma region MainHUDPresentation

bool AHeistHUD::ShowMainHUD()
{
	InitializeInventoryPresentation();
	InitializeMainHUDPresentation();
	InitializeForgeryPresentation();

	if (!IsValid(MainHUDWidget))
	{
		return false;
	}

	MainHUDWidget->SetVisibility(ESlateVisibility::Visible);
	return true;
}

void AHeistHUD::HideMainHUD()
{
	if (IsValid(MainHUDWidget))
	{
		MainHUDWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void AHeistHUD::RefreshPresentationSources()
{
	InitializeInventoryPresentation();
	InitializeMainHUDPresentation();
	InitializeForgeryPresentation();
	InitializeResultPresentation();
	InitializeLobbyPresentation();
}

UHeistHUDViewModel* AHeistHUD::GetHUDViewModel() const
{
	return HUDViewModel;
}

UHeistHUDWidget* AHeistHUD::GetMainHUDWidget() const
{
	return MainHUDWidget;
}

void AHeistHUD::InitializeMainHUDPresentation()
{
	AHeistPlayerController* HeistPlayerController = Cast<AHeistPlayerController>(GetOwningPlayerController());
	if (!IsValid(HeistPlayerController) || !HeistPlayerController->IsLocalController())
	{
		return;
	}

	if (!IsValid(HUDViewModel))
	{
		HUDViewModel = NewObject<UHeistHUDViewModel>(this);
	}

	AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	AHeistPlayerState* HeistPlayerState = HeistPlayerController->GetPlayerState<AHeistPlayerState>();
	AHeistPlayerCharacter* HeistPlayerCharacter = HeistPlayerController->GetPawn<AHeistPlayerCharacter>();
	UHeistActionComponent* ActionComponent = IsValid(HeistPlayerCharacter) ? HeistPlayerCharacter->GetActionComponent() : nullptr;
	UHeistInteractionComponent* InteractionComponent = IsValid(HeistPlayerCharacter) ? HeistPlayerCharacter->GetInteractionComponent() : nullptr;
	HUDViewModel->SetupViewModel(HeistGameState, HeistPlayerState, ActionComponent);

	if (!MainHUDWidgetClass)
	{
		return;
	}

	if (!IsValid(MainHUDWidget))
	{
		MainHUDWidget = CreateWidget<UHeistHUDWidget>(HeistPlayerController, MainHUDWidgetClass);
		if (!IsValid(MainHUDWidget))
		{
			return;
		}

		MainHUDWidget->AddToViewport();
	}

	MainHUDWidget->SetupHUDWidget(HUDViewModel, InventoryViewModel, QuickSlotViewModel, InteractionComponent);
}

#pragma endregion

#pragma region LobbyPresentation

bool AHeistHUD::ShowLobbyScreen()
{
	InitializeLobbyPresentation();

	if (!IsValid(LobbyViewModel) || !LobbyWidgetClass)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogHeistUI, Warning, TEXT("Lobby screen show skipped: HUD=%s ViewModel=%s WidgetClass=%s"), *GetNameSafe(this), *GetNameSafe(LobbyViewModel), *GetNameSafe(LobbyWidgetClass));
#endif
		return false;
	}

	if (!IsValid(LobbyWidget))
	{
		APlayerController* OwningPlayerController = GetOwningPlayerController();
		if (!IsValid(OwningPlayerController))
		{
			return false;
		}

		LobbyWidget = CreateWidget<UHeistLobbyWidget>(OwningPlayerController, LobbyWidgetClass);
		if (!IsValid(LobbyWidget))
		{
			return false;
		}

		LobbyWidget->SetupLobbyWidget(LobbyViewModel);
		LobbyWidget->AddToViewport();
	}
	else
	{
		LobbyWidget->SetVisibility(ESlateVisibility::Visible);
	}

	LobbyViewModel->RefreshLobbyData();
	return true;
}

void AHeistHUD::HideLobbyScreen()
{
	if (IsValid(LobbyWidget))
	{
		LobbyWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

UHeistLobbyViewModel* AHeistHUD::GetLobbyViewModel() const
{
	return LobbyViewModel;
}

void AHeistHUD::InitializeLobbyPresentation()
{
	APlayerController* OwningPlayerController = GetOwningPlayerController();
	if (!IsValid(OwningPlayerController) || !OwningPlayerController->IsLocalController())
	{
		return;
	}

	if (!IsValid(LobbyViewModel))
	{
		LobbyViewModel = NewObject<UHeistLobbyViewModel>(this);
	}

	AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	AHeistPlayerState* HeistPlayerState = OwningPlayerController->GetPlayerState<AHeistPlayerState>();
	LobbyViewModel->SetupViewModel(HeistGameState, HeistPlayerState);
}

#pragma endregion

#pragma region InventoryPresentation

bool AHeistHUD::ShowInventoryScreen()
{
	InitializeInventoryPresentation();
	AHeistPlayerController* HeistPlayerController = Cast<AHeistPlayerController>(GetOwningPlayerController());
	if (!IsValid(HeistPlayerController) || !IsValid(InventoryViewModel) || !IsValid(QuickSlotViewModel) || !InventoryWidgetClass)
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
	UHeistInventoryComponent* InventoryComponent = IsValid(HeistPlayerCharacter) ? HeistPlayerCharacter->GetInventoryComponent() : nullptr;
	InventoryViewModel->SetupViewModel(InventoryComponent);
	QuickSlotViewModel->SetupViewModel(InventoryComponent);
}

#pragma endregion

#pragma region ForgeryPresentation

UHeistForgeryViewModel* AHeistHUD::GetForgeryViewModel() const
{
	return ForgeryViewModel;
}

UHeistForgeryWidget* AHeistHUD::GetForgeryWidget() const
{
	return ForgeryWidget;
}

void AHeistHUD::InitializeForgeryPresentation()
{
	AHeistPlayerController* HeistPlayerController = Cast<AHeistPlayerController>(GetOwningPlayerController());
	if (!IsValid(HeistPlayerController) || !HeistPlayerController->IsLocalController())
	{
		return;
	}

	if (!IsValid(ForgeryViewModel))
	{
		ForgeryViewModel = NewObject<UHeistForgeryViewModel>(this);
	}

	AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	AHeistPlayerCharacter* HeistPlayerCharacter = HeistPlayerController->GetPawn<AHeistPlayerCharacter>();
	UHeistActionComponent* ActionComponent = IsValid(HeistPlayerCharacter) ? HeistPlayerCharacter->GetActionComponent() : nullptr;
	UHeistForgeryComponent* ForgeryComponent = IsValid(HeistPlayerCharacter) ? HeistPlayerCharacter->GetForgeryComponent() : nullptr;
	ForgeryViewModel->SetupViewModel(HeistGameState, ActionComponent, ForgeryComponent);

	if (!ForgeryWidgetClass)
	{
		return;
	}

	if (!IsValid(ForgeryWidget))
	{
		ForgeryWidget = CreateWidget<UHeistForgeryWidget>(HeistPlayerController, ForgeryWidgetClass);
		if (!IsValid(ForgeryWidget))
		{
			return;
		}

		ForgeryWidget->AddToViewport(100);
	}

	ForgeryWidget->SetupForgeryWidget(ForgeryViewModel);
}

#pragma endregion

#pragma region ResultPresentation

bool AHeistHUD::ShowResultScreen()
{
	InitializeResultPresentation();

	if (!IsValid(ResultViewModel) || !ResultWidgetClass)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogHeistUI, Warning, TEXT("Result screen show skipped: HUD=%s ViewModel=%s WidgetClass=%s"), *GetNameSafe(this), *GetNameSafe(ResultViewModel), *GetNameSafe(ResultWidgetClass));
#endif
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
