#include "UI/Lobby/Widgets/HeistLobbyMapCardWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

namespace
{
	UTexture2D* LoadMapThumbnail(const FName MapId)
	{
		const TCHAR* TexturePath = nullptr;
		if (MapId == FName(TEXT("M01")))
		{
			TexturePath = TEXT("/Game/Assets/UI/Catalogue/T_Catalogue_Map_M01.T_Catalogue_Map_M01");
		}
		else if (MapId == FName(TEXT("M02")))
		{
			TexturePath = TEXT("/Game/Assets/UI/Catalogue/T_Catalogue_Map_M02.T_Catalogue_Map_M02");
		}
		else if (MapId == FName(TEXT("M03")))
		{
			TexturePath = TEXT("/Game/Assets/UI/Catalogue/T_Catalogue_Map_M03.T_Catalogue_Map_M03");
		}

		return TexturePath ? LoadObject<UTexture2D>(nullptr, TexturePath) : nullptr;
	}
}

void UHeistLobbyMapCardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (IsValid(SelectMapButton))
	{
		SelectMapButton->OnClicked.AddUniqueDynamic(this, &UHeistLobbyMapCardWidget::HandleSelectMapClicked);
	}
	RefreshMapThumbnail();
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
	SetMapThumbnail(LoadMapThumbnail(MapId));
	if (IsValid(MapNameText))
	{
		MapNameText->SetText(InMapDisplayName);
	}
}

void UHeistLobbyMapCardWidget::SetMapThumbnail(UTexture2D* InMapThumbnail)
{
	MapThumbnail = InMapThumbnail;
	RefreshMapThumbnail();
}

void UHeistLobbyMapCardWidget::RefreshMapThumbnail()
{
	const bool bRandomMap = MapId == FName(TEXT("Random"));
	if (IsValid(MapThumbnailImage))
	{
		MapThumbnailImage->SetBrushFromTexture(MapThumbnail, true);
		MapThumbnailImage->SetVisibility(!bRandomMap && IsValid(MapThumbnail) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (IsValid(RandomQuestionText))
	{
		RandomQuestionText->SetText(NSLOCTEXT("HeistLobby", "RandomMapQuestion", "?"));
		RandomQuestionText->SetVisibility(bRandomMap ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UHeistLobbyMapCardWidget::ApplySelectionState(const bool bSelected, const bool bCanSelect)
{
	if (IsValid(SelectMapButton))
	{
		SelectMapButton->SetIsEnabled(bCanSelect);
		SelectMapButton->SetRenderOpacity(1.0f);
	}
	if (IsValid(SelectedCheckImage))
	{
		SelectedCheckImage->SetVisibility(bSelected ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
	if (IsValid(SelectionBackground))
	{
		SelectionBackground->SetRenderOpacity(bSelected ? 1.0f : 0.0f);
	}
	if (IsValid(MapThumbnailImage))
	{
		MapThumbnailImage->SetColorAndOpacity(bSelected ? SelectedThumbnailTint : ThumbnailTint);
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
