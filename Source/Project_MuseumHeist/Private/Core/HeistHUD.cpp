#include "Core/HeistHUD.h"

#include "Core/HeistGameState.h"
#include "Core/HeistLogChannels.h"
#include "Core/HeistPlayerState.h"
#include "UI/ViewModels/HeistResultViewModel.h"
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
	InitializeResultPresentation();
}

#pragma endregion

#pragma region ResultPresentation

bool AHeistHUD::ShowResultScreen()
{
	InitializeResultPresentation();

	if (!IsValid(ResultViewModel) || !ResultWidgetClass)
	{
		UE_LOG(
			LogHeistUI,
			Warning,
			TEXT("Result screen show skipped: HUD=%s ViewModel=%s WidgetClass=%s"),
			*GetNameSafe(this),
			*GetNameSafe(ResultViewModel),
			*GetNameSafe(ResultWidgetClass));
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
