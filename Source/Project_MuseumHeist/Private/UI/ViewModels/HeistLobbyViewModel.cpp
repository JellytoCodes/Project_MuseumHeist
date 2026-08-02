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

	UE_MVVM_SET_PROPERTY_VALUE(PhaseText, NSLOCTEXT("HeistLobby", "PhaseLobbyPlaceholder", "로비"));
	UE_MVVM_SET_PROPERTY_VALUE(PlayerCountText,
							   FText::Format(NSLOCTEXT("HeistLobby", "PlayerCountFormat", "플레이어  {0}/4"), FText::AsNumber(ConnectedPlayerCount)));
	UE_MVVM_SET_PROPERTY_VALUE(LocalPlayerText,
							   LocalPlayerId != INDEX_NONE
								   ? FText::Format(NSLOCTEXT("HeistLobby", "LocalPlayerFormat", "플레이어  {0}"), FText::AsNumber(LocalPlayerId))
								   : NSLOCTEXT("HeistLobby", "LocalPlayerPending", "플레이어  --"));
	UE_MVVM_SET_PROPERTY_VALUE(ReadyCountdownText, NSLOCTEXT("HeistLobby", "ReadyCountdownPlaceholder", "준비 대기 중"));
	UE_MVVM_SET_PROPERTY_VALUE(DefaultLoadoutText, NSLOCTEXT("HeistLobby", "DefaultLoadout", "기본 장비  Q 동전"));
	const bool bLocalAuthority = IsValid(GameState) && GameState->HasAuthority();
	UE_MVVM_SET_PROPERTY_VALUE(AuthorityBlockerText, NSLOCTEXT("HeistLobby", "AuthorityBlocker", "호스트만 작전을 시작할 수 있습니다."));
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
							   bHasJoinCode ? FText::Format(NSLOCTEXT("HeistLobby", "JoinCodeFormat", "참가 코드  {0}"), FText::FromString(ActiveCode)) : FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(SessionErrorVisibility, NewSessionErrorText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	UE_MVVM_SET_PROPERTY_VALUE(SessionActionHintVisibility, NewSessionActionHintText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	UE_MVVM_SET_PROPERTY_VALUE(InviteGuidanceVisibility, NewInviteGuidanceText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	UE_MVVM_SET_PROPERTY_VALUE(JoinCodeVisibility, bHasJoinCode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	UE_MVVM_SET_PROPERTY_VALUE(bCanRequestLeaveSession, IsValid(GameInstance) && !bOperationPending && bSessionActive);
	UE_MVVM_SET_PROPERTY_VALUE(bCanSelectMap, bLocalHost && !bOperationPending && IsValid(GameState) && GameState->GetMatchPhase() == EHeistMatchPhase::Lobby);
	UE_MVVM_SET_PROPERTY_VALUE(bCanRetrySessionOperation, IsValid(GameInstance) && GameInstance->CanRetryLastOnlineSessionOperation());
	UE_MVVM_SET_PROPERTY_VALUE(SelectedMapText,
							   bRandomSelection
								   ? FText::Format(NSLOCTEXT("HeistLobby", "RandomMapFormat", "맵  무작위 → {0}"), FText::FromName(SelectedMapId))
								   : FText::Format(NSLOCTEXT("HeistLobby", "SelectedMapFormat", "맵  {0}"), FText::FromName(SelectedMapId)));
	UE_MVVM_SET_PROPERTY_VALUE(MapSelectionStatusText,
							   bMapSelectionPending
								   ? NSLOCTEXT("HeistLobby", "MapSelectionUpdating", "박물관 맵을 변경하는 중...")
								   : (bLocalHost ? NSLOCTEXT("HeistLobby", "MapSelectionHost", "시작하기 전에 박물관 맵을 선택하세요.")
														 : NSLOCTEXT("HeistLobby", "MapSelectionClient", "호스트가 박물관 맵을 선택합니다.")));

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
		return NSLOCTEXT("HeistLobby", "OnlineUnavailable", "온라인 세션을 사용할 수 없습니다.");
	}

	if (GameInstance->IsSessionTravelPending())
	{
		const FName Destination = GameInstance->GetPendingTravelDestination();
		if (Destination == FName(TEXT("Gameplay")))
		{
			return NSLOCTEXT("HeistLobby", "TravellingToMuseum", "선택한 박물관으로 이동하는 중...");
		}
		if (Destination == FName(TEXT("Lobby")))
		{
			return NSLOCTEXT("HeistLobby", "TravellingToLobby", "팀을 온라인 로비로 복귀시키는 중...");
		}
		return NSLOCTEXT("HeistLobby", "TravellingToHost", "호스트 세션에 연결하는 중...");
	}

	const FName State = GameInstance->GetOnlineSessionState();
	if (State == FName(TEXT("Creating")))
	{
		return NSLOCTEXT("HeistLobby", "CreatingSession", "세션을 만드는 중...");
	}
	if (State == FName(TEXT("Hosting")))
	{
		return NSLOCTEXT("HeistLobby", "HostingSession", "세션이 생성되었습니다. 참가 코드를 공유하세요.");
	}
	if (State == FName(TEXT("Searching")))
	{
		return NSLOCTEXT("HeistLobby", "SearchingSession", "참가 코드로 세션을 찾는 중...");
	}
	if (State == FName(TEXT("Joining")))
	{
		return NSLOCTEXT("HeistLobby", "JoiningSession", "세션에 참가하는 중...");
	}
	if (State == FName(TEXT("Joined")))
	{
		return NSLOCTEXT("HeistLobby", "JoinedSession", "연결되었습니다. 호스트를 기다리는 중입니다.");
	}
	if (State == FName(TEXT("Leaving")))
	{
		return NSLOCTEXT("HeistLobby", "LeavingSession", "로비를 나가 타이틀 메뉴로 돌아가는 중...");
	}
	if (State == FName(TEXT("Failed")))
	{
		return NSLOCTEXT("HeistLobby", "SessionFailed", "세션 요청에 실패했습니다.");
	}
	return NSLOCTEXT("HeistLobby", "SessionIdle", "진행 중인 온라인 로비가 없습니다.");
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
		return NSLOCTEXT("HeistLobby", "InvalidJoinCode", "올바른 6자리 참가 코드를 입력하세요.");
	}
	if (Failure == FName(TEXT("SessionNotFound")))
	{
		return NSLOCTEXT("HeistLobby", "SessionNotFound", "해당 코드의 세션을 찾을 수 없습니다.");
	}
	if (Failure == FName(TEXT("SessionFull")))
	{
		return NSLOCTEXT("HeistLobby", "SessionFull", "해당 세션은 이미 4명으로 가득 찼습니다.");
	}
	if (Failure == FName(TEXT("VersionMismatch")))
	{
		return NSLOCTEXT("HeistLobby", "VersionMismatch", "호스트가 다른 게임 버전을 사용하고 있습니다.");
	}
	if (Failure == FName(TEXT("ConnectStringNotResolved")))
	{
		return NSLOCTEXT("HeistLobby", "AddressFailed", "호스트 주소를 확인할 수 없습니다.");
	}
	if (Failure == FName(TEXT("TravelTimedOut")))
	{
		return NSLOCTEXT("HeistLobby", "TravelTimedOut", "박물관 이동 시간이 초과되었습니다. 다시 시도하세요.");
	}
	if (Failure == FName(TEXT("TravelFailure")) || Failure == FName(TEXT("GameplayTravelRejected"))
		|| Failure == FName(TEXT("LobbyReturnTravelRejected")) || Failure == FName(TEXT("LobbyTravelRejected")))
	{
		return NSLOCTEXT("HeistLobby", "TravelFailure", "팀이 요청한 맵으로 이동하지 못했습니다. 다시 시도하세요.");
	}
	if (Failure == FName(TEXT("NetworkFailure")) || Failure == FName(TEXT("ConnectionLost")))
	{
		return NSLOCTEXT("HeistLobby", "NetworkFailure", "온라인 연결이 끊어졌습니다. 타이틀 메뉴로 돌아가 다시 시도하세요.");
	}
	if (Failure == FName(TEXT("OperationCancelled")))
	{
		return NSLOCTEXT("HeistLobby", "OperationCancelled", "세션 요청이 취소되었습니다.");
	}
	if (Failure == FName(TEXT("OnlineSessionUnavailable")))
	{
		return NSLOCTEXT("HeistLobby", "SubsystemUnavailable", "온라인 서비스를 사용할 수 없습니다.");
	}
	if (Failure == FName(TEXT("SessionAlreadyExists")))
	{
		return NSLOCTEXT("HeistLobby", "ExistingSession", "다른 세션을 시작하기 전에 현재 세션에서 나가세요.");
	}
	if (Failure == FName(TEXT("HostQuit")))
	{
		return NSLOCTEXT("HeistLobby", "HostQuit", "호스트가 세션을 닫았습니다. 타이틀 메뉴로 돌아갑니다.");
	}
	if (Failure == FName(TEXT("DestroySessionFailed")) || Failure == FName(TEXT("LeaveRequestRejected")))
	{
		return NSLOCTEXT("HeistLobby", "LeaveFailed", "세션을 닫을 수 없습니다. 다시 나가기를 시도하세요.");
	}
	if (Failure == FName(TEXT("MapSelectionUpdateFailed")) || Failure == FName(TEXT("MapSelectionUpdateRejected"))
		|| Failure == FName(TEXT("MapSelectionCommitFailed")))
	{
		return NSLOCTEXT("HeistLobby", "MapSelectionFailed", "박물관 맵을 변경할 수 없습니다.");
	}
	if (Failure == FName(TEXT("NotHost")) || Failure == FName(TEXT("HostOnly")) || Failure == FName(TEXT("SessionNotHosting")))
	{
		return NSLOCTEXT("HeistLobby", "MapSelectionHostOnly", "호스트만 박물관 맵을 변경할 수 있습니다.");
	}
	if (Failure == FName(TEXT("InvalidMapSelection")))
	{
		return NSLOCTEXT("HeistLobby", "InvalidMapSelection", "M01, M02, M03 또는 무작위를 선택하세요.");
	}
	if (Failure == FName(TEXT("LobbyNotReady")))
	{
		return NSLOCTEXT("HeistLobby", "LobbyMapSelectionLocked", "박물관 맵은 로비에서만 변경할 수 있습니다.");
	}
	if (Failure == FName(TEXT("MapUpdateTimedOut")))
	{
		return NSLOCTEXT("HeistLobby", "MapUpdateTimedOut", "박물관 맵 변경 시간이 초과되었습니다. 맵을 다시 선택하세요.");
	}
	return NSLOCTEXT("HeistLobby", "GenericSessionFailure", "온라인 세션을 계속할 수 없습니다. 다시 시도하거나 타이틀 메뉴로 돌아가세요.");
}

FText UHeistLobbyViewModel::ResolveSessionActionHintText() const
{
	if (!IsValid(GameInstance))
	{
		return FText::GetEmpty();
	}
	if (GameInstance->CanRetryLastOnlineSessionOperation())
	{
		return NSLOCTEXT("HeistLobby", "RetryAvailableHint", "다시 시도를 선택하면 실패한 이동 요청을 반복합니다.");
	}
	if (GameInstance->IsOnlineSessionOperationPending())
	{
		return NSLOCTEXT("HeistLobby", "OperationPendingHint", "요청이 완료되거나 시간이 초과될 때까지 세션 조작이 잠깁니다.");
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
				   ? NSLOCTEXT("HeistLobby", "SteamHostInviteGuidance", "6자리 참가 코드를 공유하거나 Steam 오버레이에서 초대를 보내세요.")
				   : NSLOCTEXT("HeistLobby", "HostInviteGuidance", "6자리 참가 코드를 공유하세요. Steam 빌드에서는 Steam 초대도 사용할 수 있습니다.");
	}
	return NSLOCTEXT("HeistLobby", "ClientInviteGuidance", "다른 팀원에게 6자리 참가 코드를 공유하세요.");
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

	const FText SlotLabel = FText::Format(NSLOCTEXT("HeistLobby", "SlotLabel", "슬롯 {0}"), FText::AsNumber(SlotIndex + 1));
	if (!IsValid(SlotPlayerState))
	{
		return FText::Format(NSLOCTEXT("HeistLobby", "EmptySlotFormat", "{0}  —  비어 있음"), SlotLabel);
	}

	const int32 PlayerId = SlotPlayerState->HeistPlayerId;
	const bool bIsLocalPlayer = PlayerId == LocalPlayerId;
	return FText::Format(NSLOCTEXT("HeistLobby", "PlayerSlotFormat", "{0}  —  플레이어 {1}{2}"), SlotLabel, FText::AsNumber(PlayerId),
						 bIsLocalPlayer ? NSLOCTEXT("HeistLobby", "LocalSlotSuffix", "  (나)") : FText::GetEmpty());
}

void UHeistLobbyViewModel::RefreshPlayerSlots()
{
	UE_MVVM_SET_PROPERTY_VALUE(PlayerSlot1Text, BuildPlayerSlotText(0));
	UE_MVVM_SET_PROPERTY_VALUE(PlayerSlot2Text, BuildPlayerSlotText(1));
	UE_MVVM_SET_PROPERTY_VALUE(PlayerSlot3Text, BuildPlayerSlotText(2));
	UE_MVVM_SET_PROPERTY_VALUE(PlayerSlot4Text, BuildPlayerSlotText(3));
}

#pragma endregion
