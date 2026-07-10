#include "UI/Widgets/HeistGapTrackerWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Core/HeistLogChannels.h"
#include "UI/ViewModels/HeistGapTrackerViewModel.h"

#pragma region Construction

UHeistGapTrackerWidget::UHeistGapTrackerWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#pragma endregion

#pragma region Lifecycle

void UHeistGapTrackerWidget::NativeDestruct()
{
	if (IsValid(GapTrackerViewModel))
	{
		GapTrackerViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	}

	Super::NativeDestruct();
}

#pragma endregion

#pragma region ViewModel

void UHeistGapTrackerWidget::SetupGapTrackerWidget(UHeistGapTrackerViewModel* InGapTrackerViewModel)
{
	checkf(IsValid(InGapTrackerViewModel), TEXT("HeistGapTrackerWidget requires a valid Gap Tracker ViewModel."));

	if (GapTrackerViewModel != InGapTrackerViewModel && IsValid(GapTrackerViewModel))
	{
		GapTrackerViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	}

	GapTrackerViewModel = InGapTrackerViewModel;
	GapTrackerViewModel->GetSnapshotChangedDelegate().RemoveAll(this);
	GapTrackerViewModel->GetSnapshotChangedDelegate().AddUObject(
		this,
		&UHeistGapTrackerWidget::RefreshGapTrackerPresentation);

	RefreshGapTrackerPresentation();
}

void UHeistGapTrackerWidget::RefreshGapTrackerPresentation()
{
	const bool bActive = IsValid(GapTrackerViewModel) && GapTrackerViewModel->IsGapTrackerActive();
	const bool bShowDirectionArrow = IsValid(GapTrackerViewModel) && GapTrackerViewModel->ShouldShowDirectionArrow();
	const bool bShowLeaderWarning = IsValid(GapTrackerViewModel) && GapTrackerViewModel->ShouldShowLeaderWarning();
	const float DirectionAngleDegrees = IsValid(GapTrackerViewModel)
		? GapTrackerViewModel->GetDirectionAngleDegrees()
		: 0.0f;

	SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (IsValid(GapTrackerContainer))
	{
		GapTrackerContainer->SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (IsValid(GapTrackerDirectionArrow))
	{
		GapTrackerDirectionArrow->SetVisibility(bShowDirectionArrow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		GapTrackerDirectionArrow->SetRenderTransformAngle(DirectionAngleDegrees);
	}

	if (IsValid(GapTrackerLeaderWarningContainer))
	{
		GapTrackerLeaderWarningContainer->SetVisibility(bShowLeaderWarning ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (IsValid(GapTrackerText))
	{
		const FText GapTrackerLabel = bShowLeaderWarning
			? NSLOCTEXT("HeistGapTracker", "LeaderWarning", "GAP TRACKER  YOU ARE LEADING")
			: bShowDirectionArrow
				? FText::Format(
					NSLOCTEXT("HeistGapTracker", "DirectionFormat", "GAP TRACKER  LEADER {0} DEG"),
					FText::AsNumber(FMath::RoundToInt(DirectionAngleDegrees)))
				: NSLOCTEXT("HeistGapTracker", "Inactive", "GAP TRACKER  --");
		GapTrackerText->SetText(GapTrackerLabel);
	}

	UE_LOG(
		LogHeistUI,
		Log,
		TEXT("[%s] Gap Tracker presentation refreshed: Active=%s DirectionArrow=%s LeaderWarning=%s Angle=%.1f"),
		*GetName(),
		bActive ? TEXT("true") : TEXT("false"),
		bShowDirectionArrow ? TEXT("true") : TEXT("false"),
		bShowLeaderWarning ? TEXT("true") : TEXT("false"),
		DirectionAngleDegrees);
}

#pragma endregion
