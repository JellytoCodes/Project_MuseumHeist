#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistResultPlayerRowWidget.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;
class USizeBox;

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistResultPlayerRowWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

  protected:
	virtual void NativeDestruct() override;

  public:
	void ApplyPlayerResult(const FHeistPlayerResult& PlayerResult);
	void SetDetailedPresentation(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Heist|Result")
	static FText BuildPlayerStateText(const FHeistPlayerResult& PlayerResult);

  private:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<USizeBox> CoopResultCompactFieldsSize;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<USizeBox> CoopResultDetailFieldsSize;

	void RefreshProfileImage();
	void RetryProfileImageLoad();
	bool TryLoadSteamProfileImage();
	void ClearProfileImageRetry();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> ProfileImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> PlayerNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DetailPlayerNameText;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Result", meta = (AllowPrivateAccess = "true", AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TObjectPtr<UObject> DefaultProfileTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LoadedProfileTexture;

	FTimerHandle ProfileImageRetryTimerHandle;
	FString PlatformUserId;
	int32 ProfileImageRetryCount = 0;
};
