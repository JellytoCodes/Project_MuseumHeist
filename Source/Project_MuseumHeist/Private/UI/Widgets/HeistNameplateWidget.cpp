#include "UI/Widgets/HeistNameplateWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Blueprint/WidgetTree.h"
#include "Core/HeistPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
FSlateFontInfo MakeTenadaFont(const int32 Size)
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
		UVerticalBox* TextColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("NameplateTextColumn"));
		PlayerNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PlayerNameText"));
		CrewStatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CrewStatusText"));
		PlayerNameText->SetJustification(ETextJustify::Center);
		CrewStatusText->SetJustification(ETextJustify::Center);
		PlayerNameText->SetFont(MakeTenadaFont(18));
		CrewStatusText->SetFont(MakeTenadaFont(14));
		TextColumn->AddChildToVerticalBox(PlayerNameText);
		TextColumn->AddChildToVerticalBox(CrewStatusText);
		RootBorder->SetContent(TextColumn);
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
	const float SafeMaximum = FMath::Max(1.0f, MaximumVisibleDistance);
	const float SafeFade = FMath::Clamp(FadeDistance, 1.0f, SafeMaximum);
	SetRenderOpacity(FMath::Clamp((SafeMaximum - Distance) / SafeFade, 0.0f, 1.0f));
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
