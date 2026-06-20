#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistLobbyWidget.generated.h"

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistLobbyWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

public:
	UHeistLobbyWidget(const FObjectInitializer& ObjectInitializer);
};
