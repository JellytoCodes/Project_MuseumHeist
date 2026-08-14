#include "Core/HeistHUD.h"

#include "Character/Components/HeistActionComponent.h"
#include "Character/Components/HeistForgeryComponent.h"
#include "Character/Components/HeistInventoryComponent.h"
#include "Character/Components/HeistInteractionComponent.h"
#include "Character/Components/HeistObjectAssemblyComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameInstance.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistPlayerState.h"
#include "Core/HeistLogChannels.h"
#include "UI/ViewModels/HeistHUDViewModel.h"
#include "UI/ViewModels/HeistForgeryViewModel.h"
#include "UI/ViewModels/HeistInventoryViewModel.h"
#include "UI/ViewModels/HeistLobbyViewModel.h"
#include "UI/ViewModels/HeistObjectAssemblyViewModel.h"
#include "UI/ViewModels/HeistQuickSlotViewModel.h"
#include "UI/ViewModels/HeistResultViewModel.h"
#include "UI/ViewModels/HeistTitleMenuViewModel.h"
#include "UI/Widgets/HeistHUDWidget.h"
#include "UI/Widgets/HeistForgeryWidget.h"
#include "UI/Widgets/HeistFloorPlanMapWidget.h"
#include "UI/Widgets/HeistInventoryWidget.h"
#include "UI/Widgets/HeistLobbyWidget.h"
#include "UI/Widgets/HeistObjectAssemblyWidget.h"
#include "UI/Widgets/HeistResultWidget.h"
#include "UI/Widgets/HeistTitleMenuWidget.h"

namespace
{
bool HasAttachedLocalPlayer(const APlayerController* PlayerController)
{
	return IsValid(PlayerController) && PlayerController->IsLocalController() && IsValid(PlayerController->GetLocalPlayer());
}
}

#pragma region Construction

AHeistHUD::AHeistHUD()
{
	FloorPlanMapWidgetClass = UHeistFloorPlanMapWidget::StaticClass();
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
	if (!HasAttachedLocalPlayer(GetOwningPlayerController()))
	{
		return false;
	}

	InitializeInventoryPresentation();
	InitializeMainHUDPresentation();
	InitializeForgeryPresentation();
	InitializeObjectAssemblyPresentation();

	if (!IsValid(MainHUDWidget))
	{
		return false;
	}

	MainHUDWidget->SetVisibility(ESlateVisibility::Visible);
	MainHUDWidget->RefreshHUDPresentation();
	return true;
}

void AHeistHUD::HideMainHUD()
{
	HideFloorPlanMap();
	if (IsValid(MainHUDWidget))
	{
		MainHUDWidget->ResetHiddenPresentationState();
		MainHUDWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void AHeistHUD::RefreshPresentationSources()
{
	InitializeInventoryPresentation();
	InitializeMainHUDPresentation();
	InitializeForgeryPresentation();
	InitializeObjectAssemblyPresentation();
	InitializeResultPresentation();
	InitializeTitleMenuPresentation();
	InitializeLobbyPresentation();
	InitializeFloorPlanMapPresentation();
}

#pragma region FloorPlanMapPresentation

bool AHeistHUD::ShowFloorPlanMap()
{
	InitializeFloorPlanMapPresentation();
	if (!IsValid(FloorPlanMapWidget))
	{
		return false;
	}
	FloorPlanMapWidget->SetVisibility(ESlateVisibility::Visible);
	FloorPlanMapWidget->RefreshMapPresentation();
	return true;
}

void AHeistHUD::HideFloorPlanMap()
{
	if (IsValid(FloorPlanMapWidget))
	{
		FloorPlanMapWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool AHeistHUD::IsFloorPlanMapVisible() const
{
	return IsValid(FloorPlanMapWidget) && FloorPlanMapWidget->GetVisibility() != ESlateVisibility::Collapsed && FloorPlanMapWidget->GetVisibility() != ESlateVisibility::Hidden;
}

void AHeistHUD::InitializeFloorPlanMapPresentation()
{
	AHeistPlayerController* HeistPlayerController = Cast<AHeistPlayerController>(GetOwningPlayerController());
	if (!HasAttachedLocalPlayer(HeistPlayerController) || !FloorPlanMapWidgetClass)
	{
		return;
	}
	if (!IsValid(FloorPlanMapWidget))
	{
		FloorPlanMapWidget = CreateWidget<UHeistFloorPlanMapWidget>(HeistPlayerController, FloorPlanMapWidgetClass);
		if (!IsValid(FloorPlanMapWidget))
		{
			return;
		}
		FloorPlanMapWidget->AddToViewport(50);
		FloorPlanMapWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	FloorPlanMapWidget->SetupMap(GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr, HeistPlayerController);
}

#pragma endregion

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
	if (!HasAttachedLocalPlayer(HeistPlayerController))
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

#pragma region TitleMenuPresentation

bool AHeistHUD::ShowTitleMenuScreen()
{
	if (!HasAttachedLocalPlayer(GetOwningPlayerController()))
	{
		return false;
	}

	InitializeTitleMenuPresentation();

	if (!IsValid(TitleMenuViewModel) || !TitleMenuWidgetClass)
	{
		return false;
	}

	if (!IsValid(TitleMenuWidget))
	{
		APlayerController* OwningPlayerController = GetOwningPlayerController();
		if (!IsValid(OwningPlayerController))
		{
			return false;
		}

		TitleMenuWidget = CreateWidget<UHeistTitleMenuWidget>(OwningPlayerController, TitleMenuWidgetClass);
		if (!IsValid(TitleMenuWidget))
		{
			return false;
		}

		TitleMenuWidget->SetupTitleMenuWidget(TitleMenuViewModel);
		TitleMenuWidget->AddToViewport();
	}
	else
	{
		TitleMenuWidget->SetVisibility(ESlateVisibility::Visible);
	}

	TitleMenuViewModel->RefreshTitleMenuData();
	APlayerController* OwningPlayerController = GetOwningPlayerController();
	if (IsValid(OwningPlayerController))
	{
		FInputModeUIOnly InputMode;
		if (TitleMenuWidget->IsFocusable())
		{
			InputMode.SetWidgetToFocus(TitleMenuWidget->TakeWidget());
		}
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		OwningPlayerController->SetInputMode(InputMode);
		OwningPlayerController->SetShowMouseCursor(true);
	}
	return true;
}

void AHeistHUD::HideTitleMenuScreen()
{
	if (IsValid(TitleMenuWidget))
	{
		TitleMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

UHeistTitleMenuViewModel* AHeistHUD::GetTitleMenuViewModel() const
{
	return TitleMenuViewModel;
}

void AHeistHUD::InitializeTitleMenuPresentation()
{
	APlayerController* OwningPlayerController = GetOwningPlayerController();
	if (!HasAttachedLocalPlayer(OwningPlayerController))
	{
		return;
	}

	if (!IsValid(TitleMenuViewModel))
	{
		TitleMenuViewModel = NewObject<UHeistTitleMenuViewModel>(this);
	}

	TitleMenuViewModel->SetupViewModel(Cast<UHeistGameInstance>(GetGameInstance()));
}

#pragma endregion

#pragma region LobbyPresentation

bool AHeistHUD::ShowLobbyScreen()
{
	if (!HasAttachedLocalPlayer(GetOwningPlayerController()))
	{
		return false;
	}

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
	APlayerController* OwningPlayerController = GetOwningPlayerController();
	if (IsValid(OwningPlayerController))
	{
		FInputModeUIOnly InputMode;
		if (LobbyWidget->IsFocusable())
		{
			InputMode.SetWidgetToFocus(LobbyWidget->TakeWidget());
		}
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		OwningPlayerController->SetInputMode(InputMode);
		OwningPlayerController->SetShowMouseCursor(true);
	}
	return true;
}

void AHeistHUD::HideLobbyScreen()
{
	if (IsValid(LobbyWidget))
	{
		LobbyWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	APlayerController* OwningPlayerController = GetOwningPlayerController();
	if (IsValid(OwningPlayerController))
	{
		OwningPlayerController->SetInputMode(FInputModeGameOnly());
		OwningPlayerController->SetShowMouseCursor(false);
	}
}

UHeistLobbyViewModel* AHeistHUD::GetLobbyViewModel() const
{
	return LobbyViewModel;
}

void AHeistHUD::InitializeLobbyPresentation()
{
	APlayerController* OwningPlayerController = GetOwningPlayerController();
	if (!HasAttachedLocalPlayer(OwningPlayerController))
	{
		return;
	}

	if (!IsValid(LobbyViewModel))
	{
		LobbyViewModel = NewObject<UHeistLobbyViewModel>(this);
	}

	AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	AHeistPlayerState* HeistPlayerState = OwningPlayerController->GetPlayerState<AHeistPlayerState>();
	UHeistGameInstance* HeistGameInstance = Cast<UHeistGameInstance>(GetGameInstance());
	LobbyViewModel->SetupViewModel(HeistGameState, HeistPlayerState, HeistGameInstance, Cast<AHeistPlayerController>(OwningPlayerController));
}

#pragma endregion

#pragma region InventoryPresentation

bool AHeistHUD::ShowInventoryScreen()
{
	InitializeInventoryPresentation();
	AHeistPlayerController* HeistPlayerController = Cast<AHeistPlayerController>(GetOwningPlayerController());
	if (!HasAttachedLocalPlayer(HeistPlayerController) || !IsValid(InventoryViewModel) || !IsValid(QuickSlotViewModel) || !InventoryWidgetClass)
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
	if (!HasAttachedLocalPlayer(HeistPlayerController))
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
	if (!HasAttachedLocalPlayer(HeistPlayerController))
	{
		return;
	}

	if (!IsValid(ForgeryViewModel))
	{
		ForgeryViewModel = NewObject<UHeistForgeryViewModel>(this);
	}

	AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	AHeistPlayerCharacter* HeistPlayerCharacter = HeistPlayerController->GetPawn<AHeistPlayerCharacter>();
	UHeistForgeryComponent* ForgeryComponent = IsValid(HeistPlayerCharacter) ? HeistPlayerCharacter->GetForgeryComponent() : nullptr;
	ForgeryViewModel->SetupViewModel(HeistGameState, ForgeryComponent);

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

#pragma region ObjectAssemblyPresentation

UHeistObjectAssemblyViewModel* AHeistHUD::GetObjectAssemblyViewModel() const
{
	return ObjectAssemblyViewModel;
}

UHeistObjectAssemblyWidget* AHeistHUD::GetObjectAssemblyWidget() const
{
	return ObjectAssemblyWidget;
}

void AHeistHUD::InitializeObjectAssemblyPresentation()
{
	AHeistPlayerController* HeistPlayerController = Cast<AHeistPlayerController>(GetOwningPlayerController());
	if (!HasAttachedLocalPlayer(HeistPlayerController))
	{
		return;
	}

	if (!IsValid(ObjectAssemblyViewModel))
	{
		ObjectAssemblyViewModel = NewObject<UHeistObjectAssemblyViewModel>(this);
	}

	AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	AHeistPlayerCharacter* HeistPlayerCharacter = HeistPlayerController->GetPawn<AHeistPlayerCharacter>();
	UHeistObjectAssemblyComponent* ObjectAssemblyComponent =
		IsValid(HeistPlayerCharacter) ? HeistPlayerCharacter->GetObjectAssemblyComponent() : nullptr;
	ObjectAssemblyViewModel->SetupViewModel(HeistGameState, ObjectAssemblyComponent, HeistPlayerController);

	if (!ObjectAssemblyWidgetClass)
	{
		return;
	}

	if (!IsValid(ObjectAssemblyWidget))
	{
		ObjectAssemblyWidget = CreateWidget<UHeistObjectAssemblyWidget>(HeistPlayerController, ObjectAssemblyWidgetClass);
		if (!IsValid(ObjectAssemblyWidget))
		{
			return;
		}

		ObjectAssemblyWidget->AddToViewport(110);
	}

	ObjectAssemblyWidget->SetupObjectAssemblyWidget(ObjectAssemblyViewModel, HeistPlayerController);
}

#pragma endregion

#pragma region ResultPresentation

bool AHeistHUD::ShowResultScreen()
{
	if (!HasAttachedLocalPlayer(GetOwningPlayerController()))
	{
		return false;
	}

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
		ResultWidget->ResetHiddenPresentationState();
		ResultWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

UHeistResultViewModel* AHeistHUD::GetResultViewModel() const
{
	return ResultViewModel;
}

UHeistResultWidget* AHeistHUD::GetResultWidget() const
{
	return ResultWidget;
}

void AHeistHUD::InitializeResultPresentation()
{
	APlayerController* OwningPlayerController = GetOwningPlayerController();
	if (!HasAttachedLocalPlayer(OwningPlayerController))
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
