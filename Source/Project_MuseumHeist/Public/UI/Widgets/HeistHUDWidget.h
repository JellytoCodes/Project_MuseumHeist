#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistHUDWidget.generated.h"

class UTextBlock;
class UHeistGapTrackerWidget;
class UHeistInteractionPromptWidget;
class UHeistPopupWidgetPool;
class UHeistRareLootAlertWidget;
class UHeistSoundPingMarkerWidget;
class UHeistSoundPingWidgetPool;
class UHeistUserWidgetBase;
class UPanelWidget;
class UWidget;

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
		class UHeistGapTrackerViewModel* InGapTrackerViewModel,
		class UHeistInteractionComponent* InInteractionComponent);

private:
	void RefreshHUDPresentation();
	void SetupPopupFeedbackPresentation();
	void SetupSoundPingPresentation();
	void ResolveInteractionChildWidgets();
	UHeistInteractionPromptWidget* ResolveInteractionChildWidget(
		FName WidgetName,
		UHeistInteractionPromptWidget* ExistingWidget) const;
	void ResolveRareLootChildWidgets();
	UHeistRareLootAlertWidget* ResolveRareLootChildWidget(
		FName WidgetName,
		UHeistRareLootAlertWidget* ExistingWidget) const;
	void ResolveGapTrackerChildWidget();
	void ResolveStatusFeedbackChildWidgets();
	void RefreshStatusFeedbackPresentation(bool bStunned, bool bStunImmune, bool bInSmoke);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|HUD", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistHUDViewModel> HUDViewModel;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|HUD", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistInventoryViewModel> InventoryViewModel;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|HUD", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistQuickSlotViewModel> QuickSlotViewModel;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|HUD", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistGapTrackerViewModel> GapTrackerViewModel;

	UPROPERTY(Transient)
	TObjectPtr<UHeistInteractionComponent> InteractionComponent;

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

#pragma region Debug

public:
	void DebugDumpFeedbackState() const;
	void DebugDumpSoundPingMarkers() const;
	void DebugRunSoundPingPoolTest();

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

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UHeistInteractionPromptWidget> InteractionPromptWidget;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UHeistInteractionPromptWidget> ActionProgressWidget;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UHeistRareLootAlertWidget> RareLootWarningWidget;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UHeistRareLootAlertWidget> RareLootMarkerWidget;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UHeistGapTrackerWidget> GapTrackerWidget;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UHeistUserWidgetBase> StatusFeedbackWidget;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> StatusFeedbackContainer;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusFeedbackText;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> StatusStunnedVignette;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> StatusImmuneVignette;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> StatusSmokeVignette;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UPanelWidget> PopupFeedbackLayer;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Feedback", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UHeistUserWidgetBase> PopupFeedbackWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Feedback", meta = (ClampMin = "1", ClampMax = "5", AllowPrivateAccess = "true"))
	int32 PopupFeedbackCapacity = 3;

	UPROPERTY(Transient)
	TObjectPtr<UHeistPopupWidgetPool> PopupWidgetPool;

	bool bStatusFeedbackInitialized = false;
	bool bCachedStatusStunned = false;
	bool bCachedStatusStunImmune = false;
	bool bCachedStatusInSmoke = false;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UPanelWidget> SoundPingMarkerLayer;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|SoundPing", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UHeistSoundPingMarkerWidget> SoundPingMarkerWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|SoundPing", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float SoundPingMarkerScreenMarginPixels = 80.0f;

	UPROPERTY(Transient)
	TObjectPtr<UHeistSoundPingWidgetPool> SoundPingWidgetPool;

#pragma endregion
};
