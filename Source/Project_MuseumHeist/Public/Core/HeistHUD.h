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

#pragma region TitleMenuPresentation

  public:
	bool ShowTitleMenuScreen();
	void HideTitleMenuScreen();
	class UHeistTitleMenuViewModel* GetTitleMenuViewModel() const;

  private:
	void InitializeTitleMenuPresentation();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class UHeistTitleMenuWidget> TitleMenuWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UHeistTitleMenuViewModel> TitleMenuViewModel;

	UPROPERTY(Transient)
	TObjectPtr<UHeistTitleMenuWidget> TitleMenuWidget;

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

#pragma region ForgeryPresentation

  public:
	class UHeistForgeryViewModel* GetForgeryViewModel() const;
	class UHeistForgeryWidget* GetForgeryWidget() const;

  private:
	void InitializeForgeryPresentation();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class UHeistForgeryWidget> ForgeryWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UHeistForgeryViewModel> ForgeryViewModel;

	UPROPERTY(Transient)
	TObjectPtr<UHeistForgeryWidget> ForgeryWidget;

#pragma endregion

#pragma region ObjectAssemblyPresentation

  public:
	class UHeistObjectAssemblyViewModel* GetObjectAssemblyViewModel() const;
	class UHeistObjectAssemblyWidget* GetObjectAssemblyWidget() const;

  private:
	void InitializeObjectAssemblyPresentation();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class UHeistObjectAssemblyWidget> ObjectAssemblyWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UHeistObjectAssemblyViewModel> ObjectAssemblyViewModel;

	UPROPERTY(Transient)
	TObjectPtr<UHeistObjectAssemblyWidget> ObjectAssemblyWidget;

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
