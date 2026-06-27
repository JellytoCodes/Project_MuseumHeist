#include "UI/ViewModels/HeistHUDViewModel.h"

#include "Core/HeistGameState.h"

#pragma region Construction

UHeistHUDViewModel::UHeistHUDViewModel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#pragma endregion

#pragma region Lifecycle

void UHeistHUDViewModel::BeginDestroy()
{
	if (IsValid(GameState))
	{
		GameState->GetRareLootEventStateChangedDelegate().RemoveAll(this);
	}

	Super::BeginDestroy();
}

#pragma endregion

#pragma region Setup

void UHeistHUDViewModel::SetupViewModel(AHeistGameState* InGameState)
{
	if (GameState != InGameState && IsValid(GameState))
	{
		GameState->GetRareLootEventStateChangedDelegate().RemoveAll(this);
	}

	GameState = InGameState;
	if (IsValid(GameState))
	{
		GameState->GetRareLootEventStateChangedDelegate().RemoveAll(this);
		GameState->GetRareLootEventStateChangedDelegate().AddUObject(
			this,
			&UHeistHUDViewModel::HandleRareLootEventStateChanged);
	}

	RefreshRareLootState();
}

void UHeistHUDViewModel::RefreshRareLootState()
{
	const FHeistRareLootEventState State = IsValid(GameState)
		? GameState->GetRareLootEventState()
		: FHeistRareLootEventState();

	UE_MVVM_SET_PROPERTY_VALUE(bRareLootIncoming, State.bIncomingWarningActive);
	UE_MVVM_SET_PROPERTY_VALUE(bRareLootDirectionMarkerVisible, State.bDirectionMarkerActive);
	UE_MVVM_SET_PROPERTY_VALUE(RareLootEventIndex, State.EventIndex);
	UE_MVVM_SET_PROPERTY_VALUE(RareLootItemId, State.ItemId);
	UE_MVVM_SET_PROPERTY_VALUE(RareLootWorldLocation, FVector(State.WorldLocation));
	UE_MVVM_SET_PROPERTY_VALUE(RareLootSpawnServerTime, State.SpawnServerTime);
	RareLootPresentationChangedDelegate.Broadcast();
}

void UHeistHUDViewModel::HandleRareLootEventStateChanged(const FHeistRareLootEventState&)
{
	RefreshRareLootState();
}

FHeistRareLootPresentationChanged& UHeistHUDViewModel::GetRareLootPresentationChangedDelegate()
{
	return RareLootPresentationChangedDelegate;
}

#pragma endregion

#pragma region RareLootPresentation

bool UHeistHUDViewModel::IsRareLootIncoming() const
{
	return bRareLootIncoming;
}

bool UHeistHUDViewModel::IsRareLootDirectionMarkerVisible() const
{
	return bRareLootDirectionMarkerVisible;
}

int32 UHeistHUDViewModel::GetRareLootEventIndex() const
{
	return RareLootEventIndex;
}

FName UHeistHUDViewModel::GetRareLootItemId() const
{
	return RareLootItemId;
}

FVector UHeistHUDViewModel::GetRareLootWorldLocation() const
{
	return RareLootWorldLocation;
}

float UHeistHUDViewModel::GetRareLootSpawnServerTime() const
{
	return RareLootSpawnServerTime;
}

#pragma endregion
