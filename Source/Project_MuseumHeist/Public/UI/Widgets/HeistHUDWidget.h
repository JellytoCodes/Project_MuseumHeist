#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistHUDWidget.generated.h"

class UTextBlock;
class UHeistInteractionPromptWidget;
class UHeistPopupWidgetPool;
class UHeistUserWidgetBase;
class UPanelWidget;
class UWidget;
class UAudioComponent;
class USoundBase;
class AHeistPlayerController;

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
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

#pragma endregion

#pragma region ViewModels

  public:
	void SetupHUDWidget(class UHeistHUDViewModel* InHUDViewModel, class UHeistInventoryViewModel* InInventoryViewModel, class UHeistQuickSlotViewModel* InQuickSlotViewModel,
						class UHeistInteractionComponent* InInteractionComponent);
	void RefreshHUDPresentation();

  private:
	void SetupPopupFeedbackPresentation();
	void ResolveInteractionChildWidgets();
	void ResolveCrosshairWidgets();
	void RefreshCrosshairPresentation(AActor* TargetActor, bool bAvailable);
	void RefreshToolPresentation();
	void RefreshAlertPresentation();
	void RefreshCrewStatusPresentation();
	void ResolveCrewPresentationWidgets();
	void RefreshLockdownCountdown();
	void SetupTutorialPresentation();
	void RefreshTutorialPresentation();
	void ApplyAlertAudioLayers();
	void StopAlertAudioLayers();
	UHeistInteractionPromptWidget* ResolveInteractionChildWidget(FName WidgetName, UHeistInteractionPromptWidget* ExistingWidget) const;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|HUD", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistHUDViewModel> HUDViewModel;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|HUD", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistInventoryViewModel> InventoryViewModel;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|HUD", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistQuickSlotViewModel> QuickSlotViewModel;

	UPROPERTY(Transient)
	TObjectPtr<UHeistInteractionComponent> InteractionComponent;

	UPROPERTY(Transient)
	TObjectPtr<AHeistPlayerController> TutorialPlayerController;

  protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|HUD", meta = (DisplayName = "HUD Sources Ready"))
	void BP_OnHUDSourcesReady();

	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|HUD", meta = (DisplayName = "Refresh HUD Presentation"))
	void BP_RefreshHUDPresentation(int32 LocalLootScore, float LocalLootWeight, int32 ConnectedPlayerCount, bool bLocalPlayerEscaped, bool bEscapePhaseOpen, bool bEscapeCastActive,
								   float EscapeCastEndServerTime);

#pragma endregion

#pragma region Debug

  public:
	/** Clears local-only HUD state that must not survive Result, Lobby, or seamless travel. */
	void ResetHiddenPresentationState();
	bool IsHiddenPresentationStateReset() const;
	void DebugDumpFirstPersonHUDState() const;
	void DebugDumpFeedbackState() const;
	void DebugDumpAlertPresentationState() const;
	void DebugDumpTutorialPresentationState() const;
	bool IsAlertPresentationContractSatisfied() const;
	bool IsTutorialPresentationContractSatisfied() const;

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

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> AlertText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> LockdownCountdownText;

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

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UPanelWidget> TeamStatusContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<class UBorder> StunOverlay;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> TutorialCardContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TutorialTitleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TutorialBodyText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TutorialProgressText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Feedback", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UHeistUserWidgetBase> PopupFeedbackWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Feedback", meta = (ClampMin = "1", ClampMax = "5", AllowPrivateAccess = "true"))
	int32 PopupFeedbackCapacity = 3;

	UPROPERTY(Transient)
	TObjectPtr<UHeistPopupWidgetPool> PopupWidgetPool;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Alert|Audio", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> SuspenseMusic;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Alert|Audio", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> AlarmMusic;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Alert|Audio", meta = (ClampMin = "0.0", ClampMax = "1.0", AllowPrivateAccess = "true"))
	float SuspenseMusicVolume = 0.55f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Alert|Audio", meta = (ClampMin = "0.0", ClampMax = "1.0", AllowPrivateAccess = "true"))
	float AlarmMusicVolume = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Alert|Audio", meta = (ClampMin = "0.0", ClampMax = "5.0", AllowPrivateAccess = "true"))
	float AlertMusicFadeSeconds = 0.35f;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> SuspenseMusicComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> AlarmMusicComponent;

	EHeistAlertLevel LastAppliedAudioAlertLevel = EHeistAlertLevel::Quiet;
	bool bAlertAudioInitialized = false;
	int32 LastDisplayedLockdownSeconds = INDEX_NONE;

#pragma endregion
};
