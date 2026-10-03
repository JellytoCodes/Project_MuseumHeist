#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistNameplateWidget.generated.h"

class AHeistPlayerState;
class UBorder;
class UImage;
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
	bool AreStatusIconTexturesAssignedForDebug() const;
	float CalculateDistanceOpacity(float Distance) const;
	static bool ShouldDisplayForLocalControl(bool bLocallyControlled);

  protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

  private:
	void RefreshPresentation();
	void ResolveStatusIconWidgets();
	UObject* ResolveStatusIconResource(EHeistCrewStatus CrewStatus) const;
	void HandleIdentityChanged(int32 PlayerId);
	void HandleCrewStatusChanged(EHeistCrewStatus CrewStatus);

	UPROPERTY(Transient)
	TObjectPtr<AHeistPlayerState> PlayerState;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> PlayerNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> PlayerColorMarker;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> CrewStatusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> OriginalCarrierIndicator;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UBorder> CrewStatusBadge;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> CrewStatusIconText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> CrewStatusIconImage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Nameplate|Icons", meta = (AllowPrivateAccess = "true", AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TObjectPtr<UObject> StunnedStatusIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Nameplate|Icons", meta = (AllowPrivateAccess = "true", AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TObjectPtr<UObject> ArrestedStatusIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Nameplate|Icons", meta = (AllowPrivateAccess = "true", AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TObjectPtr<UObject> CarryingOriginalStatusIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Nameplate|Icons", meta = (AllowPrivateAccess = "true", AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TObjectPtr<UObject> HeavyStatusIcon;

	UPROPERTY(EditDefaultsOnly, Category = "Heist|Nameplate", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumVisibleDistance = 2500.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Heist|Nameplate", meta = (ClampMin = "0.0", Units = "cm"))
	float FadeDistance = 500.0f;
};
