#include "Core/HeistPlayerState.h"

#include "Character/HeistPlayerCharacter.h"
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
	if (!HasAuthority() || ScoreDelta < 0 || WeightDelta < 0.0f || !FMath::IsFinite(WeightDelta))
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

#pragma region Replication

void AHeistPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeistPlayerState, HeistPlayerId);
	DOREPLIFETIME(AHeistPlayerState, PlayerColor);
	DOREPLIFETIME(AHeistPlayerState, TotalLootScore);
	DOREPLIFETIME(AHeistPlayerState, TotalLootWeight);
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
