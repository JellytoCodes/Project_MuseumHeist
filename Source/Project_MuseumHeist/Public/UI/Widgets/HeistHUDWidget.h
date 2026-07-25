#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistHUDWidget.generated.h"

class UTextBlock;
class UHeistInteractionPromptWidget;
class UHeistPopupWidgetPool;
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
	void SetupHUDWidget(class UHeistHUDViewModel* InHUDViewModel, class UHeistInventoryViewModel* InInventoryViewModel, class UHeistQuickSlotViewModel* InQuickSlotViewModel,
						class UHeistInteractionComponent* InInteractionComponent);

  private:
	void RefreshHUDPresentation();
	void SetupPopupFeedbackPresentation();
	void SetupSoundPingPresentation();
	void ResolveInteractionChildWidgets();
	void ResolveCrosshairWidgets();
	void RefreshCrosshairPresentation(AActor* TargetActor, bool bAvailable);
	void RefreshToolPresentation();
	UHeistInteractionPromptWidget* ResolveInteractionChildWidget(FName WidgetName, UHeistInteractionPromptWidget* ExistingWidget) const;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|HUD", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistHUDViewModel> HUDViewModel;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|HUD", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistInventoryViewModel> InventoryViewModel;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|HUD", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistQuickSlotViewModel> QuickSlotViewModel;

	UPROPERTY(Transient)
	TObjectPtr<UHeistInteractionComponent> InteractionComponent;

  protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|HUD", meta = (DisplayName = "HUD Sources Ready"))
	void BP_OnHUDSourcesReady();

	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|HUD", meta = (DisplayName = "Refresh HUD Presentation"))
	void BP_RefreshHUDPresentation(int32 LocalLootScore, float LocalLootWeight, int32 ConnectedPlayerCount, bool bLocalPlayerEscaped, bool bEscapePhaseOpen, bool bEscapeCastActive,
								   float EscapeCastEndServerTime);

#pragma endregion

#pragma region Debug

  public:
	void DebugDumpFirstPersonHUDState() const;
	void DebugDumpFeedbackState() const;
	void DebugDumpSoundPingMarkers() const;
	void DebugRunSoundPingPoolTest();

#pragma endregion

#pragma region Presentation

  private:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ScoreText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ToolText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> WeightText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ActionText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ObjectiveText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> AlertText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UHeistInteractionPromptWidget> InteractionPromptWidget;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UHeistInteractionPromptWidget> ActionProgressWidget;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> CrosshairContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> CrosshairIdleIndicator;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> CrosshairFocusIndicator;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UPanelWidget> PopupFeedbackLayer;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Feedback", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UHeistUserWidgetBase> PopupFeedbackWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Feedback", meta = (ClampMin = "1", ClampMax = "5", AllowPrivateAccess = "true"))
	int32 PopupFeedbackCapacity = 3;

	UPROPERTY(Transient)
	TObjectPtr<UHeistPopupWidgetPool> PopupWidgetPool;

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
