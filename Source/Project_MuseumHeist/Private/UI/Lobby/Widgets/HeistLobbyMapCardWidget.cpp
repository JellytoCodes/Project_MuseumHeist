#include "UI/Lobby/Widgets/HeistLobbyMapCardWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UHeistLobbyMapCardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (IsValid(SelectMapButton))
	{
		SelectMapButton->OnClicked.AddUniqueDynamic(this, &UHeistLobbyMapCardWidget::HandleSelectMapClicked);
	}
}

void UHeistLobbyMapCardWidget::NativeDestruct()
{
	if (IsValid(SelectMapButton))
	{
		SelectMapButton->OnClicked.RemoveDynamic(this, &UHeistLobbyMapCardWidget::HandleSelectMapClicked);
	}
	Super::NativeDestruct();
}

void UHeistLobbyMapCardWidget::ConfigureMapCard(const FName InMapId, const FText& InMapDisplayName)
{
	MapId = InMapId;
	if (IsValid(MapNameText))
	{
		MapNameText->SetText(InMapDisplayName);
	}
}

void UHeistLobbyMapCardWidget::SetMapThumbnail(UTexture2D* InMapThumbnail)
{
	MapThumbnail = InMapThumbnail;
}

void UHeistLobbyMapCardWidget::ApplySelectionState(const bool bSelected, const bool bCanSelect)
{
	if (IsValid(SelectMapButton))
	{
		SelectMapButton->SetIsEnabled(bCanSelect);
	}
	if (IsValid(SelectedCheckImage))
	{
		SelectedCheckImage->SetVisibility(bSelected ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
}

FName UHeistLobbyMapCardWidget::GetMapId() const
{
	return MapId;
}

FHeistLobbyMapCardSelected& UHeistLobbyMapCardWidget::GetMapSelectedDelegate()
{
	return MapSelectedDelegate;
}

void UHeistLobbyMapCardWidget::HandleSelectMapClicked()
{
	if (!MapId.IsNone())
	{
		MapSelectedDelegate.Broadcast(MapId);
	}
}
