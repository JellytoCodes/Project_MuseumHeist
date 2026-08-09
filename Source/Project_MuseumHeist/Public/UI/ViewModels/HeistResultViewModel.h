#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "Core/HeistTypes.h"
#include "MVVMViewModelBase.h"

#include "HeistResultViewModel.generated.h"

DECLARE_MULTICAST_DELEGATE(FHeistResultSnapshotChanged);

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
	FHeistResultSnapshotChanged& GetSnapshotChangedDelegate();

  private:
	void HandleTeamResultChanged(const FHeistTeamResult& NewTeamResult);

	UPROPERTY(Transient)
	TObjectPtr<AHeistGameState> GameState;

	UPROPERTY(Transient)
	TObjectPtr<AHeistPlayerState> LocalPlayerState;

	FHeistResultSnapshotChanged SnapshotChangedDelegate;

#pragma endregion

#pragma region ResultData

  public:
	const TArray<FHeistPlayerResult>& GetPlayerResults() const;
	const FHeistTeamResult& GetTeamResult() const;
	const TArray<FHeistReplicaRecapEntry>& GetReplicaRecap() const;
	int32 GetMyFinalScore() const;
	bool IsEscaped() const;
	const FText& GetMyFinalScoreText() const;
	ESlateVisibility GetEscapedVisibility() const;
	const FText& GetResultRow1Text() const;
	const FText& GetResultRow2Text() const;
	const FText& GetResultRow3Text() const;
	const FText& GetResultRow4Text() const;
	ESlateVisibility GetResultRow1Visibility() const;
	ESlateVisibility GetResultRow2Visibility() const;
	ESlateVisibility GetResultRow3Visibility() const;
	ESlateVisibility GetResultRow4Visibility() const;
	const FText& GetOutcomeText() const;
	const FText& GetOutcomeReasonText() const;
	const FText& GetContractProgressText() const;
	const FText& GetTeamRewardText() const;
	const FText& GetRewardBreakdownText() const;
	const FText& GetReplicaRecapText() const;
	static FText BuildOutcomeDisplayText(EHeistContractOutcome Outcome);
	static FText BuildContractProgressSummaryText(const FHeistTeamResult& InTeamResult);
	static FText BuildRewardBreakdownSummaryText(const FHeistTeamResult& InTeamResult);
	static FText BuildReplicaRecapSummaryText(const FHeistTeamResult& InTeamResult);
	static FText BuildPlayerResultSummaryText(const FHeistPlayerResult& PlayerResult);

  private:
	FText BuildResultRowText(int32 ResultIndex) const;
	ESlateVisibility BuildResultRowVisibility(int32 ResultIndex) const;
	void RefreshResultRows();

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	TArray<FHeistPlayerResult> PlayerResults;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	FHeistTeamResult TeamResult;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	FText OutcomeText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	FText OutcomeReasonText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	FText ContractProgressText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	FText TeamRewardText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	FText RewardBreakdownText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	FText ReplicaRecapText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	int32 MyFinalScore = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	bool bEscaped = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	FText MyFinalScoreText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	ESlateVisibility EscapedVisibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	FText ResultRow1Text;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	FText ResultRow2Text;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	FText ResultRow3Text;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	FText ResultRow4Text;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	ESlateVisibility ResultRow1Visibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	ESlateVisibility ResultRow2Visibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	ESlateVisibility ResultRow3Visibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	ESlateVisibility ResultRow4Visibility = ESlateVisibility::Collapsed;

#pragma endregion
};
