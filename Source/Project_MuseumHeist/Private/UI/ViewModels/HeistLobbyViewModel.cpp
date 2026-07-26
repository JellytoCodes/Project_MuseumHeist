#include "UI/ViewModels/HeistLobbyViewModel.h"

#include "Core/HeistGameInstance.h"
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
	if (IsValid(GameInstance))
	{
		GameInstance->GetOnlineSessionStateChangedDelegate().RemoveAll(this);
	}

	Super::BeginDestroy();
}

#pragma endregion

#pragma region Setup

void UHeistLobbyViewModel::SetupViewModel(AHeistGameState* InGameState, AHeistPlayerState* InLocalPlayerState, UHeistGameInstance* InGameInstance)
{
	if (GameState != InGameState && IsValid(GameState))
	{
		GameState->GetPlayerConnectionsChangedDelegate().RemoveAll(this);
	}
	if (GameInstance != InGameInstance && IsValid(GameInstance))
	{
		GameInstance->GetOnlineSessionStateChangedDelegate().RemoveAll(this);
	}

	GameState = InGameState;
	LocalPlayerState = InLocalPlayerState;
	GameInstance = InGameInstance;

	if (IsValid(GameState))
	{
		GameState->GetPlayerConnectionsChangedDelegate().RemoveAll(this);
		GameState->GetPlayerConnectionsChangedDelegate().AddUObject(this, &UHeistLobbyViewModel::HandlePlayerConnectionsChanged);
	}
	if (IsValid(GameInstance))
	{
		GameInstance->GetOnlineSessionStateChangedDelegate().RemoveAll(this);
		GameInstance->GetOnlineSessionStateChangedDelegate().AddUObject(this, &UHeistLobbyViewModel::HandleOnlineSessionStateChanged);
	}

	RefreshLobbyData();
}

void UHeistLobbyViewModel::RefreshLobbyData()
{
	const int32 NewConnectedPlayerCount = IsValid(GameState) ? GameState->GetConnectedPlayerCount() : 0;
	UE_MVVM_SET_PROPERTY_VALUE(ConnectedPlayerCount, NewConnectedPlayerCount);

	const int32 NewLocalPlayerId = IsValid(LocalPlayerState) ? LocalPlayerState->HeistPlayerId : INDEX_NONE;
	UE_MVVM_SET_PROPERTY_VALUE(LocalPlayerId, NewLocalPlayerId);

	UE_MVVM_SET_PROPERTY_VALUE(PhaseText, NSLOCTEXT("HeistLobby", "PhaseLobbyPlaceholder", "LOBBY"));
	UE_MVVM_SET_PROPERTY_VALUE(PlayerCountText,
							   FText::Format(NSLOCTEXT("HeistLobby", "PlayerCountFormat", "PLAYERS  {0}/4"), FText::AsNumber(ConnectedPlayerCount)));
	UE_MVVM_SET_PROPERTY_VALUE(LocalPlayerText,
							   LocalPlayerId != INDEX_NONE
								   ? FText::Format(NSLOCTEXT("HeistLobby", "LocalPlayerFormat", "PLAYER  {0}"), FText::AsNumber(LocalPlayerId))
								   : NSLOCTEXT("HeistLobby", "LocalPlayerPending", "PLAYER  --"));
	UE_MVVM_SET_PROPERTY_VALUE(ReadyCountdownText, NSLOCTEXT("HeistLobby", "ReadyCountdownPlaceholder", "WAITING FOR READY"));
	UE_MVVM_SET_PROPERTY_VALUE(DefaultLoadoutText, NSLOCTEXT("HeistLobby", "DefaultLoadout", "LOADOUT  Q COIN"));
	const bool bLocalAuthority = IsValid(GameState) && GameState->HasAuthority();
	UE_MVVM_SET_PROPERTY_VALUE(AuthorityBlockerText, NSLOCTEXT("HeistLobby", "AuthorityBlocker", "ONLY THE HOST CAN START THE HEIST."));
	UE_MVVM_SET_PROPERTY_VALUE(AuthorityBlockerVisibility, bLocalAuthority ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);

	const FText NewSessionStatusText = ResolveOnlineSessionStatusText();
	const FText NewSessionErrorText = ResolveOnlineSessionFailureText();
	const FString ActiveCode = IsValid(GameInstance) ? GameInstance->GetActiveJoinCode() : FString();
	const bool bHasJoinCode = !ActiveCode.IsEmpty();
	const bool bOperationPending = IsValid(GameInstance) && GameInstance->IsOnlineSessionOperationPending();
	const bool bSessionActive = IsValid(GameInstance) && (GameInstance->IsHostingOnlineSession() || GameInstance->IsJoinedOnlineSession());
	UE_MVVM_SET_PROPERTY_VALUE(SessionStatusText, NewSessionStatusText);
	UE_MVVM_SET_PROPERTY_VALUE(SessionErrorText, NewSessionErrorText);
	UE_MVVM_SET_PROPERTY_VALUE(JoinCodeText,
							   bHasJoinCode ? FText::Format(NSLOCTEXT("HeistLobby", "JoinCodeFormat", "JOIN CODE  {0}"), FText::FromString(ActiveCode)) : FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(SessionErrorVisibility, NewSessionErrorText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	UE_MVVM_SET_PROPERTY_VALUE(JoinCodeVisibility, bHasJoinCode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	UE_MVVM_SET_PROPERTY_VALUE(bCanRequestHostSession, IsValid(GameInstance) && !bOperationPending && !bSessionActive);
	UE_MVVM_SET_PROPERTY_VALUE(bCanRequestJoinSession, IsValid(GameInstance) && !bOperationPending && !bSessionActive);

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

void UHeistLobbyViewModel::HandleOnlineSessionStateChanged()
{
	RefreshLobbyData();
}

bool UHeistLobbyViewModel::RequestHostSession()
{
	return IsValid(GameInstance) && bCanRequestHostSession && GameInstance->RequestHostSession();
}

bool UHeistLobbyViewModel::RequestJoinSessionByCode(const FString& JoinCode)
{
	return IsValid(GameInstance) && bCanRequestJoinSession && GameInstance->RequestJoinSessionByCode(JoinCode);
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

const FText& UHeistLobbyViewModel::GetSessionStatusText() const
{
	return SessionStatusText;
}

const FText& UHeistLobbyViewModel::GetSessionErrorText() const
{
	return SessionErrorText;
}

const FText& UHeistLobbyViewModel::GetJoinCodeText() const
{
	return JoinCodeText;
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

ESlateVisibility UHeistLobbyViewModel::GetSessionErrorVisibility() const
{
	return SessionErrorVisibility;
}

ESlateVisibility UHeistLobbyViewModel::GetJoinCodeVisibility() const
{
	return JoinCodeVisibility;
}

bool UHeistLobbyViewModel::CanRequestHostSession() const
{
	return bCanRequestHostSession;
}

bool UHeistLobbyViewModel::CanRequestJoinSession() const
{
	return bCanRequestJoinSession;
}

FText UHeistLobbyViewModel::ResolveOnlineSessionStatusText() const
{
	if (!IsValid(GameInstance))
	{
		return NSLOCTEXT("HeistLobby", "OnlineUnavailable", "ONLINE SESSION IS UNAVAILABLE.");
	}

	const FName State = GameInstance->GetOnlineSessionState();
	if (State == FName(TEXT("Creating")))
	{
		return NSLOCTEXT("HeistLobby", "CreatingSession", "CREATING THE SESSION...");
	}
	if (State == FName(TEXT("Hosting")))
	{
		return NSLOCTEXT("HeistLobby", "HostingSession", "SESSION CREATED. SHARE THE JOIN CODE.");
	}
	if (State == FName(TEXT("Searching")))
	{
		return NSLOCTEXT("HeistLobby", "SearchingSession", "SEARCHING FOR THAT JOIN CODE...");
	}
	if (State == FName(TEXT("Joining")))
	{
		return NSLOCTEXT("HeistLobby", "JoiningSession", "JOINING THE SESSION...");
	}
	if (State == FName(TEXT("Joined")))
	{
		return NSLOCTEXT("HeistLobby", "JoinedSession", "CONNECTED. WAITING FOR THE HOST.");
	}
	if (State == FName(TEXT("Failed")))
	{
		return NSLOCTEXT("HeistLobby", "SessionFailed", "THE SESSION REQUEST FAILED.");
	}
	return NSLOCTEXT("HeistLobby", "SessionIdle", "CREATE A SESSION OR ENTER A JOIN CODE.");
}

FText UHeistLobbyViewModel::ResolveOnlineSessionFailureText() const
{
	if (!IsValid(GameInstance) || GameInstance->GetLastOnlineSessionFailure().IsNone())
	{
		return FText::GetEmpty();
	}

	const FName Failure = GameInstance->GetLastOnlineSessionFailure();
	if (Failure == FName(TEXT("InvalidJoinCode")))
	{
		return NSLOCTEXT("HeistLobby", "InvalidJoinCode", "ENTER A VALID 6-CHARACTER JOIN CODE.");
	}
	if (Failure == FName(TEXT("SessionNotFound")))
	{
		return NSLOCTEXT("HeistLobby", "SessionNotFound", "NO SESSION WAS FOUND FOR THAT CODE.");
	}
	if (Failure == FName(TEXT("SessionFull")))
	{
		return NSLOCTEXT("HeistLobby", "SessionFull", "THAT SESSION ALREADY HAS 4 PLAYERS.");
	}
	if (Failure == FName(TEXT("VersionMismatch")))
	{
		return NSLOCTEXT("HeistLobby", "VersionMismatch", "THE HOST IS USING A DIFFERENT GAME BUILD.");
	}
	if (Failure == FName(TEXT("ConnectStringNotResolved")))
	{
		return NSLOCTEXT("HeistLobby", "AddressFailed", "THE HOST ADDRESS COULD NOT BE RESOLVED.");
	}
	if (Failure == FName(TEXT("OnlineSessionUnavailable")))
	{
		return NSLOCTEXT("HeistLobby", "SubsystemUnavailable", "THE ONLINE SERVICE IS NOT AVAILABLE.");
	}
	if (Failure == FName(TEXT("SessionAlreadyExists")))
	{
		return NSLOCTEXT("HeistLobby", "ExistingSession", "LEAVE THE CURRENT SESSION BEFORE STARTING ANOTHER.");
	}
	return FText::Format(NSLOCTEXT("HeistLobby", "SessionErrorFormat", "SESSION ERROR: {0}"), FText::FromName(Failure));
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
		return FText::Format(NSLOCTEXT("HeistLobby", "EmptySlotFormat", "{0}  —  EMPTY"), SlotLabel);
	}

	const int32 PlayerId = SlotPlayerState->HeistPlayerId;
	const bool bIsLocalPlayer = PlayerId == LocalPlayerId;
	return FText::Format(NSLOCTEXT("HeistLobby", "PlayerSlotFormat", "{0}  —  PLAYER {1}{2}"), SlotLabel, FText::AsNumber(PlayerId),
						 bIsLocalPlayer ? NSLOCTEXT("HeistLobby", "LocalSlotSuffix", "  (YOU)") : FText::GetEmpty());
}

void UHeistLobbyViewModel::RefreshPlayerSlots()
{
	UE_MVVM_SET_PROPERTY_VALUE(PlayerSlot1Text, BuildPlayerSlotText(0));
	UE_MVVM_SET_PROPERTY_VALUE(PlayerSlot2Text, BuildPlayerSlotText(1));
	UE_MVVM_SET_PROPERTY_VALUE(PlayerSlot3Text, BuildPlayerSlotText(2));
	UE_MVVM_SET_PROPERTY_VALUE(PlayerSlot4Text, BuildPlayerSlotText(3));
}

#pragma endregion
