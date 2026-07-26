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

  private:
	void HandleOnlineSessionStateChanged();
	FText ResolveOnlineSessionStatusText() const;
	FText ResolveOnlineSessionFailureText() const;

	UPROPERTY(Transient)
	TObjectPtr<UHeistGameInstance> GameInstance;

	FHeistTitleMenuSnapshotChanged SnapshotChangedDelegate;

#pragma endregion

#pragma region TitleMenuData

  public:
	const FText& GetSessionStatusText() const;
	const FText& GetSessionErrorText() const;
	ESlateVisibility GetSessionErrorVisibility() const;
	bool CanRequestHostSession() const;
	bool CanRequestJoinSession() const;

  private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|TitleMenu", meta = (AllowPrivateAccess = "true"))
	FText SessionStatusText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|TitleMenu", meta = (AllowPrivateAccess = "true"))
	FText SessionErrorText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|TitleMenu", meta = (AllowPrivateAccess = "true"))
	ESlateVisibility SessionErrorVisibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|TitleMenu", meta = (AllowPrivateAccess = "true"))
	bool bCanRequestHostSession = true;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|TitleMenu", meta = (AllowPrivateAccess = "true"))
	bool bCanRequestJoinSession = true;

#pragma endregion
};
