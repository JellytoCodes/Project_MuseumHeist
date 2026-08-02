#include "UI/Widgets/HeistRareLootAlertWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Core/HeistLogChannels.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "UI/ViewModels/HeistHUDViewModel.h"

#pragma region Construction

UHeistRareLootAlertWidget::UHeistRareLootAlertWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
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

	if (!IsValid(ViewModel))
	{
		return;
	}

	if (bShowIncomingWarning && ViewModel->IsRareLootIncoming())
	{
		RefreshWarningCountdownText();
	}

	if (bShowDirectionMarker && ViewModel->IsRareLootDirectionMarkerVisible())
	{
		RefreshDirectionMarkerPresentation();
	}
}

#pragma endregion

#pragma region ViewModel

void UHeistRareLootAlertWidget::SetupRareLootAlertWidget(UHeistHUDViewModel* InViewModel)
{
	checkf(IsValid(InViewModel), TEXT("HeistRareLootAlertWidget requires a valid HUD ViewModel."));

	ViewModel = InViewModel;
	ViewModel->GetRareLootPresentationChangedDelegate().RemoveAll(this);
	ViewModel->GetRareLootPresentationChangedDelegate().AddUObject(this, &UHeistRareLootAlertWidget::RefreshRareLootPresentation);

	RefreshRareLootPresentation();
}

UHeistHUDViewModel* UHeistRareLootAlertWidget::GetHUDViewModel() const
{
	return ViewModel;
}

void UHeistRareLootAlertWidget::RefreshRareLootPresentation()
{
	const bool bIncoming = IsValid(ViewModel) && ViewModel->IsRareLootIncoming();
	const bool bMarkerActive = IsValid(ViewModel) && ViewModel->IsRareLootDirectionMarkerVisible();
	const bool bShowWidget = (bShowIncomingWarning && bIncoming) || (bShowDirectionMarker && bMarkerActive);
	SetVisibility(bShowWidget ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (IsValid(IncomingWarningContainer))
	{
		IncomingWarningContainer->SetVisibility(bShowIncomingWarning && bIncoming ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (IsValid(DirectionMarkerContainer))
	{
		DirectionMarkerContainer->SetVisibility(bShowDirectionMarker && bMarkerActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (IsValid(RareLootItemText))
	{
		FString RareLootDisplayName = IsValid(ViewModel) ? ViewModel->GetRareLootItemId().ToString() : FString();
		RareLootDisplayName.ReplaceInline(TEXT("_"), TEXT(" "));
		RareLootItemText->SetText(IsValid(ViewModel) ? FText::Format(NSLOCTEXT("HeistRareLootAlert", "RareLootItemFormat", "희귀 전리품  {0}"),
																   FText::FromString(RareLootDisplayName))
													: FText::GetEmpty());
	}

	if (IsValid(RareLootStatusText))
	{
		RareLootStatusText->SetText(bIncoming		? NSLOCTEXT("HeistRareLootAlert", "IncomingStatus", "등장 예정")
									: bMarkerActive ? NSLOCTEXT("HeistRareLootAlert", "MarkerStatus", "위치 표시됨") : FText::GetEmpty());
	}

	RefreshWarningCountdownText();
	RefreshDirectionMarkerPresentation();

	UE_LOG(LogHeistUI, Verbose, TEXT("[%s] Rare Loot alert presentation refreshed: Incoming=%s MarkerActive=%s ShowIncoming=%s ShowMarker=%s EventIndex=%d ItemId=%s Visibility=%s"),
		   *GetNameSafe(this), bIncoming ? TEXT("true") : TEXT("false"), bMarkerActive ? TEXT("true") : TEXT("false"), bShowIncomingWarning ? TEXT("true") : TEXT("false"),
		   bShowDirectionMarker ? TEXT("true") : TEXT("false"), IsValid(ViewModel) ? ViewModel->GetRareLootEventIndex() : 0,
		   IsValid(ViewModel) ? *ViewModel->GetRareLootItemId().ToString() : TEXT("None"), *UEnum::GetValueAsString(GetVisibility()));
}

void UHeistRareLootAlertWidget::RefreshWarningCountdownText()
{
	if (!IsValid(RareLootCountdownText))
	{
		return;
	}

	if (!IsValid(ViewModel) || !ViewModel->IsRareLootIncoming())
	{
		RareLootCountdownText->SetText(FText::GetEmpty());
		return;
	}

	RareLootCountdownText->SetText(FText::Format(NSLOCTEXT("HeistRareLootAlert", "CountdownFormat", "{0}초 후 등장"),
												FText::AsNumber(FMath::CeilToInt(GetRareLootWarningRemainingSeconds()))));
}

void UHeistRareLootAlertWidget::RefreshDirectionMarkerPresentation()
{
	if (!IsValid(ViewModel) || !ViewModel->IsRareLootDirectionMarkerVisible())
	{
		if (IsValid(DirectionMarkerText))
		{
			DirectionMarkerText->SetText(FText::GetEmpty());
		}
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

	if (IsValid(DirectionMarkerArrow))
	{
		DirectionMarkerArrow->SetRenderTransformAngle(AngleDegrees);
	}

	if (IsValid(DirectionMarkerText))
	{
		DirectionMarkerText->SetText(FText::Format(NSLOCTEXT("HeistRareLootAlert", "DirectionFormat", "{0}° 회전"),
												 FText::AsNumber(FMath::RoundToInt(AngleDegrees))));
	}
}

float UHeistRareLootAlertWidget::GetRareLootWarningRemainingSeconds() const
{
	if (!IsValid(ViewModel))
	{
		return 0.0f;
	}

	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = IsValid(World) ? World->GetGameState() : nullptr;
	const float ServerTimeSeconds = IsValid(GameState) ? GameState->GetServerWorldTimeSeconds() : (IsValid(World) ? World->GetTimeSeconds() : 0.0f);

	return FMath::Max(0.0f, ViewModel->GetRareLootSpawnServerTime() - ServerTimeSeconds);
}

#pragma endregion
