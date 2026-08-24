#include "UI/Lobby/Widgets/HeistLobbyMapCardWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

namespace
{
	constexpr float UnselectedMapOpacity = 0.6f;
	constexpr float SelectedMapOpacity = 1.0f;

	UTexture2D* LoadMapThumbnail(const FName MapId)
	{
		const TCHAR* TexturePath = nullptr;
		if (MapId == FName(TEXT("M01")))
		{
			TexturePath = TEXT("/Game/Assets/UI/Map/T_FloorPlan_M01.T_FloorPlan_M01");
		}
		else if (MapId == FName(TEXT("M02")))
		{
			TexturePath = TEXT("/Game/Assets/UI/Map/T_FloorPlan_M02.T_FloorPlan_M02");
		}
		else if (MapId == FName(TEXT("M03")))
		{
			TexturePath = TEXT("/Game/Assets/UI/Map/T_FloorPlan_M03.T_FloorPlan_M03");
		}

		return TexturePath ? LoadObject<UTexture2D>(nullptr, TexturePath) : nullptr;
	}

	void ApplyMapThumbnailToButton(UButton* Button, UTexture2D* Thumbnail)
	{
		if (!IsValid(Button) || !IsValid(Thumbnail))
		{
			return;
		}

		FButtonStyle ButtonStyle = Button->GetStyle();
		FSlateBrush MapBrush = ButtonStyle.Normal;
		MapBrush.SetResourceObject(Thumbnail);
		MapBrush.ImageSize = FVector2D(Thumbnail->GetSizeX(), Thumbnail->GetSizeY());
		MapBrush.DrawAs = ESlateBrushDrawType::Image;
		MapBrush.TintColor = FSlateColor(FLinearColor::White);

		ButtonStyle.SetNormal(MapBrush);
		ButtonStyle.SetHovered(MapBrush);
		ButtonStyle.SetPressed(MapBrush);
		Button->SetStyle(ButtonStyle);
	}
}

void UHeistLobbyMapCardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (IsValid(SelectMapButton))
	{
		SelectMapButton->OnClicked.AddUniqueDynamic(this, &UHeistLobbyMapCardWidget::HandleSelectMapClicked);
		ApplyMapThumbnailToButton(SelectMapButton, MapThumbnail);
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
	if (UTexture2D* Thumbnail = LoadMapThumbnail(MapId))
	{
		SetMapThumbnail(Thumbnail);
	}
	if (IsValid(MapNameText))
	{
		MapNameText->SetText(InMapDisplayName);
	}
}

void UHeistLobbyMapCardWidget::SetMapThumbnail(UTexture2D* InMapThumbnail)
{
	MapThumbnail = InMapThumbnail;
	ApplyMapThumbnailToButton(SelectMapButton, MapThumbnail);
}

void UHeistLobbyMapCardWidget::ApplySelectionState(const bool bSelected, const bool bCanSelect)
{
	if (IsValid(SelectMapButton))
	{
		SelectMapButton->SetIsEnabled(bCanSelect);
		SelectMapButton->SetRenderOpacity(bSelected ? SelectedMapOpacity : UnselectedMapOpacity);
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
