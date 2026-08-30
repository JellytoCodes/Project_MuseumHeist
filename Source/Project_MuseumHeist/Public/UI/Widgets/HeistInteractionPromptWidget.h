#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistInteractionPromptWidget.generated.h"

class UHeistHUDViewModel;
class UHeistInteractionComponent;
class UProgressBar;
class UTextBlock;
class UWidget;

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistInteractionPromptWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

#pragma region Construction

  public:
	UHeistInteractionPromptWidget(const FObjectInitializer& ObjectInitializer);

#pragma endregion

#pragma region Lifecycle

  protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

#pragma endregion

#pragma region Presentation

  public:
	void SetupInteractionPresentation(UHeistInteractionComponent* InInteractionComponent, UHeistHUDViewModel* InHUDViewModel);

  private:
	void RefreshPresentation();
	void RefreshInteractionPrompt(bool bActionActive);
	void RefreshActionProgress();
	void CacheDisplayNames();
	FText ResolveTargetLabel(const AActor* TargetActor) const;
	FText ResolveArtifactDisplayName(FName ArtifactId) const;
	FText ResolveLootDisplayName(FName ItemId) const;
	float GetServerWorldTimeSeconds() const;

	UPROPERTY(Transient)
	TObjectPtr<UHeistInteractionComponent> InteractionComponent;

	UPROPERTY(Transient)
	TObjectPtr<UHeistHUDViewModel> HUDViewModel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Interaction", meta = (AllowPrivateAccess = "true"))
	FText InteractionKeyLabel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> InteractionPromptContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TargetText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> KeyText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> AvailabilityText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> ActionProgressContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ActionTypeText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UProgressBar> ActionProgressBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ActionRemainingText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> CancelHintText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> ObservationReferenceContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ObservationReferenceText;

	FTimerHandle PresentationRefreshTimerHandle;
	FName TrackedActionType = NAME_None;
	float TrackedActionEndServerTime = 0.0f;
	float TrackedActionDuration = 0.0f;
	TMap<FName, FText> ArtifactDisplayNames;
	TMap<FName, FText> LootDisplayNames;

#pragma endregion
};
