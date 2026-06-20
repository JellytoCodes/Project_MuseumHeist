#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"

#include "HeistPlayerState.generated.h"

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistPlayerState : public APlayerState
{
	GENERATED_BODY()

#pragma region ScoreAndWeight

public:
	int32 GetTotalLootScore() const;
	float GetTotalLootWeight() const;
	bool CanAddLootScoreAndWeight(int32 ScoreDelta, float WeightDelta) const;
	bool AddLootScoreAndWeight(int32 ScoreDelta, float WeightDelta);

private:
	UPROPERTY(ReplicatedUsing = OnRep_TotalLootScore, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Score", meta = (AllowPrivateAccess = "true"))
	int32 TotalLootScore = 0;

	UPROPERTY(ReplicatedUsing = OnRep_TotalLootWeight, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Weight", meta = (AllowPrivateAccess = "true"))
	float TotalLootWeight = 0.0f;

#pragma endregion

#pragma region Replication

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_TotalLootScore();

	UFUNCTION()
	void OnRep_TotalLootWeight();

#pragma endregion

#pragma region Verification

public:
	void InitializeVerificationIdentity(int32 InHeistPlayerId, const FLinearColor& InPlayerColor);

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Verification")
	int32 HeistPlayerId = INDEX_NONE;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Verification")
	FLinearColor PlayerColor = FLinearColor::White;

#pragma endregion
};
