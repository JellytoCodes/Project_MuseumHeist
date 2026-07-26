#include "Core/HeistGameInstance.h"

#include "Core/HeistGameState.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"

namespace HeistOnlineSession
{
	const FName SessionName(TEXT("HeistSession"));
	const FName JoinCodeSetting(TEXT("HEIST_JOIN_CODE"));
	const FName MapIdSetting(TEXT("HEIST_MAP_ID"));
	const FName ProductSetting(TEXT("HEIST_PRODUCT"));
	const FString ProductId(TEXT("MUSEUM_HEIST"));
	const FString JoinCodeAlphabet(TEXT("ABCDEFGHJKLMNPQRSTUVWXYZ23456789"));

	const FName StateIdle(TEXT("Idle"));
	const FName StateCreating(TEXT("Creating"));
	const FName StateHosting(TEXT("Hosting"));
	const FName StateSearching(TEXT("Searching"));
	const FName StateJoining(TEXT("Joining"));
	const FName StateJoined(TEXT("Joined"));
	const FName StateFailed(TEXT("Failed"));
}

#pragma region Construction

UHeistGameInstance::UHeistGameInstance()
{
}

#pragma endregion

#pragma region Lifecycle

void UHeistGameInstance::Init()
{
	Super::Init();
	RefreshOnlineSessionInterface();
}

void UHeistGameInstance::Shutdown()
{
	ClearOnlineDelegates();
	ActiveSessionSearch.Reset();
	OnlineSessionInterface.Reset();
	Super::Shutdown();
}

#pragma endregion

#pragma region OnlineSession

bool UHeistGameInstance::RequestHostSession()
{
	UWorld* World = GetWorld();
	if (IsOnlineSessionOperationPending())
	{
		UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("Host"), ActiveOnlineSubsystemName, ActiveJoinCode, OnlineSessionState, false, TEXT("OperationPending"));
		return false;
	}
	if (IsHostingOnlineSession() || IsJoinedOnlineSession())
	{
		SetOnlineSessionState(OnlineSessionState, FName(TEXT("SessionAlreadyExists")));
		UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("Host"), ActiveOnlineSubsystemName, ActiveJoinCode, OnlineSessionState, false, TEXT("SessionAlreadyExists"));
		return false;
	}

	if (!IsValid(World) || World->GetNetMode() == NM_Client)
	{
		FailOnlineSessionRequest(FName(TEXT("ClientCannotHost")));
		UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("Host"), ActiveOnlineSubsystemName, ActiveJoinCode, OnlineSessionState, false, TEXT("ClientCannotHost"));
		return false;
	}

	if (!RefreshOnlineSessionInterface())
	{
		FailOnlineSessionRequest(FName(TEXT("OnlineSessionUnavailable")));
		UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("Host"), ActiveOnlineSubsystemName, ActiveJoinCode, OnlineSessionState, false, TEXT("OnlineSessionUnavailable"));
		return false;
	}

	if (OnlineSessionInterface->GetNamedSession(HeistOnlineSession::SessionName) != nullptr)
	{
		const FName FailureReason(TEXT("SessionAlreadyExists"));
		if (IsHostingOnlineSession() || IsJoinedOnlineSession())
		{
			SetOnlineSessionState(OnlineSessionState, FailureReason);
		}
		else
		{
			FailOnlineSessionRequest(FailureReason);
		}
		UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("Host"), ActiveOnlineSubsystemName, ActiveJoinCode, OnlineSessionState, false, TEXT("SessionAlreadyExists"));
		return false;
	}

	ActiveJoinCode = GenerateJoinCode();
	PendingJoinCode.Reset();
	SetOnlineSessionState(HeistOnlineSession::StateCreating);
	UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("Host"), ActiveOnlineSubsystemName, ActiveJoinCode, OnlineSessionState, true, TEXT("None"));

	if (!BeginCreateSession())
	{
		const FString RejectedJoinCode = ActiveJoinCode;
		ActiveJoinCode.Reset();
		FailOnlineSessionRequest(FName(TEXT("CreateRequestRejected")));
		UHeistDebugFunctionLibrary::DebugOnlineSessionCreateComplete(this, HeistOnlineSession::SessionName, ActiveOnlineSubsystemName, RejectedJoinCode, false, false,
																	LastOnlineSessionFailure);
		return false;
	}

	return true;
}

bool UHeistGameInstance::RequestJoinSessionByCode(const FString& JoinCode)
{
	const FString NormalizedJoinCode = NormalizeJoinCode(JoinCode);
	if (IsOnlineSessionOperationPending())
	{
		UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("JoinByCode"), ActiveOnlineSubsystemName, NormalizedJoinCode, OnlineSessionState, false, TEXT("OperationPending"));
		return false;
	}
	if (IsHostingOnlineSession() || IsJoinedOnlineSession())
	{
		SetOnlineSessionState(OnlineSessionState, FName(TEXT("SessionAlreadyExists")));
		UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("JoinByCode"), ActiveOnlineSubsystemName, NormalizedJoinCode, OnlineSessionState, false,
															 TEXT("SessionAlreadyExists"));
		return false;
	}

	if (!IsJoinCodeValid(NormalizedJoinCode))
	{
		FailOnlineSessionRequest(FName(TEXT("InvalidJoinCode")));
		UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("JoinByCode"), ActiveOnlineSubsystemName, NormalizedJoinCode, OnlineSessionState, false, TEXT("InvalidJoinCode"));
		return false;
	}

	if (!RefreshOnlineSessionInterface())
	{
		FailOnlineSessionRequest(FName(TEXT("OnlineSessionUnavailable")));
		UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("JoinByCode"), ActiveOnlineSubsystemName, NormalizedJoinCode, OnlineSessionState, false,
															 TEXT("OnlineSessionUnavailable"));
		return false;
	}

	if (OnlineSessionInterface->GetNamedSession(HeistOnlineSession::SessionName) != nullptr)
	{
		const FName FailureReason(TEXT("SessionAlreadyExists"));
		if (IsHostingOnlineSession() || IsJoinedOnlineSession())
		{
			SetOnlineSessionState(OnlineSessionState, FailureReason);
		}
		else
		{
			FailOnlineSessionRequest(FailureReason);
		}
		UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("JoinByCode"), ActiveOnlineSubsystemName, NormalizedJoinCode, OnlineSessionState, false,
															 TEXT("SessionAlreadyExists"));
		return false;
	}

	PendingJoinCode = NormalizedJoinCode;
	ActiveJoinCode.Reset();
	ActiveSessionSearch = MakeShared<FOnlineSessionSearch>();
	ActiveSessionSearch->MaxSearchResults = FMath::Clamp(MaxSessionSearchResults, 1, 500);
	ActiveSessionSearch->bIsLanQuery = ActiveOnlineSubsystemName == NULL_SUBSYSTEM;
	if (!ActiveSessionSearch->bIsLanQuery)
	{
		ActiveSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
		ActiveSessionSearch->QuerySettings.Set(HeistOnlineSession::JoinCodeSetting, PendingJoinCode, EOnlineComparisonOp::Equals);
		ActiveSessionSearch->QuerySettings.Set(HeistOnlineSession::ProductSetting, HeistOnlineSession::ProductId, EOnlineComparisonOp::Equals);
	}

	SetOnlineSessionState(HeistOnlineSession::StateSearching);
	FindSessionsDelegateHandle =
		OnlineSessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &UHeistGameInstance::HandleFindSessionsComplete));
	UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("JoinByCode"), ActiveOnlineSubsystemName, PendingJoinCode, OnlineSessionState, true, TEXT("None"));

	if (!OnlineSessionInterface->FindSessions(0, ActiveSessionSearch.ToSharedRef()))
	{
		OnlineSessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsDelegateHandle);
		FindSessionsDelegateHandle.Reset();
		ActiveSessionSearch.Reset();
		FailOnlineSessionRequest(FName(TEXT("FindRequestRejected")));
		UHeistDebugFunctionLibrary::DebugOnlineSessionFindComplete(this, PendingJoinCode, 0, 0, 0, 0, NAME_None, false, LastOnlineSessionFailure);
		return false;
	}

	return true;
}

FName UHeistGameInstance::GetOnlineSessionState() const
{
	return OnlineSessionState;
}

FName UHeistGameInstance::GetLastOnlineSessionFailure() const
{
	return LastOnlineSessionFailure;
}

FName UHeistGameInstance::GetActiveOnlineSubsystemName() const
{
	return ActiveOnlineSubsystemName;
}

FString UHeistGameInstance::GetActiveJoinCode() const
{
	return ActiveJoinCode;
}

FString UHeistGameInstance::GetPendingJoinCode() const
{
	return PendingJoinCode;
}

bool UHeistGameInstance::IsOnlineSessionOperationPending() const
{
	return OnlineSessionState == HeistOnlineSession::StateCreating || OnlineSessionState == HeistOnlineSession::StateSearching || OnlineSessionState == HeistOnlineSession::StateJoining;
}

bool UHeistGameInstance::IsHostingOnlineSession() const
{
	return OnlineSessionState == HeistOnlineSession::StateHosting;
}

bool UHeistGameInstance::IsJoinedOnlineSession() const
{
	return OnlineSessionState == HeistOnlineSession::StateJoined;
}

int32 UHeistGameInstance::GetSessionBuildUniqueId() const
{
	return GetBuildUniqueId();
}

int32 UHeistGameInstance::GetMaxPublicConnections() const
{
	return FMath::Clamp(MaxPublicConnections, 1, 4);
}

FName UHeistGameInstance::GetSelectedMapId() const
{
	return SelectedMapId;
}

FString UHeistGameInstance::GetLobbyMapPath() const
{
	return LobbyMapPath;
}

FHeistOnlineSessionStateChanged& UHeistGameInstance::GetOnlineSessionStateChangedDelegate()
{
	return OnlineSessionStateChangedDelegate;
}

bool UHeistGameInstance::RefreshOnlineSessionInterface()
{
	IOnlineSubsystem* OnlineSubsystem = nullptr;
	UWorld* World = GetWorld();
#if WITH_EDITOR
	if (GIsEditor && IsValid(World))
	{
		OnlineSubsystem = Online::GetSubsystem(World, NULL_SUBSYSTEM);
	}
#endif
	if (OnlineSubsystem == nullptr && IsValid(World))
	{
		OnlineSubsystem = Online::GetSubsystem(World);
	}
	if (OnlineSubsystem == nullptr)
	{
		OnlineSubsystem = IOnlineSubsystem::Get();
	}

	if (OnlineSubsystem == nullptr)
	{
		OnlineSessionInterface.Reset();
		ActiveOnlineSubsystemName = NAME_None;
		return false;
	}

	OnlineSessionInterface = OnlineSubsystem->GetSessionInterface();
	ActiveOnlineSubsystemName = OnlineSubsystem->GetSubsystemName();
	return OnlineSessionInterface.IsValid();
}

bool UHeistGameInstance::BeginCreateSession()
{
	if (!OnlineSessionInterface.IsValid())
	{
		return false;
	}

	FOnlineSessionSettings SessionSettings;
	SessionSettings.NumPublicConnections = GetMaxPublicConnections();
	SessionSettings.NumPrivateConnections = 0;
	SessionSettings.bShouldAdvertise = true;
	SessionSettings.bAllowJoinInProgress = true;
	SessionSettings.bIsLANMatch = ActiveOnlineSubsystemName == NULL_SUBSYSTEM;
	SessionSettings.bIsDedicated = false;
	SessionSettings.bUsesStats = false;
	SessionSettings.bAllowInvites = true;
	SessionSettings.bUsesPresence = true;
	SessionSettings.bAllowJoinViaPresence = true;
	SessionSettings.bAllowJoinViaPresenceFriendsOnly = false;
	SessionSettings.bAntiCheatProtected = false;
	SessionSettings.bUseLobbiesIfAvailable = true;
	SessionSettings.bUseLobbiesVoiceChatIfAvailable = false;
	SessionSettings.BuildUniqueId = GetBuildUniqueId();
	SessionSettings.Set(HeistOnlineSession::JoinCodeSetting, ActiveJoinCode, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionSettings.Set(HeistOnlineSession::MapIdSetting, SelectedMapId.ToString(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionSettings.Set(HeistOnlineSession::ProductSetting, HeistOnlineSession::ProductId, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionSettings.Set(SETTING_MAPNAME, LobbyMapPath, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	CreateSessionDelegateHandle =
		OnlineSessionInterface->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &UHeistGameInstance::HandleCreateSessionComplete));
	const bool bRequestAccepted = OnlineSessionInterface->CreateSession(0, HeistOnlineSession::SessionName, SessionSettings);
	if (!bRequestAccepted)
	{
		OnlineSessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionDelegateHandle);
		CreateSessionDelegateHandle.Reset();
	}
	return bRequestAccepted;
}

bool UHeistGameInstance::TravelHostToLobby()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	if (World->GetNetMode() == NM_ListenServer)
	{
		if (AHeistGameState* HeistGameState = World->GetGameState<AHeistGameState>())
		{
			HeistGameState->SetMatchPhase(EHeistMatchPhase::Lobby);
		}
		return true;
	}

	if (LobbyMapPath.IsEmpty())
	{
		return false;
	}

	const FString TravelOptions = ActiveOnlineSubsystemName == NULL_SUBSYSTEM ? TEXT("listen?HeistLobby=1?bIsLanMatch=1") : TEXT("listen?HeistLobby=1");
	UGameplayStatics::OpenLevel(this, FName(*LobbyMapPath), true, TravelOptions);
	return true;
}

void UHeistGameInstance::ClearOnlineDelegates()
{
	if (!OnlineSessionInterface.IsValid())
	{
		return;
	}

	if (CreateSessionDelegateHandle.IsValid())
	{
		OnlineSessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionDelegateHandle);
		CreateSessionDelegateHandle.Reset();
	}
	if (FindSessionsDelegateHandle.IsValid())
	{
		OnlineSessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsDelegateHandle);
		FindSessionsDelegateHandle.Reset();
	}
	if (JoinSessionDelegateHandle.IsValid())
	{
		OnlineSessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionDelegateHandle);
		JoinSessionDelegateHandle.Reset();
	}
}

void UHeistGameInstance::SetOnlineSessionState(const FName NewState, const FName FailureReason)
{
	const bool bStateChanged = OnlineSessionState != NewState || LastOnlineSessionFailure != FailureReason;
	OnlineSessionState = NewState;
	LastOnlineSessionFailure = FailureReason;
	if (bStateChanged)
	{
		OnlineSessionStateChangedDelegate.Broadcast();
	}
}

void UHeistGameInstance::FailOnlineSessionRequest(const FName FailureReason)
{
	SetOnlineSessionState(HeistOnlineSession::StateFailed, FailureReason);
}

void UHeistGameInstance::HandleCreateSessionComplete(const FName SessionName, const bool bWasSuccessful)
{
	if (OnlineSessionInterface.IsValid() && CreateSessionDelegateHandle.IsValid())
	{
		OnlineSessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionDelegateHandle);
		CreateSessionDelegateHandle.Reset();
	}

	const bool bCorrectSession = SessionName == HeistOnlineSession::SessionName;
	if (!bWasSuccessful || !bCorrectSession)
	{
		const FString FailedJoinCode = ActiveJoinCode;
		ActiveJoinCode.Reset();
		FailOnlineSessionRequest(FName(bCorrectSession ? TEXT("CreateFailed") : TEXT("UnexpectedSessionName")));
		UHeistDebugFunctionLibrary::DebugOnlineSessionCreateComplete(this, SessionName, ActiveOnlineSubsystemName, FailedJoinCode, bWasSuccessful, false, LastOnlineSessionFailure);
		return;
	}

	SetOnlineSessionState(HeistOnlineSession::StateHosting);
	const bool bTravelAccepted = TravelHostToLobby();
	if (!bTravelAccepted)
	{
		FailOnlineSessionRequest(FName(TEXT("LobbyTravelRejected")));
	}
	UHeistDebugFunctionLibrary::DebugOnlineSessionCreateComplete(this, SessionName, ActiveOnlineSubsystemName, ActiveJoinCode, bWasSuccessful, bTravelAccepted,
																 LastOnlineSessionFailure);
}

void UHeistGameInstance::HandleFindSessionsComplete(const bool bWasSuccessful)
{
	if (OnlineSessionInterface.IsValid() && FindSessionsDelegateHandle.IsValid())
	{
		OnlineSessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsDelegateHandle);
		FindSessionsDelegateHandle.Reset();
	}

	const int32 ResultCount = ActiveSessionSearch.IsValid() ? ActiveSessionSearch->SearchResults.Num() : 0;
	if (!bWasSuccessful || !ActiveSessionSearch.IsValid())
	{
		ActiveSessionSearch.Reset();
		FailOnlineSessionRequest(FName(TEXT("FindFailed")));
		UHeistDebugFunctionLibrary::DebugOnlineSessionFindComplete(this, PendingJoinCode, ResultCount, 0, 0, 0, NAME_None, false, LastOnlineSessionFailure);
		return;
	}

	const FOnlineSessionSearchResult* BestResult = nullptr;
	int32 MatchingCodeCount = 0;
	int32 FullMatchCount = 0;
	int32 VersionMismatchCount = 0;
	for (const FOnlineSessionSearchResult& SearchResult : ActiveSessionSearch->SearchResults)
	{
		FString ResultProduct;
		FString ResultJoinCode;
		SearchResult.Session.SessionSettings.Get(HeistOnlineSession::ProductSetting, ResultProduct);
		SearchResult.Session.SessionSettings.Get(HeistOnlineSession::JoinCodeSetting, ResultJoinCode);
		if (ResultProduct != HeistOnlineSession::ProductId || NormalizeJoinCode(ResultJoinCode) != PendingJoinCode)
		{
			continue;
		}

		++MatchingCodeCount;
		if (SearchResult.Session.SessionSettings.BuildUniqueId != GetBuildUniqueId())
		{
			++VersionMismatchCount;
			continue;
		}
		if (SearchResult.Session.NumOpenPublicConnections <= 0)
		{
			++FullMatchCount;
			continue;
		}
		if (BestResult == nullptr || SearchResult.PingInMs < BestResult->PingInMs)
		{
			BestResult = &SearchResult;
		}
	}

	if (BestResult == nullptr)
	{
		const FName FailureReason = MatchingCodeCount == 0
										? FName(TEXT("SessionNotFound"))
										: (VersionMismatchCount == MatchingCodeCount ? FName(TEXT("VersionMismatch"))
																				   : (FullMatchCount > 0 ? FName(TEXT("SessionFull")) : FName(TEXT("SessionNotJoinable"))));
		FailOnlineSessionRequest(FailureReason);
		UHeistDebugFunctionLibrary::DebugOnlineSessionFindComplete(this, PendingJoinCode, ResultCount, MatchingCodeCount, FullMatchCount, VersionMismatchCount, NAME_None, false,
																 LastOnlineSessionFailure);
		ActiveSessionSearch.Reset();
		return;
	}

	const FName SelectedSessionId(*BestResult->GetSessionIdStr());
	SetOnlineSessionState(HeistOnlineSession::StateJoining);
	JoinSessionDelegateHandle =
		OnlineSessionInterface->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &UHeistGameInstance::HandleJoinSessionComplete));
	const bool bJoinRequestAccepted = OnlineSessionInterface->JoinSession(0, HeistOnlineSession::SessionName, *BestResult);
	UHeistDebugFunctionLibrary::DebugOnlineSessionFindComplete(this, PendingJoinCode, ResultCount, MatchingCodeCount, FullMatchCount, VersionMismatchCount, SelectedSessionId,
															 bJoinRequestAccepted, bJoinRequestAccepted ? NAME_None : FName(TEXT("JoinRequestRejected")));
	if (!bJoinRequestAccepted)
	{
		OnlineSessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionDelegateHandle);
		JoinSessionDelegateHandle.Reset();
		ActiveSessionSearch.Reset();
		FailOnlineSessionRequest(FName(TEXT("JoinRequestRejected")));
	}
}

void UHeistGameInstance::HandleJoinSessionComplete(const FName SessionName, const EOnJoinSessionCompleteResult::Type JoinResult)
{
	if (OnlineSessionInterface.IsValid() && JoinSessionDelegateHandle.IsValid())
	{
		OnlineSessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionDelegateHandle);
		JoinSessionDelegateHandle.Reset();
	}

	const bool bJoinSucceeded = JoinResult == EOnJoinSessionCompleteResult::Success && SessionName == HeistOnlineSession::SessionName;
	FString ConnectString;
	const bool bConnectStringResolved =
		bJoinSucceeded && OnlineSessionInterface.IsValid() && OnlineSessionInterface->GetResolvedConnectString(HeistOnlineSession::SessionName, ConnectString);
	APlayerController* LocalPlayerController = GetFirstLocalPlayerController();
	if (!bJoinSucceeded || !bConnectStringResolved || !IsValid(LocalPlayerController))
	{
		const FName FailureReason = !bJoinSucceeded ? ResolveJoinResultReason(JoinResult)
												  : (!bConnectStringResolved ? FName(TEXT("ConnectStringNotResolved")) : FName(TEXT("MissingLocalPlayerController")));
		FailOnlineSessionRequest(FailureReason);
		UHeistDebugFunctionLibrary::DebugOnlineSessionJoinComplete(this, SessionName, PendingJoinCode, static_cast<int32>(JoinResult), bConnectStringResolved, false,
																  LastOnlineSessionFailure);
		ActiveSessionSearch.Reset();
		return;
	}

	ActiveJoinCode = PendingJoinCode;
	PendingJoinCode.Reset();
	SetOnlineSessionState(HeistOnlineSession::StateJoined);
	UHeistDebugFunctionLibrary::DebugOnlineSessionJoinComplete(this, SessionName, ActiveJoinCode, static_cast<int32>(JoinResult), true, true, NAME_None);
	ActiveSessionSearch.Reset();
	LocalPlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
}

FString UHeistGameInstance::NormalizeJoinCode(const FString& JoinCode)
{
	FString Normalized = JoinCode;
	Normalized.TrimStartAndEndInline();
	Normalized.ReplaceInline(TEXT("-"), TEXT(""));
	Normalized.ReplaceInline(TEXT(" "), TEXT(""));
	Normalized.ToUpperInline();
	return Normalized;
}

bool UHeistGameInstance::IsJoinCodeValid(const FString& JoinCode)
{
	if (JoinCode.Len() != 6)
	{
		return false;
	}

	for (const TCHAR Character : JoinCode)
	{
		int32 CharacterIndex = INDEX_NONE;
		if (!HeistOnlineSession::JoinCodeAlphabet.FindChar(Character, CharacterIndex))
		{
			return false;
		}
	}
	return true;
}

FString UHeistGameInstance::GenerateJoinCode()
{
	const FGuid Guid = FGuid::NewGuid();
	uint64 RandomBits = (static_cast<uint64>(Guid.A) << 32) | static_cast<uint32>(Guid.B);
	FString JoinCode;
	JoinCode.Reserve(6);
	for (int32 Index = 0; Index < 6; ++Index)
	{
		JoinCode.AppendChar(HeistOnlineSession::JoinCodeAlphabet[RandomBits % HeistOnlineSession::JoinCodeAlphabet.Len()]);
		RandomBits /= HeistOnlineSession::JoinCodeAlphabet.Len();
	}
	return JoinCode;
}

FName UHeistGameInstance::ResolveJoinResultReason(const EOnJoinSessionCompleteResult::Type JoinResult)
{
	switch (JoinResult)
	{
		case EOnJoinSessionCompleteResult::SessionIsFull:
			return FName(TEXT("SessionFull"));
		case EOnJoinSessionCompleteResult::SessionDoesNotExist:
			return FName(TEXT("SessionNotFound"));
		case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress:
			return FName(TEXT("ConnectStringNotResolved"));
		case EOnJoinSessionCompleteResult::AlreadyInSession:
			return FName(TEXT("SessionAlreadyExists"));
		case EOnJoinSessionCompleteResult::UnknownError:
			return FName(TEXT("JoinUnknownError"));
		default:
			return FName(TEXT("JoinFailed"));
	}
}

#pragma endregion
