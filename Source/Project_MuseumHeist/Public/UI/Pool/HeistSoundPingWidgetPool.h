#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "UObject/Object.h"

#include "HeistSoundPingWidgetPool.generated.h"

UCLASS()
class PROJECT_MUSEUMHEIST_API UHeistSoundPingWidgetPool : public UObject
{
	GENERATED_BODY()

#pragma region Construction

public:
	UHeistSoundPingWidgetPool(const FObjectInitializer& ObjectInitializer);

#pragma endregion

#pragma region Lifecycle

public:
	void SetupPool(
		class APlayerController* InOwningPlayerController,
		class AHeistGameState* InGameState,
		class UPanelWidget* InMarkerLayer,
		TSubclassOf<class UHeistSoundPingMarkerWidget> InMarkerWidgetClass,
		float InScreenMarginPixels);
	void ShutdownPool();

#pragma endregion

#pragma region Debug

public:
	void DebugDumpState() const;
	void DebugRunPresentationTest();

#pragma endregion

#pragma region Pool

private:
	struct FActiveSoundPingMarker
	{
		UHeistSoundPingMarkerWidget* Widget = nullptr;
		FHeistSoundPingEvent Event;
		FVector2D ScreenDirection = FVector2D::ZeroVector;
		int32 Priority = MAX_int32;
		float EndServerTime = 0.0f;
		float LastEventServerTime = 0.0f;
	};

	void HandleSoundPingReported(const FHeistSoundPingEvent& SoundPingEvent);
	void ActivateMarker(const FHeistSoundPingEvent& SoundPingEvent, const FVector2D& ScreenDirection, int32 Priority);
	void ReleaseMarkerAt(int32 ActiveMarkerIndex, const TCHAR* Reason);
	void ReleaseAllMarkers(const TCHAR* Reason);
	UHeistSoundPingMarkerWidget* AcquireMarkerWidget();
	bool ComputeScreenDirection(const FVector& WorldLocation, FVector2D& OutScreenDirection) const;
	FVector2D ComputeScreenEdgeTranslation(const FVector2D& ScreenDirection) const;
	void RefreshExpirationTimer();
	void HandleExpirationTimer();
	float GetServerTimeSeconds() const;
	static int32 ResolvePriority(EHeistSoundPingType PingType);
	static float ResolveDirectionAngleDegrees(const FVector2D& ScreenDirection);

	static constexpr int32 MaxActiveMarkerCount = 4;
	static constexpr float MergeAngleDegrees = 15.0f;
	static constexpr float MergeWindowSeconds = 0.5f;

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> OwningPlayerController;

	UPROPERTY(Transient)
	TObjectPtr<AHeistGameState> GameState;

	UPROPERTY(Transient)
	TObjectPtr<UPanelWidget> MarkerLayer;

	UPROPERTY(Transient)
	TSubclassOf<UHeistSoundPingMarkerWidget> MarkerWidgetClass;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHeistSoundPingMarkerWidget>> AllMarkerWidgets;

	TArray<UHeistSoundPingMarkerWidget*> AvailableMarkerWidgets;
	TArray<FActiveSoundPingMarker> ActiveMarkers;
	FTimerHandle ExpirationTimerHandle;
	float ScreenMarginPixels = 80.0f;
	bool bInitialized = false;

#pragma endregion
};
