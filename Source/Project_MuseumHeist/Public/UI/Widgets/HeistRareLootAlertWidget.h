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

private:
	void RefreshRareLootPresentation();

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|RareLoot", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistHUDViewModel> ViewModel;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|RareLoot", meta = (DisplayName = "Refresh Rare Loot Presentation"))
	void BP_RefreshRareLootPresentation(
		bool bIncomingWarningActive,
		bool bDirectionMarkerActive,
		int32 EventIndex,
		FName ItemId,
		float SpawnServerTime);

	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|RareLoot", meta = (DisplayName = "Update Rare Loot Direction Marker"))
	void BP_UpdateRareLootDirectionMarker(FVector2D Direction, float AngleDegrees);

#pragma endregion
};
