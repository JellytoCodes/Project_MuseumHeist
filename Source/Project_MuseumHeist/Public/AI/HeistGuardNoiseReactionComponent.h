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
	bool ReactToSoundPing(const FHeistSoundPingEvent& SoundPingEvent);

private:
	void HandleSoundPingReported(const FHeistSoundPingEvent& SoundPingEvent);
	void HandleGuardStateChanged(EHeistGuardState PreviousState, EHeistGuardState NewState);
	static int32 ResolveCandidatePriority(EHeistSoundPingType PingType);

	float InvestigateDuration = 0.0f;
	FHeistSoundPingEvent CurrentCandidate;
	float CurrentCandidateDistance = TNumericLimits<float>::Max();
	bool bHasCurrentCandidate = false;

#pragma endregion
};
