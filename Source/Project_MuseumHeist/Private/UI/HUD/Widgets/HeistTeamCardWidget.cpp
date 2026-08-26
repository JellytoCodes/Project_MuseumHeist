#include "UI/HUD/Widgets/HeistTeamCardWidget.h"

#include "Character/HeistPlayerCharacter.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistPlayerState.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UI/ViewModels/HeistHUDViewModel.h"

#if WITH_HEIST_STEAM_AVATAR
#include "steam/steam_api.h"
#endif

namespace
{
constexpr int32 TeamCardMaxSteamAvatarRetryCount = 10;
constexpr float TeamCardSteamAvatarRetrySeconds = 0.5f;
constexpr float RemoteVoiceSpeakingThreshold = 0.02f;
}

void UHeistTeamCardWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshVoicePresentation();
}

void UHeistTeamCardWidget::NativeDestruct()
{
	ClearProfileImageRetry();
	Super::NativeDestruct();
}

void UHeistTeamCardWidget::ApplyCrewData(const FHeistCrewStatusEntry& CrewEntry, AHeistPlayerController* InOwningPlayerController)
{
	const FString PreviousPlatformUserId = PlatformUserId;
	PlayerSlot = FMath::Clamp(CrewEntry.PlayerId, 1, 4);
	bOccupied = IsValid(CrewEntry.PlayerState);
	PlayerState = CrewEntry.PlayerState;
	OwningHeistPlayerController = InOwningPlayerController;
	PlatformUserId = CrewEntry.PlatformUserId;
	CrewStatus = CrewEntry.Status;
	SetVisibility(bOccupied ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);

	if (IsValid(PlayerNameText))
	{
		PlayerNameText->SetText(bOccupied ? CrewEntry.PlayerName : FText::GetEmpty());
		PlayerNameText->SetColorAndOpacity(FSlateColor(CrewEntry.PlayerColor));
	}
	if (IsValid(StatusText))
	{
		StatusText->SetText(bOccupied ? HeistCrewStatus::ToDisplayText(CrewStatus) : NSLOCTEXT("HeistTeamCard", "EmptyStatus", "대기 중"));
		StatusText->SetColorAndOpacity(FSlateColor(bOccupied ? HeistCrewStatus::GetPresentationColor(CrewStatus) : FLinearColor(0.45f, 0.45f, 0.45f)));
	}
	if (IsValid(StatusIcon))
	{
		UTexture2D* IconTexture = ResolveStatusIcon();
		if (IsValid(IconTexture))
		{
			StatusIcon->SetBrushFromTexture(IconTexture, false);
		}
		StatusIcon->SetVisibility(IsValid(IconTexture) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (!bOccupied || PreviousPlatformUserId != PlatformUserId)
	{
		LoadedProfileTexture = nullptr;
		ProfileImageRetryCount = 0;
		RefreshProfileImage();
	}
	RefreshVoicePresentation();
}

void UHeistTeamCardWidget::ApplyEmptySlot(const int32 InPlayerSlot)
{
	ClearProfileImageRetry();
	PlayerSlot = FMath::Clamp(InPlayerSlot, 1, 4);
	bOccupied = false;
	PlayerState = nullptr;
	PlatformUserId.Reset();
	LoadedProfileTexture = nullptr;
	CrewStatus = EHeistCrewStatus::Active;
	SetVisibility(ESlateVisibility::Hidden);
	if (IsValid(PlayerNameText))
	{
		PlayerNameText->SetText(FText::GetEmpty());
	}
	if (IsValid(StatusText))
	{
		StatusText->SetText(NSLOCTEXT("HeistTeamCard", "WaitingForPlayer", "플레이어 대기 중"));
		StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.45f, 0.45f)));
	}
	if (IsValid(StatusIcon))
	{
		StatusIcon->SetVisibility(ESlateVisibility::Collapsed);
	}
	RefreshProfileImage();
	RefreshVoicePresentation();
}

void UHeistTeamCardWidget::RefreshProfileImage()
{
	ClearProfileImageRetry();
	if (!IsValid(ProfileImage))
	{
		return;
	}
	if (IsValid(DefaultProfileTexture))
	{
		ProfileImage->SetBrushFromTexture(DefaultProfileTexture, false);
	}
	if (!bOccupied || PlatformUserId.IsEmpty() || TryLoadSteamProfileImage())
	{
		return;
	}

#if WITH_HEIST_STEAM_AVATAR
	if (ProfileImageRetryCount < TeamCardMaxSteamAvatarRetryCount && IsValid(GetWorld()))
	{
		++ProfileImageRetryCount;
		GetWorld()->GetTimerManager().SetTimer(ProfileImageRetryTimerHandle, this, &UHeistTeamCardWidget::RetryProfileImageLoad, TeamCardSteamAvatarRetrySeconds, false);
	}
#endif
}

void UHeistTeamCardWidget::RetryProfileImageLoad()
{
	if (!bOccupied || PlatformUserId.IsEmpty() || TryLoadSteamProfileImage())
	{
		ClearProfileImageRetry();
		return;
	}

#if WITH_HEIST_STEAM_AVATAR
	if (ProfileImageRetryCount < TeamCardMaxSteamAvatarRetryCount && IsValid(GetWorld()))
	{
		++ProfileImageRetryCount;
		GetWorld()->GetTimerManager().SetTimer(ProfileImageRetryTimerHandle, this, &UHeistTeamCardWidget::RetryProfileImageLoad, TeamCardSteamAvatarRetrySeconds, false);
	}
#endif
}

bool UHeistTeamCardWidget::TryLoadSteamProfileImage()
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

	LoadedProfileTexture = UTexture2D::CreateTransient(static_cast<int32>(AvatarWidth), static_cast<int32>(AvatarHeight), PF_B8G8R8A8, NAME_None, BgraBytes);
	if (!IsValid(LoadedProfileTexture))
	{
		return false;
	}
	LoadedProfileTexture->SRGB = true;
	LoadedProfileTexture->UpdateResource();
	ProfileImage->SetBrushFromTexture(LoadedProfileTexture, false);
	return true;
#else
	return false;
#endif
}

void UHeistTeamCardWidget::ClearProfileImageRetry()
{
	if (IsValid(GetWorld()))
	{
		GetWorld()->GetTimerManager().ClearTimer(ProfileImageRetryTimerHandle);
	}
}

void UHeistTeamCardWidget::RefreshVoicePresentation()
{
	bool bMuted = false;
	bool bPushToTalkHeld = false;
	bool bSpeaking = false;
	if (bOccupied && IsValid(PlayerState) && IsValid(OwningHeistPlayerController))
	{
		bMuted = OwningHeistPlayerController->IsPlayerVoiceMuted(PlayerState);
		const bool bLocalPlayer = OwningHeistPlayerController->PlayerState == PlayerState;
		if (bLocalPlayer)
		{
			bPushToTalkHeld = OwningHeistPlayerController->IsVoicePushToTalkHeld();
			bSpeaking = OwningHeistPlayerController->IsLocalVoiceSpeaking();
		}
		else if (const AHeistPlayerCharacter* Character = Cast<AHeistPlayerCharacter>(PlayerState->GetPawn()))
		{
			bSpeaking = Character->GetVoiceLevel() > RemoteVoiceSpeakingThreshold;
		}
	}

	const FLinearColor MicColor = !bOccupied ? FLinearColor(0.32f, 0.32f, 0.32f) :
		(bMuted ? FLinearColor(0.80f, 0.18f, 0.16f) : (bSpeaking ? FLinearColor(0.20f, 0.78f, 0.38f) : (bPushToTalkHeld ? FLinearColor(0.88f, 0.68f, 0.20f) : FLinearColor(0.72f, 0.72f, 0.72f))));
	if (IsValid(MicStatusImage))
	{
		MicStatusImage->SetColorAndOpacity(MicColor);
		MicStatusImage->SetVisibility(bOccupied ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

UTexture2D* UHeistTeamCardWidget::ResolveStatusIcon() const
{
	switch (CrewStatus)
	{
	case EHeistCrewStatus::Forging:
		return ForgingStatusIcon.Get();
	case EHeistCrewStatus::CarryingOriginal:
		return CarryingOriginalStatusIcon.Get();
	case EHeistCrewStatus::Heavy:
		return HeavyStatusIcon.Get();
	case EHeistCrewStatus::Stunned:
		return StunnedStatusIcon.Get();
	case EHeistCrewStatus::Arrested:
		return ArrestedStatusIcon.Get();
	case EHeistCrewStatus::Escaped:
		return EscapedStatusIcon.Get();
	default:
		return nullptr;
	}
}
