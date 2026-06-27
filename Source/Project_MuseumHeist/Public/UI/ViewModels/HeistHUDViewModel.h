#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "MVVMViewModelBase.h"

#include "HeistHUDViewModel.generated.h"

DECLARE_MULTICAST_DELEGATE(FHeistRareLootPresentationChanged);

UCLASS(BlueprintType)
class PROJECT_MUSEUMHEIST_API UHeistHUDViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

#pragma region Construction

public:
	UHeistHUDViewModel(const FObjectInitializer& ObjectInitializer);

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void BeginDestroy() override;

#pragma endregion

#pragma region Setup

public:
	void SetupViewModel(class AHeistGameState* InGameState);
	void RefreshRareLootState();
	FHeistRareLootPresentationChanged& GetRareLootPresentationChangedDelegate();

private:
	void HandleRareLootEventStateChanged(const FHeistRareLootEventState& EventState);

	UPROPERTY(Transient)
	TObjectPtr<AHeistGameState> GameState;

	FHeistRareLootPresentationChanged RareLootPresentationChangedDelegate;

#pragma endregion

#pragma region RareLootPresentation

public:
	bool IsRareLootIncoming() const;
	bool IsRareLootDirectionMarkerVisible() const;
	int32 GetRareLootEventIndex() const;
	FName GetRareLootItemId() const;
	FVector GetRareLootWorldLocation() const;
	float GetRareLootSpawnServerTime() const;

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|RareLoot", meta = (AllowPrivateAccess = "true"))
	bool bRareLootIncoming = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|RareLoot", meta = (AllowPrivateAccess = "true"))
	bool bRareLootDirectionMarkerVisible = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|RareLoot", meta = (AllowPrivateAccess = "true"))
	int32 RareLootEventIndex = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|RareLoot", meta = (AllowPrivateAccess = "true"))
	FName RareLootItemId = NAME_None;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|RareLoot", meta = (AllowPrivateAccess = "true"))
	FVector RareLootWorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|RareLoot", meta = (AllowPrivateAccess = "true"))
	float RareLootSpawnServerTime = -1.0f;

#pragma endregion
};
