#include "UI/ViewModels/HeistResultViewModel.h"

#include "Core/HeistGameState.h"
#include "Core/HeistPlayerState.h"

#pragma region Construction

UHeistResultViewModel::UHeistResultViewModel(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

#pragma endregion

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

void UHeistResultViewModel::SetupViewModel(AHeistGameState* InGameState, AHeistPlayerState* InLocalPlayerState)
{
	if (GameState != InGameState && IsValid(GameState))
	{
		GameState->GetPlayerResultsChangedDelegate().RemoveAll(this);
		GameState->GetTeamResultChangedDelegate().RemoveAll(this);
	}

	GameState = InGameState;
	LocalPlayerState = InLocalPlayerState;

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
	const FText NewOutcomeText = TeamResult.Outcome == EHeistContractOutcome::Success ? NSLOCTEXT("HeistResult", "OutcomeSuccess", "계약 완료")
		: TeamResult.Outcome == EHeistContractOutcome::PartialHaul ? NSLOCTEXT("HeistResult", "OutcomePartial", "일부 확보")
		: TeamResult.Outcome == EHeistContractOutcome::Failed ? NSLOCTEXT("HeistResult", "OutcomeFailed", "계약 실패") : FText::GetEmpty();
	UE_MVVM_SET_PROPERTY_VALUE(OutcomeText, NewOutcomeText);
	UE_MVVM_SET_PROPERTY_VALUE(OutcomeReasonText, HeistContractOutcomeReasons::ToDisplayText(TeamResult.OutcomeReasonId));
	UE_MVVM_SET_PROPERTY_VALUE(ContractProgressText,
		FText::Format(NSLOCTEXT("HeistResult", "ContractProgressFormat", "확보 가치 {0} / {1}   초과 {2}   핵심 목표 {3}"), FText::AsNumber(TeamResult.SecuredValue),
			FText::AsNumber(TeamResult.LootValueQuota), FText::AsNumber(TeamResult.ExtraValue),
			TeamResult.bRequiredTargetSecured ? NSLOCTEXT("HeistResult", "TargetSecured", "확보 완료") : NSLOCTEXT("HeistResult", "TargetMissing", "미확보")));
	UE_MVVM_SET_PROPERTY_VALUE(TeamRewardText,
		FText::Format(NSLOCTEXT("HeistResult", "TeamRewardFormat", "팀 보상  {0}"), FText::AsNumber(TeamResult.TeamReward)));
	UE_MVVM_SET_PROPERTY_VALUE(RewardBreakdownText,
		FText::Format(NSLOCTEXT("HeistResult", "RewardBreakdownFormat", "핵심 목표 {0} × 위조 품질 {1} × 잠입 {2}  +  추가 전리품 {3}  −  체포 {4}"),
			FText::AsNumber(TeamResult.RequiredTargetValue), FText::AsNumber(TeamResult.ForgeryRewardMultiplier), FText::AsNumber(TeamResult.StealthRewardMultiplier),
			FText::AsNumber(TeamResult.SecuredLooseLootValue), FText::AsNumber(TeamResult.ArrestPenalty)));
	FString RecapLines;
	for (const FHeistReplicaRecapEntry& Recap : TeamResult.ReplicaRecap)
	{
		if (!RecapLines.IsEmpty())
		{
			RecapLines += TEXT("\n");
		}
		RecapLines += FString::Printf(TEXT("%s%s  |  %s  |  %.0f"), Recap.bRequiredTarget ? TEXT("[TARGET] ") : TEXT(""), *Recap.ArtifactId.ToString(),
			*UEnum::GetValueAsString(Recap.ForgeryType), Recap.QualityScore);
	}
	UE_MVVM_SET_PROPERTY_VALUE(ReplicaRecapText, RecapLines.IsEmpty() ? NSLOCTEXT("HeistResult", "NoReplicaRecap", "기록된 복제품 없음") : FText::FromString(RecapLines));

	const int32 LocalPlayerId = IsValid(LocalPlayerState) ? LocalPlayerState->HeistPlayerId : INDEX_NONE;
	const FHeistPlayerResult* LocalResult = PlayerResults.FindByPredicate([LocalPlayerId](const FHeistPlayerResult& PlayerResult) { return PlayerResult.PlayerId == LocalPlayerId; });

	const int32 NewMyFinalScore = LocalResult ? LocalResult->FinalScore : 0;
	UE_MVVM_SET_PROPERTY_VALUE(MyFinalScore, NewMyFinalScore);
	UE_MVVM_SET_PROPERTY_VALUE(MyFinalScoreText, FText::Format(NSLOCTEXT("HeistResult", "LocalFinalScoreFormat", "전리품  {0}"), FText::AsNumber(NewMyFinalScore)));

	const bool bNewEscaped = LocalResult ? LocalResult->bEscaped : false;
	UE_MVVM_SET_PROPERTY_VALUE(bEscaped, bNewEscaped);
	UE_MVVM_SET_PROPERTY_VALUE(EscapedVisibility, bNewEscaped ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	RefreshResultRows();
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

int32 UHeistResultViewModel::GetMyFinalScore() const
{
	return MyFinalScore;
}

bool UHeistResultViewModel::IsEscaped() const
{
	return bEscaped;
}

const FText& UHeistResultViewModel::GetMyFinalScoreText() const
{
	return MyFinalScoreText;
}

ESlateVisibility UHeistResultViewModel::GetEscapedVisibility() const
{
	return EscapedVisibility;
}

const FText& UHeistResultViewModel::GetResultRow1Text() const
{
	return ResultRow1Text;
}

const FText& UHeistResultViewModel::GetResultRow2Text() const
{
	return ResultRow2Text;
}

const FText& UHeistResultViewModel::GetResultRow3Text() const
{
	return ResultRow3Text;
}

const FText& UHeistResultViewModel::GetResultRow4Text() const
{
	return ResultRow4Text;
}

ESlateVisibility UHeistResultViewModel::GetResultRow1Visibility() const
{
	return ResultRow1Visibility;
}

ESlateVisibility UHeistResultViewModel::GetResultRow2Visibility() const
{
	return ResultRow2Visibility;
}

ESlateVisibility UHeistResultViewModel::GetResultRow3Visibility() const
{
	return ResultRow3Visibility;
}

ESlateVisibility UHeistResultViewModel::GetResultRow4Visibility() const
{
	return ResultRow4Visibility;
}

const FText& UHeistResultViewModel::GetOutcomeText() const { return OutcomeText; }
const FText& UHeistResultViewModel::GetOutcomeReasonText() const { return OutcomeReasonText; }
const FText& UHeistResultViewModel::GetContractProgressText() const { return ContractProgressText; }
const FText& UHeistResultViewModel::GetTeamRewardText() const { return TeamRewardText; }
const FText& UHeistResultViewModel::GetRewardBreakdownText() const { return RewardBreakdownText; }
const FText& UHeistResultViewModel::GetReplicaRecapText() const { return ReplicaRecapText; }

FText UHeistResultViewModel::BuildResultRowText(const int32 ResultIndex) const
{
	if (!PlayerResults.IsValidIndex(ResultIndex))
	{
		return FText::GetEmpty();
	}

	const FHeistPlayerResult& PlayerResult = PlayerResults[ResultIndex];
	const FText EscapeStateText =
		PlayerResult.bEscaped ? NSLOCTEXT("HeistResult", "PlayerEscaped", "탈출") : NSLOCTEXT("HeistResult", "PlayerCaught", "체포");
	const FHeistPlayerContribution& Contribution = PlayerResult.Contribution;
	return FText::Format(
		NSLOCTEXT("HeistResult", "ResultRowFormat", "플레이어 {0}  |  {1}  |  평면/조립 위조 {2}/{3}  |  원본 회수 {4}  |  전리품 {5}  |  경비 교란 {6}  |  동료 구조 {7}  |  경보 {8}"),
		FText::AsNumber(PlayerResult.PlayerId), EscapeStateText, FText::AsNumber(Contribution.SurfaceForgeries), FText::AsNumber(Contribution.Assemblies),
		FText::AsNumber(Contribution.ArtifactsRecovered), FText::AsNumber(Contribution.SecuredLootValue), FText::AsNumber(Contribution.GuardsDistracted),
		FText::AsNumber(Contribution.TeammatesRescued), FText::AsNumber(Contribution.AlarmsTriggered));
}

ESlateVisibility UHeistResultViewModel::BuildResultRowVisibility(const int32 ResultIndex) const
{
	return PlayerResults.IsValidIndex(ResultIndex) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
}

void UHeistResultViewModel::RefreshResultRows()
{
	UE_MVVM_SET_PROPERTY_VALUE(ResultRow1Text, BuildResultRowText(0));
	UE_MVVM_SET_PROPERTY_VALUE(ResultRow2Text, BuildResultRowText(1));
	UE_MVVM_SET_PROPERTY_VALUE(ResultRow3Text, BuildResultRowText(2));
	UE_MVVM_SET_PROPERTY_VALUE(ResultRow4Text, BuildResultRowText(3));
	UE_MVVM_SET_PROPERTY_VALUE(ResultRow1Visibility, BuildResultRowVisibility(0));
	UE_MVVM_SET_PROPERTY_VALUE(ResultRow2Visibility, BuildResultRowVisibility(1));
	UE_MVVM_SET_PROPERTY_VALUE(ResultRow3Visibility, BuildResultRowVisibility(2));
	UE_MVVM_SET_PROPERTY_VALUE(ResultRow4Visibility, BuildResultRowVisibility(3));
}

#pragma endregion
