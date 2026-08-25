#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistResultPlayerRowWidget.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistResultPlayerRowWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

  protected:
	virtual void NativeDestruct() override;

  public:
	void ApplyPlayerResult(const FHeistPlayerResult& PlayerResult);

	UFUNCTION(BlueprintPure, Category = "Heist|Result")
	static FText BuildPlayerStateText(const FHeistPlayerResult& PlayerResult);

  private:
	void RefreshProfileImage();
	void RetryProfileImageLoad();
	bool TryLoadSteamProfileImage();
	void ClearProfileImageRetry();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> ProfileImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> PlayerNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> PlayerStateText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> SurfaceForgeryCountText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> BestSurfaceQualityText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ArtifactsRecoveredText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> SecuredLootValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> GuardsDistractedText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TeammatesRescuedText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> AlarmsTriggeredText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> DefaultProfileTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LoadedProfileTexture;

	FTimerHandle ProfileImageRetryTimerHandle;
	FString PlatformUserId;
	int32 ProfileImageRetryCount = 0;
};
