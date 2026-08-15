#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistNameplateWidget.generated.h"

class AHeistPlayerState;
class UBorder;
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
	bool IsPresentationContractSatisfied() const;
	float CalculateDistanceOpacity(float Distance) const;
	static bool ShouldDisplayForLocalControl(bool bLocallyControlled);

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

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UBorder> CrewStatusBadge;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> CrewStatusIconText;

	UPROPERTY(EditDefaultsOnly, Category = "Heist|Nameplate", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumVisibleDistance = 2500.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Heist|Nameplate", meta = (ClampMin = "0.0", Units = "cm"))
	float FadeDistance = 500.0f;
};
