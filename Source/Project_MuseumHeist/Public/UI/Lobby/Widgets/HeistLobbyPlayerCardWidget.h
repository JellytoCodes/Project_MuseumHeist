#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistLobbyPlayerCardWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;
struct FHeistLobbyPlayerCardData;

DECLARE_MULTICAST_DELEGATE_OneParam(FHeistLobbyPlayerCardReadyRequested, int32);

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistLobbyPlayerCardWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

  protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

  public:
	void ConfigurePlayerSlot(int32 InPlayerSlot);
	void ApplyPlayerData(const FHeistLobbyPlayerCardData& PlayerCardData, bool bCanToggleReady);
	FHeistLobbyPlayerCardReadyRequested& GetReadyRequestedDelegate();

  private:
	UFUNCTION()
	void HandleReadyClicked();

	void RefreshProfileImage();
	void RetryProfileImageLoad();
	bool TryLoadSteamProfileImage();
	void ClearProfileImageRetry();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> PlayerSlotText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> ProfileImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> PlayerNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> ReadyButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> ReadyCheckImage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> DefaultProfileTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LoadedProfileTexture;

	FTimerHandle ProfileImageRetryTimerHandle;
	FHeistLobbyPlayerCardReadyRequested ReadyRequestedDelegate;
	FString PlatformUserId;
	int32 PlayerSlot = INDEX_NONE;
	int32 ProfileImageRetryCount = 0;
	bool bOccupied = false;
};
