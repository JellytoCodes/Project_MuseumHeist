#include "Character/Components/HeistNoiseEmitterComponent.h"

#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistGameState.h"
#include "Core/HeistLogChannels.h"
#include "Core/HeistPlayerState.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Inventory/HeistItemDataTypes.h"

UHeistNoiseEmitterComponent::UHeistNoiseEmitterComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = 0.05f;
}

bool UHeistNoiseEmitterComponent::IsHeavyWeight(const float TotalLootWeight) const
{
	return FMath::IsFinite(TotalLootWeight) && TotalLootWeight >= FMath::Max(0.0f, HeavyWeightThreshold);
}

void UHeistNoiseEmitterComponent::BeginPlay()
{
	Super::BeginPlay();

	const AActor* OwnerActor = GetOwner();
	SetComponentTickEnabled(IsValid(OwnerActor) && OwnerActor->HasAuthority());
}

void UHeistNoiseEmitterComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TryEmitFootstepNoise();
}

bool UHeistNoiseEmitterComponent::TryEmitFootstepNoise()
{
	AHeistPlayerCharacter* Character = Cast<AHeistPlayerCharacter>(GetOwner());
	if (!IsValid(Character) || !Character->HasAuthority() || !Character->CanPerformGameplayActions())
	{
		return false;
	}

	const UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement();
	const float HorizontalSpeed = IsValid(MovementComponent) ? MovementComponent->Velocity.Size2D() : 0.0f;
	if (!IsValid(MovementComponent) || !MovementComponent->IsMovingOnGround() || HorizontalSpeed < MinimumFootstepSpeed)
	{
		return false;
	}

	AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const AHeistPlayerState* HeistPlayerState = Character->GetPlayerState<AHeistPlayerState>();
	if (!IsValid(HeistGameMode) || !IsValid(HeistGameState) || !IsValid(HeistPlayerState))
	{
		return false;
	}

	const float MaximumSpeed = FMath::Max(MinimumFootstepSpeed, MovementComponent->GetMaxSpeed());
	const bool bRunning = Character->IsSprinting();
	const FName SoundPingId = bRunning ? FName(TEXT("Ping_Footstep_Run")) : FName(TEXT("Ping_Footstep_Walk"));
	FHeistSoundPingDataRow SoundPingDefinition;
	if (!HeistGameMode->TryGetSoundPingDefinition(SoundPingId, SoundPingDefinition))
	{
		UHeistDebugFunctionLibrary::DebugSoundPingDefinitionRejected(this, SoundPingId, TEXT("MissingFootstepDefinition"));
		return false;
	}

	const float ServerTime = HeistGameState->GetServerWorldTimeSeconds();
	const float RefreshInterval = FMath::Max(0.0f, SoundPingDefinition.RefreshInterval);
	if (LastFootstepServerTime >= 0.0f && ServerTime - LastFootstepServerTime < RefreshInterval)
	{
		return false;
	}

	const float TotalLootWeight = FMath::Max(0.0f, HeistPlayerState->GetTotalLootWeight());
	const float WeightRadiusBonus = ResolveLootWeightBonus(TotalLootWeight);
	FHeistSoundPingEvent SoundPingEvent;
	SoundPingEvent.SoundPingTag = SoundPingDefinition.SoundPingTag;
	SoundPingEvent.PingType = SoundPingDefinition.PingType;
	SoundPingEvent.WorldLocation = Character->GetActorLocation();
	SoundPingEvent.Radius = FMath::Max(0.0f, SoundPingDefinition.Radius + WeightRadiusBonus);
	SoundPingEvent.Duration = FMath::Max(0.0f, SoundPingDefinition.Duration);
	SoundPingEvent.bAffectsGuards = SoundPingDefinition.bAffectsGuards;
	HeistGameState->ReportSoundPing(SoundPingEvent);
	LastFootstepServerTime = ServerTime;

#if !UE_BUILD_SHIPPING
	UE_LOG(LogHeist, Verbose,
		   TEXT("Footstep noise emitted: PlayerId=%d Mode=%s Speed=%.1f MaxSpeed=%.1f Weight=%.1f BaseRadius=%.1f WeightBonus=%.1f FinalRadius=%.1f RefreshInterval=%.2f Authority=true"),
		   HeistPlayerState->HeistPlayerId, bRunning ? TEXT("Run") : TEXT("Walk"), HorizontalSpeed, MaximumSpeed, TotalLootWeight, SoundPingDefinition.Radius, WeightRadiusBonus, SoundPingEvent.Radius,
		   RefreshInterval);
#endif
	return true;
}

float UHeistNoiseEmitterComponent::ResolveLootWeightBonus(const float TotalLootWeight) const
{
	if (TotalLootWeight >= FMath::Max(MediumWeightThreshold, HeavyWeightThreshold))
	{
		return FMath::Max(0.0f, HeavyWeightRadiusBonus);
	}

	if (TotalLootWeight >= FMath::Min(MediumWeightThreshold, HeavyWeightThreshold))
	{
		return FMath::Max(0.0f, MediumWeightRadiusBonus);
	}

	return 0.0f;
}
