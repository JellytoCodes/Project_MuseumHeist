#include "UI/Lobby/ViewModels/HeistLobbyViewModel.h"

#include "Core/HeistGameInstance.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistPlayerState.h"
#include "GameFramework/PlayerState.h"

namespace
{
constexpr int32 MaxLobbyPlayers = 4;
}

#pragma region Construction

UHeistLobbyViewModel::UHeistLobbyViewModel(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PlayerCards.SetNum(MaxLobbyPlayers);
}

#pragma endregion

#pragma region Lifecycle

void UHeistLobbyViewModel::BeginDestroy()
{
	UnbindPlayerStateDelegates();
	if (IsValid(GameState))
	{
		GameState->GetPlayerConnectionsChangedDelegate().RemoveAll(this);
		GameState->GetLobbyMapSelectionChangedDelegate().RemoveAll(this);
	}
	if (IsValid(GameInstance))
	{
		GameInstance->GetOnlineSessionStateChangedDelegate().RemoveAll(this);
	}

	Super::BeginDestroy();
}

#pragma endregion

#pragma region Setup

void UHeistLobbyViewModel::SetupViewModel(AHeistGameState* InGameState, AHeistPlayerState* InLocalPlayerState, UHeistGameInstance* InGameInstance,
	AHeistPlayerController* InPlayerController)
{
	UnbindPlayerStateDelegates();
	if (IsValid(GameState))
	{
		GameState->GetPlayerConnectionsChangedDelegate().RemoveAll(this);
		GameState->GetLobbyMapSelectionChangedDelegate().RemoveAll(this);
	}
	if (IsValid(GameInstance))
	{
		GameInstance->GetOnlineSessionStateChangedDelegate().RemoveAll(this);
	}

	GameState = InGameState;
	LocalPlayerState = InLocalPlayerState;
	GameInstance = InGameInstance;
	PlayerController = InPlayerController;

	if (IsValid(GameState))
	{
		GameState->GetPlayerConnectionsChangedDelegate().AddUObject(this, &UHeistLobbyViewModel::HandlePlayerConnectionsChanged);
		GameState->GetLobbyMapSelectionChangedDelegate().AddUObject(this, &UHeistLobbyViewModel::HandleLobbyMapSelectionChanged);
	}
	if (IsValid(GameInstance))
	{
		GameInstance->GetOnlineSessionStateChangedDelegate().AddUObject(this, &UHeistLobbyViewModel::HandleOnlineSessionStateChanged);
	}

	RebindPlayerStateDelegates();
	RefreshLobbyData();
}

void UHeistLobbyViewModel::RefreshLobbyData()
{
	const int32 NewConnectedPlayerCount = IsValid(GameState) ? GameState->GetConnectedPlayerCount() : 0;
	const int32 NewReadyPlayerCount = IsValid(GameState) ? GameState->GetLobbyReadyPlayerCount() : 0;
	const int32 NewLocalPlayerId = IsValid(LocalPlayerState) ? LocalPlayerState->HeistPlayerId : INDEX_NONE;
	const FString ActiveJoinCode = IsValid(GameInstance) ? GameInstance->GetActiveJoinCode() : FString();
	const bool bOperationPending = IsValid(GameInstance) && GameInstance->IsOnlineSessionOperationPending();
	const bool bSessionActive = IsValid(GameInstance)
		&& (GameInstance->IsHostingOnlineSession() || GameInstance->IsJoinedOnlineSession() || GameInstance->HasActiveNamedOnlineSession());
	const bool bLobbyPhase = IsValid(GameState) && GameState->GetMatchPhase() == EHeistMatchPhase::Lobby;
	const bool bLocalHost = IsValid(PlayerController) && PlayerController->HasAuthority() && IsValid(GameInstance) && GameInstance->IsHostingOnlineSession();
	const FName NewSelectedMapId = IsValid(GameState) ? GameState->GetSelectedLobbyMapId()
		: (IsValid(GameInstance) ? GameInstance->GetSelectedMapId() : FName(TEXT("M01")));
	const bool bNewRandomMapSelection = IsValid(GameState) ? GameState->IsRandomLobbyMapSelection()
		: (IsValid(GameInstance) && GameInstance->IsRandomMapSelection());

	UE_MVVM_SET_PROPERTY_VALUE(ConnectedPlayerCount, NewConnectedPlayerCount);
	UE_MVVM_SET_PROPERTY_VALUE(ReadyPlayerCount, NewReadyPlayerCount);
	UE_MVVM_SET_PROPERTY_VALUE(LocalPlayerId, NewLocalPlayerId);
	UE_MVVM_SET_PROPERTY_VALUE(PlayerCountText,
		FText::Format(NSLOCTEXT("HeistLobby", "PlayerCountFormat", "{0} / 4"), FText::AsNumber(NewConnectedPlayerCount)));
	UE_MVVM_SET_PROPERTY_VALUE(JoinCodeText, ActiveJoinCode.IsEmpty() ? FText::GetEmpty() : FText::FromString(ActiveJoinCode));
	UE_MVVM_SET_PROPERTY_VALUE(SelectedMapId, NewSelectedMapId);
	UE_MVVM_SET_PROPERTY_VALUE(bRandomMapSelection, bNewRandomMapSelection);
	UE_MVVM_SET_PROPERTY_VALUE(bCanRequestLeaveSession, IsValid(PlayerController) && IsValid(GameInstance) && !bOperationPending && bSessionActive);
	UE_MVVM_SET_PROPERTY_VALUE(bCanSelectMap, bLocalHost && bLobbyPhase && !bOperationPending && !GameInstance->IsMapSelectionUpdatePending());
	UE_MVVM_SET_PROPERTY_VALUE(bCanToggleLocalReady, IsValid(PlayerController) && IsValid(LocalPlayerState) && bLobbyPhase && !bOperationPending);
	UE_MVVM_SET_PROPERTY_VALUE(bCanStartGame, bLocalHost && bLobbyPhase && !bOperationPending && GameState->AreAllConnectedPlayersLobbyReady());

	RefreshPlayerCards();
	SnapshotChangedDelegate.Broadcast();
}

FHeistLobbySnapshotChanged& UHeistLobbyViewModel::GetSnapshotChangedDelegate()
{
	return SnapshotChangedDelegate;
}

void UHeistLobbyViewModel::RequestLeaveSession()
{
	if (IsValid(PlayerController) && bCanRequestLeaveSession)
	{
		PlayerController->RequestLeaveOnlineSession();
	}
}

void UHeistLobbyViewModel::RequestSelectMap(const FName RequestedMapId)
{
	if (IsValid(PlayerController) && bCanSelectMap)
	{
		PlayerController->RequestSetLobbyMapSelection(RequestedMapId);
	}
}

void UHeistLobbyViewModel::RequestToggleLocalReady()
{
	if (IsValid(PlayerController) && IsValid(LocalPlayerState) && bCanToggleLocalReady)
	{
		PlayerController->RequestSetLobbyReady(!LocalPlayerState->IsLobbyReady());
	}
}

void UHeistLobbyViewModel::RequestStartGame()
{
	if (IsValid(PlayerController) && bCanStartGame)
	{
		PlayerController->RequestStartSelectedGameplayMap();
	}
}

void UHeistLobbyViewModel::HandlePlayerConnectionsChanged(const int32)
{
	RebindPlayerStateDelegates();
	RefreshLobbyData();
}

void UHeistLobbyViewModel::HandlePlayerIdentityChanged(const int32)
{
	RefreshLobbyData();
}

void UHeistLobbyViewModel::HandlePlayerReadyChanged(const bool)
{
	RefreshLobbyData();
}

void UHeistLobbyViewModel::HandleOnlineSessionStateChanged()
{
	RefreshLobbyData();
}

void UHeistLobbyViewModel::HandleLobbyMapSelectionChanged(const FName, const bool, const int32)
{
	RefreshLobbyData();
}

void UHeistLobbyViewModel::RebindPlayerStateDelegates()
{
	UnbindPlayerStateDelegates();
	if (!IsValid(GameState))
	{
		return;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		AHeistPlayerState* HeistPlayerState = Cast<AHeistPlayerState>(PlayerState);
		if (!IsValid(HeistPlayerState))
		{
			continue;
		}

		HeistPlayerState->GetPlayerIdentityChangedDelegate().AddUObject(this, &UHeistLobbyViewModel::HandlePlayerIdentityChanged);
		HeistPlayerState->GetLobbyReadyChangedDelegate().AddUObject(this, &UHeistLobbyViewModel::HandlePlayerReadyChanged);
		BoundPlayerStates.Add(HeistPlayerState);
	}
}

void UHeistLobbyViewModel::UnbindPlayerStateDelegates()
{
	for (const TWeakObjectPtr<AHeistPlayerState>& BoundPlayerState : BoundPlayerStates)
	{
		if (AHeistPlayerState* HeistPlayerState = BoundPlayerState.Get(); IsValid(HeistPlayerState))
		{
			HeistPlayerState->GetPlayerIdentityChangedDelegate().RemoveAll(this);
			HeistPlayerState->GetLobbyReadyChangedDelegate().RemoveAll(this);
		}
	}
	BoundPlayerStates.Reset();
}

void UHeistLobbyViewModel::RefreshPlayerCards()
{
	PlayerCards.SetNum(MaxLobbyPlayers);
	for (int32 SlotIndex = 0; SlotIndex < MaxLobbyPlayers; ++SlotIndex)
	{
		FHeistLobbyPlayerCardData& CardData = PlayerCards[SlotIndex];
		CardData = FHeistLobbyPlayerCardData();
		CardData.PlayerSlot = SlotIndex + 1;
	}

	if (!IsValid(GameState))
	{
		return;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		const AHeistPlayerState* HeistPlayerState = Cast<AHeistPlayerState>(PlayerState);
		if (!IsValid(HeistPlayerState) || HeistPlayerState->HeistPlayerId < 1 || HeistPlayerState->HeistPlayerId > MaxLobbyPlayers)
		{
			continue;
		}

		FHeistLobbyPlayerCardData& CardData = PlayerCards[HeistPlayerState->HeistPlayerId - 1];
		CardData.bOccupied = true;
		CardData.bLocalPlayer = HeistPlayerState == LocalPlayerState;
		CardData.bReady = HeistPlayerState->IsLobbyReady();
		CardData.PlayerName = HeistPlayerState->GetHeistDisplayName();
		CardData.PlatformUserId = HeistPlayerState->GetUniqueId().IsValid() ? HeistPlayerState->GetUniqueId().ToString() : FString();
	}
}

#pragma endregion

#pragma region LobbyData

int32 UHeistLobbyViewModel::GetConnectedPlayerCount() const
{
	return ConnectedPlayerCount;
}

int32 UHeistLobbyViewModel::GetReadyPlayerCount() const
{
	return ReadyPlayerCount;
}

int32 UHeistLobbyViewModel::GetLocalPlayerId() const
{
	return LocalPlayerId;
}

const FText& UHeistLobbyViewModel::GetPlayerCountText() const
{
	return PlayerCountText;
}

const FText& UHeistLobbyViewModel::GetJoinCodeText() const
{
	return JoinCodeText;
}

FName UHeistLobbyViewModel::GetSelectedMapId() const
{
	return SelectedMapId;
}

bool UHeistLobbyViewModel::IsRandomMapSelection() const
{
	return bRandomMapSelection;
}

bool UHeistLobbyViewModel::TryGetPlayerCardData(const int32 PlayerSlot, FHeistLobbyPlayerCardData& OutPlayerCardData) const
{
	if (PlayerSlot < 1 || PlayerSlot > PlayerCards.Num())
	{
		return false;
	}

	OutPlayerCardData = PlayerCards[PlayerSlot - 1];
	return true;
}

bool UHeistLobbyViewModel::CanRequestLeaveSession() const
{
	return bCanRequestLeaveSession;
}

bool UHeistLobbyViewModel::CanSelectMap() const
{
	return bCanSelectMap;
}

bool UHeistLobbyViewModel::CanToggleLocalReady() const
{
	return bCanToggleLocalReady;
}

bool UHeistLobbyViewModel::CanStartGame() const
{
	return bCanStartGame;
}

#pragma endregion
