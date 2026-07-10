#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistRareLootAlertWidget.generated.h"

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistRareLootAlertWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

#pragma region Construction

public:
	UHeistRareLootAlertWidget(const FObjectInitializer& ObjectInitializer);

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

#pragma endregion

#pragma region ViewModel

public:
	void SetupRareLootAlertWidget(class UHeistHUDViewModel* InViewModel);
	class UHeistHUDViewModel* GetHUDViewModel() const;

private:
	void RefreshRareLootPresentation();
	void RefreshWarningCountdownText();
	void RefreshDirectionMarkerPresentation();
	float GetRareLootWarningRemainingSeconds() const;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|RareLoot", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistHUDViewModel> ViewModel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|RareLoot", meta = (AllowPrivateAccess = "true"))
	bool bShowIncomingWarning = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|RareLoot", meta = (AllowPrivateAccess = "true"))
	bool bShowDirectionMarker = true;

#pragma endregion

#pragma region BindWidgets

private:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<class UWidget> IncomingWarningContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<class UTextBlock> RareLootItemText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<class UTextBlock> RareLootCountdownText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<class UTextBlock> RareLootStatusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<class UWidget> DirectionMarkerContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<class UWidget> DirectionMarkerArrow;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<class UTextBlock> DirectionMarkerText;

#pragma endregion
};
