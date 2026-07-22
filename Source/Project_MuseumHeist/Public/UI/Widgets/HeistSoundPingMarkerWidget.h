#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistSoundPingMarkerWidget.generated.h"

class UTextBlock;
class UWidget;
struct FHeistSoundPingEvent;

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistSoundPingMarkerWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

#pragma region Construction

  public:
	UHeistSoundPingMarkerWidget(const FObjectInitializer& ObjectInitializer);

#pragma endregion

#pragma region Presentation

  public:
	void ShowSoundPingMarker(const FHeistSoundPingEvent& SoundPingEvent, const FVector2D& ScreenDirection, const FVector2D& ScreenEdgeTranslation);
	void ReleaseSoundPingMarker();

  private:
	static FText GetPingTypeText(const FHeistSoundPingEvent& SoundPingEvent);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> SoundPingMarkerContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> SoundPingDirectionArrow;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> SoundPingTypeText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> SoundPingDurationText;

#pragma endregion
};
