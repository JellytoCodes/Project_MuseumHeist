#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "MVVMViewModelBase.h"

#include "HeistLobbyViewModel.generated.h"

class AHeistGameState;
class AHeistPlayerController;
class AHeistPlayerState;
class UHeistGameInstance;

DECLARE_MULTICAST_DELEGATE(FHeistLobbySnapshotChanged);

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
	void RequestRetrySessionOperation();

  private:
	void HandlePlayerConnectionsChanged(int32 NewConnectedPlayerCount);
	void HandlePlayerIdentityChanged(int32 PlayerId);
	void HandleOnlineSessionStateChanged();
	void HandleLobbyMapSelectionChanged(FName SelectedMapId, bool bRandomSelection, int32 Revision);
	void RebindPlayerIdentityDelegates();
	void UnbindPlayerIdentityDelegates();

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
	int32 GetLocalPlayerId() const;
	const FText& GetPhaseText() const;
	const FText& GetPlayerCountText() const;
	const FText& GetLocalPlayerText() const;
	const FText& GetReadyCountdownText() const;
	const FText& GetDefaultLoadoutText() const;
	const FText& GetAuthorityBlockerText() const;
	const FText& GetSessionStatusText() const;
	const FText& GetSessionErrorText() const;
	const FText& GetSessionActionHintText() const;
	const FText& GetInviteGuidanceText() const;
	const FText& GetJoinCodeText() const;
	const FText& GetSelectedMapText() const;
	const FText& GetMapSelectionStatusText() const;
	const FText& GetPlayerSlot1Text() const;
	const FText& GetPlayerSlot2Text() const;
	const FText& GetPlayerSlot3Text() const;
	const FText& GetPlayerSlot4Text() const;
	ESlateVisibility GetAuthorityBlockerVisibility() const;
	ESlateVisibility GetSessionErrorVisibility() const;
	ESlateVisibility GetSessionActionHintVisibility() const;
	ESlateVisibility GetInviteGuidanceVisibility() const;
	ESlateVisibility GetJoinCodeVisibility() const;
	bool CanRequestLeaveSession() const;
	bool CanSelectMap() const;
	bool CanRetrySessionOperation() const;

  private:
	FText BuildPlayerSlotText(int32 SlotIndex) const;
	FText ResolveOnlineSessionStatusText() const;
	FText ResolveOnlineSessionFailureText() const;
	FText ResolveSessionActionHintText() const;
	FText ResolveInviteGuidanceText() const;
	void RefreshPlayerSlots();

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	int32 ConnectedPlayerCount = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	int32 LocalPlayerId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	FText PhaseText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	FText PlayerCountText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	FText LocalPlayerText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	FText ReadyCountdownText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	FText DefaultLoadoutText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	FText AuthorityBlockerText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	FText SessionStatusText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	FText SessionErrorText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	FText SessionActionHintText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	FText InviteGuidanceText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	FText JoinCodeText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	FText SelectedMapText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	FText MapSelectionStatusText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	FText PlayerSlot1Text;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	FText PlayerSlot2Text;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	FText PlayerSlot3Text;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	FText PlayerSlot4Text;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	ESlateVisibility AuthorityBlockerVisibility = ESlateVisibility::Visible;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	ESlateVisibility SessionErrorVisibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	ESlateVisibility SessionActionHintVisibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	ESlateVisibility InviteGuidanceVisibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	ESlateVisibility JoinCodeVisibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	bool bCanRequestLeaveSession = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	bool bCanSelectMap = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	bool bCanRetrySessionOperation = false;

#pragma endregion
};
