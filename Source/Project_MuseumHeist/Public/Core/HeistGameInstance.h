#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"

#include "HeistGameInstance.generated.h"

class AHeistGameState;
class UNetDriver;
class UWorld;

DECLARE_MULTICAST_DELEGATE(FHeistOnlineSessionStateChanged);

UCLASS(Config = Game)
class PROJECT_MUSEUMHEIST_API UHeistGameInstance : public UGameInstance
{
	GENERATED_BODY()

#pragma region Construction

  public:
	UHeistGameInstance();

#pragma endregion

#pragma region Lifecycle

  public:
	virtual void Init() override;
	virtual void Shutdown() override;

#pragma endregion

#pragma region SurfaceTemplateSelection

  public:
	bool SelectSurfaceTemplateForMatch(FName PoolId, const TArray<FName>& CandidateTemplateIds, FName& OutTemplateId, int32& OutSelectionRevision, int32& OutBagCycle,
									   int32& OutRemainingTemplateCount);
	bool RunSurfaceTemplateShuffleBagSelfTestForDebug(int32 PoolSize, int32& OutDrawCount, int32& OutFirstCycleUniqueCount, int32& OutSecondCycleUniqueCount,
													  int32& OutRecentProtectionCheckCount, int32& OutRecentProtectionPassCount) const;
	bool RunRandomMapShuffleBagSelfTestForDebug(int32& OutDrawCount, int32& OutFirstCycleUniqueCount, int32& OutSecondCycleUniqueCount) const;

  private:
	void ResetSurfaceTemplateShuffleState();

	TMap<FName, TArray<FName>> SurfaceTemplateCatalogByPool;
	TMap<FName, TArray<FName>> RemainingSurfaceTemplateIdsByPool;
	TMap<FName, TArray<FName>> RecentSurfaceTemplateIdsByPool;
	TMap<FName, int32> SurfaceTemplateBagCycleByPool;
	int32 SurfaceTemplateSelectionRevision = 0;
	FRandomStream SurfaceTemplateRandomStream;

#pragma endregion

#pragma region OnlineSession

  public:
	UFUNCTION(BlueprintCallable, Category = "Heist|Online")
	bool RequestHostSession();

	UFUNCTION(BlueprintCallable, Category = "Heist|Online")
	bool RequestJoinSessionByCode(const FString& JoinCode);

	UFUNCTION(BlueprintCallable, Category = "Heist|Online")
	bool RequestLeaveSession();

	UFUNCTION(BlueprintCallable, Category = "Heist|Online")
	bool RequestCancelOnlineSessionOperation();

	UFUNCTION(BlueprintCallable, Category = "Heist|Online")
	bool RequestRetryLastOnlineSessionOperation();

	bool RequestStartSelectedGameplayMap();
	bool RequestReturnToLobby();
	bool RequestSetLobbyMapSelection(FName RequestedMapId);
	int32 ConsumePendingContractStartPlayerCount();
	void HandleHostSessionEnded(FName Reason);
	void NotifySessionWorldReady();
	void SynchronizeSessionMapSelection(const AHeistGameState* SourceGameState, FName NewSelectedMapId, bool bNewRandomSelection);

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	FName GetOnlineSessionState() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	FName GetLastOnlineSessionFailure() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	FName GetActiveOnlineSubsystemName() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	FString GetActiveJoinCode() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	FString GetPendingJoinCode() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	bool IsOnlineSessionOperationPending() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	bool CanCancelOnlineSessionOperation() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	bool CanRetryLastOnlineSessionOperation() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	bool IsOnlineSessionCancellationPending() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	bool IsSessionTravelPending() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	FName GetActiveOnlineSessionOperation() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	FName GetPendingTravelDestination() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	FName GetLastRetryRequest() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	float GetOnlineSessionOperationTimeoutRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	bool IsHostingOnlineSession() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	bool IsJoinedOnlineSession() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	int32 GetSessionBuildUniqueId() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	int32 GetMaxPublicConnections() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	FName GetSelectedMapId() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	bool IsRandomMapSelection() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	bool IsMapSelectionUpdatePending() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	bool HasActiveNamedOnlineSession() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	FString GetLobbyMapPath() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	FString GetTitleMenuMapPath() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	FString GetSelectedGameplayMapPath() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	bool IsCurrentWorldTitleMenu() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	bool IsCurrentWorldLobby() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Online")
	bool IsCurrentWorldSelectedGameplayMap() const;

	FHeistOnlineSessionStateChanged& GetOnlineSessionStateChangedDelegate();

	bool ForceOnlineSessionFailureForDebug(FName FailureReason);
	bool RunOnlineSessionCancelTestForDebug();

  private:
	bool RefreshOnlineSessionInterface();
	bool BeginCreateSession();
	bool BeginDestroySession(FName LeaveReason, bool bWasHosting, FName PreservedFailure = NAME_None);
	bool TravelHostToLobby();
	bool TravelHostToSelectedGameplayMap();
	bool BeginHostServerTravel(const FString& MapPath, const TCHAR* TravelOption);
	bool ReturnToTitleMenu(FName ReturnReason);
	bool IsCurrentWorldMap(const FString& MapPath, const TCHAR* TravelOption) const;
	bool ResolveRequestedMapSelection(FName RequestedMapId, FName& OutSelectedMapId, bool& bOutRandomSelection);
	FName DrawRandomMapSelection();
	bool CommitLobbyMapSelection(FName NewSelectedMapId, bool bNewRandomSelection);
	void NotifyRemoteClientsSessionEnded(FName Reason) const;
	void HandleHostLeaveGracePeriodElapsed();
	void ClearPendingContractStartPlayerCount();
	void ResetOnlineSessionRuntimeState(FName PreservedFailure = NAME_None);
	void ClearOnlineDelegates();
	void BeginOnlineSessionOperation(FName OperationName, float TimeoutSeconds);
	void CompleteOnlineSessionOperation();
	void ClearOnlineSessionOperationTimeout();
	bool HandleOnlineSessionOperationTimeout(float DeltaTime);
	void HandleEngineNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleEngineTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
	void HandleAbortedCreateSessionComplete(FName SessionName, bool bWasSuccessful, FName AbortFailure);
	void HandleAbortedJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type JoinResult, FName AbortFailure);
	void SetOnlineSessionState(FName NewState, FName FailureReason = NAME_None);
	void FailOnlineSessionRequest(FName FailureReason);
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type JoinResult);
	void HandleUpdateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);

	static FString NormalizeJoinCode(const FString& JoinCode);
	static bool IsJoinCodeValid(const FString& JoinCode);
	static FString GenerateJoinCode();
	static FName ResolveJoinResultReason(EOnJoinSessionCompleteResult::Type JoinResult);

	UPROPERTY(Config)
	int32 MaxPublicConnections = 4;

	UPROPERTY(Config)
	int32 MaxSessionSearchResults = 100;

	UPROPERTY(Config)
	FName SelectedMapId = FName(TEXT("M01"));

	UPROPERTY(Config)
	FString TitleMenuMapPath = TEXT("/Game/Maps/TitleMenuMap");

	UPROPERTY(Config)
	FString LobbyMapPath = TEXT("/Game/Maps/LobbyMap");

	UPROPERTY(Config)
	FString M01GameplayMapPath = TEXT("/Game/Maps/M01_ClassicalPrototype");

	UPROPERTY(Config)
	FString M02GameplayMapPath = TEXT("/Game/Maps/M02_MoonlitPrototype");

	UPROPERTY(Config)
	FString M03GameplayMapPath = TEXT("/Game/Maps/M03_GlasshousePrototype");

	UPROPERTY(Config)
	float HostLeaveGraceSeconds = 1.0f;

	UPROPERTY(Config)
	float OnlineSessionOperationTimeoutSeconds = 20.0f;

	UPROPERTY(Config)
	float OnlineSessionTravelTimeoutSeconds = 30.0f;

	IOnlineSessionPtr OnlineSessionInterface;
	TSharedPtr<FOnlineSessionSearch> ActiveSessionSearch;
	FDelegateHandle CreateSessionDelegateHandle;
	FDelegateHandle FindSessionsDelegateHandle;
	FDelegateHandle JoinSessionDelegateHandle;
	FDelegateHandle UpdateSessionDelegateHandle;
	FDelegateHandle DestroySessionDelegateHandle;
	FName OnlineSessionState = FName(TEXT("Idle"));
	FName LastOnlineSessionFailure = NAME_None;
	FName ActiveOnlineSubsystemName = NAME_None;
	FName ActiveOnlineSessionOperation = NAME_None;
	FName LastRetryRequest = NAME_None;
	FName PendingTravelDestination = NAME_None;
	FName PendingFailureAfterDestroy = NAME_None;
	FName PendingOperationAbortFailure = NAME_None;
	FString ActiveJoinCode;
	FString PendingJoinCode;
	FString LastRetryJoinCode;
	FName PendingSelectedMapId = NAME_None;
	bool bPendingRandomMapSelection = false;
	bool bRandomMapSelection = false;
	bool bMapSelectionUpdatePending = false;
	bool bSessionTravelPending = false;
	bool bOnlineSessionCancellationPending = false;
	int32 PendingContractStartPlayerCount = 0;
	bool bLeaveWasHosting = false;
	FName PendingLeaveReason = NAME_None;
	FName DefaultSelectedMapId = FName(TEXT("M01"));
	TArray<FName> RemainingRandomMapIds;
	FRandomStream RandomMapSelectionStream;
	double OnlineSessionOperationDeadlineSeconds = 0.0;
	FTimerHandle HostLeaveGraceTimerHandle;
	FTSTicker::FDelegateHandle OnlineSessionOperationTimeoutHandle;
	FHeistOnlineSessionStateChanged OnlineSessionStateChangedDelegate;

#pragma endregion
};
