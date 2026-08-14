#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistNameplateWidget.generated.h"

class AHeistPlayerState;
class UTextBlock;
class UWidget;
class SWidget;

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistNameplateWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

  public:
	void SetupPlayerState(AHeistPlayerState* InPlayerState);
	AHeistPlayerState* GetPresentedPlayerState() const { return PlayerState; }

  protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

  private:
	void RefreshPresentation();
	void HandleIdentityChanged(int32 PlayerId);
	void HandleCrewStatusChanged(EHeistCrewStatus CrewStatus);

	UPROPERTY(Transient)
	TObjectPtr<AHeistPlayerState> PlayerState;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> PlayerNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> CrewStatusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> OriginalCarrierIndicator;

	UPROPERTY(EditDefaultsOnly, Category = "Heist|Nameplate", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumVisibleDistance = 2500.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Heist|Nameplate", meta = (ClampMin = "0.0", Units = "cm"))
	float FadeDistance = 500.0f;
};
