#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "HeistNoiseEmitterComponent.generated.h"

UCLASS(ClassGroup = (Heist), meta = (BlueprintSpawnableComponent))
class PROJECT_MUSEUMHEIST_API UHeistNoiseEmitterComponent : public UActorComponent
{
	GENERATED_BODY()

  public:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

  public:
	UHeistNoiseEmitterComponent();
	bool IsHeavyWeight(float TotalLootWeight) const;
	bool TryEmitVoiceNoise();
	float GetVoiceNoiseRadius() const { return VoiceNoiseRadius; }
	float GetVoiceNoiseDuration() const { return VoiceNoiseDuration; }
	float GetVoiceNoiseRefreshInterval() const { return VoiceNoiseRefreshInterval; }

  private:
	bool TryEmitFootstepNoise();
	float ResolveLootWeightBonus(float TotalLootWeight) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Noise", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm/s"))
	float MinimumFootstepSpeed = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Noise", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "kg"))
	float MediumWeightThreshold = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Noise", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "kg"))
	float HeavyWeightThreshold = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Noise", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float MediumWeightRadiusBonus = 250.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Noise", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float HeavyWeightRadiusBonus = 500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Noise|Voice", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float VoiceNoiseRadius = 800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Noise|Voice", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float VoiceNoiseDuration = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Noise|Voice", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float VoiceNoiseRefreshInterval = 0.8f;

	float LastFootstepServerTime = -1.0f;
	float LastVoiceServerTime = -1.0f;
};
