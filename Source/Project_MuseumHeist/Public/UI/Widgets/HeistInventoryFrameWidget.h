#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistInventoryFrameWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UOverlay;
class UUniformGridPanel;

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistInventoryFrameWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

  public:
	UUniformGridPanel* GetInventoryGrid() const;
	UCanvasPanel* GetItemOverlay() const;

  private:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UBorder> InventoryFrame;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UOverlay> InventoryOverlay;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UUniformGridPanel> InventoryGrid;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCanvasPanel> ItemOverlay;
};
