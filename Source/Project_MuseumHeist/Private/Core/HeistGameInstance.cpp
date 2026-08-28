#include "Core/HeistGameInstance.h"

#include "Core/HeistGameMode.h"
#include "Core/HeistGameUserSettings.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerController.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "TimerManager.h"

namespace HeistOnlineSession
{
const FName SessionName(TEXT("HeistSession"));
const FName JoinCodeSetting(TEXT("HEIST_JOIN_CODE"));
const FName MapIdSetting(TEXT("HEIST_MAP_ID"));
const FName MapSelectionModeSetting(TEXT("HEIST_MAP_MODE"));
const FName ProductSetting(TEXT("HEIST_PRODUCT"));
const FString ProductId(TEXT("MUSEUM_HEIST"));
const FString JoinCodeAlphabet(TEXT("ABCDEFGHJKLMNPQRSTUVWXYZ23456789"));
const FName RandomMapSelection(TEXT("RANDOM"));
const FString FixedMapSelectionMode(TEXT("FIXED"));
const FString RandomMapSelectionMode(TEXT("RANDOM"));

const FName StateIdle(TEXT("Idle"));
const FName StateCreating(TEXT("Creating"));
const FName StateHosting(TEXT("Hosting"));
const FName StateSearching(TEXT("Searching"));
const FName StateJoining(TEXT("Joining"));
const FName StateJoined(TEXT("Joined"));
const FName StateLeaving(TEXT("Leaving"));
const FName StateFailed(TEXT("Failed"));

const FName OperationCreate(TEXT("Create"));
const FName OperationFind(TEXT("Find"));
const FName OperationJoin(TEXT("Join"));
const FName OperationLeave(TEXT("Leave"));
const FName OperationMapUpdate(TEXT("MapUpdate"));
const FName OperationTravelLobby(TEXT("TravelLobby"));
const FName OperationTravelGameplay(TEXT("TravelGameplay"));
const FName OperationTravelJoin(TEXT("TravelJoin"));

const FName RetryHost(TEXT("Host"));
const FName RetryJoin(TEXT("Join"));
const FName RetryTravelLobby(TEXT("TravelLobby"));
const FName RetryTravelGameplay(TEXT("TravelGameplay"));

const FName DestinationLobby(TEXT("Lobby"));
const FName DestinationGameplay(TEXT("Gameplay"));
const FName DestinationJoinedSession(TEXT("JoinedSession"));
}

namespace HeistSurfaceTemplate
{
constexpr int32 RecentHistoryLimit = 3;

TArray<FName> NormalizeCandidateIds(const TArray<FName>& CandidateTemplateIds)
{
	TArray<FName> NormalizedIds;
	TSet<FName> UniqueIds;
	for (const FName TemplateId : CandidateTemplateIds)
	{
		if (!TemplateId.IsNone() && !UniqueIds.Contains(TemplateId))
		{
			UniqueIds.Add(TemplateId);
			NormalizedIds.Add(TemplateId);
		}
	}

	NormalizedIds.Sort([](const FName Left, const FName Right) { return Left.ToString() < Right.ToString(); });
	return NormalizedIds;
}

bool DrawFromShuffleBag(const TArray<FName>& CandidateTemplateIds, TArray<FName>& RemainingTemplateIds, TArray<FName>& RecentTemplateIds, int32& BagCycle, FRandomStream& RandomStream,
						FName& OutTemplateId, const TSet<FName>* ExcludedTemplateIds = nullptr)
{
	OutTemplateId = NAME_None;
	if (CandidateTemplateIds.IsEmpty())
	{
		return false;
	}

	TSet<FName> CandidateSet;
	for (const FName TemplateId : CandidateTemplateIds)
	{
		CandidateSet.Add(TemplateId);
	}
	RemainingTemplateIds.RemoveAll([&CandidateSet](const FName TemplateId) { return TemplateId.IsNone() || !CandidateSet.Contains(TemplateId); });
	RecentTemplateIds.RemoveAll([&CandidateSet](const FName TemplateId) { return TemplateId.IsNone() || !CandidateSet.Contains(TemplateId); });

	if (RemainingTemplateIds.IsEmpty())
	{
		RemainingTemplateIds = CandidateTemplateIds;
		++BagCycle;
	}

	TArray<int32> EligibleIndices;
	for (int32 TemplateIndex = 0; TemplateIndex < RemainingTemplateIds.Num(); ++TemplateIndex)
	{
		const FName CandidateId = RemainingTemplateIds[TemplateIndex];
		if (!RecentTemplateIds.Contains(CandidateId) && (ExcludedTemplateIds == nullptr || !ExcludedTemplateIds->Contains(CandidateId)))
		{
			EligibleIndices.Add(TemplateIndex);
		}
	}
	if (EligibleIndices.IsEmpty() && ExcludedTemplateIds != nullptr)
	{
		for (int32 TemplateIndex = 0; TemplateIndex < RemainingTemplateIds.Num(); ++TemplateIndex)
		{
			if (!ExcludedTemplateIds->Contains(RemainingTemplateIds[TemplateIndex]))
			{
				EligibleIndices.Add(TemplateIndex);
			}
		}
	}
	if (EligibleIndices.IsEmpty())
	{
		return false;
	}

	const int32 SelectedIndex = EligibleIndices[RandomStream.RandRange(0, EligibleIndices.Num() - 1)];
	OutTemplateId = RemainingTemplateIds[SelectedIndex];
	RemainingTemplateIds.RemoveAtSwap(SelectedIndex, 1, EAllowShrinking::No);
	RecentTemplateIds.Add(OutTemplateId);
	if (RecentTemplateIds.Num() > RecentHistoryLimit)
	{
		RecentTemplateIds.RemoveAt(0, RecentTemplateIds.Num() - RecentHistoryLimit, EAllowShrinking::No);
	}
	return true;
}
}

#pragma region Lifecycle

void UHeistGameInstance::Init()
{
	Super::Init();
	if (UHeistGameUserSettings* Settings = UHeistGameUserSettings::GetHeistGameUserSettings())
	{
		Settings->ApplyMasterVolumeToActiveAudioDevices();
	}
	DefaultSelectedMapId = SelectedMapId;
	SurfaceTemplateRandomStream.GenerateNewSeed();
	RandomMapSelectionStream.GenerateNewSeed();
	RefreshOnlineSessionInterface();
	if (GEngine != nullptr)
	{
		GEngine->OnNetworkFailure().AddUObject(this, &UHeistGameInstance::HandleEngineNetworkFailure);
		GEngine->OnTravelFailure().AddUObject(this, &UHeistGameInstance::HandleEngineTravelFailure);
	}
}

void UHeistGameInstance::Shutdown()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HostLeaveGraceTimerHandle);
	}
	if (GEngine != nullptr)
	{
		GEngine->OnNetworkFailure().RemoveAll(this);
		GEngine->OnTravelFailure().RemoveAll(this);
	}
	CompleteOnlineSessionOperation();
	ClearOnlineDelegates();
	ActiveSessionSearch.Reset();
	OnlineSessionInterface.Reset();
	Super::Shutdown();
}

#pragma endregion

#pragma region SurfaceTemplateSelection

bool UHeistGameInstance::SelectSurfaceTemplateForMatch(const FName PoolId, const TArray<FName>& CandidateTemplateIds, FName& OutTemplateId, int32& OutSelectionRevision, int32& OutBagCycle,
													   int32& OutRemainingTemplateCount)
{
	OutTemplateId = NAME_None;
	TArray<FName> SelectedTemplateIds;
	if (!SelectSurfaceTemplatesForMatch(PoolId, CandidateTemplateIds, 1, SelectedTemplateIds, OutSelectionRevision, OutBagCycle, OutRemainingTemplateCount) || SelectedTemplateIds.Num() != 1)
	{
		return false;
	}

	OutTemplateId = SelectedTemplateIds[0];
	return true;
}

bool UHeistGameInstance::SelectSurfaceTemplatesForMatch(const FName PoolId, const TArray<FName>& CandidateTemplateIds, const int32 RequestedTemplateCount, TArray<FName>& OutTemplateIds,
														int32& OutSelectionRevision, int32& OutBagCycle, int32& OutRemainingTemplateCount)
{
	OutTemplateIds.Reset();
	OutSelectionRevision = 0;
	OutBagCycle = 0;
	OutRemainingTemplateCount = 0;
	UWorld* World = GetWorld();
	const TArray<FName> NormalizedCandidateIds = HeistSurfaceTemplate::NormalizeCandidateIds(CandidateTemplateIds);
	if (PoolId.IsNone() || NormalizedCandidateIds.IsEmpty() || RequestedTemplateCount <= 0 || !IsValid(World) || World->GetNetMode() == NM_Client)
	{
		return false;
	}

	TArray<FName>& Catalog = SurfaceTemplateCatalogByPool.FindOrAdd(PoolId);
	TArray<FName>& RemainingIds = RemainingSurfaceTemplateIdsByPool.FindOrAdd(PoolId);
	TArray<FName>& RecentIds = RecentSurfaceTemplateIdsByPool.FindOrAdd(PoolId);
	int32& BagCycle = SurfaceTemplateBagCycleByPool.FindOrAdd(PoolId);
	if (Catalog != NormalizedCandidateIds)
	{
		Catalog = NormalizedCandidateIds;
		RemainingIds.Reset();
		RecentIds.Reset();
		BagCycle = 0;
	}

	const int32 ResolvedTemplateCount = FMath::Min(RequestedTemplateCount, Catalog.Num());
	TSet<FName> SelectedThisMatch;
	OutTemplateIds.Reserve(ResolvedTemplateCount);
	for (int32 SelectionIndex = 0; SelectionIndex < ResolvedTemplateCount; ++SelectionIndex)
	{
		FName SelectedTemplateId = NAME_None;
		if (!HeistSurfaceTemplate::DrawFromShuffleBag(Catalog, RemainingIds, RecentIds, BagCycle, SurfaceTemplateRandomStream, SelectedTemplateId, &SelectedThisMatch))
		{
			OutTemplateIds.Reset();
			return false;
		}

		SelectedThisMatch.Add(SelectedTemplateId);
		OutTemplateIds.Add(SelectedTemplateId);
	}

	++SurfaceTemplateSelectionRevision;
	OutSelectionRevision = SurfaceTemplateSelectionRevision;
	OutBagCycle = BagCycle;
	OutRemainingTemplateCount = RemainingIds.Num();
	return true;
}

bool UHeistGameInstance::RunSurfaceTemplateShuffleBagSelfTestForDebug(const int32 PoolSize, int32& OutDrawCount, int32& OutFirstCycleUniqueCount, int32& OutSecondCycleUniqueCount,
																	  int32& OutRecentProtectionCheckCount, int32& OutRecentProtectionPassCount) const
{
	OutDrawCount = 0;
	OutFirstCycleUniqueCount = 0;
	OutSecondCycleUniqueCount = 0;
	OutRecentProtectionCheckCount = 0;
	OutRecentProtectionPassCount = 0;
#if UE_BUILD_SHIPPING
	return false;
#else
	if (PoolSize < 4 || PoolSize > 64)
	{
		return false;
	}

	TArray<FName> Candidates;
	Candidates.Reserve(PoolSize);
	for (int32 TemplateIndex = 0; TemplateIndex < PoolSize; ++TemplateIndex)
	{
		Candidates.Add(FName(*FString::Printf(TEXT("SelfTest_Surface_%02d"), TemplateIndex + 1)));
	}

	TArray<FName> RemainingIds;
	TArray<FName> RecentIds;
	TSet<FName> FirstCycleIds;
	TSet<FName> SecondCycleIds;
	TArray<FName> FirstPassSequence;
	FirstPassSequence.Reserve(PoolSize * 2);
	int32 BagCycle = 0;
	FRandomStream TestRandomStream(160516);
	for (int32 DrawIndex = 0; DrawIndex < PoolSize * 2; ++DrawIndex)
	{
		const TArray<FName> RecentBeforeDraw = RecentIds;
		FName SelectedTemplateId = NAME_None;
		if (!HeistSurfaceTemplate::DrawFromShuffleBag(Candidates, RemainingIds, RecentIds, BagCycle, TestRandomStream, SelectedTemplateId))
		{
			return false;
		}

		if (RecentBeforeDraw.Num() == HeistSurfaceTemplate::RecentHistoryLimit)
		{
			++OutRecentProtectionCheckCount;
			if (!RecentBeforeDraw.Contains(SelectedTemplateId))
			{
				++OutRecentProtectionPassCount;
			}
		}
		if (DrawIndex < PoolSize)
		{
			FirstCycleIds.Add(SelectedTemplateId);
		}
		else
		{
			SecondCycleIds.Add(SelectedTemplateId);
		}
		FirstPassSequence.Add(SelectedTemplateId);
		++OutDrawCount;
	}

	TArray<FName> ReplayRemainingIds;
	TArray<FName> ReplayRecentIds;
	int32 ReplayBagCycle = 0;
	FRandomStream ReplayRandomStream(160516);
	for (const FName ExpectedTemplateId : FirstPassSequence)
	{
		FName ReplayedTemplateId = NAME_None;
		if (!HeistSurfaceTemplate::DrawFromShuffleBag(Candidates, ReplayRemainingIds, ReplayRecentIds, ReplayBagCycle, ReplayRandomStream, ReplayedTemplateId) ||
			ReplayedTemplateId != ExpectedTemplateId)
		{
			return false;
		}
	}

	OutFirstCycleUniqueCount = FirstCycleIds.Num();
	OutSecondCycleUniqueCount = SecondCycleIds.Num();
	return OutDrawCount == PoolSize * 2 && OutFirstCycleUniqueCount == PoolSize && OutSecondCycleUniqueCount == PoolSize && OutRecentProtectionCheckCount > 0 &&
		   OutRecentProtectionPassCount == OutRecentProtectionCheckCount;
#endif
}

bool UHeistGameInstance::RunSurfaceTemplateMatchSelectionSelfTestForDebug(const int32 PoolSize, const int32 RequestedTemplateCount, int32& OutSelectedCount, int32& OutUniqueCount,
																		  int32& OutBagCycle) const
{
	OutSelectedCount = 0;
	OutUniqueCount = 0;
	OutBagCycle = 0;
#if UE_BUILD_SHIPPING
	return false;
#else
	if (PoolSize < 4 || PoolSize > 64 || RequestedTemplateCount <= 0 || RequestedTemplateCount > PoolSize)
	{
		return false;
	}

	TArray<FName> Candidates;
	Candidates.Reserve(PoolSize);
	for (int32 TemplateIndex = 0; TemplateIndex < PoolSize; ++TemplateIndex)
	{
		Candidates.Add(FName(*FString::Printf(TEXT("SelfTest_Surface_%02d"), TemplateIndex + 1)));
	}

	TArray<FName> RemainingIds;
	const int32 InitialRemainingCount = FMath::Max(1, FMath::Min(RequestedTemplateCount / 2, PoolSize));
	RemainingIds.Append(Candidates.GetData(), InitialRemainingCount);
	TArray<FName> RecentIds;
	TSet<FName> SelectedIds;
	FRandomStream TestRandomStream(200840);
	OutBagCycle = 1;
	for (int32 SelectionIndex = 0; SelectionIndex < RequestedTemplateCount; ++SelectionIndex)
	{
		FName SelectedTemplateId = NAME_None;
		if (!HeistSurfaceTemplate::DrawFromShuffleBag(Candidates, RemainingIds, RecentIds, OutBagCycle, TestRandomStream, SelectedTemplateId, &SelectedIds))
		{
			return false;
		}

		SelectedIds.Add(SelectedTemplateId);
		++OutSelectedCount;
	}

	OutUniqueCount = SelectedIds.Num();
	return OutSelectedCount == RequestedTemplateCount && OutUniqueCount == RequestedTemplateCount;
#endif
}

void UHeistGameInstance::ResetSurfaceTemplateShuffleState()
{
	SurfaceTemplateCatalogByPool.Reset();
	RemainingSurfaceTemplateIdsByPool.Reset();
	RecentSurfaceTemplateIdsByPool.Reset();
	SurfaceTemplateBagCycleByPool.Reset();
	SurfaceTemplateSelectionRevision = 0;
	SurfaceTemplateRandomStream.GenerateNewSeed();
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
	if (!IsCurrentWorldTitleMenu())
	{
		FailOnlineSessionRequest(FName(TEXT("TitleMenuOnly")));
		UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("Host"), ActiveOnlineSubsystemName, ActiveJoinCode, OnlineSessionState, false, TEXT("TitleMenuOnly"));
		return false;
	}

	if (!IsValid(World) || World->GetNetMode() == NM_Client)
	{
		FailOnlineSessionRequest(FName(TEXT("ClientCannotHost")));
		UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("Host"), ActiveOnlineSubsystemName, ActiveJoinCode, OnlineSessionState, false, TEXT("ClientCannotHost"));
		return false;
	}

	LastRetryRequest = HeistOnlineSession::RetryHost;
	LastRetryJoinCode.Reset();
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
	BeginOnlineSessionOperation(HeistOnlineSession::OperationCreate, OnlineSessionOperationTimeoutSeconds);
	SetOnlineSessionState(HeistOnlineSession::StateCreating);
	UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("Host"), ActiveOnlineSubsystemName, ActiveJoinCode, OnlineSessionState, true, TEXT("None"));

	if (!BeginCreateSession())
	{
		CompleteOnlineSessionOperation();
		const FString RejectedJoinCode = ActiveJoinCode;
		ActiveJoinCode.Reset();
		FailOnlineSessionRequest(FName(TEXT("CreateRequestRejected")));
		UHeistDebugFunctionLibrary::DebugOnlineSessionCreateComplete(this, HeistOnlineSession::SessionName, ActiveOnlineSubsystemName, RejectedJoinCode, false, false, LastOnlineSessionFailure);
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
		UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("JoinByCode"), ActiveOnlineSubsystemName, NormalizedJoinCode, OnlineSessionState, false, TEXT("SessionAlreadyExists"));
		return false;
	}
	if (!IsCurrentWorldTitleMenu())
	{
		FailOnlineSessionRequest(FName(TEXT("TitleMenuOnly")));
		UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("JoinByCode"), ActiveOnlineSubsystemName, NormalizedJoinCode, OnlineSessionState, false, TEXT("TitleMenuOnly"));
		return false;
	}

	if (!IsJoinCodeValid(NormalizedJoinCode))
	{
		FailOnlineSessionRequest(FName(TEXT("InvalidJoinCode")));
		UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("JoinByCode"), ActiveOnlineSubsystemName, NormalizedJoinCode, OnlineSessionState, false, TEXT("InvalidJoinCode"));
		return false;
	}

	LastRetryRequest = HeistOnlineSession::RetryJoin;
	LastRetryJoinCode = NormalizedJoinCode;
	if (!RefreshOnlineSessionInterface())
	{
		FailOnlineSessionRequest(FName(TEXT("OnlineSessionUnavailable")));
		UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("JoinByCode"), ActiveOnlineSubsystemName, NormalizedJoinCode, OnlineSessionState, false, TEXT("OnlineSessionUnavailable"));
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
		UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("JoinByCode"), ActiveOnlineSubsystemName, NormalizedJoinCode, OnlineSessionState, false, TEXT("SessionAlreadyExists"));
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

	BeginOnlineSessionOperation(HeistOnlineSession::OperationFind, OnlineSessionOperationTimeoutSeconds);
	SetOnlineSessionState(HeistOnlineSession::StateSearching);
	FindSessionsDelegateHandle =
		OnlineSessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &UHeistGameInstance::HandleFindSessionsComplete));
	UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("JoinByCode"), ActiveOnlineSubsystemName, PendingJoinCode, OnlineSessionState, true, TEXT("None"));

	if (!OnlineSessionInterface->FindSessions(0, ActiveSessionSearch.ToSharedRef()))
	{
		CompleteOnlineSessionOperation();
		OnlineSessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsDelegateHandle);
		FindSessionsDelegateHandle.Reset();
		ActiveSessionSearch.Reset();
		FailOnlineSessionRequest(FName(TEXT("FindRequestRejected")));
		UHeistDebugFunctionLibrary::DebugOnlineSessionFindComplete(this, PendingJoinCode, 0, 0, 0, 0, NAME_None, false, LastOnlineSessionFailure);
		return false;
	}

	return true;
}

bool UHeistGameInstance::RequestLeaveSession()
{
	if (IsOnlineSessionOperationPending())
	{
		UHeistDebugFunctionLibrary::DebugOnlineSessionLeaveRequest(this, IsHostingOnlineSession(), OnlineSessionState, false, FName(TEXT("OperationPending")));
		return false;
	}
	if (!IsHostingOnlineSession() && !IsJoinedOnlineSession())
	{
		UHeistDebugFunctionLibrary::DebugOnlineSessionLeaveRequest(this, false, OnlineSessionState, false, FName(TEXT("SessionNotActive")));
		return false;
	}
	if (!RefreshOnlineSessionInterface())
	{
		FailOnlineSessionRequest(FName(TEXT("OnlineSessionUnavailable")));
		UHeistDebugFunctionLibrary::DebugOnlineSessionLeaveRequest(this, IsHostingOnlineSession(), OnlineSessionState, false, LastOnlineSessionFailure);
		return false;
	}

	const bool bWasHosting = IsHostingOnlineSession();
	const FName LeaveReason = bWasHosting ? FName(TEXT("HostLeave")) : FName(TEXT("ClientLeave"));
	bool bAccepted = false;
	if (bWasHosting)
	{
		if (AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr)
		{
			HeistGameMode->PrepareForOnlineSessionShutdown(LeaveReason);
		}
		NotifyRemoteClientsSessionEnded(FName(TEXT("HostQuit")));

		if (UWorld* World = GetWorld())
		{
			bLeaveWasHosting = true;
			PendingLeaveReason = LeaveReason;
			SetOnlineSessionState(HeistOnlineSession::StateLeaving);
			World->GetTimerManager().SetTimer(HostLeaveGraceTimerHandle, this, &UHeistGameInstance::HandleHostLeaveGracePeriodElapsed, FMath::Clamp(HostLeaveGraceSeconds, 0.25f, 3.0f), false);
			bAccepted = true;
		}
		else
		{
			bAccepted = BeginDestroySession(LeaveReason, true);
		}
	}
	else
	{
		bAccepted = BeginDestroySession(LeaveReason, false);
	}

	UHeistDebugFunctionLibrary::DebugOnlineSessionLeaveRequest(this, bWasHosting, OnlineSessionState, bAccepted, bAccepted ? NAME_None : LastOnlineSessionFailure);
	return bAccepted;
}

bool UHeistGameInstance::RequestCancelOnlineSessionOperation()
{
	if (!CanCancelOnlineSessionOperation())
	{
		UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("Cancel"), ActiveOnlineSubsystemName, PendingJoinCode, OnlineSessionState, false, TEXT("OperationNotCancellable"));
		return false;
	}

	const FName CancelledOperation = ActiveOnlineSessionOperation;
	if (CancelledOperation == HeistOnlineSession::OperationFind)
	{
		if (OnlineSessionInterface.IsValid() && FindSessionsDelegateHandle.IsValid())
		{
			OnlineSessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsDelegateHandle);
			FindSessionsDelegateHandle.Reset();
			OnlineSessionInterface->CancelFindSessions();
		}

		CompleteOnlineSessionOperation();
		ActiveSessionSearch.Reset();
		PendingSelectedMapId = NAME_None;
		bPendingRandomMapSelection = false;
		FailOnlineSessionRequest(FName(TEXT("OperationCancelled")));
		UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("Cancel"), ActiveOnlineSubsystemName, PendingJoinCode, OnlineSessionState, true, *CancelledOperation.ToString());
		return true;
	}

	bOnlineSessionCancellationPending = true;
	PendingOperationAbortFailure = FName(TEXT("OperationCancelled"));
	ClearOnlineSessionOperationTimeout();
	SetOnlineSessionState(HeistOnlineSession::StateFailed, PendingOperationAbortFailure);
	UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("Cancel"), ActiveOnlineSubsystemName, PendingJoinCode, OnlineSessionState, true, *CancelledOperation.ToString());
	return true;
}

bool UHeistGameInstance::RequestRetryLastOnlineSessionOperation()
{
	if (!CanRetryLastOnlineSessionOperation())
	{
		UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("Retry"), ActiveOnlineSubsystemName, LastRetryJoinCode, OnlineSessionState, false, TEXT("RetryUnavailable"));
		return false;
	}

	const FName RetryRequest = LastRetryRequest;
	const FString RetryJoinCode = LastRetryJoinCode;
	SetOnlineSessionState(OnlineSessionState);

	if (RetryRequest == HeistOnlineSession::RetryHost)
	{
		return RequestHostSession();
	}
	if (RetryRequest == HeistOnlineSession::RetryJoin)
	{
		return RequestJoinSessionByCode(RetryJoinCode);
	}
	if (RetryRequest == HeistOnlineSession::RetryTravelLobby)
	{
		const bool bAccepted = TravelHostToLobby();
		if (!bAccepted)
		{
			SetOnlineSessionState(HeistOnlineSession::StateHosting, FName(TEXT("LobbyTravelRejected")));
		}
		return bAccepted;
	}
	if (RetryRequest == HeistOnlineSession::RetryTravelGameplay)
	{
		return RequestStartSelectedGameplayMap();
	}

	return false;
}

bool UHeistGameInstance::RequestStartSelectedGameplayMap()
{
	UWorld* World = GetWorld();
	const AHeistGameState* HeistGameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
	FName RejectReason = NAME_None;

	if (!IsValid(World) || World->GetNetMode() == NM_Client)
	{
		RejectReason = FName(TEXT("NotHost"));
	}
	else if (!IsHostingOnlineSession() || !HasActiveNamedOnlineSession())
	{
		RejectReason = FName(TEXT("SessionNotHosting"));
	}
	else if (IsOnlineSessionOperationPending())
	{
		RejectReason = FName(TEXT("OperationPending"));
	}
	else if (!IsCurrentWorldLobby() || !IsValid(HeistGameState) || HeistGameState->GetMatchPhase() != EHeistMatchPhase::Lobby)
	{
		RejectReason = FName(TEXT("LobbyNotReady"));
	}
	else if (HeistGameState->GetConnectedPlayerCount() < HeistSessionContract::MinimumPublicPlayerCount)
	{
		RejectReason = FName(TEXT("MinimumPlayersRequired"));
	}
	else if (HeistGameState->GetConnectedPlayerCount() > HeistSessionContract::MaximumPublicPlayerCount)
	{
		RejectReason = FName(TEXT("MaximumPlayersExceeded"));
	}
	else if (!HeistGameState->AreAllConnectedPlayersLobbyReady())
	{
		RejectReason = FName(TEXT("PlayersNotReady"));
	}
	else if (HeistGameState->GetSelectedLobbyMapId() != SelectedMapId)
	{
		RejectReason = FName(TEXT("SelectedMapMismatch"));
	}
	else if (GetSelectedGameplayMapPath().IsEmpty())
	{
		RejectReason = FName(TEXT("GameplayMapNotConfigured"));
	}

	if (!RejectReason.IsNone())
	{
		SetOnlineSessionState(HeistOnlineSession::StateHosting, RejectReason);
		return false;
	}

	LastRetryRequest = HeistOnlineSession::RetryTravelGameplay;
	PendingContractStartPlayerCount = HeistGameState->GetConnectedPlayerCount();
	SetOnlineSessionState(HeistOnlineSession::StateHosting);
	if (TravelHostToSelectedGameplayMap())
	{
		return true;
	}

	ClearPendingContractStartPlayerCount();
	SetOnlineSessionState(HeistOnlineSession::StateHosting, FName(TEXT("GameplayTravelRejected")));
	return false;
}

int32 UHeistGameInstance::ConsumePendingContractStartPlayerCount()
{
	const int32 CapturedPlayerCount = PendingContractStartPlayerCount;
	ClearPendingContractStartPlayerCount();
	return HeistSessionContract::IsPublicStartPlayerCountSupported(CapturedPlayerCount) ? CapturedPlayerCount : 0;
}

bool UHeistGameInstance::RequestReturnToLobby()
{
	UWorld* World = GetWorld();
	const AHeistGameState* HeistGameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
	FName RejectReason = NAME_None;

	if (!IsValid(World) || World->GetNetMode() == NM_Client)
	{
		RejectReason = FName(TEXT("NotHost"));
	}
	else if (!IsHostingOnlineSession() || !HasActiveNamedOnlineSession())
	{
		RejectReason = FName(TEXT("SessionNotHosting"));
	}
	else if (IsOnlineSessionOperationPending())
	{
		RejectReason = FName(TEXT("OperationPending"));
	}
	else if (LobbyMapPath.IsEmpty() || !FPackageName::IsValidLongPackageName(LobbyMapPath) || !FPackageName::DoesPackageExist(LobbyMapPath))
	{
		RejectReason = FName(TEXT("LobbyMapNotConfigured"));
	}
	else if (!IsCurrentWorldSelectedGameplayMap() || !IsValid(HeistGameState) ||
			 (HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame && HeistGameState->GetMatchPhase() != EHeistMatchPhase::End))
	{
		RejectReason = FName(TEXT("GameplayWorldNotReady"));
	}

	if (!RejectReason.IsNone())
	{
		SetOnlineSessionState(HeistOnlineSession::StateHosting, RejectReason);
		return false;
	}

	if (AHeistGameMode* HeistGameMode = World->GetAuthGameMode<AHeistGameMode>())
	{
		HeistGameMode->PrepareForOnlineSessionShutdown(FName(TEXT("ReturnLobbyTravel")));
	}

	ClearPendingContractStartPlayerCount();
	SetOnlineSessionState(HeistOnlineSession::StateHosting);
	if (TravelHostToLobby())
	{
		return true;
	}

	SetOnlineSessionState(HeistOnlineSession::StateHosting, FName(TEXT("LobbyReturnTravelRejected")));
	return false;
}

bool UHeistGameInstance::RequestSetLobbyMapSelection(const FName RequestedMapId)
{
	UWorld* World = GetWorld();
	AHeistGameState* HeistGameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
	FName ResolvedMapId = NAME_None;
	bool bResolvedRandomSelection = false;
	FName RejectReason = NAME_None;

	if (!IsValid(World) || World->GetNetMode() == NM_Client)
	{
		RejectReason = FName(TEXT("NotHost"));
	}
	else if (!IsHostingOnlineSession())
	{
		RejectReason = FName(TEXT("SessionNotHosting"));
	}
	else if (IsOnlineSessionOperationPending())
	{
		RejectReason = FName(TEXT("OperationPending"));
	}
	else if (!IsValid(HeistGameState) || HeistGameState->GetMatchPhase() != EHeistMatchPhase::Lobby)
	{
		RejectReason = FName(TEXT("LobbyNotReady"));
	}
	else if (!ResolveRequestedMapSelection(RequestedMapId, ResolvedMapId, bResolvedRandomSelection))
	{
		RejectReason = FName(TEXT("InvalidMapSelection"));
	}

	if (!RejectReason.IsNone())
	{
		SetOnlineSessionState(OnlineSessionState, RejectReason);
		UHeistDebugFunctionLibrary::DebugOnlineSessionMapSelection(this, RequestedMapId, ResolvedMapId, bResolvedRandomSelection, false, false, RejectReason);
		return false;
	}
	if (SelectedMapId == ResolvedMapId && bRandomMapSelection == bResolvedRandomSelection)
	{
		SetOnlineSessionState(HeistOnlineSession::StateHosting);
		UHeistDebugFunctionLibrary::DebugOnlineSessionMapSelection(this, RequestedMapId, ResolvedMapId, bResolvedRandomSelection, false, true, NAME_None);
		return true;
	}
	if (!RefreshOnlineSessionInterface())
	{
		SetOnlineSessionState(OnlineSessionState, FName(TEXT("OnlineSessionUnavailable")));
		UHeistDebugFunctionLibrary::DebugOnlineSessionMapSelection(this, RequestedMapId, ResolvedMapId, bResolvedRandomSelection, false, false, LastOnlineSessionFailure);
		return false;
	}

	FNamedOnlineSession* NamedSession = OnlineSessionInterface->GetNamedSession(HeistOnlineSession::SessionName);
	if (NamedSession == nullptr)
	{
		SetOnlineSessionState(OnlineSessionState, FName(TEXT("SessionNotFound")));
		UHeistDebugFunctionLibrary::DebugOnlineSessionMapSelection(this, RequestedMapId, ResolvedMapId, bResolvedRandomSelection, false, false, LastOnlineSessionFailure);
		return false;
	}

	FOnlineSessionSettings UpdatedSettings = NamedSession->SessionSettings;
	UpdatedSettings.Set(HeistOnlineSession::MapIdSetting, ResolvedMapId.ToString(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	UpdatedSettings.Set(HeistOnlineSession::MapSelectionModeSetting, bResolvedRandomSelection ? HeistOnlineSession::RandomMapSelectionMode : HeistOnlineSession::FixedMapSelectionMode,
						EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	PendingSelectedMapId = ResolvedMapId;
	bPendingRandomMapSelection = bResolvedRandomSelection;
	bMapSelectionUpdatePending = true;
	LastOnlineSessionFailure = NAME_None;
	BeginOnlineSessionOperation(HeistOnlineSession::OperationMapUpdate, OnlineSessionOperationTimeoutSeconds);
	UpdateSessionDelegateHandle =
		OnlineSessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(FOnUpdateSessionCompleteDelegate::CreateUObject(this, &UHeistGameInstance::HandleUpdateSessionComplete));
	OnlineSessionStateChangedDelegate.Broadcast();

	const bool bUpdateRequested = OnlineSessionInterface->UpdateSession(HeistOnlineSession::SessionName, UpdatedSettings, true);
	if (!bUpdateRequested)
	{
		CompleteOnlineSessionOperation();
		OnlineSessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionDelegateHandle);
		UpdateSessionDelegateHandle.Reset();
		bMapSelectionUpdatePending = false;
		PendingSelectedMapId = NAME_None;
		bPendingRandomMapSelection = false;
		SetOnlineSessionState(HeistOnlineSession::StateHosting, FName(TEXT("MapSelectionUpdateRejected")));
	}
	UHeistDebugFunctionLibrary::DebugOnlineSessionMapSelection(this, RequestedMapId, ResolvedMapId, bResolvedRandomSelection, bUpdateRequested, bUpdateRequested,
															   bUpdateRequested ? NAME_None : LastOnlineSessionFailure);
	return bUpdateRequested;
}

void UHeistGameInstance::HandleHostSessionEnded(const FName Reason)
{
	const FName ResolvedReason = Reason.IsNone() ? FName(TEXT("HostQuit")) : Reason;
	if (!IsJoinedOnlineSession())
	{
		ResetOnlineSessionRuntimeState();
		ReturnToTitleMenu(ResolvedReason);
		UHeistDebugFunctionLibrary::DebugOnlineSessionRemoteEnded(this, ResolvedReason, false);
		return;
	}

	const bool bLeaveStarted = BeginDestroySession(ResolvedReason, false, ResolvedReason);
	UHeistDebugFunctionLibrary::DebugOnlineSessionRemoteEnded(this, ResolvedReason, bLeaveStarted);
}

void UHeistGameInstance::NotifySessionWorldReady()
{
	const bool bWasTravelPending = bSessionTravelPending;
	bSessionTravelPending = false;
	PendingTravelDestination = NAME_None;
	if (ActiveOnlineSessionOperation == HeistOnlineSession::OperationTravelLobby || ActiveOnlineSessionOperation == HeistOnlineSession::OperationTravelGameplay ||
		ActiveOnlineSessionOperation == HeistOnlineSession::OperationTravelJoin)
	{
		CompleteOnlineSessionOperation();
	}
	RefreshOnlineSessionInterface();
	if (bWasTravelPending)
	{
		OnlineSessionStateChangedDelegate.Broadcast();
	}
}

void UHeistGameInstance::SynchronizeSessionMapSelection(const AHeistGameState* SourceGameState, const FName NewSelectedMapId, const bool bNewRandomSelection)
{
	const UWorld* SourceWorld = IsValid(SourceGameState) ? SourceGameState->GetWorld() : nullptr;
	const bool bValidMapId = NewSelectedMapId == FName(TEXT("M01")) || NewSelectedMapId == FName(TEXT("M02")) || NewSelectedMapId == FName(TEXT("M03"));
	if (!IsValid(SourceWorld) || SourceWorld->GetGameInstance() != this || !bValidMapId)
	{
		return;
	}

	SelectedMapId = NewSelectedMapId;
	bRandomMapSelection = bNewRandomSelection;
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
	return !ActiveOnlineSessionOperation.IsNone() || OnlineSessionState == HeistOnlineSession::StateCreating || OnlineSessionState == HeistOnlineSession::StateSearching ||
		   OnlineSessionState == HeistOnlineSession::StateJoining || OnlineSessionState == HeistOnlineSession::StateLeaving || bMapSelectionUpdatePending || bSessionTravelPending;
}

bool UHeistGameInstance::CanCancelOnlineSessionOperation() const
{
	if (bOnlineSessionCancellationPending)
	{
		return false;
	}

	return ActiveOnlineSessionOperation == HeistOnlineSession::OperationCreate || ActiveOnlineSessionOperation == HeistOnlineSession::OperationFind ||
		   ActiveOnlineSessionOperation == HeistOnlineSession::OperationJoin;
}

bool UHeistGameInstance::CanRetryLastOnlineSessionOperation() const
{
	if (IsOnlineSessionOperationPending() || LastOnlineSessionFailure.IsNone() || LastRetryRequest.IsNone())
	{
		return false;
	}

	if (LastRetryRequest == HeistOnlineSession::RetryHost || LastRetryRequest == HeistOnlineSession::RetryJoin)
	{
		return IsCurrentWorldTitleMenu() && !IsHostingOnlineSession() && !IsJoinedOnlineSession() && !HasActiveNamedOnlineSession();
	}
	if (LastRetryRequest == HeistOnlineSession::RetryTravelLobby)
	{
		return IsHostingOnlineSession() && !LobbyMapPath.IsEmpty();
	}
	if (LastRetryRequest == HeistOnlineSession::RetryTravelGameplay)
	{
		return IsHostingOnlineSession() && IsCurrentWorldLobby();
	}
	return false;
}

bool UHeistGameInstance::IsOnlineSessionCancellationPending() const
{
	return bOnlineSessionCancellationPending;
}

bool UHeistGameInstance::IsSessionTravelPending() const
{
	return bSessionTravelPending;
}

FName UHeistGameInstance::GetActiveOnlineSessionOperation() const
{
	return ActiveOnlineSessionOperation;
}

FName UHeistGameInstance::GetPendingTravelDestination() const
{
	return PendingTravelDestination;
}

FName UHeistGameInstance::GetLastRetryRequest() const
{
	return LastRetryRequest;
}

float UHeistGameInstance::GetOnlineSessionOperationTimeoutRemaining() const
{
	if (OnlineSessionOperationDeadlineSeconds <= 0.0)
	{
		return 0.0f;
	}

	return static_cast<float>(FMath::Max(0.0, OnlineSessionOperationDeadlineSeconds - FPlatformTime::Seconds()));
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
	return FMath::Clamp(MaxPublicConnections, HeistSessionContract::MinimumPublicPlayerCount, HeistSessionContract::MaximumPublicPlayerCount);
}

FName UHeistGameInstance::GetSelectedMapId() const
{
	return SelectedMapId;
}

bool UHeistGameInstance::IsRandomMapSelection() const
{
	return bRandomMapSelection;
}

bool UHeistGameInstance::IsMapSelectionUpdatePending() const
{
	return bMapSelectionUpdatePending;
}

bool UHeistGameInstance::HasActiveNamedOnlineSession() const
{
	return OnlineSessionInterface.IsValid() && OnlineSessionInterface->GetNamedSession(HeistOnlineSession::SessionName) != nullptr;
}

FString UHeistGameInstance::GetLobbyMapPath() const
{
	return LobbyMapPath;
}

FString UHeistGameInstance::GetTitleMenuMapPath() const
{
	return TitleMenuMapPath;
}

FString UHeistGameInstance::GetSelectedGameplayMapPath() const
{
	if (SelectedMapId == FName(TEXT("M01")))
	{
		return M01GameplayMapPath;
	}
	if (SelectedMapId == FName(TEXT("M02")))
	{
		return M02GameplayMapPath;
	}
	if (SelectedMapId == FName(TEXT("M03")))
	{
		return M03GameplayMapPath;
	}
	return FString();
}

bool UHeistGameInstance::IsCurrentWorldTitleMenu() const
{
	return IsCurrentWorldMap(TitleMenuMapPath, TEXT("HeistTitleMenu"));
}

bool UHeistGameInstance::IsCurrentWorldLobby() const
{
	return IsCurrentWorldMap(LobbyMapPath, TEXT("HeistLobby"));
}

bool UHeistGameInstance::IsCurrentWorldSelectedGameplayMap() const
{
	const FString SelectedGameplayMapPath = GetSelectedGameplayMapPath();
	return !SelectedGameplayMapPath.IsEmpty() && IsCurrentWorldMap(SelectedGameplayMapPath, nullptr);
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
	SessionSettings.Set(HeistOnlineSession::MapSelectionModeSetting, bRandomMapSelection ? HeistOnlineSession::RandomMapSelectionMode : HeistOnlineSession::FixedMapSelectionMode,
						EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
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

bool UHeistGameInstance::BeginDestroySession(const FName LeaveReason, const bool bWasHosting, const FName PreservedFailure)
{
	if (!OnlineSessionInterface.IsValid() && !RefreshOnlineSessionInterface())
	{
		FailOnlineSessionRequest(FName(TEXT("OnlineSessionUnavailable")));
		return false;
	}

	bLeaveWasHosting = bWasHosting;
	PendingLeaveReason = LeaveReason;
	PendingFailureAfterDestroy = PreservedFailure;
	BeginOnlineSessionOperation(HeistOnlineSession::OperationLeave, OnlineSessionOperationTimeoutSeconds);
	SetOnlineSessionState(HeistOnlineSession::StateLeaving);

	if (OnlineSessionInterface->GetNamedSession(HeistOnlineSession::SessionName) == nullptr)
	{
		HandleDestroySessionComplete(HeistOnlineSession::SessionName, true);
		return true;
	}

	DestroySessionDelegateHandle =
		OnlineSessionInterface->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &UHeistGameInstance::HandleDestroySessionComplete));
	if (OnlineSessionInterface->DestroySession(HeistOnlineSession::SessionName))
	{
		return true;
	}

	OnlineSessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionDelegateHandle);
	DestroySessionDelegateHandle.Reset();
	CompleteOnlineSessionOperation();
	SetOnlineSessionState(bLeaveWasHosting ? HeistOnlineSession::StateHosting : HeistOnlineSession::StateJoined, FName(TEXT("LeaveRequestRejected")));
	PendingLeaveReason = NAME_None;
	PendingFailureAfterDestroy = NAME_None;
	bLeaveWasHosting = false;
	return false;
}

void UHeistGameInstance::HandleHostLeaveGracePeriodElapsed()
{
	HostLeaveGraceTimerHandle.Invalidate();
	if (OnlineSessionState != HeistOnlineSession::StateLeaving || !bLeaveWasHosting || PendingLeaveReason.IsNone())
	{
		return;
	}

	const FName LeaveReason = PendingLeaveReason;
	if (!BeginDestroySession(LeaveReason, true))
	{
		UHeistDebugFunctionLibrary::DebugOnlineSessionLeaveRequest(this, true, OnlineSessionState, false, LastOnlineSessionFailure);
	}
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
		if (IsCurrentWorldLobby())
		{
			if (AHeistGameState* HeistGameState = World->GetGameState<AHeistGameState>())
			{
				HeistGameState->SetMatchPhase(EHeistMatchPhase::Lobby);
				HeistGameState->InitializeSessionMapSelection(SelectedMapId, bRandomMapSelection);
			}
			return true;
		}

		LastRetryRequest = HeistOnlineSession::RetryTravelLobby;
		return BeginHostServerTravel(LobbyMapPath, TEXT("HeistLobby"));
	}

	if (LobbyMapPath.IsEmpty())
	{
		return false;
	}

	LastRetryRequest = HeistOnlineSession::RetryTravelLobby;
	bSessionTravelPending = true;
	PendingTravelDestination = HeistOnlineSession::DestinationLobby;
	BeginOnlineSessionOperation(HeistOnlineSession::OperationTravelLobby, OnlineSessionTravelTimeoutSeconds);
	OnlineSessionStateChangedDelegate.Broadcast();
	const FString TravelOptions = ActiveOnlineSubsystemName == NULL_SUBSYSTEM ? TEXT("listen?HeistLobby=1?bIsLanMatch=1") : TEXT("listen?HeistLobby=1");
	UGameplayStatics::OpenLevel(this, FName(*LobbyMapPath), true, TravelOptions);
	return true;
}

bool UHeistGameInstance::TravelHostToSelectedGameplayMap()
{
	return BeginHostServerTravel(GetSelectedGameplayMapPath(), TEXT("HeistGameplay"));
}

bool UHeistGameInstance::BeginHostServerTravel(const FString& MapPath, const TCHAR* TravelOption)
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || World->GetNetMode() != NM_ListenServer || MapPath.IsEmpty() || !FPackageName::IsValidLongPackageName(MapPath) || !FPackageName::DoesPackageExist(MapPath))
	{
		return false;
	}

	FString TravelURL = FString::Printf(TEXT("%s?%s=1"), *MapPath, TravelOption != nullptr ? TravelOption : TEXT("HeistTravel"));
	if (ActiveOnlineSubsystemName == NULL_SUBSYSTEM)
	{
		TravelURL += TEXT("?bIsLanMatch=1");
	}

	bSessionTravelPending = true;
	const bool bTravellingToLobby = TravelOption != nullptr && FCString::Stricmp(TravelOption, TEXT("HeistLobby")) == 0;
	PendingTravelDestination = bTravellingToLobby ? HeistOnlineSession::DestinationLobby : HeistOnlineSession::DestinationGameplay;
	BeginOnlineSessionOperation(bTravellingToLobby ? HeistOnlineSession::OperationTravelLobby : HeistOnlineSession::OperationTravelGameplay, OnlineSessionTravelTimeoutSeconds);
	OnlineSessionStateChangedDelegate.Broadcast();
	if (World->ServerTravel(TravelURL, true))
	{
		return true;
	}

	bSessionTravelPending = false;
	PendingTravelDestination = NAME_None;
	CompleteOnlineSessionOperation();
	return false;
}

bool UHeistGameInstance::ReturnToTitleMenu(const FName)
{
	if (TitleMenuMapPath.IsEmpty())
	{
		return false;
	}

	UGameplayStatics::OpenLevel(this, FName(*TitleMenuMapPath), true, TEXT("HeistTitleMenu=1"));
	return true;
}

bool UHeistGameInstance::IsCurrentWorldMap(const FString& MapPath, const TCHAR* TravelOption) const
{
	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	if (TravelOption != nullptr && World->URL.HasOption(TravelOption))
	{
		return true;
	}

	const FString ExpectedMapName = FPackageName::GetShortName(MapPath);
	return !ExpectedMapName.IsEmpty() && UGameplayStatics::GetCurrentLevelName(this, true).Equals(ExpectedMapName, ESearchCase::IgnoreCase);
}

bool UHeistGameInstance::ResolveRequestedMapSelection(const FName RequestedMapId, FName& OutSelectedMapId, bool& bOutRandomSelection)
{
	FString NormalizedMapId = RequestedMapId.ToString();
	NormalizedMapId.TrimStartAndEndInline();
	NormalizedMapId.ToUpperInline();
	const FName NormalizedName(*NormalizedMapId);
	bOutRandomSelection = NormalizedName == HeistOnlineSession::RandomMapSelection;
	if (bOutRandomSelection)
	{
		OutSelectedMapId = DrawRandomMapSelection();
		return true;
	}

	if (NormalizedName == FName(TEXT("M01")) || NormalizedName == FName(TEXT("M02")) || NormalizedName == FName(TEXT("M03")))
	{
		OutSelectedMapId = NormalizedName;
		return true;
	}

	OutSelectedMapId = NAME_None;
	return false;
}

FName UHeistGameInstance::DrawRandomMapSelection()
{
	if (RemainingRandomMapIds.IsEmpty())
	{
		RemainingRandomMapIds = {FName(TEXT("M01")), FName(TEXT("M02")), FName(TEXT("M03"))};
	}
	const int32 Index = RandomMapSelectionStream.RandRange(0, RemainingRandomMapIds.Num() - 1);
	const FName Selected = RemainingRandomMapIds[Index];
	RemainingRandomMapIds.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	return Selected;
}

bool UHeistGameInstance::RunRandomMapShuffleBagSelfTestForDebug(int32& OutDrawCount, int32& OutFirstCycleUniqueCount, int32& OutSecondCycleUniqueCount) const
{
	TArray<FName> Remaining;
	FRandomStream Stream(701103);
	TSet<FName> FirstCycle;
	TSet<FName> SecondCycle;
	for (int32 DrawIndex = 0; DrawIndex < 6; ++DrawIndex)
	{
		if (Remaining.IsEmpty())
		{
			Remaining = {FName(TEXT("M01")), FName(TEXT("M02")), FName(TEXT("M03"))};
		}
		const int32 Index = Stream.RandRange(0, Remaining.Num() - 1);
		const FName Selected = Remaining[Index];
		Remaining.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		(DrawIndex < 3 ? FirstCycle : SecondCycle).Add(Selected);
	}
	OutDrawCount = 6;
	OutFirstCycleUniqueCount = FirstCycle.Num();
	OutSecondCycleUniqueCount = SecondCycle.Num();
	return FirstCycle.Num() == 3 && SecondCycle.Num() == 3;
}

bool UHeistGameInstance::CommitLobbyMapSelection(const FName NewSelectedMapId, const bool bNewRandomSelection)
{
	AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistGameState) || !HeistGameState->SetLobbyMapSelection(NewSelectedMapId, bNewRandomSelection))
	{
		return false;
	}

	SelectedMapId = NewSelectedMapId;
	bRandomMapSelection = bNewRandomSelection;
	return true;
}

void UHeistGameInstance::NotifyRemoteClientsSessionEnded(const FName Reason) const
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	for (FConstPlayerControllerIterator PlayerControllerIterator = World->GetPlayerControllerIterator(); PlayerControllerIterator; ++PlayerControllerIterator)
	{
		AHeistPlayerController* HeistPlayerController = Cast<AHeistPlayerController>(PlayerControllerIterator->Get());
		if (IsValid(HeistPlayerController) && !HeistPlayerController->IsLocalController())
		{
			HeistPlayerController->Client_NotifyOnlineSessionEnded(Reason);
		}
	}
}

void UHeistGameInstance::ClearPendingContractStartPlayerCount()
{
	PendingContractStartPlayerCount = 0;
}

void UHeistGameInstance::ResetOnlineSessionRuntimeState(const FName PreservedFailure)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HostLeaveGraceTimerHandle);
	}
	HostLeaveGraceTimerHandle.Invalidate();
	CompleteOnlineSessionOperation();
	ActiveSessionSearch.Reset();
	ActiveJoinCode.Reset();
	PendingJoinCode.Reset();
	PendingSelectedMapId = NAME_None;
	bPendingRandomMapSelection = false;
	SelectedMapId = DefaultSelectedMapId;
	bRandomMapSelection = false;
	bMapSelectionUpdatePending = false;
	bSessionTravelPending = false;
	PendingTravelDestination = NAME_None;
	bOnlineSessionCancellationPending = false;
	ClearPendingContractStartPlayerCount();
	PendingOperationAbortFailure = NAME_None;
	bLeaveWasHosting = false;
	PendingLeaveReason = NAME_None;
	PendingFailureAfterDestroy = NAME_None;
	ResetSurfaceTemplateShuffleState();
	RemainingRandomMapIds.Reset();
	RandomMapSelectionStream.GenerateNewSeed();
	SetOnlineSessionState(HeistOnlineSession::StateIdle, PreservedFailure);
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
	if (UpdateSessionDelegateHandle.IsValid())
	{
		OnlineSessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionDelegateHandle);
		UpdateSessionDelegateHandle.Reset();
	}
	if (DestroySessionDelegateHandle.IsValid())
	{
		OnlineSessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionDelegateHandle);
		DestroySessionDelegateHandle.Reset();
	}
}

void UHeistGameInstance::BeginOnlineSessionOperation(const FName OperationName, const float TimeoutSeconds)
{
	ClearOnlineSessionOperationTimeout();
	ActiveOnlineSessionOperation = OperationName;
	const float SafeTimeoutSeconds = FMath::Clamp(TimeoutSeconds, 5.0f, 120.0f);
	OnlineSessionOperationDeadlineSeconds = FPlatformTime::Seconds() + SafeTimeoutSeconds;
	OnlineSessionOperationTimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UHeistGameInstance::HandleOnlineSessionOperationTimeout), SafeTimeoutSeconds);
}

void UHeistGameInstance::CompleteOnlineSessionOperation()
{
	ClearOnlineSessionOperationTimeout();
	ActiveOnlineSessionOperation = NAME_None;
}

void UHeistGameInstance::ClearOnlineSessionOperationTimeout()
{
	if (OnlineSessionOperationTimeoutHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(OnlineSessionOperationTimeoutHandle);
	}
	OnlineSessionOperationTimeoutHandle.Reset();
	OnlineSessionOperationDeadlineSeconds = 0.0;
}

bool UHeistGameInstance::HandleOnlineSessionOperationTimeout(const float)
{
	OnlineSessionOperationTimeoutHandle.Reset();
	OnlineSessionOperationDeadlineSeconds = 0.0;
	const FName TimedOutOperation = ActiveOnlineSessionOperation;
	if (TimedOutOperation.IsNone())
	{
		return false;
	}

	FName TimeoutFailure(TEXT("OperationTimedOut"));
	if (TimedOutOperation == HeistOnlineSession::OperationCreate)
	{
		TimeoutFailure = FName(TEXT("CreateTimedOut"));
		bOnlineSessionCancellationPending = true;
		PendingOperationAbortFailure = TimeoutFailure;
		SetOnlineSessionState(HeistOnlineSession::StateFailed, TimeoutFailure);
	}
	else if (TimedOutOperation == HeistOnlineSession::OperationFind)
	{
		TimeoutFailure = FName(TEXT("FindTimedOut"));
		if (OnlineSessionInterface.IsValid() && FindSessionsDelegateHandle.IsValid())
		{
			OnlineSessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsDelegateHandle);
			FindSessionsDelegateHandle.Reset();
			OnlineSessionInterface->CancelFindSessions();
		}
		CompleteOnlineSessionOperation();
		ActiveSessionSearch.Reset();
		PendingSelectedMapId = NAME_None;
		bPendingRandomMapSelection = false;
		FailOnlineSessionRequest(TimeoutFailure);
	}
	else if (TimedOutOperation == HeistOnlineSession::OperationJoin)
	{
		TimeoutFailure = FName(TEXT("JoinTimedOut"));
		bOnlineSessionCancellationPending = true;
		PendingOperationAbortFailure = TimeoutFailure;
		SetOnlineSessionState(HeistOnlineSession::StateFailed, TimeoutFailure);
	}
	else if (TimedOutOperation == HeistOnlineSession::OperationTravelLobby || TimedOutOperation == HeistOnlineSession::OperationTravelGameplay ||
			 TimedOutOperation == HeistOnlineSession::OperationTravelJoin)
	{
		TimeoutFailure = FName(TEXT("TravelTimedOut"));
		if (TimedOutOperation == HeistOnlineSession::OperationTravelGameplay)
		{
			ClearPendingContractStartPlayerCount();
		}
		const bool bWasJoinedClient = IsJoinedOnlineSession() || TimedOutOperation == HeistOnlineSession::OperationTravelJoin;
		bSessionTravelPending = false;
		PendingTravelDestination = NAME_None;
		CompleteOnlineSessionOperation();
		if (bWasJoinedClient)
		{
			if (!BeginDestroySession(FName(TEXT("TravelTimeoutCleanup")), false, TimeoutFailure))
			{
				ResetOnlineSessionRuntimeState(TimeoutFailure);
				ReturnToTitleMenu(TimeoutFailure);
			}
		}
		else
		{
			SetOnlineSessionState(IsHostingOnlineSession() ? HeistOnlineSession::StateHosting : HeistOnlineSession::StateFailed, TimeoutFailure);
		}
	}
	else if (TimedOutOperation == HeistOnlineSession::OperationLeave)
	{
		TimeoutFailure = FName(TEXT("LeaveTimedOut"));
		PendingFailureAfterDestroy = TimeoutFailure;
		SetOnlineSessionState(HeistOnlineSession::StateLeaving, TimeoutFailure);
	}
	else if (TimedOutOperation == HeistOnlineSession::OperationMapUpdate)
	{
		TimeoutFailure = FName(TEXT("MapUpdateTimedOut"));
		SetOnlineSessionState(HeistOnlineSession::StateHosting, TimeoutFailure);
	}

	UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("Timeout"), ActiveOnlineSubsystemName, PendingJoinCode, OnlineSessionState, true, *TimedOutOperation.ToString());
	return false;
}

void UHeistGameInstance::HandleEngineNetworkFailure(UWorld* World, UNetDriver*, const ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	if (IsValid(World) && World->GetGameInstance() != this)
	{
		return;
	}

	const bool bClientFailure = (IsValid(World) && World->GetNetMode() == NM_Client) || IsJoinedOnlineSession();
	const FName FailureReason = ErrorString.Contains(TEXT("MatchAlreadyStarted"), ESearchCase::IgnoreCase)
									? FName(TEXT("MatchAlreadyStarted"))
									: (FailureType == ENetworkFailure::ConnectionLost ? FName(TEXT("ConnectionLost")) : FName(TEXT("NetworkFailure")));
	UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("NetworkFailure"), ActiveOnlineSubsystemName, ActiveJoinCode, OnlineSessionState, false, *FailureReason.ToString());

	if (ActiveOnlineSessionOperation == HeistOnlineSession::OperationLeave)
	{
		PendingFailureAfterDestroy = FailureReason;
		SetOnlineSessionState(HeistOnlineSession::StateLeaving, FailureReason);
		return;
	}

	if (bClientFailure)
	{
		bSessionTravelPending = false;
		PendingTravelDestination = NAME_None;
		CompleteOnlineSessionOperation();
		if (!BeginDestroySession(FailureReason, false, FailureReason))
		{
			ResetOnlineSessionRuntimeState(FailureReason);
			ReturnToTitleMenu(FailureReason);
		}
		return;
	}

	if (bSessionTravelPending)
	{
		ClearPendingContractStartPlayerCount();
		bSessionTravelPending = false;
		PendingTravelDestination = NAME_None;
		CompleteOnlineSessionOperation();
		SetOnlineSessionState(IsHostingOnlineSession() ? HeistOnlineSession::StateHosting : HeistOnlineSession::StateFailed, FailureReason);
	}
}

void UHeistGameInstance::HandleEngineTravelFailure(UWorld* World, const ETravelFailure::Type, const FString&)
{
	if (IsValid(World) && World->GetGameInstance() != this)
	{
		return;
	}

	const FName FailureReason(TEXT("TravelFailure"));
	ClearPendingContractStartPlayerCount();
	const bool bJoinedClient = (IsValid(World) && World->GetNetMode() == NM_Client) || IsJoinedOnlineSession();
	if (ActiveOnlineSessionOperation == HeistOnlineSession::OperationLeave)
	{
		PendingFailureAfterDestroy = FailureReason;
		SetOnlineSessionState(HeistOnlineSession::StateLeaving, FailureReason);
		return;
	}

	bSessionTravelPending = false;
	PendingTravelDestination = NAME_None;
	CompleteOnlineSessionOperation();
	UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("TravelFailure"), ActiveOnlineSubsystemName, ActiveJoinCode, OnlineSessionState, false, *FailureReason.ToString());

	if (bJoinedClient)
	{
		if (!BeginDestroySession(FailureReason, false, FailureReason))
		{
			ResetOnlineSessionRuntimeState(FailureReason);
			ReturnToTitleMenu(FailureReason);
		}
		return;
	}

	SetOnlineSessionState(IsHostingOnlineSession() ? HeistOnlineSession::StateHosting : HeistOnlineSession::StateFailed, FailureReason);
}

void UHeistGameInstance::HandleAbortedCreateSessionComplete(const FName SessionName, const bool bWasSuccessful, const FName AbortFailure)
{
	const FName ResolvedAbortFailure = AbortFailure.IsNone() ? FName(TEXT("OperationCancelled")) : AbortFailure;
	const bool bCreatedExpectedSession = bWasSuccessful && SessionName == HeistOnlineSession::SessionName;
	UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("CreateAbortComplete"), ActiveOnlineSubsystemName, ActiveJoinCode, OnlineSessionState, true, *ResolvedAbortFailure.ToString());
	if (bCreatedExpectedSession && BeginDestroySession(FName(TEXT("CreateAbortCleanup")), true, ResolvedAbortFailure))
	{
		return;
	}

	ResetOnlineSessionRuntimeState(ResolvedAbortFailure);
	ReturnToTitleMenu(ResolvedAbortFailure);
}

void UHeistGameInstance::HandleAbortedJoinSessionComplete(const FName SessionName, const EOnJoinSessionCompleteResult::Type JoinResult, const FName AbortFailure)
{
	const FName ResolvedAbortFailure = AbortFailure.IsNone() ? FName(TEXT("OperationCancelled")) : AbortFailure;
	const bool bJoinedExpectedSession = JoinResult == EOnJoinSessionCompleteResult::Success && SessionName == HeistOnlineSession::SessionName;
	UHeistDebugFunctionLibrary::DebugOnlineSessionRequest(this, TEXT("JoinAbortComplete"), ActiveOnlineSubsystemName, PendingJoinCode, OnlineSessionState, true, *ResolvedAbortFailure.ToString());
	ActiveSessionSearch.Reset();
	if ((bJoinedExpectedSession || HasActiveNamedOnlineSession()) && BeginDestroySession(FName(TEXT("JoinAbortCleanup")), false, ResolvedAbortFailure))
	{
		return;
	}

	ResetOnlineSessionRuntimeState(ResolvedAbortFailure);
	ReturnToTitleMenu(ResolvedAbortFailure);
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

bool UHeistGameInstance::ForceOnlineSessionFailureForDebug(const FName FailureReason)
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const bool bSupportedFailure = FailureReason == FName(TEXT("CreateTimedOut")) || FailureReason == FName(TEXT("FindTimedOut")) || FailureReason == FName(TEXT("JoinTimedOut")) ||
								   FailureReason == FName(TEXT("TravelTimedOut")) || FailureReason == FName(TEXT("OperationCancelled")) || FailureReason == FName(TEXT("NetworkFailure")) ||
								   FailureReason == FName(TEXT("TravelFailure"));
	if (!bSupportedFailure)
	{
		return false;
	}

	if (!ActiveOnlineSessionOperation.IsNone() && FailureReason.ToString().EndsWith(TEXT("TimedOut")))
	{
		HandleOnlineSessionOperationTimeout(0.0f);
		return true;
	}

	if (FailureReason == FName(TEXT("FindTimedOut")) && IsCurrentWorldTitleMenu() && !HasActiveNamedOnlineSession())
	{
		LastRetryRequest = HeistOnlineSession::RetryJoin;
		if (LastRetryJoinCode.IsEmpty())
		{
			LastRetryJoinCode = TEXT("AAAAAA");
		}
		BeginOnlineSessionOperation(HeistOnlineSession::OperationFind, OnlineSessionOperationTimeoutSeconds);
		SetOnlineSessionState(HeistOnlineSession::StateSearching);
		HandleOnlineSessionOperationTimeout(0.0f);
		return true;
	}
	if (FailureReason == FName(TEXT("TravelTimedOut")) && IsHostingOnlineSession() && IsCurrentWorldLobby())
	{
		LastRetryRequest = HeistOnlineSession::RetryTravelGameplay;
		bSessionTravelPending = true;
		PendingTravelDestination = HeistOnlineSession::DestinationGameplay;
		BeginOnlineSessionOperation(HeistOnlineSession::OperationTravelGameplay, OnlineSessionTravelTimeoutSeconds);
		OnlineSessionStateChangedDelegate.Broadcast();
		HandleOnlineSessionOperationTimeout(0.0f);
		return true;
	}
	if (FailureReason == FName(TEXT("CreateTimedOut")) && IsCurrentWorldTitleMenu())
	{
		LastRetryRequest = HeistOnlineSession::RetryHost;
		LastRetryJoinCode.Reset();
	}
	else if ((FailureReason == FName(TEXT("FindTimedOut")) || FailureReason == FName(TEXT("JoinTimedOut"))) && IsCurrentWorldTitleMenu())
	{
		LastRetryRequest = HeistOnlineSession::RetryJoin;
		if (LastRetryJoinCode.IsEmpty())
		{
			LastRetryJoinCode = TEXT("AAAAAA");
		}
	}
	const FName FailureState = IsHostingOnlineSession() ? HeistOnlineSession::StateHosting : (IsJoinedOnlineSession() ? HeistOnlineSession::StateJoined : HeistOnlineSession::StateFailed);
	SetOnlineSessionState(FailureState, FailureReason);
	return true;
#endif
}

bool UHeistGameInstance::RunOnlineSessionCancelTestForDebug()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	if (!IsCurrentWorldTitleMenu() || IsOnlineSessionOperationPending() || IsHostingOnlineSession() || IsJoinedOnlineSession() || HasActiveNamedOnlineSession())
	{
		return false;
	}

	LastRetryRequest = HeistOnlineSession::RetryHost;
	LastRetryJoinCode.Reset();
	BeginOnlineSessionOperation(HeistOnlineSession::OperationFind, OnlineSessionOperationTimeoutSeconds);
	SetOnlineSessionState(HeistOnlineSession::StateSearching);
	return CanCancelOnlineSessionOperation();
#endif
}

void UHeistGameInstance::HandleCreateSessionComplete(const FName SessionName, const bool bWasSuccessful)
{
	if (OnlineSessionInterface.IsValid() && CreateSessionDelegateHandle.IsValid())
	{
		OnlineSessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionDelegateHandle);
		CreateSessionDelegateHandle.Reset();
	}

	const bool bOperationAborted = bOnlineSessionCancellationPending && ActiveOnlineSessionOperation == HeistOnlineSession::OperationCreate;
	const FName AbortFailure = PendingOperationAbortFailure;
	CompleteOnlineSessionOperation();
	if (bOperationAborted)
	{
		bOnlineSessionCancellationPending = false;
		PendingOperationAbortFailure = NAME_None;
		HandleAbortedCreateSessionComplete(SessionName, bWasSuccessful, AbortFailure);
		return;
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
	UHeistDebugFunctionLibrary::DebugOnlineSessionCreateComplete(this, SessionName, ActiveOnlineSubsystemName, ActiveJoinCode, bWasSuccessful, bTravelAccepted, LastOnlineSessionFailure);
}

void UHeistGameInstance::HandleFindSessionsComplete(const bool bWasSuccessful)
{
	if (OnlineSessionInterface.IsValid() && FindSessionsDelegateHandle.IsValid())
	{
		OnlineSessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsDelegateHandle);
		FindSessionsDelegateHandle.Reset();
	}
	CompleteOnlineSessionOperation();

	const int32 ResultCount = ActiveSessionSearch.IsValid() ? ActiveSessionSearch->SearchResults.Num() : 0;
	if (!bWasSuccessful || !ActiveSessionSearch.IsValid())
	{
		ActiveSessionSearch.Reset();
		PendingSelectedMapId = NAME_None;
		bPendingRandomMapSelection = false;
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
		const FName FailureReason = MatchingCodeCount == 0 ? FName(TEXT("SessionNotFound"))
														   : (VersionMismatchCount == MatchingCodeCount ? FName(TEXT("VersionMismatch"))
																										: (FullMatchCount > 0 ? FName(TEXT("SessionFull")) : FName(TEXT("SessionNotJoinable"))));
		FailOnlineSessionRequest(FailureReason);
		UHeistDebugFunctionLibrary::DebugOnlineSessionFindComplete(this, PendingJoinCode, ResultCount, MatchingCodeCount, FullMatchCount, VersionMismatchCount, NAME_None, false,
																   LastOnlineSessionFailure);
		ActiveSessionSearch.Reset();
		PendingSelectedMapId = NAME_None;
		bPendingRandomMapSelection = false;
		return;
	}

	FString ResultMapId;
	FString ResultMapSelectionMode;
	BestResult->Session.SessionSettings.Get(HeistOnlineSession::MapIdSetting, ResultMapId);
	BestResult->Session.SessionSettings.Get(HeistOnlineSession::MapSelectionModeSetting, ResultMapSelectionMode);
	FName ResolvedJoinMapId = NAME_None;
	bool bIgnoredRandomSelection = false;
	if (ResolveRequestedMapSelection(FName(*ResultMapId), ResolvedJoinMapId, bIgnoredRandomSelection))
	{
		PendingSelectedMapId = ResolvedJoinMapId;
		bPendingRandomMapSelection = ResultMapSelectionMode.Equals(HeistOnlineSession::RandomMapSelectionMode, ESearchCase::IgnoreCase);
	}
	else
	{
		PendingSelectedMapId = FName(TEXT("M01"));
		bPendingRandomMapSelection = false;
	}

	const FName SelectedSessionId(*BestResult->GetSessionIdStr());
	BeginOnlineSessionOperation(HeistOnlineSession::OperationJoin, OnlineSessionOperationTimeoutSeconds);
	SetOnlineSessionState(HeistOnlineSession::StateJoining);
	JoinSessionDelegateHandle = OnlineSessionInterface->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &UHeistGameInstance::HandleJoinSessionComplete));
	const bool bJoinRequestAccepted = OnlineSessionInterface->JoinSession(0, HeistOnlineSession::SessionName, *BestResult);
	UHeistDebugFunctionLibrary::DebugOnlineSessionFindComplete(this, PendingJoinCode, ResultCount, MatchingCodeCount, FullMatchCount, VersionMismatchCount, SelectedSessionId, bJoinRequestAccepted,
															   bJoinRequestAccepted ? NAME_None : FName(TEXT("JoinRequestRejected")));
	if (!bJoinRequestAccepted)
	{
		CompleteOnlineSessionOperation();
		OnlineSessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionDelegateHandle);
		JoinSessionDelegateHandle.Reset();
		ActiveSessionSearch.Reset();
		PendingSelectedMapId = NAME_None;
		bPendingRandomMapSelection = false;
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

	const bool bOperationAborted = bOnlineSessionCancellationPending && ActiveOnlineSessionOperation == HeistOnlineSession::OperationJoin;
	const FName AbortFailure = PendingOperationAbortFailure;
	CompleteOnlineSessionOperation();
	if (bOperationAborted)
	{
		bOnlineSessionCancellationPending = false;
		PendingOperationAbortFailure = NAME_None;
		HandleAbortedJoinSessionComplete(SessionName, JoinResult, AbortFailure);
		return;
	}

	const bool bJoinSucceeded = JoinResult == EOnJoinSessionCompleteResult::Success && SessionName == HeistOnlineSession::SessionName;
	FString ConnectString;
	const bool bConnectStringResolved = bJoinSucceeded && OnlineSessionInterface.IsValid() && OnlineSessionInterface->GetResolvedConnectString(HeistOnlineSession::SessionName, ConnectString);
	APlayerController* LocalPlayerController = GetFirstLocalPlayerController();
	if (!bJoinSucceeded || !bConnectStringResolved || !IsValid(LocalPlayerController))
	{
		const FName FailureReason =
			!bJoinSucceeded ? ResolveJoinResultReason(JoinResult) : (!bConnectStringResolved ? FName(TEXT("ConnectStringNotResolved")) : FName(TEXT("MissingLocalPlayerController")));
		FailOnlineSessionRequest(FailureReason);
		UHeistDebugFunctionLibrary::DebugOnlineSessionJoinComplete(this, SessionName, PendingJoinCode, static_cast<int32>(JoinResult), bConnectStringResolved, false, LastOnlineSessionFailure);
		ActiveSessionSearch.Reset();
		PendingSelectedMapId = NAME_None;
		bPendingRandomMapSelection = false;
		return;
	}

	ActiveJoinCode = PendingJoinCode;
	PendingJoinCode.Reset();
	if (!PendingSelectedMapId.IsNone())
	{
		SelectedMapId = PendingSelectedMapId;
		bRandomMapSelection = bPendingRandomMapSelection;
	}
	PendingSelectedMapId = NAME_None;
	bPendingRandomMapSelection = false;
	SetOnlineSessionState(HeistOnlineSession::StateJoined);
	UHeistDebugFunctionLibrary::DebugOnlineSessionJoinComplete(this, SessionName, ActiveJoinCode, static_cast<int32>(JoinResult), true, true, NAME_None);
	ActiveSessionSearch.Reset();
	bSessionTravelPending = true;
	PendingTravelDestination = HeistOnlineSession::DestinationJoinedSession;
	BeginOnlineSessionOperation(HeistOnlineSession::OperationTravelJoin, OnlineSessionTravelTimeoutSeconds);
	OnlineSessionStateChangedDelegate.Broadcast();
	LocalPlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
}

void UHeistGameInstance::HandleUpdateSessionComplete(const FName SessionName, const bool bWasSuccessful)
{
	if (OnlineSessionInterface.IsValid() && UpdateSessionDelegateHandle.IsValid())
	{
		OnlineSessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionDelegateHandle);
		UpdateSessionDelegateHandle.Reset();
	}
	CompleteOnlineSessionOperation();

	const FName RequestedMapId = PendingSelectedMapId;
	const bool bRequestedRandomSelection = bPendingRandomMapSelection;
	bMapSelectionUpdatePending = false;
	PendingSelectedMapId = NAME_None;
	bPendingRandomMapSelection = false;

	const bool bCorrectSession = SessionName == HeistOnlineSession::SessionName;
	const bool bCommitted = bWasSuccessful && bCorrectSession && CommitLobbyMapSelection(RequestedMapId, bRequestedRandomSelection);
	SetOnlineSessionState(HeistOnlineSession::StateHosting, bCommitted ? NAME_None : FName(bWasSuccessful && bCorrectSession ? TEXT("MapSelectionCommitFailed") : TEXT("MapSelectionUpdateFailed")));
	UHeistDebugFunctionLibrary::DebugOnlineSessionMapSelection(this, bRequestedRandomSelection ? HeistOnlineSession::RandomMapSelection : RequestedMapId, RequestedMapId, bRequestedRandomSelection,
															   true, bCommitted, LastOnlineSessionFailure);
}

void UHeistGameInstance::HandleDestroySessionComplete(const FName SessionName, const bool bWasSuccessful)
{
	if (OnlineSessionInterface.IsValid() && DestroySessionDelegateHandle.IsValid())
	{
		OnlineSessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionDelegateHandle);
		DestroySessionDelegateHandle.Reset();
	}
	CompleteOnlineSessionOperation();

	const bool bCorrectSession = SessionName == HeistOnlineSession::SessionName;
	const bool bDestroySucceeded = bWasSuccessful && bCorrectSession;
	const bool bWasHost = bLeaveWasHosting;
	const FName LeaveReason = PendingLeaveReason;
	const FName RequestedPreservedFailure = PendingFailureAfterDestroy;
	const FName PreservedFailure = bDestroySucceeded ? RequestedPreservedFailure : FName(TEXT("DestroySessionFailed"));
	ResetOnlineSessionRuntimeState(PreservedFailure);
	const bool bReturnedToTitleMenu = ReturnToTitleMenu(PreservedFailure);
	if (!bDestroySucceeded)
	{
		SetOnlineSessionState(HeistOnlineSession::StateFailed, FName(TEXT("DestroySessionFailed")));
	}
	UHeistDebugFunctionLibrary::DebugOnlineSessionDestroyComplete(this, SessionName, bWasHost, bDestroySucceeded, bReturnedToTitleMenu, LeaveReason, LastOnlineSessionFailure);
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
