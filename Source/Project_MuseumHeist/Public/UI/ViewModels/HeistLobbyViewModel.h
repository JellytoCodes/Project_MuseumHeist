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
	void SetupViewModel(class AHeistGameState* InGameState, class AHeistPlayerState* InLocalPlayerState);
	void RefreshLobbyData();
	FHeistLobbySnapshotChanged& GetSnapshotChangedDelegate();

private:
	void HandlePlayerConnectionsChanged(int32 NewConnectedPlayerCount);

	UPROPERTY(Transient)
	TObjectPtr<AHeistGameState> GameState;

	UPROPERTY(Transient)
	TObjectPtr<AHeistPlayerState> LocalPlayerState;

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
	const FText& GetPlayerSlot1Text() const;
	const FText& GetPlayerSlot2Text() const;
	const FText& GetPlayerSlot3Text() const;
	const FText& GetPlayerSlot4Text() const;
	ESlateVisibility GetAuthorityBlockerVisibility() const;

private:
	FText BuildPlayerSlotText(int32 SlotIndex) const;
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
	FText PlayerSlot1Text;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	FText PlayerSlot2Text;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	FText PlayerSlot3Text;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	FText PlayerSlot4Text;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	ESlateVisibility AuthorityBlockerVisibility = ESlateVisibility::Visible;

#pragma endregion
};
