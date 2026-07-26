#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"

#include "HeistGameInstance.generated.h"

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

#pragma region OnlineSession

  public:
	UFUNCTION(BlueprintCallable, Category = "Heist|Online")
	bool RequestHostSession();

	UFUNCTION(BlueprintCallable, Category = "Heist|Online")
	bool RequestJoinSessionByCode(const FString& JoinCode);

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
	FString GetLobbyMapPath() const;

	FHeistOnlineSessionStateChanged& GetOnlineSessionStateChangedDelegate();

  private:
	bool RefreshOnlineSessionInterface();
	bool BeginCreateSession();
	bool TravelHostToLobby();
	void ClearOnlineDelegates();
	void SetOnlineSessionState(FName NewState, FName FailureReason = NAME_None);
	void FailOnlineSessionRequest(FName FailureReason);
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type JoinResult);

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
	FString LobbyMapPath = TEXT("/Game/Maps/SandBoxMap");

	IOnlineSessionPtr OnlineSessionInterface;
	TSharedPtr<FOnlineSessionSearch> ActiveSessionSearch;
	FDelegateHandle CreateSessionDelegateHandle;
	FDelegateHandle FindSessionsDelegateHandle;
	FDelegateHandle JoinSessionDelegateHandle;
	FName OnlineSessionState = FName(TEXT("Idle"));
	FName LastOnlineSessionFailure = NAME_None;
	FName ActiveOnlineSubsystemName = NAME_None;
	FString ActiveJoinCode;
	FString PendingJoinCode;
	FHeistOnlineSessionStateChanged OnlineSessionStateChangedDelegate;

#pragma endregion
};
