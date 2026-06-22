#include "Core/HeistPlayerState.h"

#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameState.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Net/UnrealNetwork.h"

#pragma region ScoreAndWeight

int32 AHeistPlayerState::GetTotalLootScore() const
{
	return TotalLootScore;
}

float AHeistPlayerState::GetTotalLootWeight() const
{
	return TotalLootWeight;
}

bool AHeistPlayerState::CanAddLootScoreAndWeight(int32 ScoreDelta, float WeightDelta) const
{
	if (!HasAuthority() || bEscaped || ScoreDelta < 0 || WeightDelta < 0.0f || !FMath::IsFinite(WeightDelta))
	{
		return false;
	}

	const int64 NewScore = static_cast<int64>(TotalLootScore) + static_cast<int64>(ScoreDelta);
	const double NewWeight = static_cast<double>(TotalLootWeight) + static_cast<double>(WeightDelta);

	return NewScore <= MAX_int32
		&& FMath::IsFinite(NewWeight)
		&& NewWeight <= static_cast<double>(TNumericLimits<float>::Max());
}

bool AHeistPlayerState::AddLootScoreAndWeight(int32 ScoreDelta, float WeightDelta)
{
	if (!HasAuthority())
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			FString::Printf(
				TEXT("Loot score/weight rejected: PlayerState=%s Reason=NotAuthority"),
				*GetNameSafe(this)),
			EHeistDebugLevel::Warning);
		return false;
	}

	if (bEscaped)
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			FString::Printf(
				TEXT("Loot score/weight rejected: PlayerState=%s Reason=AlreadyEscaped"),
				*GetNameSafe(this)),
			EHeistDebugLevel::Warning);
		return false;
	}

	if (!CanAddLootScoreAndWeight(ScoreDelta, WeightDelta))
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			FString::Printf(
				TEXT("Loot score/weight rejected: PlayerState=%s Reason=InvalidLootValues ScoreDelta=%d WeightDelta=%.2f"),
				*GetNameSafe(this),
				ScoreDelta,
				WeightDelta),
			EHeistDebugLevel::Warning);
		return false;
	}

	TotalLootScore += ScoreDelta;
	TotalLootWeight += WeightDelta;
	ForceNetUpdate();

	if (AHeistPlayerCharacter* HeistPlayerCharacter = Cast<AHeistPlayerCharacter>(GetPawn()))
	{
		HeistPlayerCharacter->RefreshMovementSpeedFromWeight();
	}
	else
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			FString::Printf(
				TEXT("Weight movement speed skipped: PlayerState=%s Reason=MissingCharacter"),
				*GetNameSafe(this)),
			EHeistDebugLevel::Warning);
	}

	UHeistDebugFunctionLibrary::Message(
		this,
		FString::Printf(
			TEXT("Loot score/weight applied: PlayerState=%s ScoreDelta=%d WeightDelta=%.2f TotalScore=%d TotalWeight=%.2f"),
			*GetNameSafe(this),
			ScoreDelta,
			WeightDelta,
			TotalLootScore,
			TotalLootWeight));

	return true;
}

#pragma endregion

#pragma region EscapeState

bool AHeistPlayerState::IsEscaped() const
{
	return bEscaped;
}

bool AHeistPlayerState::MarkEscaped()
{
	if (!HasAuthority())
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			FString::Printf(
				TEXT("Player escape state rejected: PlayerState=%s Reason=NotAuthority"),
				*GetNameSafe(this)),
			EHeistDebugLevel::Warning);
		return false;
	}

	if (bEscaped)
	{
		UHeistDebugFunctionLibrary::Message(
			this,
			FString::Printf(
				TEXT("Player escape state rejected: PlayerState=%s Reason=AlreadyEscaped"),
				*GetNameSafe(this)),
			EHeistDebugLevel::Warning);
		return false;
	}

	bEscaped = true;
	FinalScore = TotalLootScore;
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	EscapeTimeSeconds = IsValid(HeistGameState)
		? HeistGameState->GetServerWorldTimeSeconds()
		: (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);

	if (AHeistPlayerCharacter* HeistPlayerCharacter = Cast<AHeistPlayerCharacter>(GetPawn()))
	{
		HeistPlayerCharacter->ApplyEscapedGameplayRestrictions();
	}

	if (AHeistGameState* MutableHeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr)
	{
		MutableHeistGameState->RebuildPlayerResults();
	}

	ForceNetUpdate();
	EscapeStateChangedDelegate.Broadcast(bEscaped);

	UHeistDebugFunctionLibrary::Message(
		this,
		FString::Printf(
			TEXT("Player escape state committed: PlayerState=%s HeistPlayerId=%d IsEscaped=true FinalScore=%d EscapeTime=%.2f ScoreFrozen=true"),
			*GetNameSafe(this),
			HeistPlayerId,
			FinalScore,
			EscapeTimeSeconds));

	return true;
}

int32 AHeistPlayerState::GetFinalScore() const
{
	return FinalScore;
}

float AHeistPlayerState::GetEscapeTimeSeconds() const
{
	return EscapeTimeSeconds;
}

int32 AHeistPlayerState::GetPlayerRank() const
{
	return PlayerRank;
}

void AHeistPlayerState::SetPlayerRank(int32 InPlayerRank)
{
	check(HasAuthority());

	PlayerRank = FMath::Max(0, InPlayerRank);
	ForceNetUpdate();
}

FHeistPlayerEscapeStateChanged& AHeistPlayerState::GetEscapeStateChangedDelegate()
{
	return EscapeStateChangedDelegate;
}

void AHeistPlayerState::OnRep_Escaped()
{
	if (AHeistPlayerCharacter* HeistPlayerCharacter = Cast<AHeistPlayerCharacter>(GetPawn()))
	{
		HeistPlayerCharacter->ApplyEscapedGameplayRestrictions();
	}

	EscapeStateChangedDelegate.Broadcast(bEscaped);

	UHeistDebugFunctionLibrary::Message(
		this,
		FString::Printf(
			TEXT("Player escape state replicated: PlayerState=%s HeistPlayerId=%d IsEscaped=%s"),
			*GetNameSafe(this),
			HeistPlayerId,
			bEscaped ? TEXT("true") : TEXT("false")));
}

#pragma endregion

#pragma region Replication

void AHeistPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeistPlayerState, HeistPlayerId);
	DOREPLIFETIME(AHeistPlayerState, PlayerColor);
	DOREPLIFETIME(AHeistPlayerState, TotalLootScore);
	DOREPLIFETIME(AHeistPlayerState, TotalLootWeight);
	DOREPLIFETIME(AHeistPlayerState, bEscaped);
	DOREPLIFETIME(AHeistPlayerState, FinalScore);
	DOREPLIFETIME(AHeistPlayerState, EscapeTimeSeconds);
	DOREPLIFETIME(AHeistPlayerState, PlayerRank);
}

void AHeistPlayerState::OnRep_TotalLootScore()
{
	UHeistDebugFunctionLibrary::Message(
		this,
		FString::Printf(
			TEXT("PlayerState score replicated: PlayerState=%s TotalLootScore=%d"),
			*GetNameSafe(this),
			TotalLootScore));
}

void AHeistPlayerState::OnRep_TotalLootWeight()
{
	UHeistDebugFunctionLibrary::Message(
		this,
		FString::Printf(
			TEXT("PlayerState weight replicated: PlayerState=%s TotalLootWeight=%.2f"),
			*GetNameSafe(this),
			TotalLootWeight));
}

#pragma endregion

#pragma region Verification

void AHeistPlayerState::InitializeVerificationIdentity(int32 InHeistPlayerId, const FLinearColor& InPlayerColor)
{
	check(HasAuthority());

	HeistPlayerId = InHeistPlayerId;
	PlayerColor = InPlayerColor;
	ForceNetUpdate();
}

#pragma endregion
