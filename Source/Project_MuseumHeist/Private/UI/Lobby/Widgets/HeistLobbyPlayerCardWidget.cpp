#include "UI/Lobby/Widgets/HeistLobbyPlayerCardWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UI/Lobby/ViewModels/HeistLobbyViewModel.h"

#if WITH_HEIST_STEAM_AVATAR
#include "steam/steam_api.h"
#endif

namespace
{
constexpr int32 LobbyPlayerCardMaxSteamAvatarRetryCount = 10;
constexpr float LobbyPlayerCardSteamAvatarRetrySeconds = 0.5f;
}

void UHeistLobbyPlayerCardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (IsValid(ReadyButton))
	{
		ReadyButton->OnClicked.AddUniqueDynamic(this, &UHeistLobbyPlayerCardWidget::HandleReadyClicked);
	}
}

void UHeistLobbyPlayerCardWidget::NativeDestruct()
{
	ClearProfileImageRetry();
	if (IsValid(ReadyButton))
	{
		ReadyButton->OnClicked.RemoveDynamic(this, &UHeistLobbyPlayerCardWidget::HandleReadyClicked);
	}
	Super::NativeDestruct();
}

void UHeistLobbyPlayerCardWidget::ConfigurePlayerSlot(const int32 InPlayerSlot)
{
	PlayerSlot = FMath::Clamp(InPlayerSlot, 1, 4);
	if (IsValid(PlayerSlotText))
	{
		PlayerSlotText->SetText(FText::Format(NSLOCTEXT("HeistLobby", "PlayerSlotLabel", "플레이어 {0}"), FText::AsNumber(PlayerSlot)));
	}
}

void UHeistLobbyPlayerCardWidget::ApplyPlayerData(const FHeistLobbyPlayerCardData& PlayerCardData, const bool bCanToggleReady)
{
	ConfigurePlayerSlot(PlayerCardData.PlayerSlot);
	const FString PreviousPlatformUserId = PlatformUserId;
	bOccupied = PlayerCardData.bOccupied;
	PlatformUserId = PlayerCardData.PlatformUserId;

	if (IsValid(PlayerNameText))
	{
		PlayerNameText->SetText(bOccupied ? PlayerCardData.PlayerName : FText::GetEmpty());
	}
	if (IsValid(ReadyButton))
	{
		ReadyButton->SetIsEnabled(bOccupied && PlayerCardData.bLocalPlayer && bCanToggleReady);
	}
	if (IsValid(ReadyCheckImage))
	{
		ReadyCheckImage->SetVisibility(bOccupied && PlayerCardData.bReady ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}

	if (!bOccupied || PreviousPlatformUserId != PlatformUserId)
	{
		LoadedProfileTexture = nullptr;
		ProfileImageRetryCount = 0;
		RefreshProfileImage();
	}
}

FHeistLobbyPlayerCardReadyRequested& UHeistLobbyPlayerCardWidget::GetReadyRequestedDelegate()
{
	return ReadyRequestedDelegate;
}

void UHeistLobbyPlayerCardWidget::HandleReadyClicked()
{
	if (bOccupied && PlayerSlot >= 1 && PlayerSlot <= 4)
	{
		ReadyRequestedDelegate.Broadcast(PlayerSlot);
	}
}

void UHeistLobbyPlayerCardWidget::RefreshProfileImage()
{
	ClearProfileImageRetry();
	if (!IsValid(ProfileImage))
	{
		return;
	}

	if (IsValid(DefaultProfileTexture))
	{
		ProfileImage->SetBrushFromTexture(DefaultProfileTexture, true);
	}
	if (!bOccupied || PlatformUserId.IsEmpty())
	{
		return;
	}

	if (TryLoadSteamProfileImage())
	{
		return;
	}

#if WITH_HEIST_STEAM_AVATAR
	if (ProfileImageRetryCount < LobbyPlayerCardMaxSteamAvatarRetryCount && IsValid(GetWorld()))
	{
		++ProfileImageRetryCount;
		GetWorld()->GetTimerManager().SetTimer(ProfileImageRetryTimerHandle, this, &UHeistLobbyPlayerCardWidget::RetryProfileImageLoad, LobbyPlayerCardSteamAvatarRetrySeconds, false);
	}
#endif
}

void UHeistLobbyPlayerCardWidget::RetryProfileImageLoad()
{
	if (!bOccupied || PlatformUserId.IsEmpty() || TryLoadSteamProfileImage())
	{
		ClearProfileImageRetry();
		return;
	}

#if WITH_HEIST_STEAM_AVATAR
	if (ProfileImageRetryCount < LobbyPlayerCardMaxSteamAvatarRetryCount && IsValid(GetWorld()))
	{
		++ProfileImageRetryCount;
		GetWorld()->GetTimerManager().SetTimer(ProfileImageRetryTimerHandle, this, &UHeistLobbyPlayerCardWidget::RetryProfileImageLoad, LobbyPlayerCardSteamAvatarRetrySeconds, false);
	}
#endif
}

bool UHeistLobbyPlayerCardWidget::TryLoadSteamProfileImage()
{
#if WITH_HEIST_STEAM_AVATAR
	if (!SteamAPI_IsSteamRunning() || !SteamFriends() || !SteamUtils())
	{
		return false;
	}

	uint64 SteamIdValue = 0;
	if (!LexTryParseString(SteamIdValue, *PlatformUserId) || SteamIdValue == 0)
	{
		return false;
	}

	const CSteamID SteamUserId(SteamIdValue);
	SteamFriends()->RequestUserInformation(SteamUserId, false);
	const int AvatarImageHandle = SteamFriends()->GetLargeFriendAvatar(SteamUserId);
	if (AvatarImageHandle <= 0)
	{
		return false;
	}

	uint32 AvatarWidth = 0;
	uint32 AvatarHeight = 0;
	if (!SteamUtils()->GetImageSize(AvatarImageHandle, &AvatarWidth, &AvatarHeight) || AvatarWidth == 0 || AvatarHeight == 0)
	{
		return false;
	}

	TArray<uint8> RgbaBytes;
	RgbaBytes.SetNumUninitialized(static_cast<int32>(AvatarWidth * AvatarHeight * 4));
	if (!SteamUtils()->GetImageRGBA(AvatarImageHandle, RgbaBytes.GetData(), RgbaBytes.Num()))
	{
		return false;
	}

	TArray<uint8> BgraBytes;
	BgraBytes.SetNumUninitialized(RgbaBytes.Num());
	for (int32 ByteIndex = 0; ByteIndex < RgbaBytes.Num(); ByteIndex += 4)
	{
		BgraBytes[ByteIndex] = RgbaBytes[ByteIndex + 2];
		BgraBytes[ByteIndex + 1] = RgbaBytes[ByteIndex + 1];
		BgraBytes[ByteIndex + 2] = RgbaBytes[ByteIndex];
		BgraBytes[ByteIndex + 3] = RgbaBytes[ByteIndex + 3];
	}

	LoadedProfileTexture = UTexture2D::CreateTransient(static_cast<int32>(AvatarWidth), static_cast<int32>(AvatarHeight), PF_B8G8R8A8, NAME_None, BgraBytes);
	if (!IsValid(LoadedProfileTexture))
	{
		return false;
	}

	LoadedProfileTexture->SRGB = true;
	LoadedProfileTexture->UpdateResource();
	ProfileImage->SetBrushFromTexture(LoadedProfileTexture, true);
	return true;
#else
	return false;
#endif
}

void UHeistLobbyPlayerCardWidget::ClearProfileImageRetry()
{
	if (IsValid(GetWorld()))
	{
		GetWorld()->GetTimerManager().ClearTimer(ProfileImageRetryTimerHandle);
	}
}
