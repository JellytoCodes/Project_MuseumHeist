#include "UI/ViewModels/HeistGapTrackerViewModel.h"

#include "Core/HeistGameState.h"
#include "Core/HeistPlayerState.h"

#pragma region Construction

UHeistGapTrackerViewModel::UHeistGapTrackerViewModel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#pragma endregion

#pragma region Lifecycle

void UHeistGapTrackerViewModel::BeginDestroy()
{
	if (IsValid(GameState))
	{
		GameState->GetGapTrackerStateChangedDelegate().RemoveAll(this);
	}
	if (IsValid(LocalPlayerState))
	{
		LocalPlayerState->GetGapTrackerDirectionChangedDelegate().RemoveAll(this);
	}

	Super::BeginDestroy();
}

#pragma endregion

#pragma region Setup

void UHeistGapTrackerViewModel::SetupViewModel(
	AHeistGameState* InGameState,
	AHeistPlayerState* InLocalPlayerState)
{
	if (GameState != InGameState && IsValid(GameState))
	{
		GameState->GetGapTrackerStateChangedDelegate().RemoveAll(this);
	}
	if (LocalPlayerState != InLocalPlayerState && IsValid(LocalPlayerState))
	{
		LocalPlayerState->GetGapTrackerDirectionChangedDelegate().RemoveAll(this);
	}

	GameState = InGameState;
	LocalPlayerState = InLocalPlayerState;
	if (IsValid(GameState))
	{
		GameState->GetGapTrackerStateChangedDelegate().RemoveAll(this);
		GameState->GetGapTrackerStateChangedDelegate().AddUObject(
			this,
			&UHeistGapTrackerViewModel::HandleGlobalStateChanged);
	}
	if (IsValid(LocalPlayerState))
	{
		LocalPlayerState->GetGapTrackerDirectionChangedDelegate().RemoveAll(this);
		LocalPlayerState->GetGapTrackerDirectionChangedDelegate().AddUObject(
			this,
			&UHeistGapTrackerViewModel::HandleDirectionChanged);
	}

	RefreshGapTrackerState();
}

void UHeistGapTrackerViewModel::RefreshGapTrackerState()
{
	const bool bNewActive = IsValid(GameState) && GameState->IsGapTrackerActive();
	const bool bNewLocalPlayerLeader = bNewActive
		&& IsValid(LocalPlayerState)
		&& GameState->GetGapTrackerLeaderPlayerId() == LocalPlayerState->HeistPlayerId;
	const FVector NewDirection = IsValid(LocalPlayerState)
		? LocalPlayerState->GetGapTrackerDirection()
		: FVector::ZeroVector;

	UE_MVVM_SET_PROPERTY_VALUE(bGapTrackerActive, bNewActive);
	UE_MVVM_SET_PROPERTY_VALUE(bLocalPlayerLeader, bNewLocalPlayerLeader);
	UE_MVVM_SET_PROPERTY_VALUE(bShowDirectionArrow, bNewActive && !bNewLocalPlayerLeader && !NewDirection.IsNearlyZero());
	UE_MVVM_SET_PROPERTY_VALUE(bShowLeaderWarning, bNewActive && bNewLocalPlayerLeader);
	UE_MVVM_SET_PROPERTY_VALUE(Direction, NewDirection);
	UE_MVVM_SET_PROPERTY_VALUE(
		DirectionAngleDegrees,
		NewDirection.IsNearlyZero()
			? 0.0f
			: FMath::RadiansToDegrees(FMath::Atan2(NewDirection.Y, NewDirection.X)));
}

void UHeistGapTrackerViewModel::HandleGlobalStateChanged(const bool, const int32)
{
	RefreshGapTrackerState();
}

void UHeistGapTrackerViewModel::HandleDirectionChanged(const FVector&)
{
	RefreshGapTrackerState();
}

#pragma endregion

#pragma region Presentation

bool UHeistGapTrackerViewModel::IsGapTrackerActive() const
{
	return bGapTrackerActive;
}

bool UHeistGapTrackerViewModel::IsLocalPlayerLeader() const
{
	return bLocalPlayerLeader;
}

bool UHeistGapTrackerViewModel::ShouldShowDirectionArrow() const
{
	return bShowDirectionArrow;
}

bool UHeistGapTrackerViewModel::ShouldShowLeaderWarning() const
{
	return bShowLeaderWarning;
}

FVector UHeistGapTrackerViewModel::GetDirection() const
{
	return Direction;
}

float UHeistGapTrackerViewModel::GetDirectionAngleDegrees() const
{
	return DirectionAngleDegrees;
}

#pragma endregion
