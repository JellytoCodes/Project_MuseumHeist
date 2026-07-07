#include "UI/ViewModels/HeistHUDViewModel.h"

#include "Character/Components/HeistActionComponent.h"
#include "Character/Components/HeistStatusComponent.h"
#include "Core/HeistGameplayTags.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerState.h"

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
		GameState->GetPlayerConnectionsChangedDelegate().RemoveAll(this);
		GameState->GetEscapePhaseStateChangedDelegate().RemoveAll(this);
		GameState->GetRareLootEventStateChangedDelegate().RemoveAll(this);
	}

	if (IsValid(LocalPlayerState))
	{
		LocalPlayerState->GetPlayerIdentityChangedDelegate().RemoveAll(this);
		LocalPlayerState->GetLootTotalsChangedDelegate().RemoveAll(this);
		LocalPlayerState->GetEscapeStateChangedDelegate().RemoveAll(this);
	}

	if (IsValid(StatusComponent))
	{
		StatusComponent->GetStatusTagsChangedDelegate().RemoveAll(this);
	}

	if (IsValid(ActionComponent))
	{
		ActionComponent->GetActionStateChangedDelegate().RemoveAll(this);
	}

	Super::BeginDestroy();
}

#pragma endregion

#pragma region Setup

void UHeistHUDViewModel::SetupViewModel(
	AHeistGameState* InGameState,
	AHeistPlayerState* InLocalPlayerState,
	UHeistStatusComponent* InStatusComponent,
	UHeistActionComponent* InActionComponent)
{
	if (GameState != InGameState && IsValid(GameState))
	{
		GameState->GetPlayerConnectionsChangedDelegate().RemoveAll(this);
		GameState->GetEscapePhaseStateChangedDelegate().RemoveAll(this);
		GameState->GetRareLootEventStateChangedDelegate().RemoveAll(this);
	}

	if (LocalPlayerState != InLocalPlayerState && IsValid(LocalPlayerState))
	{
		LocalPlayerState->GetPlayerIdentityChangedDelegate().RemoveAll(this);
		LocalPlayerState->GetLootTotalsChangedDelegate().RemoveAll(this);
		LocalPlayerState->GetEscapeStateChangedDelegate().RemoveAll(this);
	}

	if (StatusComponent != InStatusComponent && IsValid(StatusComponent))
	{
		StatusComponent->GetStatusTagsChangedDelegate().RemoveAll(this);
	}

	if (ActionComponent != InActionComponent && IsValid(ActionComponent))
	{
		ActionComponent->GetActionStateChangedDelegate().RemoveAll(this);
	}

	GameState = InGameState;
	LocalPlayerState = InLocalPlayerState;
	StatusComponent = InStatusComponent;
	ActionComponent = InActionComponent;

	if (IsValid(GameState))
	{
		GameState->GetPlayerConnectionsChangedDelegate().RemoveAll(this);
		GameState->GetPlayerConnectionsChangedDelegate().AddUObject(
			this,
			&UHeistHUDViewModel::HandlePlayerConnectionsChanged);
		GameState->GetEscapePhaseStateChangedDelegate().RemoveAll(this);
		GameState->GetEscapePhaseStateChangedDelegate().AddUObject(
			this,
			&UHeistHUDViewModel::HandleEscapePhaseStateChanged);
		GameState->GetRareLootEventStateChangedDelegate().RemoveAll(this);
		GameState->GetRareLootEventStateChangedDelegate().AddUObject(
			this,
			&UHeistHUDViewModel::HandleRareLootEventStateChanged);
	}

	if (IsValid(LocalPlayerState))
	{
		LocalPlayerState->GetPlayerIdentityChangedDelegate().RemoveAll(this);
		LocalPlayerState->GetPlayerIdentityChangedDelegate().AddUObject(
			this,
			&UHeistHUDViewModel::HandlePlayerIdentityChanged);
		LocalPlayerState->GetLootTotalsChangedDelegate().RemoveAll(this);
		LocalPlayerState->GetLootTotalsChangedDelegate().AddUObject(
			this,
			&UHeistHUDViewModel::HandleLootTotalsChanged);
		LocalPlayerState->GetEscapeStateChangedDelegate().RemoveAll(this);
		LocalPlayerState->GetEscapeStateChangedDelegate().AddUObject(
			this,
			&UHeistHUDViewModel::HandleEscapeStateChanged);
	}

	if (IsValid(StatusComponent))
	{
		StatusComponent->GetStatusTagsChangedDelegate().RemoveAll(this);
		StatusComponent->GetStatusTagsChangedDelegate().AddUObject(
			this,
			&UHeistHUDViewModel::HandleStatusTagsChanged);
	}

	if (IsValid(ActionComponent))
	{
		ActionComponent->GetActionStateChangedDelegate().RemoveAll(this);
		ActionComponent->GetActionStateChangedDelegate().AddUObject(
			this,
			&UHeistHUDViewModel::HandleActionStateChanged);
	}

	RefreshPresentationState();
	RefreshRareLootState();
}

void UHeistHUDViewModel::RefreshPresentationState()
{
	const FHeistGameplayTags& GameplayTags = FHeistGameplayTags::Get();

	UE_MVVM_SET_PROPERTY_VALUE(
		LocalLootScore,
		IsValid(LocalPlayerState) ? LocalPlayerState->GetTotalLootScore() : 0);
	UE_MVVM_SET_PROPERTY_VALUE(
		LocalLootWeight,
		IsValid(LocalPlayerState) ? LocalPlayerState->GetTotalLootWeight() : 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(
		LocalPlayerId,
		IsValid(LocalPlayerState) ? LocalPlayerState->HeistPlayerId : INDEX_NONE);
	UE_MVVM_SET_PROPERTY_VALUE(
		ConnectedPlayerCount,
		IsValid(GameState) ? GameState->GetConnectedPlayerCount() : 0);
	UE_MVVM_SET_PROPERTY_VALUE(
		bLocalPlayerEscaped,
		IsValid(LocalPlayerState) && LocalPlayerState->IsEscaped());
	UE_MVVM_SET_PROPERTY_VALUE(
		bEscapePhaseOpen,
		IsValid(GameState) && GameState->IsEscapePhaseOpen());
	UE_MVVM_SET_PROPERTY_VALUE(
		bStunned,
		IsValid(StatusComponent) && StatusComponent->IsStunned());
	UE_MVVM_SET_PROPERTY_VALUE(
		bStunImmune,
		IsValid(StatusComponent) && StatusComponent->IsStunImmune());
	UE_MVVM_SET_PROPERTY_VALUE(
		bInSmoke,
		IsValid(StatusComponent)
			&& StatusComponent->HasStatusTag(GameplayTags.State_InSmoke));
	UE_MVVM_SET_PROPERTY_VALUE(
		bEscapeCastActive,
		IsValid(ActionComponent) && ActionComponent->IsEscapeCastActive());
	UE_MVVM_SET_PROPERTY_VALUE(
		EscapeCastEndServerTime,
		IsValid(ActionComponent)
			? ActionComponent->GetEscapeCastEndServerTime()
			: 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(
		bTrapPlacementCastActive,
		IsValid(ActionComponent) && ActionComponent->IsTrapPlacementCastActive());
	UE_MVVM_SET_PROPERTY_VALUE(
		TrapPlacementCastEndServerTime,
		IsValid(ActionComponent)
			? ActionComponent->GetTrapPlacementCastEndServerTime()
			: 0.0f);

	PresentationChangedDelegate.Broadcast();
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

void UHeistHUDViewModel::HandlePlayerConnectionsChanged(const int32)
{
	RefreshPresentationState();
}

void UHeistHUDViewModel::HandlePlayerIdentityChanged(const int32)
{
	RefreshPresentationState();
}

void UHeistHUDViewModel::HandleEscapePhaseStateChanged(const bool)
{
	RefreshPresentationState();
}

void UHeistHUDViewModel::HandleLootTotalsChanged(const int32, const float)
{
	RefreshPresentationState();
}

void UHeistHUDViewModel::HandleEscapeStateChanged(const bool)
{
	RefreshPresentationState();
}

void UHeistHUDViewModel::HandleStatusTagsChanged(const TArray<FHeistTimedTagState>&)
{
	RefreshPresentationState();
}

void UHeistHUDViewModel::HandleActionStateChanged()
{
	RefreshPresentationState();
}

FHeistHUDPresentationChanged& UHeistHUDViewModel::GetPresentationChangedDelegate()
{
	return PresentationChangedDelegate;
}

FHeistRareLootPresentationChanged& UHeistHUDViewModel::GetRareLootPresentationChangedDelegate()
{
	return RareLootPresentationChangedDelegate;
}

#pragma endregion

#pragma region GeneralPresentation

int32 UHeistHUDViewModel::GetLocalLootScore() const
{
	return LocalLootScore;
}

float UHeistHUDViewModel::GetLocalLootWeight() const
{
	return LocalLootWeight;
}

int32 UHeistHUDViewModel::GetLocalPlayerId() const
{
	return LocalPlayerId;
}

int32 UHeistHUDViewModel::GetConnectedPlayerCount() const
{
	return ConnectedPlayerCount;
}

bool UHeistHUDViewModel::IsLocalPlayerEscaped() const
{
	return bLocalPlayerEscaped;
}

bool UHeistHUDViewModel::IsEscapePhaseOpen() const
{
	return bEscapePhaseOpen;
}

bool UHeistHUDViewModel::IsStunned() const
{
	return bStunned;
}

bool UHeistHUDViewModel::IsStunImmune() const
{
	return bStunImmune;
}

bool UHeistHUDViewModel::IsInSmoke() const
{
	return bInSmoke;
}

bool UHeistHUDViewModel::IsEscapeCastActive() const
{
	return bEscapeCastActive;
}

float UHeistHUDViewModel::GetEscapeCastEndServerTime() const
{
	return EscapeCastEndServerTime;
}

bool UHeistHUDViewModel::IsTrapPlacementCastActive() const
{
	return bTrapPlacementCastActive;
}

float UHeistHUDViewModel::GetTrapPlacementCastEndServerTime() const
{
	return TrapPlacementCastEndServerTime;
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
