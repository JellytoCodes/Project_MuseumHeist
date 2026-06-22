#include "UI/ViewModels/HeistResultViewModel.h"

#include "Core/HeistGameState.h"
#include "Core/HeistPlayerState.h"

#pragma region Construction

UHeistResultViewModel::UHeistResultViewModel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
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
	const TArray<FHeistPlayerResult> NewPlayerResults = IsValid(GameState)
		? GameState->GetPlayerResults()
		: TArray<FHeistPlayerResult>();

	UE_MVVM_SET_PROPERTY_VALUE(PlayerResults, NewPlayerResults);

	const int32 NewWinnerId = IsValid(GameState) ? GameState->GetWinnerPlayerId() : INDEX_NONE;
	UE_MVVM_SET_PROPERTY_VALUE(WinnerId, NewWinnerId);
	UE_MVVM_SET_PROPERTY_VALUE(
		WinnerIdText,
		NewWinnerId != INDEX_NONE ? FText::AsNumber(NewWinnerId) : FText::GetEmpty());

	const int32 LocalPlayerId = IsValid(LocalPlayerState)
		? LocalPlayerState->HeistPlayerId
		: INDEX_NONE;
	const FHeistPlayerResult* LocalResult = PlayerResults.FindByPredicate(
		[LocalPlayerId](const FHeistPlayerResult& PlayerResult)
		{
			return PlayerResult.PlayerId == LocalPlayerId;
		});

	const int32 NewMyRank = LocalResult ? LocalResult->Rank : 0;
	const int32 NewMyFinalScore = LocalResult ? LocalResult->FinalScore : 0;
	UE_MVVM_SET_PROPERTY_VALUE(MyRank, NewMyRank);
	UE_MVVM_SET_PROPERTY_VALUE(MyFinalScore, NewMyFinalScore);
	UE_MVVM_SET_PROPERTY_VALUE(MyRankText, FText::AsNumber(NewMyRank));
	UE_MVVM_SET_PROPERTY_VALUE(MyFinalScoreText, FText::AsNumber(NewMyFinalScore));

	const bool bNewEscaped = LocalResult ? LocalResult->bEscaped : false;
	UE_MVVM_SET_PROPERTY_VALUE(bEscaped, bNewEscaped);
	UE_MVVM_SET_PROPERTY_VALUE(
		EscapedVisibility,
		bNewEscaped ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

#pragma endregion

#pragma region ResultData

const TArray<FHeistPlayerResult>& UHeistResultViewModel::GetPlayerResults() const
{
	return PlayerResults;
}

int32 UHeistResultViewModel::GetWinnerId() const
{
	return WinnerId;
}

int32 UHeistResultViewModel::GetMyRank() const
{
	return MyRank;
}

int32 UHeistResultViewModel::GetMyFinalScore() const
{
	return MyFinalScore;
}

bool UHeistResultViewModel::IsEscaped() const
{
	return bEscaped;
}

const FText& UHeistResultViewModel::GetWinnerIdText() const
{
	return WinnerIdText;
}

const FText& UHeistResultViewModel::GetMyRankText() const
{
	return MyRankText;
}

const FText& UHeistResultViewModel::GetMyFinalScoreText() const
{
	return MyFinalScoreText;
}

ESlateVisibility UHeistResultViewModel::GetEscapedVisibility() const
{
	return EscapedVisibility;
}

#pragma endregion
