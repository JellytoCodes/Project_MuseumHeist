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

FHeistLootTotalsChanged& AHeistPlayerState::GetLootTotalsChangedDelegate()
{
	return LootTotalsChangedDelegate;
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
		UHeistDebugFunctionLibrary::DebugLootScoreWeightRejected(this, TEXT("NotAuthority"));
		return false;
	}

	if (bEscaped)
	{
		UHeistDebugFunctionLibrary::DebugLootScoreWeightRejected(this, TEXT("AlreadyEscaped"));
		return false;
	}

	if (!CanAddLootScoreAndWeight(ScoreDelta, WeightDelta))
	{
		UHeistDebugFunctionLibrary::DebugLootScoreWeightRejected(
			this,
			TEXT("InvalidLootValues"),
			ScoreDelta,
			WeightDelta);
		return false;
	}

	TotalLootScore += ScoreDelta;
	TotalLootWeight += WeightDelta;
	ForceNetUpdate();
	BroadcastLootTotalsChanged();

	if (AHeistPlayerCharacter* HeistPlayerCharacter = Cast<AHeistPlayerCharacter>(GetPawn()))
	{
		HeistPlayerCharacter->RefreshMovementSpeedFromWeight();
	}
	else
	{
		UHeistDebugFunctionLibrary::DebugWeightMovementSkipped(this, TEXT("MissingCharacter"));
	}

	UHeistDebugFunctionLibrary::DebugLootScoreWeightApplied(
		this,
		ScoreDelta,
		WeightDelta,
		TotalLootScore,
		TotalLootWeight);

	return true;
}

bool AHeistPlayerState::CanRemoveLootScoreAndWeight(const int32 ScoreDelta, const float WeightDelta) const
{
	return HasAuthority()
		&& !bEscaped
		&& ScoreDelta >= 0
		&& WeightDelta >= 0.0f
		&& FMath::IsFinite(WeightDelta)
		&& ScoreDelta <= TotalLootScore
		&& WeightDelta <= TotalLootWeight + KINDA_SMALL_NUMBER;
}

bool AHeistPlayerState::RemoveLootScoreAndWeight(const int32 ScoreDelta, const float WeightDelta)
{
	if (!CanRemoveLootScoreAndWeight(ScoreDelta, WeightDelta))
	{
		return false;
	}

	TotalLootScore -= ScoreDelta;
	TotalLootWeight = FMath::Max(0.0f, TotalLootWeight - WeightDelta);
	ForceNetUpdate();
	BroadcastLootTotalsChanged();

	if (AHeistPlayerCharacter* HeistPlayerCharacter = Cast<AHeistPlayerCharacter>(GetPawn()))
	{
		HeistPlayerCharacter->RefreshMovementSpeedFromWeight();
	}

	UHeistDebugFunctionLibrary::DebugLootScoreWeightRemoved(
		this,
		ScoreDelta,
		WeightDelta,
		TotalLootScore,
		TotalLootWeight);
	return true;
}

void AHeistPlayerState::BroadcastLootTotalsChanged()
{
	LootTotalsChangedDelegate.Broadcast(TotalLootScore, TotalLootWeight);
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
		UHeistDebugFunctionLibrary::DebugPlayerEscapeStateRejected(this, TEXT("NotAuthority"));
		return false;
	}

	if (bEscaped)
	{
		UHeistDebugFunctionLibrary::DebugPlayerEscapeStateRejected(this, TEXT("AlreadyEscaped"));
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

	UHeistDebugFunctionLibrary::DebugPlayerEscapeStateCommitted(
		this,
		HeistPlayerId,
		FinalScore,
		EscapeTimeSeconds);

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

	UHeistDebugFunctionLibrary::DebugPlayerEscapeStateReplicated(this, HeistPlayerId, bEscaped);
}

#pragma endregion

#pragma region Replication

void AHeistPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeistPlayerState, HeistPlayerId);
	DOREPLIFETIME(AHeistPlayerState, PlayerColor);
	DOREPLIFETIME_CONDITION(AHeistPlayerState, TotalLootScore, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AHeistPlayerState, TotalLootWeight, COND_OwnerOnly);
	DOREPLIFETIME(AHeistPlayerState, bEscaped);
	DOREPLIFETIME(AHeistPlayerState, FinalScore);
	DOREPLIFETIME(AHeistPlayerState, EscapeTimeSeconds);
}

void AHeistPlayerState::OnRep_TotalLootScore()
{
	BroadcastLootTotalsChanged();
	UHeistDebugFunctionLibrary::DebugPlayerStateScoreReplicated(this, TotalLootScore);
}

void AHeistPlayerState::OnRep_TotalLootWeight()
{
	BroadcastLootTotalsChanged();
	UHeistDebugFunctionLibrary::DebugPlayerStateWeightReplicated(this, TotalLootWeight);
}

#pragma endregion

#pragma region Debug

void AHeistPlayerState::DebugSetTotalLootScore(const int32 InScore)
{
#if !UE_BUILD_SHIPPING
	if (!HasAuthority())
	{
		return;
	}

	TotalLootScore = FMath::Max(0, InScore);
	ForceNetUpdate();
	BroadcastLootTotalsChanged();
#endif
}

void AHeistPlayerState::DebugSetResultState(
	const int32 InScore,
	const bool bInEscaped,
	const float InEscapeTimeSeconds)
{
#if !UE_BUILD_SHIPPING
	if (!HasAuthority())
	{
		return;
	}

	TotalLootScore = FMath::Max(0, InScore);
	FinalScore = TotalLootScore;
	bEscaped = bInEscaped;
	EscapeTimeSeconds = bEscaped ? FMath::Max(0.0f, InEscapeTimeSeconds) : -1.0f;
	ForceNetUpdate();
	BroadcastLootTotalsChanged();
	EscapeStateChangedDelegate.Broadcast(bEscaped);

	UHeistDebugFunctionLibrary::Message(
		this,
		FString::Printf(
			TEXT("Result debug state seeded: PlayerId=%d Escaped=%s FinalScore=%d EscapeTime=%.2f"),
			HeistPlayerId,
			bEscaped ? TEXT("true") : TEXT("false"),
			FinalScore,
			EscapeTimeSeconds));
#endif
}

#pragma endregion

#pragma region Verification

void AHeistPlayerState::InitializeVerificationIdentity(int32 InHeistPlayerId, const FLinearColor& InPlayerColor)
{
	check(HasAuthority());

	HeistPlayerId = InHeistPlayerId;
	PlayerColor = InPlayerColor;
	ForceNetUpdate();
	PlayerIdentityChangedDelegate.Broadcast(HeistPlayerId);
}

FHeistPlayerIdentityChanged& AHeistPlayerState::GetPlayerIdentityChangedDelegate()
{
	return PlayerIdentityChangedDelegate;
}

void AHeistPlayerState::OnRep_HeistPlayerId()
{
	PlayerIdentityChangedDelegate.Broadcast(HeistPlayerId);
}

#pragma endregion
