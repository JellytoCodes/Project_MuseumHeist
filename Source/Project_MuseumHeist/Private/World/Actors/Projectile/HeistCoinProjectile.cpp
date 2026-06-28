#include "World/Actors/Projectile/HeistCoinProjectile.h"

#include "Character/Components/HeistStatusComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistGameState.h"
#include "Core/HeistGameplayTags.h"
#include "Core/HeistTypes.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "GameFramework/DamageType.h"
#include "Inventory/HeistItemDataTypes.h"
#include "Kismet/GameplayStatics.h"

#pragma region Construction

AHeistCoinProjectile::AHeistCoinProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
}

#pragma endregion

#pragma region Projectile

void AHeistCoinProjectile::HandleAuthorityImpact(const FHitResult& Hit)
{
	AHeistPlayerCharacter* HitCharacter = Cast<AHeistPlayerCharacter>(Hit.GetActor());
	if (IsValid(HitCharacter) && HitCharacter != GetThrowerCharacter())
	{
		UGameplayStatics::ApplyDamage(
			HitCharacter,
			1.0f,
			GetInstigatorController(),
			this,
			UDamageType::StaticClass());
		UHeistDebugFunctionLibrary::DebugCoinProjectileDamageApplied(this, this, HitCharacter, 1.0f);

		UHeistStatusComponent* StatusComponent = HitCharacter->GetStatusComponent();
		checkf(IsValid(StatusComponent), TEXT("HeistPlayerCharacter requires HeistStatusComponent"));
		const float StunDurationSeconds = FMath::Max(0.0f, GetEffectDurationSeconds());
		if (StatusComponent->ApplyStun(StunDurationSeconds, this))
		{
			UHeistDebugFunctionLibrary::DebugCoinProjectileStunApplied(
				this,
				this,
				HitCharacter,
				StunDurationSeconds);
		}
		else
		{
			UHeistDebugFunctionLibrary::DebugCoinProjectileStunRejected(this, this, HitCharacter, TEXT("StatusRejected"));
		}
	}
	else if (AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr)
	{
		const FName SoundPingId(TEXT("Ping_CoinImpact"));
		const AHeistGameMode* HeistGameMode =
			GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
		FHeistSoundPingDataRow SoundPingDefinition;
		if (!IsValid(HeistGameMode)
			|| !HeistGameMode->TryGetSoundPingDefinition(
				SoundPingId,
				SoundPingDefinition))
		{
			UHeistDebugFunctionLibrary::DebugSoundPingDefinitionRejected(
				this,
				SoundPingId,
				TEXT("MissingSoundPingDataRow"));
			Super::HandleAuthorityImpact(Hit);
			return;
		}

		FHeistSoundPingEvent SoundPingEvent;
		SoundPingEvent.SoundPingTag = SoundPingDefinition.SoundPingTag;
		SoundPingEvent.PingType = SoundPingDefinition.PingType;
		SoundPingEvent.WorldLocation = Hit.ImpactPoint;
		SoundPingEvent.Radius = FMath::Max(0.0f, SoundPingDefinition.Radius);
		SoundPingEvent.Duration = FMath::Max(0.0f, SoundPingDefinition.Duration);
		SoundPingEvent.bAffectsGuards = SoundPingDefinition.bAffectsGuards;
		SoundPingEvent.bAffectsPlayers = SoundPingDefinition.bAffectsPlayers;
		HeistGameState->ReportSoundPing(SoundPingEvent);
	}

	Super::HandleAuthorityImpact(Hit);
}

#pragma endregion
