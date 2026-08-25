#include "UI/Result/Widgets/HeistResultPlayerRowWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "TimerManager.h"

#if WITH_HEIST_STEAM_AVATAR
#include "steam/steam_api.h"
#endif

namespace
{
constexpr int32 ResultPlayerRowMaxSteamAvatarRetryCount = 10;
constexpr float ResultPlayerRowSteamAvatarRetrySeconds = 0.5f;
}

void UHeistResultPlayerRowWidget::NativeDestruct()
{
	ClearProfileImageRetry();
	Super::NativeDestruct();
}

void UHeistResultPlayerRowWidget::ApplyPlayerResult(const FHeistPlayerResult& PlayerResult)
{
	PlatformUserId = PlayerResult.PlatformUserId;
	LoadedProfileTexture = nullptr;
	ProfileImageRetryCount = 0;

	const FText DisplayName = PlayerResult.PlayerDisplayName.IsEmpty()
		? FText::Format(NSLOCTEXT("HeistResult", "FallbackPlayerIdentity", "PLAYER {0}"), FText::AsNumber(PlayerResult.PlayerId))
		: FText::FromString(PlayerResult.PlayerDisplayName);
	if (IsValid(PlayerNameText))
	{
		PlayerNameText->SetText(DisplayName);
	}
	if (IsValid(PlayerStateText))
	{
		PlayerStateText->SetText(BuildPlayerStateText(PlayerResult));
	}

	const FHeistPlayerContribution& Contribution = PlayerResult.Contribution;
	if (IsValid(SurfaceForgeryCountText))
	{
		SurfaceForgeryCountText->SetText(FText::AsNumber(Contribution.SurfaceForgeries));
	}
	if (IsValid(BestSurfaceQualityText))
	{
		BestSurfaceQualityText->SetText(FText::AsNumber(FMath::RoundToInt(Contribution.BestSurfaceQuality)));
	}
	if (IsValid(ArtifactsRecoveredText))
	{
		ArtifactsRecoveredText->SetText(FText::AsNumber(Contribution.ArtifactsRecovered));
	}
	if (IsValid(SecuredLootValueText))
	{
		SecuredLootValueText->SetText(FText::AsNumber(Contribution.SecuredLootValue));
	}
	if (IsValid(GuardsDistractedText))
	{
		GuardsDistractedText->SetText(FText::AsNumber(Contribution.GuardsDistracted));
	}
	if (IsValid(TeammatesRescuedText))
	{
		TeammatesRescuedText->SetText(FText::AsNumber(Contribution.TeammatesRescued));
	}
	if (IsValid(AlarmsTriggeredText))
	{
		AlarmsTriggeredText->SetText(FText::AsNumber(Contribution.AlarmsTriggered));
	}

	RefreshProfileImage();
}

FText UHeistResultPlayerRowWidget::BuildPlayerStateText(const FHeistPlayerResult& PlayerResult)
{
	if (PlayerResult.bEscaped)
	{
		return NSLOCTEXT("HeistResult", "PlayerResultEscaped", "탈출");
	}
	if (PlayerResult.bArrested)
	{
		return NSLOCTEXT("HeistResult", "PlayerResultArrested", "체포");
	}
	return NSLOCTEXT("HeistResult", "PlayerResultUnresolved", "미탈출");
}

void UHeistResultPlayerRowWidget::RefreshProfileImage()
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
	if (PlatformUserId.IsEmpty() || TryLoadSteamProfileImage())
	{
		return;
	}

#if WITH_HEIST_STEAM_AVATAR
	if (ProfileImageRetryCount < ResultPlayerRowMaxSteamAvatarRetryCount && IsValid(GetWorld()))
	{
		++ProfileImageRetryCount;
		GetWorld()->GetTimerManager().SetTimer(ProfileImageRetryTimerHandle, this,
			&UHeistResultPlayerRowWidget::RetryProfileImageLoad, ResultPlayerRowSteamAvatarRetrySeconds, false);
	}
#endif
}

void UHeistResultPlayerRowWidget::RetryProfileImageLoad()
{
	if (PlatformUserId.IsEmpty() || TryLoadSteamProfileImage())
	{
		ClearProfileImageRetry();
		return;
	}

#if WITH_HEIST_STEAM_AVATAR
	if (ProfileImageRetryCount < ResultPlayerRowMaxSteamAvatarRetryCount && IsValid(GetWorld()))
	{
		++ProfileImageRetryCount;
		GetWorld()->GetTimerManager().SetTimer(ProfileImageRetryTimerHandle, this,
			&UHeistResultPlayerRowWidget::RetryProfileImageLoad, ResultPlayerRowSteamAvatarRetrySeconds, false);
	}
#endif
}

bool UHeistResultPlayerRowWidget::TryLoadSteamProfileImage()
{
#if WITH_HEIST_STEAM_AVATAR
	if (!IsValid(ProfileImage) || !SteamAPI_IsSteamRunning() || !SteamFriends() || !SteamUtils())
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
	uint32 AvatarWidth = 0;
	uint32 AvatarHeight = 0;
	if (AvatarImageHandle <= 0 || !SteamUtils()->GetImageSize(AvatarImageHandle, &AvatarWidth, &AvatarHeight) || AvatarWidth == 0 || AvatarHeight == 0)
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

	LoadedProfileTexture = UTexture2D::CreateTransient(static_cast<int32>(AvatarWidth), static_cast<int32>(AvatarHeight),
		PF_B8G8R8A8, NAME_None, BgraBytes);
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

void UHeistResultPlayerRowWidget::ClearProfileImageRetry()
{
	if (IsValid(GetWorld()))
	{
		GetWorld()->GetTimerManager().ClearTimer(ProfileImageRetryTimerHandle);
	}
}
