#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "MVVMViewModelBase.h"

#include "HeistTitleMenuViewModel.generated.h"

DECLARE_MULTICAST_DELEGATE(FHeistTitleMenuSnapshotChanged);

UCLASS(BlueprintType)
class PROJECT_MUSEUMHEIST_API UHeistTitleMenuViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

#pragma region Construction

  public:
	UHeistTitleMenuViewModel(const FObjectInitializer& ObjectInitializer);

#pragma endregion

#pragma region Lifecycle

  protected:
	virtual void BeginDestroy() override;

#pragma endregion

#pragma region Setup

  public:
	void SetupViewModel(class UHeistGameInstance* InGameInstance);
	void RefreshTitleMenuData();
	FHeistTitleMenuSnapshotChanged& GetSnapshotChangedDelegate();

	UFUNCTION(BlueprintCallable, Category = "Heist|TitleMenu")
	bool RequestHostSession();

	UFUNCTION(BlueprintCallable, Category = "Heist|TitleMenu")
	bool RequestJoinSessionByCode(const FString& JoinCode);

	UFUNCTION(BlueprintCallable, Category = "Heist|TitleMenu")
	bool RequestCancelSessionOperation();

	UFUNCTION(BlueprintCallable, Category = "Heist|TitleMenu")
	bool RequestRetrySessionOperation();

	UFUNCTION(BlueprintCallable, Category = "Heist|TitleMenu|Settings")
	bool RequestApplySettings(float FieldOfView, float MouseSensitivity, float MasterVolume, int32 ResolutionWidth, int32 ResolutionHeight, int32 WindowModeValue);

	UFUNCTION(BlueprintCallable, Category = "Heist|TitleMenu|Settings")
	bool RequestRestoreDefaultSettings();

	UFUNCTION(BlueprintCallable, Category = "Heist|TitleMenu|Settings")
	void GetSupportedSettingsResolutions(TArray<FIntPoint>& OutResolutions) const;

  private:
	void HandleOnlineSessionStateChanged();
	void RefreshSettingsData();
	FText ResolveOnlineSessionStatusText() const;
	FText ResolveOnlineSessionFailureText() const;
	FText ResolveSessionActionHintText() const;

	UPROPERTY(Transient)
	TObjectPtr<UHeistGameInstance> GameInstance;

	FHeistTitleMenuSnapshotChanged SnapshotChangedDelegate;

#pragma endregion

#pragma region TitleMenuData

  public:
	const FText& GetSessionStatusText() const;
	const FText& GetSessionErrorText() const;
	const FText& GetSessionActionHintText() const;
	ESlateVisibility GetSessionErrorVisibility() const;
	ESlateVisibility GetSessionActionHintVisibility() const;
	bool CanRequestHostSession() const;
	bool CanRequestJoinSession() const;
	bool CanCancelSessionOperation() const;
	bool CanRetrySessionOperation() const;
	float GetSettingsFieldOfView() const;
	float GetSettingsMouseSensitivity() const;
	float GetSettingsMasterVolume() const;
	FIntPoint GetSettingsScreenResolution() const;
	int32 GetSettingsWindowModeValue() const;
	const FText& GetSettingsStatusText() const;

  private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|TitleMenu", meta = (AllowPrivateAccess = "true"))
	FText SessionStatusText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|TitleMenu", meta = (AllowPrivateAccess = "true"))
	FText SessionErrorText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|TitleMenu", meta = (AllowPrivateAccess = "true"))
	FText SessionActionHintText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|TitleMenu", meta = (AllowPrivateAccess = "true"))
	ESlateVisibility SessionErrorVisibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|TitleMenu", meta = (AllowPrivateAccess = "true"))
	ESlateVisibility SessionActionHintVisibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|TitleMenu", meta = (AllowPrivateAccess = "true"))
	bool bCanRequestHostSession = true;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|TitleMenu", meta = (AllowPrivateAccess = "true"))
	bool bCanRequestJoinSession = true;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|TitleMenu", meta = (AllowPrivateAccess = "true"))
	bool bCanCancelSessionOperation = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|TitleMenu", meta = (AllowPrivateAccess = "true"))
	bool bCanRetrySessionOperation = false;

#pragma endregion

#pragma region SettingsData

  private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|TitleMenu|Settings", meta = (AllowPrivateAccess = "true"))
	float SettingsFieldOfView = 90.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|TitleMenu|Settings", meta = (AllowPrivateAccess = "true"))
	float SettingsMouseSensitivity = 1.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|TitleMenu|Settings", meta = (AllowPrivateAccess = "true"))
	float SettingsMasterVolume = 1.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|TitleMenu|Settings", meta = (AllowPrivateAccess = "true"))
	FIntPoint SettingsScreenResolution = FIntPoint(1920, 1080);

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|TitleMenu|Settings", meta = (AllowPrivateAccess = "true"))
	int32 SettingsWindowModeValue = 1;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|TitleMenu|Settings", meta = (AllowPrivateAccess = "true"))
	FText SettingsStatusText;

#pragma endregion
};
