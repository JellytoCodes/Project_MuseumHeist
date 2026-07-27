#include "UI/ViewModels/HeistLobbyViewModel.h"

#include "Core/HeistGameInstance.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerController.h"
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
	UnbindPlayerIdentityDelegates();
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
	UnbindPlayerIdentityDelegates();
	if (GameState != InGameState && IsValid(GameState))
	{
		GameState->GetPlayerConnectionsChangedDelegate().RemoveAll(this);
		GameState->GetLobbyMapSelectionChangedDelegate().RemoveAll(this);
	}
	if (GameInstance != InGameInstance && IsValid(GameInstance))
	{
		GameInstance->GetOnlineSessionStateChangedDelegate().RemoveAll(this);
	}

	GameState = InGameState;
	LocalPlayerState = InLocalPlayerState;
	GameInstance = InGameInstance;
	PlayerController = InPlayerController;

	if (IsValid(GameState))
	{
		GameState->GetPlayerConnectionsChangedDelegate().RemoveAll(this);
		GameState->GetPlayerConnectionsChangedDelegate().AddUObject(this, &UHeistLobbyViewModel::HandlePlayerConnectionsChanged);
		GameState->GetLobbyMapSelectionChangedDelegate().RemoveAll(this);
		GameState->GetLobbyMapSelectionChangedDelegate().AddUObject(this, &UHeistLobbyViewModel::HandleLobbyMapSelectionChanged);
	}
	if (IsValid(GameInstance))
	{
		GameInstance->GetOnlineSessionStateChangedDelegate().RemoveAll(this);
		GameInstance->GetOnlineSessionStateChangedDelegate().AddUObject(this, &UHeistLobbyViewModel::HandleOnlineSessionStateChanged);
	}

	RebindPlayerIdentityDelegates();
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
	const FText NewSessionActionHintText = ResolveSessionActionHintText();
	const FText NewInviteGuidanceText = ResolveInviteGuidanceText();
	const FString ActiveCode = IsValid(GameInstance) ? GameInstance->GetActiveJoinCode() : FString();
	const bool bHasJoinCode = !ActiveCode.IsEmpty();
	const bool bOperationPending = IsValid(GameInstance) && GameInstance->IsOnlineSessionOperationPending();
	const bool bSessionActive = IsValid(GameInstance)
		&& (GameInstance->IsHostingOnlineSession() || GameInstance->IsJoinedOnlineSession() || GameInstance->HasActiveNamedOnlineSession());
	const bool bLocalHost = IsValid(PlayerController) && PlayerController->HasAuthority() && IsValid(GameInstance) && GameInstance->IsHostingOnlineSession();
	const FName SelectedMapId = IsValid(GameState) ? GameState->GetSelectedLobbyMapId() : (IsValid(GameInstance) ? GameInstance->GetSelectedMapId() : NAME_None);
	const bool bRandomSelection = IsValid(GameState) ? GameState->IsRandomLobbyMapSelection() : (IsValid(GameInstance) && GameInstance->IsRandomMapSelection());
	const bool bMapSelectionPending = IsValid(GameInstance) && GameInstance->IsMapSelectionUpdatePending();
	UE_MVVM_SET_PROPERTY_VALUE(SessionStatusText, NewSessionStatusText);
	UE_MVVM_SET_PROPERTY_VALUE(SessionErrorText, NewSessionErrorText);
	UE_MVVM_SET_PROPERTY_VALUE(SessionActionHintText, NewSessionActionHintText);
	UE_MVVM_SET_PROPERTY_VALUE(InviteGuidanceText, NewInviteGuidanceText);
	UE_MVVM_SET_PROPERTY_VALUE(JoinCodeText,
							   bHasJoinCode ? FText::Format(NSLOCTEXT("HeistLobby", "JoinCodeFormat", "JOIN CODE  {0}"), FText::FromString(ActiveCode)) : FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(SessionErrorVisibility, NewSessionErrorText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	UE_MVVM_SET_PROPERTY_VALUE(SessionActionHintVisibility, NewSessionActionHintText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	UE_MVVM_SET_PROPERTY_VALUE(InviteGuidanceVisibility, NewInviteGuidanceText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	UE_MVVM_SET_PROPERTY_VALUE(JoinCodeVisibility, bHasJoinCode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	UE_MVVM_SET_PROPERTY_VALUE(bCanRequestLeaveSession, IsValid(GameInstance) && !bOperationPending && bSessionActive);
	UE_MVVM_SET_PROPERTY_VALUE(bCanSelectMap, bLocalHost && !bOperationPending && IsValid(GameState) && GameState->GetMatchPhase() == EHeistMatchPhase::Lobby);
	UE_MVVM_SET_PROPERTY_VALUE(bCanRetrySessionOperation, IsValid(GameInstance) && GameInstance->CanRetryLastOnlineSessionOperation());
	UE_MVVM_SET_PROPERTY_VALUE(SelectedMapText,
							   bRandomSelection
								   ? FText::Format(NSLOCTEXT("HeistLobby", "RandomMapFormat", "MAP  RANDOM → {0}"), FText::FromName(SelectedMapId))
								   : FText::Format(NSLOCTEXT("HeistLobby", "SelectedMapFormat", "MAP  {0}"), FText::FromName(SelectedMapId)));
	UE_MVVM_SET_PROPERTY_VALUE(MapSelectionStatusText,
							   bMapSelectionPending
								   ? NSLOCTEXT("HeistLobby", "MapSelectionUpdating", "UPDATING THE MUSEUM MAP...")
								   : (bLocalHost ? NSLOCTEXT("HeistLobby", "MapSelectionHost", "SELECT THE MUSEUM MAP BEFORE STARTING.")
												 : NSLOCTEXT("HeistLobby", "MapSelectionClient", "THE HOST SELECTS THE MUSEUM MAP.")));

	RefreshPlayerSlots();
	SnapshotChangedDelegate.Broadcast();
}

FHeistLobbySnapshotChanged& UHeistLobbyViewModel::GetSnapshotChangedDelegate()
{
	return SnapshotChangedDelegate;
}

void UHeistLobbyViewModel::HandlePlayerConnectionsChanged(const int32)
{
	RebindPlayerIdentityDelegates();
	RefreshLobbyData();
}

void UHeistLobbyViewModel::HandlePlayerIdentityChanged(const int32)
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

void UHeistLobbyViewModel::RebindPlayerIdentityDelegates()
{
	UnbindPlayerIdentityDelegates();
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

		HeistPlayerState->GetPlayerIdentityChangedDelegate().RemoveAll(this);
		HeistPlayerState->GetPlayerIdentityChangedDelegate().AddUObject(this, &UHeistLobbyViewModel::HandlePlayerIdentityChanged);
		BoundPlayerStates.Add(HeistPlayerState);
	}
}

void UHeistLobbyViewModel::UnbindPlayerIdentityDelegates()
{
	for (const TWeakObjectPtr<AHeistPlayerState>& BoundPlayerState : BoundPlayerStates)
	{
		if (AHeistPlayerState* HeistPlayerState = BoundPlayerState.Get(); IsValid(HeistPlayerState))
		{
			HeistPlayerState->GetPlayerIdentityChangedDelegate().RemoveAll(this);
		}
	}
	BoundPlayerStates.Reset();
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

void UHeistLobbyViewModel::RequestRetrySessionOperation()
{
	if (IsValid(GameInstance) && bCanRetrySessionOperation)
	{
		GameInstance->RequestRetryLastOnlineSessionOperation();
	}
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

const FText& UHeistLobbyViewModel::GetSessionActionHintText() const
{
	return SessionActionHintText;
}

const FText& UHeistLobbyViewModel::GetInviteGuidanceText() const
{
	return InviteGuidanceText;
}

const FText& UHeistLobbyViewModel::GetJoinCodeText() const
{
	return JoinCodeText;
}

const FText& UHeistLobbyViewModel::GetSelectedMapText() const
{
	return SelectedMapText;
}

const FText& UHeistLobbyViewModel::GetMapSelectionStatusText() const
{
	return MapSelectionStatusText;
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

ESlateVisibility UHeistLobbyViewModel::GetSessionActionHintVisibility() const
{
	return SessionActionHintVisibility;
}

ESlateVisibility UHeistLobbyViewModel::GetInviteGuidanceVisibility() const
{
	return InviteGuidanceVisibility;
}

ESlateVisibility UHeistLobbyViewModel::GetJoinCodeVisibility() const
{
	return JoinCodeVisibility;
}

bool UHeistLobbyViewModel::CanRequestLeaveSession() const
{
	return bCanRequestLeaveSession;
}

bool UHeistLobbyViewModel::CanSelectMap() const
{
	return bCanSelectMap;
}

bool UHeistLobbyViewModel::CanRetrySessionOperation() const
{
	return bCanRetrySessionOperation;
}

FText UHeistLobbyViewModel::ResolveOnlineSessionStatusText() const
{
	if (!IsValid(GameInstance))
	{
		return NSLOCTEXT("HeistLobby", "OnlineUnavailable", "ONLINE SESSION IS UNAVAILABLE.");
	}

	if (GameInstance->IsSessionTravelPending())
	{
		const FName Destination = GameInstance->GetPendingTravelDestination();
		if (Destination == FName(TEXT("Gameplay")))
		{
			return NSLOCTEXT("HeistLobby", "TravellingToMuseum", "TRAVELLING TO THE SELECTED MUSEUM...");
		}
		if (Destination == FName(TEXT("Lobby")))
		{
			return NSLOCTEXT("HeistLobby", "TravellingToLobby", "RETURNING THE CREW TO THE ONLINE LOBBY...");
		}
		return NSLOCTEXT("HeistLobby", "TravellingToHost", "CONNECTING TO THE HOST'S SESSION...");
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
	if (State == FName(TEXT("Leaving")))
	{
		return NSLOCTEXT("HeistLobby", "LeavingSession", "LEAVING THE LOBBY AND RETURNING TO THE TITLE MENU...");
	}
	if (State == FName(TEXT("Failed")))
	{
		return NSLOCTEXT("HeistLobby", "SessionFailed", "THE SESSION REQUEST FAILED.");
	}
	return NSLOCTEXT("HeistLobby", "SessionIdle", "NO ONLINE LOBBY IS ACTIVE.");
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
	if (Failure == FName(TEXT("TravelTimedOut")))
	{
		return NSLOCTEXT("HeistLobby", "TravelTimedOut", "TRAVEL TO THE MUSEUM TIMED OUT. SELECT RETRY TO TRY AGAIN.");
	}
	if (Failure == FName(TEXT("TravelFailure")) || Failure == FName(TEXT("GameplayTravelRejected"))
		|| Failure == FName(TEXT("LobbyReturnTravelRejected")) || Failure == FName(TEXT("LobbyTravelRejected")))
	{
		return NSLOCTEXT("HeistLobby", "TravelFailure", "THE CREW COULD NOT TRAVEL TO THE REQUESTED MAP. SELECT RETRY TO TRY AGAIN.");
	}
	if (Failure == FName(TEXT("NetworkFailure")) || Failure == FName(TEXT("ConnectionLost")))
	{
		return NSLOCTEXT("HeistLobby", "NetworkFailure", "THE ONLINE CONNECTION WAS LOST. RETURN TO THE TITLE MENU AND RETRY.");
	}
	if (Failure == FName(TEXT("OperationCancelled")))
	{
		return NSLOCTEXT("HeistLobby", "OperationCancelled", "THE SESSION REQUEST WAS CANCELLED.");
	}
	if (Failure == FName(TEXT("OnlineSessionUnavailable")))
	{
		return NSLOCTEXT("HeistLobby", "SubsystemUnavailable", "THE ONLINE SERVICE IS NOT AVAILABLE.");
	}
	if (Failure == FName(TEXT("SessionAlreadyExists")))
	{
		return NSLOCTEXT("HeistLobby", "ExistingSession", "LEAVE THE CURRENT SESSION BEFORE STARTING ANOTHER.");
	}
	if (Failure == FName(TEXT("HostQuit")))
	{
		return NSLOCTEXT("HeistLobby", "HostQuit", "THE HOST CLOSED THE SESSION. RETURNING TO THE TITLE MENU.");
	}
	if (Failure == FName(TEXT("DestroySessionFailed")) || Failure == FName(TEXT("LeaveRequestRejected")))
	{
		return NSLOCTEXT("HeistLobby", "LeaveFailed", "THE SESSION COULD NOT BE CLOSED. TRY LEAVING AGAIN.");
	}
	if (Failure == FName(TEXT("MapSelectionUpdateFailed")) || Failure == FName(TEXT("MapSelectionUpdateRejected"))
		|| Failure == FName(TEXT("MapSelectionCommitFailed")))
	{
		return NSLOCTEXT("HeistLobby", "MapSelectionFailed", "THE MUSEUM MAP COULD NOT BE UPDATED.");
	}
	if (Failure == FName(TEXT("NotHost")) || Failure == FName(TEXT("HostOnly")) || Failure == FName(TEXT("SessionNotHosting")))
	{
		return NSLOCTEXT("HeistLobby", "MapSelectionHostOnly", "ONLY THE HOST CAN CHANGE THE MUSEUM MAP.");
	}
	if (Failure == FName(TEXT("InvalidMapSelection")))
	{
		return NSLOCTEXT("HeistLobby", "InvalidMapSelection", "SELECT M01, M02, M03, OR RANDOM.");
	}
	if (Failure == FName(TEXT("LobbyNotReady")))
	{
		return NSLOCTEXT("HeistLobby", "LobbyMapSelectionLocked", "THE MUSEUM MAP CAN ONLY BE CHANGED IN THE LOBBY.");
	}
	if (Failure == FName(TEXT("MapUpdateTimedOut")))
	{
		return NSLOCTEXT("HeistLobby", "MapUpdateTimedOut", "THE MUSEUM MAP UPDATE TIMED OUT. SELECT THE MAP AGAIN.");
	}
	return NSLOCTEXT("HeistLobby", "GenericSessionFailure", "THE ONLINE SESSION COULD NOT CONTINUE. RETRY THE REQUEST OR RETURN TO THE TITLE MENU.");
}

FText UHeistLobbyViewModel::ResolveSessionActionHintText() const
{
	if (!IsValid(GameInstance))
	{
		return FText::GetEmpty();
	}
	if (GameInstance->CanRetryLastOnlineSessionOperation())
	{
		return NSLOCTEXT("HeistLobby", "RetryAvailableHint", "SELECT RETRY TO REPEAT THE FAILED TRAVEL REQUEST.");
	}
	if (GameInstance->IsOnlineSessionOperationPending())
	{
		return NSLOCTEXT("HeistLobby", "OperationPendingHint", "SESSION CONTROLS ARE LOCKED UNTIL THIS REQUEST FINISHES OR TIMES OUT.");
	}
	return FText::GetEmpty();
}

FText UHeistLobbyViewModel::ResolveInviteGuidanceText() const
{
	if (!IsValid(GameInstance) || GameInstance->GetActiveJoinCode().IsEmpty())
	{
		return FText::GetEmpty();
	}

	if (GameInstance->IsHostingOnlineSession())
	{
		return GameInstance->GetActiveOnlineSubsystemName() == FName(TEXT("STEAM"))
				   ? NSLOCTEXT("HeistLobby", "SteamHostInviteGuidance", "SHARE THE 6-CHARACTER JOIN CODE, OR SEND A STEAM INVITE FROM THE STEAM OVERLAY.")
				   : NSLOCTEXT("HeistLobby", "HostInviteGuidance", "SHARE THE 6-CHARACTER JOIN CODE. STEAM INVITES ARE AVAILABLE IN A STEAM BUILD.");
	}
	return NSLOCTEXT("HeistLobby", "ClientInviteGuidance", "SHARE THE 6-CHARACTER JOIN CODE WITH THE REST OF THE CREW.");
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
