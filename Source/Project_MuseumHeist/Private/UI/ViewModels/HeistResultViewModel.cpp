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
	const FText NewOutcomeText = TeamResult.Outcome == EHeistContractOutcome::Success ? NSLOCTEXT("HeistResult", "OutcomeSuccess", "CONTRACT COMPLETE")
		: TeamResult.Outcome == EHeistContractOutcome::PartialHaul ? NSLOCTEXT("HeistResult", "OutcomePartial", "PARTIAL HAUL")
		: TeamResult.Outcome == EHeistContractOutcome::Failed ? NSLOCTEXT("HeistResult", "OutcomeFailed", "CONTRACT FAILED") : FText::GetEmpty();
	UE_MVVM_SET_PROPERTY_VALUE(OutcomeText, NewOutcomeText);
	UE_MVVM_SET_PROPERTY_VALUE(OutcomeReasonText, HeistContractOutcomeReasons::ToDisplayText(TeamResult.OutcomeReasonId));
	UE_MVVM_SET_PROPERTY_VALUE(ContractProgressText,
		FText::Format(NSLOCTEXT("HeistResult", "ContractProgressFormat", "SECURED {0} / {1}   EXTRA {2}   TARGET {3}"), FText::AsNumber(TeamResult.SecuredValue),
			FText::AsNumber(TeamResult.LootValueQuota), FText::AsNumber(TeamResult.ExtraValue),
			TeamResult.bRequiredTargetSecured ? NSLOCTEXT("HeistResult", "TargetSecured", "SECURED") : NSLOCTEXT("HeistResult", "TargetMissing", "MISSING")));
	UE_MVVM_SET_PROPERTY_VALUE(TeamRewardText,
		FText::Format(NSLOCTEXT("HeistResult", "TeamRewardFormat", "TEAM REWARD  {0}"), FText::AsNumber(TeamResult.TeamReward)));
	UE_MVVM_SET_PROPERTY_VALUE(RewardBreakdownText,
		FText::Format(NSLOCTEXT("HeistResult", "RewardBreakdownFormat", "TARGET {0} × QUALITY {1} × STEALTH {2}  +  LOOSE {3}  −  ARREST {4}"),
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
	UE_MVVM_SET_PROPERTY_VALUE(ReplicaRecapText, RecapLines.IsEmpty() ? NSLOCTEXT("HeistResult", "NoReplicaRecap", "NO REPLICA RECORDED") : FText::FromString(RecapLines));

	const int32 LocalPlayerId = IsValid(LocalPlayerState) ? LocalPlayerState->HeistPlayerId : INDEX_NONE;
	const FHeistPlayerResult* LocalResult = PlayerResults.FindByPredicate([LocalPlayerId](const FHeistPlayerResult& PlayerResult) { return PlayerResult.PlayerId == LocalPlayerId; });

	const int32 NewMyFinalScore = LocalResult ? LocalResult->FinalScore : 0;
	UE_MVVM_SET_PROPERTY_VALUE(MyFinalScore, NewMyFinalScore);
	UE_MVVM_SET_PROPERTY_VALUE(MyFinalScoreText, FText::Format(NSLOCTEXT("HeistResult", "LocalFinalScoreFormat", "LOOT  {0}"), FText::AsNumber(NewMyFinalScore)));

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
		PlayerResult.bEscaped ? NSLOCTEXT("HeistResult", "PlayerEscaped", "ESCAPED") : NSLOCTEXT("HeistResult", "PlayerCaught", "CAUGHT");
	const FHeistPlayerContribution& Contribution = PlayerResult.Contribution;
	return FText::Format(
		NSLOCTEXT("HeistResult", "ResultRowFormat", "PLAYER {0}  |  {1}  |  FORGERY {2}/{3}  |  RECOVERED {4}  |  SECURED {5}  |  DISTRACT {6}  |  RESCUE {7}  |  ALARMS {8}"),
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
