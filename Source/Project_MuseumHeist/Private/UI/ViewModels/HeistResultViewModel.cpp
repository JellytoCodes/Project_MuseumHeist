#include "UI/ViewModels/HeistResultViewModel.h"

#include "Core/HeistGameState.h"

#pragma region Lifecycle

void UHeistResultViewModel::BeginDestroy()
{
	if (IsValid(GameState))
	{
		GameState->GetPlayerResultsChangedDelegate().RemoveAll(this);
		GameState->GetTeamResultChangedDelegate().RemoveAll(this);
	}

	Super::BeginDestroy();
}

#pragma endregion

#pragma region Setup

void UHeistResultViewModel::SetupViewModel(AHeistGameState* InGameState)
{
	if (GameState != InGameState && IsValid(GameState))
	{
		GameState->GetPlayerResultsChangedDelegate().RemoveAll(this);
		GameState->GetTeamResultChangedDelegate().RemoveAll(this);
	}

	GameState = InGameState;

	if (IsValid(GameState))
	{
		GameState->GetPlayerResultsChangedDelegate().RemoveAll(this);
		GameState->GetPlayerResultsChangedDelegate().AddUObject(this, &UHeistResultViewModel::RefreshResultData);
		GameState->GetTeamResultChangedDelegate().RemoveAll(this);
		GameState->GetTeamResultChangedDelegate().AddUObject(this, &UHeistResultViewModel::HandleTeamResultChanged);
	}

	RefreshResultData();
}

void UHeistResultViewModel::HandleTeamResultChanged(const FHeistTeamResult&)
{
	RefreshResultData();
}

void UHeistResultViewModel::RefreshResultData()
{
	const TArray<FHeistPlayerResult> NewPlayerResults = IsValid(GameState) ? GameState->GetPlayerResults() : TArray<FHeistPlayerResult>();
	const FHeistTeamResult NewTeamResult = IsValid(GameState) ? GameState->GetTeamResult() : FHeistTeamResult();

	UE_MVVM_SET_PROPERTY_VALUE(PlayerResults, NewPlayerResults);
	UE_MVVM_SET_PROPERTY_VALUE(TeamResult, NewTeamResult);
	UE_MVVM_SET_PROPERTY_VALUE(OutcomeText, BuildOutcomeDisplayText(TeamResult.Outcome));
	UE_MVVM_SET_PROPERTY_VALUE(OutcomeReasonText, TeamResult.Outcome == EHeistContractOutcome::Failed ? HeistContractOutcomeReasons::ToDisplayText(TeamResult.OutcomeReasonId) : FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(TeamRewardText, FText::Format(NSLOCTEXT("HeistResult", "TeamRewardFormat", "팀 보상  {0}"), FText::AsNumber(TeamResult.TeamReward)));
	UE_MVVM_SET_PROPERTY_VALUE(ReplicaRecapText, BuildReplicaRecapSummaryText(TeamResult));

	SnapshotChangedDelegate.Broadcast();
}

FHeistResultSnapshotChanged& UHeistResultViewModel::GetSnapshotChangedDelegate()
{
	return SnapshotChangedDelegate;
}

#pragma endregion

#pragma region ResultData

const TArray<FHeistPlayerResult>& UHeistResultViewModel::GetPlayerResults() const
{
	return PlayerResults;
}

const FHeistTeamResult& UHeistResultViewModel::GetTeamResult() const
{
	return TeamResult;
}

const TArray<FHeistReplicaRecapEntry>& UHeistResultViewModel::GetReplicaRecap() const
{
	return TeamResult.ReplicaRecap;
}

const FText& UHeistResultViewModel::GetOutcomeText() const
{
	return OutcomeText;
}
const FText& UHeistResultViewModel::GetOutcomeReasonText() const
{
	return OutcomeReasonText;
}
const FText& UHeistResultViewModel::GetTeamRewardText() const
{
	return TeamRewardText;
}
const FText& UHeistResultViewModel::GetReplicaRecapText() const
{
	return ReplicaRecapText;
}

FText UHeistResultViewModel::BuildOutcomeDisplayText(const EHeistContractOutcome Outcome)
{
	if (Outcome == EHeistContractOutcome::Success)
	{
		return NSLOCTEXT("HeistResult", "OutcomeSuccess", "임무 성공");
	}
	if (Outcome == EHeistContractOutcome::PartialHaul)
	{
		return NSLOCTEXT("HeistResult", "OutcomePartialHaul", "부분 성공");
	}
	if (Outcome == EHeistContractOutcome::Failed)
	{
		return NSLOCTEXT("HeistResult", "OutcomeFailed", "임무 실패");
	}
	return FText::GetEmpty();
}

FText UHeistResultViewModel::BuildReplicaRecapSummaryText(const FHeistTeamResult& InTeamResult)
{
	FString RecapLines;
	for (const FHeistReplicaRecapEntry& Recap : InTeamResult.ReplicaRecap)
	{
		if (!RecapLines.IsEmpty())
		{
			RecapLines += TEXT("\n");
		}
		const TCHAR* TypeText = Recap.ForgeryType == EHeistForgeryType::Assembly ? TEXT("조립") : TEXT("그림");
		const FString DisplayName = Recap.ArtifactDisplayName.IsEmpty() ? Recap.ArtifactId.ToString() : Recap.ArtifactDisplayName.ToString();
		RecapLines += FString::Printf(TEXT("%s%s  |  %s  |  품질 %.0f"), Recap.bRequiredTarget ? TEXT("[필수 목표] ") : TEXT(""), *DisplayName, TypeText, Recap.QualityScore);
	}
	return RecapLines.IsEmpty() ? NSLOCTEXT("HeistResult", "NoReplicaRecap", "기록된 복제품 없음") : FText::FromString(RecapLines);
}

#pragma endregion
