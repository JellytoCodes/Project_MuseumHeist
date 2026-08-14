#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistFloorPlanMapWidget.generated.h"

class AHeistGameState;
class AHeistPlayerController;
class UCanvasPanel;
class UTextBlock;
class SWidget;

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistFloorPlanMapWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

  public:
	void SetupMap(AHeistGameState* InGameState, AHeistPlayerController* InPlayerController);
	void RefreshMapPresentation();

  protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

  private:
	void AddMarker(const FVector& WorldLocation, const FText& Label, const FLinearColor& Color);
	FVector2D ProjectWorldLocation(const FVector& WorldLocation) const;
	void ResolveWorldBounds();

	UPROPERTY(Transient)
	TObjectPtr<AHeistGameState> GameState;

	UPROPERTY(Transient)
	TObjectPtr<AHeistPlayerController> PlayerController;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> MapTitleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> MapHintText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCanvasPanel> MarkerContainer;

	FBox2D WorldBounds = FBox2D(FVector2D(-3000.0, -3000.0), FVector2D(3000.0, 3000.0));
	float RefreshAccumulator = 0.0f;
};
