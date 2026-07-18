#include "UI/Widgets/HeistForgeryWidget.h"

#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Widget.h"
#include "Core/HeistLogChannels.h"
#include "GameFramework/PlayerController.h"
#include "UI/ViewModels/HeistForgeryViewModel.h"

UHeistForgeryWidget::UHeistForgeryWidget(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UHeistForgeryWidget::NativeDestruct()
{
	if (IsValid(ForgeryViewModel))
	{
		ForgeryViewModel->GetPresentationChangedDelegate().RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UHeistForgeryWidget::SetupForgeryWidget(
	UHeistForgeryViewModel* InForgeryViewModel)
{
	checkf(
		IsValid(InForgeryViewModel),
		TEXT("HeistForgeryWidget requires a valid Forgery ViewModel."));

	if (ForgeryViewModel != InForgeryViewModel
		&& IsValid(ForgeryViewModel))
	{
		ForgeryViewModel->GetPresentationChangedDelegate().RemoveAll(this);
	}

	ForgeryViewModel = InForgeryViewModel;
	ForgeryViewModel->GetPresentationChangedDelegate().RemoveAll(this);
	ForgeryViewModel->GetPresentationChangedDelegate().AddUObject(
		this,
		&UHeistForgeryWidget::RefreshForgeryPresentation);

	BP_OnForgerySourcesReady();
	RefreshForgeryPresentation();
}

bool UHeistForgeryWidget::IsOwnerOnlyContractSatisfied() const
{
	const APlayerController* OwningPlayerController = GetOwningPlayer();
	return IsValid(OwningPlayerController)
		&& OwningPlayerController->IsLocalController()
		&& IsValid(ForgeryViewModel)
		&& (!ForgeryViewModel->IsPresentationVisible()
			|| ForgeryViewModel->GetVisibleStateCount() == 1);
}

bool UHeistForgeryWidget::IsWidgetPresentationVisible() const
{
	return GetVisibility() != ESlateVisibility::Collapsed
		&& GetVisibility() != ESlateVisibility::Hidden;
}

void UHeistForgeryWidget::RefreshForgeryPresentation()
{
	const APlayerController* OwningPlayerController = GetOwningPlayer();
	const bool bOwnerLocal = IsValid(OwningPlayerController)
		&& OwningPlayerController->IsLocalController();
	const bool bPresentationVisible = bOwnerLocal
		&& IsValid(ForgeryViewModel)
		&& ForgeryViewModel->IsPresentationVisible();

	SetVisibility(
		bPresentationVisible
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);

	const bool bObservation = bPresentationVisible
		&& ForgeryViewModel->IsObservationVisible();
	const bool bDrawing = bPresentationVisible
		&& ForgeryViewModel->IsDrawingVisible();
	const bool bValidation = bPresentationVisible
		&& ForgeryViewModel->IsValidationVisible();
	const bool bResult = bPresentationVisible
		&& ForgeryViewModel->IsResultVisible();

	ApplyStateVisibility(ObservationContainer, bObservation);
	ApplyStateVisibility(DrawingContainer, bDrawing);
	ApplyStateVisibility(ValidationContainer, bValidation);
	ApplyStateVisibility(ResultContainer, bResult);
	ApplyStateVisibility(ReferenceImage, bObservation || bDrawing);

	if (IsValid(StateText))
	{
		StateText->SetText(ForgeryViewModel->GetStateText());
	}
	if (IsValid(ReferenceText))
	{
		ReferenceText->SetText(ForgeryViewModel->GetReferenceText());
	}
	if (IsValid(ReferenceImage))
	{
		ReferenceImage->SetBrushFromTexture(
			ForgeryViewModel->GetReferenceImage(),
			true);
	}
	if (IsValid(ResultText))
	{
		ResultText->SetText(ForgeryViewModel->GetResultText());
	}
	if (IsValid(ResultScoreText))
	{
		ResultScoreText->SetText(
			FText::AsNumber(FMath::RoundToInt(ForgeryViewModel->GetResultScore())));
	}

	BP_RefreshForgeryPresentation(
		bObservation,
		bDrawing,
		bValidation,
		bResult,
		IsValid(ForgeryViewModel)
			? ForgeryViewModel->GetStateEndServerTime()
			: 0.0f,
		IsValid(ForgeryViewModel)
			? ForgeryViewModel->GetResultScore()
			: 0.0f);

	UE_LOG(
		LogHeistUI,
		Verbose,
		TEXT("[%s] Forgery widget refreshed: LocalOwner=%s Visible=%s Observation=%s Drawing=%s Validation=%s Result=%s StateCount=%d Contract=%s"),
		*GetName(),
		bOwnerLocal ? TEXT("true") : TEXT("false"),
		bPresentationVisible ? TEXT("true") : TEXT("false"),
		bObservation ? TEXT("true") : TEXT("false"),
		bDrawing ? TEXT("true") : TEXT("false"),
		bValidation ? TEXT("true") : TEXT("false"),
		bResult ? TEXT("true") : TEXT("false"),
		IsValid(ForgeryViewModel)
			? ForgeryViewModel->GetVisibleStateCount()
			: 0,
		IsOwnerOnlyContractSatisfied() ? TEXT("PASS") : TEXT("FAIL"));
}

void UHeistForgeryWidget::ApplyStateVisibility(
	UWidget* TargetWidget,
	const bool bVisible) const
{
	if (IsValid(TargetWidget))
	{
		TargetWidget->SetVisibility(
			bVisible
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}
}
