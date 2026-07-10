#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistGapTrackerWidget.generated.h"

class UHeistGapTrackerViewModel;
class UTextBlock;
class UWidget;

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistGapTrackerWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

#pragma region Construction

public:
	UHeistGapTrackerWidget(const FObjectInitializer& ObjectInitializer);

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void NativeDestruct() override;

#pragma endregion

#pragma region ViewModel

public:
	void SetupGapTrackerWidget(UHeistGapTrackerViewModel* InGapTrackerViewModel);

private:
	void RefreshGapTrackerPresentation();

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|GapTracker", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistGapTrackerViewModel> GapTrackerViewModel;

#pragma endregion

#pragma region Presentation

private:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> GapTrackerContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> GapTrackerDirectionArrow;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> GapTrackerLeaderWarningContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> GapTrackerText;

#pragma endregion
};
