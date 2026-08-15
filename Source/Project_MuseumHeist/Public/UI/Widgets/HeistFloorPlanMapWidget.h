#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "Data/HeistMapPresentationDataTypes.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistFloorPlanMapWidget.generated.h"

class AHeistGameState;
class AHeistPlayerController;
class UCanvasPanel;
class UImage;
class UOverlay;
class UTextBlock;
class UTexture2D;
class SWidget;

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistFloorPlanMapWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

  public:
	bool SetupMap(AHeistGameState* InGameState, AHeistPlayerController* InPlayerController, bool bLogFailure = false);
	void RefreshMapPresentation();
	bool IsMapPresentationReady() const;

	static bool ShouldShowExactPaintingTarget(EHeistDisplayCaseState DisplayCaseState, bool bRequiredTargetSecured);
	static bool ShouldShowExactObjectTarget(EHeistObjectAssemblyState AssemblyState, bool bRequiredTargetSecured);
	static bool IsAllowedMarkerType(EHeistFloorPlanMarkerType MarkerType);

  protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

  private:
	bool ResolveMapPresentation(bool bLogFailure);
	void ResetMapPresentation();
	void RebuildStaticMarkers();
	void RefreshStaticMarkerPositions();
	void BeginDynamicMarkerRefresh();
	void FinishDynamicMarkerRefresh();
	void AddStaticMarker(const FVector2D& WorldLocation, const FText& Label, EHeistFloorPlanMarkerType MarkerType);
	void AddDynamicMarker(const FVector& WorldLocation, const FText& Label, EHeistFloorPlanMarkerType MarkerType, const FLinearColor& CustomColor = FLinearColor::Transparent);
	UTextBlock* CreateMarkerWidget(UCanvasPanel* ParentContainer);
	void ApplyMarkerPresentation(UTextBlock* Marker, const UCanvasPanel* ParentContainer, const FVector2D& WorldLocation, const FText& Label,
		EHeistFloorPlanMarkerType MarkerType,
		const FLinearColor& CustomColor = FLinearColor::Transparent) const;
	FVector2D ProjectWorldLocation(const FVector2D& WorldLocation, const UCanvasPanel* TargetContainer) const;
	static FLinearColor ResolveMarkerColor(EHeistFloorPlanMarkerType MarkerType, const FLinearColor& CustomColor);

	UPROPERTY(Transient)
	TObjectPtr<AHeistGameState> GameState;

	UPROPERTY(Transient)
	TObjectPtr<AHeistPlayerController> PlayerController;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> MapTitleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> MapHintText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> LegendText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UOverlay> MapOverlay;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> FloorPlanImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCanvasPanel> StaticMarkerContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCanvasPanel> DynamicMarkerContainer;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ResolvedFloorPlanTexture;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> StaticMarkerWidgets;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> DynamicMarkerPool;

	TArray<FVector2D> StaticMarkerWorldLocations;
	FHeistMapPresentationRow MapPresentation;
	FName ResolvedMapId = NAME_None;
	int32 ActiveDynamicMarkerCount = 0;
	bool bMapPresentationReady = false;

	float RefreshAccumulator = 0.0f;
};
