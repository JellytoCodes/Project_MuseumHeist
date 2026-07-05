#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistHUDWidget.generated.h"

class UTextBlock;

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistHUDWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

#pragma region Construction

public:
	UHeistHUDWidget(const FObjectInitializer& ObjectInitializer);

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void NativeDestruct() override;

#pragma endregion

#pragma region ViewModels

public:
	void SetupHUDWidget(
		class UHeistHUDViewModel* InHUDViewModel,
		class UHeistInventoryViewModel* InInventoryViewModel,
		class UHeistQuickSlotViewModel* InQuickSlotViewModel,
		class UHeistGapTrackerViewModel* InGapTrackerViewModel);

private:
	void RefreshHUDPresentation();

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|HUD", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistHUDViewModel> HUDViewModel;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|HUD", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistInventoryViewModel> InventoryViewModel;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|HUD", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistQuickSlotViewModel> QuickSlotViewModel;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|HUD", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistGapTrackerViewModel> GapTrackerViewModel;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|HUD", meta = (DisplayName = "HUD Sources Ready"))
	void BP_OnHUDSourcesReady();

	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|HUD", meta = (DisplayName = "Refresh HUD Presentation"))
	void BP_RefreshHUDPresentation(
		int32 LocalLootScore,
		float LocalLootWeight,
		int32 ConnectedPlayerCount,
		bool bLocalPlayerEscaped,
		bool bEscapePhaseOpen,
		bool bStunned,
		bool bStunImmune,
		bool bInSmoke,
		bool bEscapeCastActive,
		float EscapeCastEndServerTime,
		bool bTrapPlacementCastActive,
		float TrapPlacementCastEndServerTime);

#pragma endregion

#pragma region Presentation

private:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ScoreText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> WeightText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ActionText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> AlertText;

#pragma endregion
};
