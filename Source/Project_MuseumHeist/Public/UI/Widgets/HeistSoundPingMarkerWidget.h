#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistSoundPingMarkerWidget.generated.h"

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistSoundPingMarkerWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

public:
	UHeistSoundPingMarkerWidget(const FObjectInitializer& ObjectInitializer);
};
