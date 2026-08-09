#include "World/Actors/Projectile/HeistCoinProjectile.h"

#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistGameState.h"
#include "Core/HeistGameplayTags.h"
#include "Core/HeistPlayerState.h"
#include "Core/HeistTypes.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Inventory/HeistItemDataTypes.h"

#pragma region Construction

AHeistCoinProjectile::AHeistCoinProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
}

#pragma endregion

#pragma region Projectile

void AHeistCoinProjectile::HandleAuthorityImpact(const FHitResult& Hit)
{
	const FName SoundPingId(TEXT("Ping_CoinImpact"));
	AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistGameState))
	{
		UHeistDebugFunctionLibrary::DebugSoundPingDefinitionRejected(this, SoundPingId, TEXT("MissingGameState"));
		Super::HandleAuthorityImpact(Hit);
		return;
	}

	const AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	FHeistSoundPingDataRow SoundPingDefinition;
	if (!IsValid(HeistGameMode) || !HeistGameMode->TryGetSoundPingDefinition(SoundPingId, SoundPingDefinition))
	{
		UHeistDebugFunctionLibrary::DebugSoundPingDefinitionRejected(this, SoundPingId, TEXT("MissingSoundPingDataRow"));
		Super::HandleAuthorityImpact(Hit);
		return;
	}

	const FHeistGameplayTags& GameplayTags = FHeistGameplayTags::Get();
	if (SoundPingDefinition.PingType != EHeistSoundPingType::CoinImpact || SoundPingDefinition.SoundPingTag != GameplayTags.Event_SoundPing_CoinImpact || !SoundPingDefinition.bAffectsGuards ||
		SoundPingDefinition.Radius <= 0.0f)
	{
		UHeistDebugFunctionLibrary::DebugSoundPingDefinitionRejected(this, SoundPingId, TEXT("InvalidGuardDistractionDefinition"));
		Super::HandleAuthorityImpact(Hit);
		return;
	}

	FHeistSoundPingEvent SoundPingEvent;
	SoundPingEvent.SoundPingTag = SoundPingDefinition.SoundPingTag;
	SoundPingEvent.PingType = SoundPingDefinition.PingType;
	SoundPingEvent.WorldLocation = Hit.ImpactPoint;
	SoundPingEvent.Radius = SoundPingDefinition.Radius;
	SoundPingEvent.Duration = FMath::Max(0.0f, SoundPingDefinition.Duration);
	SoundPingEvent.bAffectsGuards = true;
	const int32 DistractedGuardCount = HeistGameState->ReportSoundPing(SoundPingEvent);
	AHeistPlayerCharacter* Thrower = GetThrowerCharacter();
	if (DistractedGuardCount > 0 && IsValid(Thrower))
	{
		if (AHeistPlayerState* ThrowerPlayerState = Thrower->GetPlayerState<AHeistPlayerState>())
		{
			ThrowerPlayerState->RecordGuardDistractionContribution(DistractedGuardCount);
		}
	}

	Super::HandleAuthorityImpact(Hit);
}

#pragma endregion
