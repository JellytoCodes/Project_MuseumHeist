#include "UI/Widgets/HeistNameplateWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Blueprint/WidgetTree.h"
#include "Core/HeistPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
FSlateFontInfo MakeNameplateTenadaFont(const int32 Size)
{
	static UObject* TenadaFont = LoadObject<UObject>(nullptr, TEXT("/Game/Blueprints/UI/Fonts/F_TENADA.F_TENADA"));
	return FSlateFontInfo(TenadaFont, Size);
}
}

TSharedRef<SWidget> UHeistNameplateWidget::RebuildWidget()
{
	if (IsValid(WidgetTree) && !IsValid(WidgetTree->RootWidget))
	{
		UBorder* RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("NameplateBorder"));
		RootBorder->SetBrushColor(FLinearColor(0.01f, 0.02f, 0.04f, 0.72f));
		RootBorder->SetPadding(FMargin(8.0f, 3.0f));
		UHorizontalBox* ContentRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("NameplateContentRow"));
		CrewStatusBadge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CrewStatusBadge"));
		CrewStatusBadge->SetPadding(FMargin(6.0f, 2.0f));
		CrewStatusIconText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CrewStatusIconText"));
		CrewStatusIconText->SetJustification(ETextJustify::Center);
		CrewStatusIconText->SetFont(MakeNameplateTenadaFont(18));
		CrewStatusIconText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		CrewStatusBadge->SetContent(CrewStatusIconText);
		UVerticalBox* TextColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("NameplateTextColumn"));
		PlayerNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PlayerNameText"));
		CrewStatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CrewStatusText"));
		PlayerNameText->SetJustification(ETextJustify::Center);
		CrewStatusText->SetJustification(ETextJustify::Center);
		PlayerNameText->SetFont(MakeNameplateTenadaFont(18));
		CrewStatusText->SetFont(MakeNameplateTenadaFont(14));
		TextColumn->AddChildToVerticalBox(PlayerNameText);
		TextColumn->AddChildToVerticalBox(CrewStatusText);
		if (UHorizontalBoxSlot* BadgeSlot = ContentRow->AddChildToHorizontalBox(CrewStatusBadge))
		{
			BadgeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			BadgeSlot->SetVerticalAlignment(VAlign_Center);
			BadgeSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		}
		if (UHorizontalBoxSlot* TextSlot = ContentRow->AddChildToHorizontalBox(TextColumn))
		{
			TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			TextSlot->SetVerticalAlignment(VAlign_Center);
		}
		RootBorder->SetContent(ContentRow);
		WidgetTree->RootWidget = RootBorder;
	}
	return Super::RebuildWidget();
}

void UHeistNameplateWidget::SetupPlayerState(AHeistPlayerState* InPlayerState)
{
	if (PlayerState != InPlayerState && IsValid(PlayerState))
	{
		PlayerState->GetPlayerIdentityChangedDelegate().RemoveAll(this);
		PlayerState->GetCrewStatusChangedDelegate().RemoveAll(this);
	}
	PlayerState = InPlayerState;
	if (IsValid(PlayerState))
	{
		PlayerState->GetPlayerIdentityChangedDelegate().RemoveAll(this);
		PlayerState->GetPlayerIdentityChangedDelegate().AddUObject(this, &UHeistNameplateWidget::HandleIdentityChanged);
		PlayerState->GetCrewStatusChangedDelegate().RemoveAll(this);
		PlayerState->GetCrewStatusChangedDelegate().AddUObject(this, &UHeistNameplateWidget::HandleCrewStatusChanged);
	}
	RefreshPresentation();
}

void UHeistNameplateWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const APlayerController* LocalController = GetOwningPlayer();
	if (!IsValid(LocalController))
	{
		LocalController = UGameplayStatics::GetPlayerController(this, 0);
	}
	const APawn* LocalPawn = IsValid(LocalController) ? LocalController->GetPawn() : nullptr;
	const APawn* TargetPawn = IsValid(PlayerState) ? PlayerState->GetPawn() : nullptr;
	if (!IsValid(LocalPawn) || !IsValid(TargetPawn) || LocalPawn == TargetPawn)
	{
		SetRenderOpacity(0.0f);
		return;
	}
	const float Distance = FVector::Dist(LocalPawn->GetActorLocation(), TargetPawn->GetActorLocation());
	SetRenderOpacity(CalculateDistanceOpacity(Distance));
}

bool UHeistNameplateWidget::IsPresentationContractSatisfied() const
{
	if (!IsValid(PlayerState) || !IsValid(PlayerNameText) || !IsValid(CrewStatusText) || !IsValid(CrewStatusBadge) || !IsValid(CrewStatusIconText))
	{
		return false;
	}

	const FLinearColor ExpectedStatusColor = HeistCrewStatus::GetPresentationColor(PlayerState->GetCrewStatus());
	return PlayerNameText->GetText().ToString() == PlayerState->GetHeistDisplayName().ToString() &&
		PlayerNameText->GetColorAndOpacity().GetSpecifiedColor().Equals(PlayerState->PlayerColor) &&
		CrewStatusText->GetText().ToString() == HeistCrewStatus::ToCompactText(PlayerState->GetCrewStatus()).ToString() &&
		CrewStatusText->GetColorAndOpacity().GetSpecifiedColor().Equals(ExpectedStatusColor) &&
		CrewStatusBadge->GetBrushColor().Equals(ExpectedStatusColor) &&
		CrewStatusIconText->GetText().ToString() == HeistCrewStatus::ToIconGlyph(PlayerState->GetCrewStatus()).ToString();
}

float UHeistNameplateWidget::CalculateDistanceOpacity(const float Distance) const
{
	const float SafeMaximum = FMath::Max(1.0f, MaximumVisibleDistance);
	const float SafeFade = FMath::Clamp(FadeDistance, 1.0f, SafeMaximum);
	return FMath::Clamp((SafeMaximum - FMath::Max(0.0f, Distance)) / SafeFade, 0.0f, 1.0f);
}

bool UHeistNameplateWidget::ShouldDisplayForLocalControl(const bool bLocallyControlled)
{
	return !bLocallyControlled;
}

void UHeistNameplateWidget::NativeDestruct()
{
	if (IsValid(PlayerState))
	{
		PlayerState->GetPlayerIdentityChangedDelegate().RemoveAll(this);
		PlayerState->GetCrewStatusChangedDelegate().RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UHeistNameplateWidget::RefreshPresentation()
{
	if (!IsValid(PlayerState))
	{
		return;
	}
	if (IsValid(PlayerNameText))
	{
		PlayerNameText->SetText(PlayerState->GetHeistDisplayName());
		PlayerNameText->SetColorAndOpacity(FSlateColor(PlayerState->PlayerColor));
	}
	if (IsValid(CrewStatusText))
	{
		CrewStatusText->SetText(HeistCrewStatus::ToCompactText(PlayerState->GetCrewStatus()));
		CrewStatusText->SetColorAndOpacity(FSlateColor(HeistCrewStatus::GetPresentationColor(PlayerState->GetCrewStatus())));
	}
	if (IsValid(CrewStatusBadge))
	{
		CrewStatusBadge->SetBrushColor(HeistCrewStatus::GetPresentationColor(PlayerState->GetCrewStatus()));
	}
	if (IsValid(CrewStatusIconText))
	{
		CrewStatusIconText->SetText(HeistCrewStatus::ToIconGlyph(PlayerState->GetCrewStatus()));
	}
	if (IsValid(OriginalCarrierIndicator))
	{
		OriginalCarrierIndicator->SetVisibility(PlayerState->GetCrewStatus() == EHeistCrewStatus::CarryingOriginal ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UHeistNameplateWidget::HandleIdentityChanged(const int32)
{
	RefreshPresentation();
}

void UHeistNameplateWidget::HandleCrewStatusChanged(const EHeistCrewStatus)
{
	RefreshPresentation();
}
