#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "HeistPopupWidgetPool.generated.h"

UCLASS()
class PROJECT_MUSEUMHEIST_API UHeistPopupWidgetPool : public UObject
{
	GENERATED_BODY()

#pragma region Lifecycle

  public:
	void SetupPool(class AHeistPlayerController* InPlayerController, class UPanelWidget* InPopupLayer, TSubclassOf<class UHeistUserWidgetBase> InPopupWidgetClass, int32 InCapacity);
	void ShutdownPool();

#pragma endregion

#pragma region Debug

  public:
	void DebugDumpState() const;

#pragma endregion

#pragma region Pool

  private:
	struct FActivePopup
	{
		UHeistUserWidgetBase* Widget = nullptr;
		FText Message;
		float EndWorldTime = 0.0f;
		int32 SequenceId = 0;
	};

	void HandlePopupFeedbackRequested(const FText& Message, float DurationSeconds);
	void ShowPopupWidget(UHeistUserWidgetBase* PopupWidget, const FText& Message) const;
	void ReleasePopupAt(int32 ActivePopupIndex, const TCHAR* Reason);
	void ReleaseAllPopups(const TCHAR* Reason);
	UHeistUserWidgetBase* AcquirePopupWidget();
	void RefreshExpirationTimer();
	void HandleExpirationTimer();

	UPROPERTY(Transient)
	TObjectPtr<AHeistPlayerController> PlayerController;

	UPROPERTY(Transient)
	TObjectPtr<UPanelWidget> PopupLayer;

	UPROPERTY(Transient)
	TSubclassOf<UHeistUserWidgetBase> PopupWidgetClass;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHeistUserWidgetBase>> AllPopupWidgets;

	TArray<UHeistUserWidgetBase*> AvailablePopupWidgets;
	TArray<FActivePopup> ActivePopups;
	FTimerHandle ExpirationTimerHandle;
	int32 NextSequenceId = 1;
	int32 Capacity = 3;
	bool bInitialized = false;

#pragma endregion
};
