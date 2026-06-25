#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/HeistTypes.h"

#include "HeistStatusComponent.generated.h"

UCLASS(ClassGroup = (Heist), meta = (BlueprintSpawnableComponent))
class PROJECT_MUSEUMHEIST_API UHeistStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeistStatusComponent();

#pragma region StatusQueries

public:
	bool IsStunned() const;
	bool IsStunImmune() const;
	bool HasStatusTag(FGameplayTag StateTag) const;
	const TArray<FHeistTimedTagState>& GetStatusTags() const;

#pragma endregion

#pragma region StatusMutation

public:
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Heist|Status", meta = (ClampMin = "0.0", Units = "s"))
	bool ApplyStun(float DurationSeconds, AActor* StunSource = nullptr);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Heist|Status", meta = (ClampMin = "0.0", Units = "s"))
	bool ApplyStunImmunity(float DurationSeconds);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Heist|Status", meta = (ClampMin = "0.0", Units = "s"))
	bool ApplyTimedStatusTag(FGameplayTag StateTag, float DurationSeconds);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Heist|Status")
	bool ClearStatusTag(FGameplayTag StateTag);

private:
	FHeistTimedTagState* FindMutableStatusTag(FGameplayTag StateTag);
	const FHeistTimedTagState* FindStatusTag(FGameplayTag StateTag) const;
	void RefreshStatusTagTimer(const FHeistTimedTagState& StatusTagState);
	void ClearStatusTagTimer(FGameplayTag StateTag);
	void ExpireStatusTag(FGameplayTag StateTag);
	void StopOwnerMovementForStun() const;
	float GetStatusEndServerTime(float DurationSeconds) const;

	UPROPERTY(ReplicatedUsing = OnRep_StatusTags, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Status", meta = (AllowPrivateAccess = "true"))
	TArray<FHeistTimedTagState> StatusTags;

	TMap<FGameplayTag, FTimerHandle> StatusTagTimers;

#pragma endregion

#pragma region Replication

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_StatusTags();

#pragma endregion
};
