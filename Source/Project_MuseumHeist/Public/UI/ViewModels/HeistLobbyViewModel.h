#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "MVVMViewModelBase.h"

#include "HeistLobbyViewModel.generated.h"

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
	void SetupViewModel(class AHeistGameState* InGameState, class AHeistPlayerState* InLocalPlayerState, class UHeistGameInstance* InGameInstance);
	void RefreshLobbyData();
	FHeistLobbySnapshotChanged& GetSnapshotChangedDelegate();

	UFUNCTION(BlueprintCallable, Category = "Heist|Lobby")
	bool RequestHostSession();

	UFUNCTION(BlueprintCallable, Category = "Heist|Lobby")
	bool RequestJoinSessionByCode(const FString& JoinCode);

  private:
	void HandlePlayerConnectionsChanged(int32 NewConnectedPlayerCount);
	void HandleOnlineSessionStateChanged();

	UPROPERTY(Transient)
	TObjectPtr<AHeistGameState> GameState;

	UPROPERTY(Transient)
	TObjectPtr<AHeistPlayerState> LocalPlayerState;

	UPROPERTY(Transient)
	TObjectPtr<UHeistGameInstance> GameInstance;

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
	const FText& GetJoinCodeText() const;
	const FText& GetPlayerSlot1Text() const;
	const FText& GetPlayerSlot2Text() const;
	const FText& GetPlayerSlot3Text() const;
	const FText& GetPlayerSlot4Text() const;
	ESlateVisibility GetAuthorityBlockerVisibility() const;
	ESlateVisibility GetSessionErrorVisibility() const;
	ESlateVisibility GetJoinCodeVisibility() const;
	bool CanRequestHostSession() const;
	bool CanRequestJoinSession() const;

  private:
	FText BuildPlayerSlotText(int32 SlotIndex) const;
	FText ResolveOnlineSessionStatusText() const;
	FText ResolveOnlineSessionFailureText() const;
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
	FText JoinCodeText;

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
	ESlateVisibility JoinCodeVisibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	bool bCanRequestHostSession = true;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	bool bCanRequestJoinSession = true;

#pragma endregion
};
