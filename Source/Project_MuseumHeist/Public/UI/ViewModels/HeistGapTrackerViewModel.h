#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"

#include "HeistGapTrackerViewModel.generated.h"

UCLASS(BlueprintType)
class PROJECT_MUSEUMHEIST_API UHeistGapTrackerViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

#pragma region Construction

public:
	UHeistGapTrackerViewModel(const FObjectInitializer& ObjectInitializer);

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void BeginDestroy() override;

#pragma endregion

#pragma region Setup

public:
	void SetupViewModel(
		class AHeistGameState* InGameState,
		class AHeistPlayerState* InLocalPlayerState);
	void RefreshGapTrackerState();

private:
	void HandleGlobalStateChanged(bool bActive, int32 LeaderPlayerId);
	void HandleDirectionChanged(const FVector& Direction);

	UPROPERTY(Transient)
	TObjectPtr<AHeistGameState> GameState;

	UPROPERTY(Transient)
	TObjectPtr<AHeistPlayerState> LocalPlayerState;

#pragma endregion

#pragma region Presentation

public:
	bool IsGapTrackerActive() const;
	bool IsLocalPlayerLeader() const;
	bool ShouldShowDirectionArrow() const;
	bool ShouldShowLeaderWarning() const;
	FVector GetDirection() const;
	float GetDirectionAngleDegrees() const;

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|GapTracker", meta = (AllowPrivateAccess = "true"))
	bool bGapTrackerActive = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|GapTracker", meta = (AllowPrivateAccess = "true"))
	bool bLocalPlayerLeader = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|GapTracker", meta = (AllowPrivateAccess = "true"))
	bool bShowDirectionArrow = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|GapTracker", meta = (AllowPrivateAccess = "true"))
	bool bShowLeaderWarning = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|GapTracker", meta = (AllowPrivateAccess = "true"))
	FVector Direction = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|GapTracker", meta = (AllowPrivateAccess = "true"))
	float DirectionAngleDegrees = 0.0f;

#pragma endregion
};
