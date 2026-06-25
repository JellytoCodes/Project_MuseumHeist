#include "World/HeistCoinProjectile.h"

#include "Character/Components/HeistStatusComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameState.h"
#include "Core/HeistGameplayTags.h"
#include "Core/HeistTypes.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "GameFramework/DamageType.h"
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
		FHeistSoundPingEvent SoundPingEvent;
		SoundPingEvent.SoundPingTag = FHeistGameplayTags::Get().Event_SoundPing_CoinImpact;
		SoundPingEvent.PingType = EHeistSoundPingType::CoinImpact;
		SoundPingEvent.WorldLocation = Hit.ImpactPoint;
		HeistGameState->ReportSoundPing(SoundPingEvent);
	}

	Super::HandleAuthorityImpact(Hit);
}

#pragma endregion
