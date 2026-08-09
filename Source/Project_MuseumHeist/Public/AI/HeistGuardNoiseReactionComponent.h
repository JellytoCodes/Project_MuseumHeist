#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/HeistTypes.h"

#include "HeistGuardNoiseReactionComponent.generated.h"

struct FHeistGuardDataRow;

UCLASS(ClassGroup = (Heist), meta = (BlueprintSpawnableComponent))
class PROJECT_MUSEUMHEIST_API UHeistGuardNoiseReactionComponent : public UActorComponent
{
	GENERATED_BODY()

#pragma region Construction

  public:
	UHeistGuardNoiseReactionComponent();

#pragma endregion

#pragma region Lifecycle

  protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#pragma endregion

#pragma region NoiseReaction

  public:
	void ConfigureGuardProfile(const FHeistGuardDataRow& GuardData);
	void SetAlertNoiseRadiusMultiplier(float Multiplier);
	float GetAlertNoiseRadiusMultiplier() const;
	float GetPerceptionRangeMultiplier() const;
	bool ReactToSoundPing(const FHeistSoundPingEvent& SoundPingEvent);

  private:
	void HandleSoundPingReported(const FHeistSoundPingEvent& SoundPingEvent, int32* InOutAcceptedGuardCount);
	void HandleGuardStateChanged(EHeistGuardState PreviousState, EHeistGuardState NewState);
	static int32 ResolveCandidatePriority(EHeistSoundPingType PingType);

	float InvestigateDuration = 0.0f;
	float AlertNoiseRadiusMultiplier = 1.0f;
	float PerceptionRangeMultiplier = 1.0f;
	FHeistSoundPingEvent CurrentCandidate;
	bool bHasCurrentCandidate = false;

#pragma endregion
};
