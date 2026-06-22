#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "Core/HeistTypes.h"
#include "MVVMViewModelBase.h"

#include "HeistResultViewModel.generated.h"

UCLASS(BlueprintType)
class PROJECT_MUSEUMHEIST_API UHeistResultViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

#pragma region Construction

public:
	UHeistResultViewModel(const FObjectInitializer& ObjectInitializer);

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void BeginDestroy() override;

#pragma endregion

#pragma region Setup

public:
	void SetupViewModel(class AHeistGameState* InGameState, class AHeistPlayerState* InLocalPlayerState);
	void RefreshResultData();

private:
	UPROPERTY(Transient)
	TObjectPtr<AHeistGameState> GameState;

	UPROPERTY(Transient)
	TObjectPtr<AHeistPlayerState> LocalPlayerState;

#pragma endregion

#pragma region ResultData

public:
	const TArray<FHeistPlayerResult>& GetPlayerResults() const;
	int32 GetWinnerId() const;
	int32 GetMyRank() const;
	int32 GetMyFinalScore() const;
	bool IsEscaped() const;
	const FText& GetWinnerIdText() const;
	const FText& GetMyRankText() const;
	const FText& GetMyFinalScoreText() const;
	ESlateVisibility GetEscapedVisibility() const;

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	TArray<FHeistPlayerResult> PlayerResults;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	int32 WinnerId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	int32 MyRank = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	int32 MyFinalScore = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	bool bEscaped = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	FText WinnerIdText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	FText MyRankText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	FText MyFinalScoreText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	ESlateVisibility EscapedVisibility = ESlateVisibility::Collapsed;

#pragma endregion
};
