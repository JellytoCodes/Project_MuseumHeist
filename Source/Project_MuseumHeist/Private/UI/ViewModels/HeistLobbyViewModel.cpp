#include "UI/ViewModels/HeistLobbyViewModel.h"

#include "Core/HeistGameState.h"
#include "Core/HeistPlayerState.h"
#include "GameFramework/PlayerState.h"

#pragma region Construction

UHeistLobbyViewModel::UHeistLobbyViewModel(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

#pragma endregion

#pragma region Lifecycle

void UHeistLobbyViewModel::BeginDestroy()
{
	if (IsValid(GameState))
	{
		GameState->GetPlayerConnectionsChangedDelegate().RemoveAll(this);
	}

	Super::BeginDestroy();
}

#pragma endregion

#pragma region Setup

void UHeistLobbyViewModel::SetupViewModel(AHeistGameState* InGameState, AHeistPlayerState* InLocalPlayerState)
{
	if (GameState != InGameState && IsValid(GameState))
	{
		GameState->GetPlayerConnectionsChangedDelegate().RemoveAll(this);
	}

	GameState = InGameState;
	LocalPlayerState = InLocalPlayerState;

	if (IsValid(GameState))
	{
		GameState->GetPlayerConnectionsChangedDelegate().RemoveAll(this);
		GameState->GetPlayerConnectionsChangedDelegate().AddUObject(this, &UHeistLobbyViewModel::HandlePlayerConnectionsChanged);
	}

	RefreshLobbyData();
}

void UHeistLobbyViewModel::RefreshLobbyData()
{
	const int32 NewConnectedPlayerCount = IsValid(GameState) ? GameState->GetConnectedPlayerCount() : 0;
	UE_MVVM_SET_PROPERTY_VALUE(ConnectedPlayerCount, NewConnectedPlayerCount);

	const int32 NewLocalPlayerId = IsValid(LocalPlayerState) ? LocalPlayerState->HeistPlayerId : INDEX_NONE;
	UE_MVVM_SET_PROPERTY_VALUE(LocalPlayerId, NewLocalPlayerId);

	UE_MVVM_SET_PROPERTY_VALUE(PhaseText, NSLOCTEXT("HeistLobby", "PhaseLobbyPlaceholder", "PHASE  LOBBY"));
	UE_MVVM_SET_PROPERTY_VALUE(PlayerCountText, FText::Format(NSLOCTEXT("HeistLobby", "PlayerCountFormat", "PLAYERS  {0}/4"), FText::AsNumber(ConnectedPlayerCount)));
	UE_MVVM_SET_PROPERTY_VALUE(LocalPlayerText, LocalPlayerId != INDEX_NONE ? FText::Format(NSLOCTEXT("HeistLobby", "LocalPlayerFormat", "LOCAL PLAYER  P{0}"), FText::AsNumber(LocalPlayerId))
																			: NSLOCTEXT("HeistLobby", "LocalPlayerPending", "LOCAL PLAYER  --"));
	UE_MVVM_SET_PROPERTY_VALUE(ReadyCountdownText, NSLOCTEXT("HeistLobby", "ReadyCountdownPlaceholder", "READY COUNTDOWN  --"));
	UE_MVVM_SET_PROPERTY_VALUE(DefaultLoadoutText, NSLOCTEXT("HeistLobby", "DefaultLoadout", "DEFAULT LOADOUT  [Q] COIN"));
	UE_MVVM_SET_PROPERTY_VALUE(AuthorityBlockerText, NSLOCTEXT("HeistLobby", "AuthorityBlocker", "READY / MATCH PHASE AUTHORITY SOURCE PENDING"));
	UE_MVVM_SET_PROPERTY_VALUE(AuthorityBlockerVisibility, ESlateVisibility::Visible);

	RefreshPlayerSlots();
	SnapshotChangedDelegate.Broadcast();
}

FHeistLobbySnapshotChanged& UHeistLobbyViewModel::GetSnapshotChangedDelegate()
{
	return SnapshotChangedDelegate;
}

void UHeistLobbyViewModel::HandlePlayerConnectionsChanged(const int32)
{
	RefreshLobbyData();
}

#pragma endregion

#pragma region LobbyData

int32 UHeistLobbyViewModel::GetConnectedPlayerCount() const
{
	return ConnectedPlayerCount;
}

int32 UHeistLobbyViewModel::GetLocalPlayerId() const
{
	return LocalPlayerId;
}

const FText& UHeistLobbyViewModel::GetPhaseText() const
{
	return PhaseText;
}

const FText& UHeistLobbyViewModel::GetPlayerCountText() const
{
	return PlayerCountText;
}

const FText& UHeistLobbyViewModel::GetLocalPlayerText() const
{
	return LocalPlayerText;
}

const FText& UHeistLobbyViewModel::GetReadyCountdownText() const
{
	return ReadyCountdownText;
}

const FText& UHeistLobbyViewModel::GetDefaultLoadoutText() const
{
	return DefaultLoadoutText;
}

const FText& UHeistLobbyViewModel::GetAuthorityBlockerText() const
{
	return AuthorityBlockerText;
}

const FText& UHeistLobbyViewModel::GetPlayerSlot1Text() const
{
	return PlayerSlot1Text;
}

const FText& UHeistLobbyViewModel::GetPlayerSlot2Text() const
{
	return PlayerSlot2Text;
}

const FText& UHeistLobbyViewModel::GetPlayerSlot3Text() const
{
	return PlayerSlot3Text;
}

const FText& UHeistLobbyViewModel::GetPlayerSlot4Text() const
{
	return PlayerSlot4Text;
}

ESlateVisibility UHeistLobbyViewModel::GetAuthorityBlockerVisibility() const
{
	return AuthorityBlockerVisibility;
}

FText UHeistLobbyViewModel::BuildPlayerSlotText(const int32 SlotIndex) const
{
	constexpr int32 MaxLobbyPlayers = 4;
	if (SlotIndex < 0 || SlotIndex >= MaxLobbyPlayers)
	{
		return FText::GetEmpty();
	}

	const int32 ExpectedPlayerId = SlotIndex + 1;
	const AHeistPlayerState* SlotPlayerState = nullptr;
	if (IsValid(GameState))
	{
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (const AHeistPlayerState* HeistPlayerState = Cast<AHeistPlayerState>(PlayerState))
			{
				if (HeistPlayerState->HeistPlayerId == ExpectedPlayerId)
				{
					SlotPlayerState = HeistPlayerState;
					break;
				}
			}
		}
	}

	const FText SlotLabel = FText::Format(NSLOCTEXT("HeistLobby", "SlotLabel", "SLOT {0}"), FText::AsNumber(SlotIndex + 1));
	if (!IsValid(SlotPlayerState))
	{
		return FText::Format(NSLOCTEXT("HeistLobby", "EmptySlotFormat", "{0}  EMPTY"), SlotLabel);
	}

	const int32 PlayerId = SlotPlayerState->HeistPlayerId;
	const bool bIsLocalPlayer = PlayerId == LocalPlayerId;
	return FText::Format(NSLOCTEXT("HeistLobby", "PlayerSlotFormat", "{0}  P{1}  READY --  LOADOUT DEFAULT{2}"), SlotLabel, FText::AsNumber(PlayerId),
						 bIsLocalPlayer ? FText::FromString(TEXT("  LOCAL")) : FText::GetEmpty());
}

void UHeistLobbyViewModel::RefreshPlayerSlots()
{
	UE_MVVM_SET_PROPERTY_VALUE(PlayerSlot1Text, BuildPlayerSlotText(0));
	UE_MVVM_SET_PROPERTY_VALUE(PlayerSlot2Text, BuildPlayerSlotText(1));
	UE_MVVM_SET_PROPERTY_VALUE(PlayerSlot3Text, BuildPlayerSlotText(2));
	UE_MVVM_SET_PROPERTY_VALUE(PlayerSlot4Text, BuildPlayerSlotText(3));
}

#pragma endregion
