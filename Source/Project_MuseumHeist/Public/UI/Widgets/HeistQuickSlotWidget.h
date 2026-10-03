#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"
#include "UI/ViewModels/HeistQuickSlotViewModel.h"

#include "HeistQuickSlotWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistQuickSlotWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

#pragma region Presentation

  public:
	void SetupHUDQuickSlot(const FHeistQuickSlotPresentation& InConfirmedPresentation, UObject* InIcon);

  private:
	void RefreshPresentation();

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|QuickSlot", meta = (AllowPrivateAccess = "true"))
	FHeistQuickSlotPresentation ConfirmedPresentation;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UBorder> SlotBackground;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> KeyLabelText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> PlaceholderIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> CountText;

#pragma endregion
};
