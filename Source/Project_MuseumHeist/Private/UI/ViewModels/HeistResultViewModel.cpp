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
	}

	GameState = InGameState;
	LocalPlayerState = InLocalPlayerState;

	if (IsValid(GameState))
	{
		GameState->GetPlayerResultsChangedDelegate().RemoveAll(this);
		GameState->GetPlayerResultsChangedDelegate().AddUObject(this, &UHeistResultViewModel::RefreshResultData);
	}

	RefreshResultData();
}

void UHeistResultViewModel::RefreshResultData()
{
	const TArray<FHeistPlayerResult> NewPlayerResults = IsValid(GameState) ? GameState->GetPlayerResults() : TArray<FHeistPlayerResult>();

	UE_MVVM_SET_PROPERTY_VALUE(PlayerResults, NewPlayerResults);

	const int32 LocalPlayerId = IsValid(LocalPlayerState) ? LocalPlayerState->HeistPlayerId : INDEX_NONE;
	const FHeistPlayerResult* LocalResult = PlayerResults.FindByPredicate([LocalPlayerId](const FHeistPlayerResult& PlayerResult) { return PlayerResult.PlayerId == LocalPlayerId; });

	const int32 NewMyFinalScore = LocalResult ? LocalResult->FinalScore : 0;
	UE_MVVM_SET_PROPERTY_VALUE(MyFinalScore, NewMyFinalScore);
	UE_MVVM_SET_PROPERTY_VALUE(MyFinalScoreText, FText::AsNumber(NewMyFinalScore));

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

FText UHeistResultViewModel::BuildResultRowText(const int32 ResultIndex) const
{
	if (!PlayerResults.IsValidIndex(ResultIndex))
	{
		return FText::GetEmpty();
	}

	const FHeistPlayerResult& PlayerResult = PlayerResults[ResultIndex];
	const FText EscapeStateText = PlayerResult.bEscaped ? FText::FromString(TEXT("ESCAPED")) : FText::FromString(TEXT("CAUGHT"));
	return FText::Format(NSLOCTEXT("HeistResult", "ResultRowFormat", "P{0} \n Loot {1} \n Weight {2} \n {3}"), FText::AsNumber(PlayerResult.PlayerId), FText::AsNumber(PlayerResult.FinalScore),
						 FText::AsNumber(FMath::RoundToInt(PlayerResult.LootWeight)), EscapeStateText);
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
