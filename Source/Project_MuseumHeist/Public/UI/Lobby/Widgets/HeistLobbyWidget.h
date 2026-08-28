#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/HeistUserWidgetBase.h"

#include "HeistLobbyWidget.generated.h"

class UButton;
class UHeistLobbyMapCardWidget;
class UHeistLobbyPlayerCardWidget;
class UHeistLobbyViewModel;
class UTextBlock;

UCLASS(Blueprintable)
class PROJECT_MUSEUMHEIST_API UHeistLobbyWidget : public UHeistUserWidgetBase
{
	GENERATED_BODY()

#pragma region Lifecycle

  protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

#pragma endregion

#pragma region ViewModel

  public:
	void SetupLobbyWidget(UHeistLobbyViewModel* InLobbyViewModel);
	UHeistLobbyViewModel* GetLobbyViewModel() const;

  private:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heist|Lobby", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeistLobbyViewModel> LobbyViewModel;

#pragma endregion

#pragma region Presentation

  private:
	UFUNCTION()
	void HandleCopyJoinCodeClicked();

	UFUNCTION()
	void HandleLeaveSessionClicked();

	UFUNCTION()
	void HandleStartGameClicked();

	void HandlePlayerReadyRequested(int32 PlayerSlot);
	void HandleMapSelected(FName MapId);
	void ConfigureChildWidgets();
	void BindChildWidgetDelegates();
	void UnbindChildWidgetDelegates();
	void RefreshLobbyPresentation();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> CopyJoinCodeButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> LeaveSessionButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> StartGameButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> JoinCodeText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> PlayerCountText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UHeistLobbyPlayerCardWidget> PlayerCard1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UHeistLobbyPlayerCardWidget> PlayerCard2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UHeistLobbyPlayerCardWidget> PlayerCard3;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UHeistLobbyPlayerCardWidget> PlayerCard4;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UHeistLobbyMapCardWidget> MapRandomCard;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UHeistLobbyMapCardWidget> MapM01Card;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UHeistLobbyMapCardWidget> MapM02Card;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UHeistLobbyMapCardWidget> MapM03Card;

#pragma endregion
};
