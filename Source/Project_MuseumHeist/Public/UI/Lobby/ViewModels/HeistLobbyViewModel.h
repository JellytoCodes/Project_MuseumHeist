#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"

#include "HeistLobbyViewModel.generated.h"

class AHeistGameState;
class AHeistPlayerController;
class AHeistPlayerState;
class UHeistGameInstance;

DECLARE_MULTICAST_DELEGATE(FHeistLobbySnapshotChanged);

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistLobbyPlayerCardData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Lobby")
	int32 PlayerSlot = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Lobby")
	bool bOccupied = false;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Lobby")
	bool bLocalPlayer = false;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Lobby")
	bool bReady = false;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Lobby")
	FText PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "Heist|Lobby")
	FString PlatformUserId;
};

UCLASS(BlueprintType)
class PROJECT_MUSEUMHEIST_API UHeistLobbyViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

#pragma region Construction

  public:
	UHeistLobbyViewModel(const FObjectInitializer& ObjectInitializer);

#pragma endregion

#pragma region Lifecycle

  protected:
	virtual void BeginDestroy() override;

#pragma endregion

#pragma region Setup

  public:
	void SetupViewModel(AHeistGameState* InGameState, AHeistPlayerState* InLocalPlayerState, UHeistGameInstance* InGameInstance,
		AHeistPlayerController* InPlayerController);
	void RefreshLobbyData();
	FHeistLobbySnapshotChanged& GetSnapshotChangedDelegate();

	UFUNCTION(BlueprintCallable, Category = "Heist|Lobby")
	void RequestLeaveSession();

	UFUNCTION(BlueprintCallable, Category = "Heist|Lobby")
	void RequestSelectMap(FName RequestedMapId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Lobby")
	void RequestToggleLocalReady();

	UFUNCTION(BlueprintCallable, Category = "Heist|Lobby")
	void RequestStartGame();

  private:
	void HandlePlayerConnectionsChanged(int32 NewConnectedPlayerCount);
	void HandlePlayerIdentityChanged(int32 PlayerId);
	void HandlePlayerReadyChanged(bool bReady);
	void HandleOnlineSessionStateChanged();
	void HandleLobbyMapSelectionChanged(FName SelectedMapId, bool bRandomSelection, int32 Revision);
	void RebindPlayerStateDelegates();
	void UnbindPlayerStateDelegates();
	void RefreshPlayerCards();

	UPROPERTY(Transient)
	TObjectPtr<AHeistGameState> GameState;

	UPROPERTY(Transient)
	TObjectPtr<AHeistPlayerState> LocalPlayerState;

	UPROPERTY(Transient)
	TObjectPtr<UHeistGameInstance> GameInstance;

	UPROPERTY(Transient)
	TObjectPtr<AHeistPlayerController> PlayerController;

	TArray<TWeakObjectPtr<AHeistPlayerState>> BoundPlayerStates;
	FHeistLobbySnapshotChanged SnapshotChangedDelegate;

#pragma endregion

#pragma region LobbyData

  public:
	int32 GetConnectedPlayerCount() const;
	int32 GetReadyPlayerCount() const;
	int32 GetLocalPlayerId() const;
	const FText& GetPlayerCountText() const;
	const FText& GetJoinCodeText() const;
	FName GetSelectedMapId() const;
	bool IsRandomMapSelection() const;

	UFUNCTION(BlueprintPure, Category = "Heist|Lobby")
	bool TryGetPlayerCardData(int32 PlayerSlot, FHeistLobbyPlayerCardData& OutPlayerCardData) const;

	bool CanRequestLeaveSession() const;
	bool CanSelectMap() const;
	bool CanToggleLocalReady() const;
	bool CanStartGame() const;

  private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	int32 ConnectedPlayerCount = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	int32 ReadyPlayerCount = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	int32 LocalPlayerId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	FText PlayerCountText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	FText JoinCodeText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	FName SelectedMapId = FName(TEXT("M01"));

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	bool bRandomMapSelection = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	bool bCanRequestLeaveSession = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	bool bCanSelectMap = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	bool bCanToggleLocalReady = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	bool bCanStartGame = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	TArray<FHeistLobbyPlayerCardData> PlayerCards;

#pragma endregion
};
