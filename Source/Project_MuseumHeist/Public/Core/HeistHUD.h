#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"

#include "HeistHUD.generated.h"

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistHUD : public AHUD
{
	GENERATED_BODY()

#pragma region Construction

public:
	AHeistHUD();

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void BeginPlay() override;

#pragma endregion

#pragma region MainHUDPresentation

public:
	bool ShowMainHUD();
	void HideMainHUD();
	void RefreshPresentationSources();
	class UHeistHUDViewModel* GetHUDViewModel() const;
	class UHeistHUDWidget* GetMainHUDWidget() const;

private:
	void InitializeMainHUDPresentation();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UHeistHUDWidget> MainHUDWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UHeistHUDViewModel> HUDViewModel;

	UPROPERTY(Transient)
	TObjectPtr<UHeistHUDWidget> MainHUDWidget;

#pragma endregion

#pragma region LobbyPresentation

public:
	bool ShowLobbyScreen();
	void HideLobbyScreen();
	class UHeistLobbyViewModel* GetLobbyViewModel() const;

private:
	void InitializeLobbyPresentation();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class UHeistLobbyWidget> LobbyWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UHeistLobbyViewModel> LobbyViewModel;

	UPROPERTY(Transient)
	TObjectPtr<UHeistLobbyWidget> LobbyWidget;

#pragma endregion

#pragma region InventoryPresentation

public:
	bool ShowInventoryScreen();
	class UHeistInventoryViewModel* GetInventoryViewModel() const;
	class UHeistQuickSlotViewModel* GetQuickSlotViewModel() const;

private:
	void InitializeInventoryPresentation();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class UHeistInventoryWidget> InventoryWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UHeistInventoryViewModel> InventoryViewModel;

	UPROPERTY(Transient)
	TObjectPtr<UHeistQuickSlotViewModel> QuickSlotViewModel;

	UPROPERTY(Transient)
	TObjectPtr<UHeistInventoryWidget> InventoryWidget;

#pragma endregion

#pragma region GapTrackerPresentation

public:
	class UHeistGapTrackerViewModel* GetGapTrackerViewModel() const;

private:
	void InitializeGapTrackerPresentation();

	UPROPERTY(Transient)
	TObjectPtr<UHeistGapTrackerViewModel> GapTrackerViewModel;

#pragma endregion

#pragma region ResultPresentation

public:
	bool ShowResultScreen();
	void HideResultScreen();
	class UHeistResultViewModel* GetResultViewModel() const;

private:
	void InitializeResultPresentation();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class UHeistResultWidget> ResultWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UHeistResultViewModel> ResultViewModel;

	UPROPERTY(Transient)
	TObjectPtr<UHeistResultWidget> ResultWidget;

#pragma endregion
};
