#pragma once

#include "CoreMinimal.h"
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
	void SetupViewModel(class AHeistGameState* InGameState);
	void RefreshResultData();
	FHeistResultSnapshotChanged& GetSnapshotChangedDelegate();

  private:
	void HandleTeamResultChanged(const FHeistTeamResult& NewTeamResult);

	UPROPERTY(Transient)
	TObjectPtr<AHeistGameState> GameState;

	FHeistResultSnapshotChanged SnapshotChangedDelegate;

#pragma endregion

#pragma region ResultData

  public:
	const TArray<FHeistPlayerResult>& GetPlayerResults() const;
	const FHeistTeamResult& GetTeamResult() const;
	const TArray<FHeistReplicaRecapEntry>& GetReplicaRecap() const;
	const FText& GetOutcomeText() const;
	const FText& GetOutcomeReasonText() const;
	const FText& GetTeamRewardText() const;
	const FText& GetReplicaRecapText() const;
	static FText BuildOutcomeDisplayText(EHeistContractOutcome Outcome);
	static FText BuildReplicaRecapSummaryText(const FHeistTeamResult& InTeamResult);

  private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	TArray<FHeistPlayerResult> PlayerResults;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	FHeistTeamResult TeamResult;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	FText OutcomeText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	FText OutcomeReasonText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	FText TeamRewardText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Result", meta = (AllowPrivateAccess = "true"))
	FText ReplicaRecapText;

#pragma endregion
};
