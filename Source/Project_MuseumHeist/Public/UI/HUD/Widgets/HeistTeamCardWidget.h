#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistTeamCardWidget.generated.h"

class AHeistPlayerController;
class AHeistPlayerState;
class UImage;
class UTextBlock;
class UTexture2D;
struct FHeistCrewStatusEntry;

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistTeamCardWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

  protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

  public:
	void ApplyCrewData(const FHeistCrewStatusEntry& CrewEntry, AHeistPlayerController* InOwningPlayerController);
	void ApplyEmptySlot(int32 PlayerSlot);

  private:
	void RefreshProfileImage();
	void RetryProfileImageLoad();
	bool TryLoadSteamProfileImage();
	void ClearProfileImageRetry();
	void RefreshVoicePresentation();
	UObject* ResolveStatusIcon() const;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> ProfileImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> PlayerNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> PlayerColorMarker;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> StatusIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> MicStatusImage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|TeamCard", meta = (AllowPrivateAccess = "true", AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TObjectPtr<UObject> DefaultProfileTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|TeamCard|Status", meta = (AllowPrivateAccess = "true", AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TObjectPtr<UObject> ForgingStatusIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|TeamCard|Status", meta = (AllowPrivateAccess = "true", AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TObjectPtr<UObject> CarryingOriginalStatusIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|TeamCard|Status", meta = (AllowPrivateAccess = "true", AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TObjectPtr<UObject> HeavyStatusIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|TeamCard|Status", meta = (AllowPrivateAccess = "true", AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TObjectPtr<UObject> StunnedStatusIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|TeamCard|Status", meta = (AllowPrivateAccess = "true", AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TObjectPtr<UObject> ArrestedStatusIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|TeamCard|Status", meta = (AllowPrivateAccess = "true", AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TObjectPtr<UObject> EscapedStatusIcon;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LoadedProfileTexture;

	UPROPERTY(Transient)
	TObjectPtr<AHeistPlayerState> PlayerState;

	UPROPERTY(Transient)
	TObjectPtr<AHeistPlayerController> OwningHeistPlayerController;

	FTimerHandle ProfileImageRetryTimerHandle;
	FString PlatformUserId;
	int32 PlayerSlot = INDEX_NONE;
	int32 ProfileImageRetryCount = 0;
	EHeistCrewStatus CrewStatus = EHeistCrewStatus::Active;
	bool bOccupied = false;
};
