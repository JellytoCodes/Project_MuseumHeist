#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistForgeryWidget.generated.h"

class UTextBlock;
class UWidget;

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistForgeryWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

#pragma region Construction

public:
	UHeistForgeryWidget(const FObjectInitializer& ObjectInitializer);

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void NativeDestruct() override;

#pragma endregion

#pragma region Setup

public:
	void SetupForgeryWidget(class UHeistForgeryViewModel* InForgeryViewModel);
	bool IsOwnerOnlyContractSatisfied() const;
	bool IsWidgetPresentationVisible() const;

private:
	void RefreshForgeryPresentation();
	void ApplyStateVisibility(UWidget* TargetWidget, bool bVisible) const;

	UPROPERTY(
		Transient,
		BlueprintReadOnly,
		Category = "Heist|Forgery",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistForgeryViewModel> ForgeryViewModel;

protected:
	UFUNCTION(
		BlueprintImplementableEvent,
		Category = "Heist|Forgery",
		meta = (DisplayName = "Forgery Sources Ready"))
	void BP_OnForgerySourcesReady();

	UFUNCTION(
		BlueprintImplementableEvent,
		Category = "Heist|Forgery",
		meta = (DisplayName = "Refresh Forgery Presentation"))
	void BP_RefreshForgeryPresentation(
		bool bObservation,
		bool bDrawing,
		bool bValidation,
		bool bResult,
		float StateEndServerTime,
		float ResultScore);

#pragma endregion

#pragma region Presentation

private:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> ObservationContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> DrawingContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> ValidationContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> ResultContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> StateText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ReferenceText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ResultText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ResultScoreText;

#pragma endregion
};
