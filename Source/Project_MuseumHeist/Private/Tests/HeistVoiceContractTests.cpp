#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "AI/HeistGuardNoiseReactionComponent.h"
#include "Character/Components/HeistNoiseEmitterComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameplayTags.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistVoiceStaticContractTest, "Heist.Voice.StaticContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistVoiceStaticContractTest::RunTest(const FString& Parameters)
{
	const UHeistNoiseEmitterComponent* NoiseDefaults = GetDefault<UHeistNoiseEmitterComponent>();
	const AHeistPlayerCharacter* CharacterDefaults = GetDefault<AHeistPlayerCharacter>();
	TestNotNull(TEXT("Noise emitter defaults exist"), NoiseDefaults);
	TestNotNull(TEXT("Player character defaults exist"), CharacterDefaults);
	if (!IsValid(NoiseDefaults) || !IsValid(CharacterDefaults))
	{
		return false;
	}

	TestTrue(TEXT("Voice guard radius is 800 cm"), FMath::IsNearlyEqual(NoiseDefaults->GetVoiceNoiseRadius(), 800.0f));
	TestTrue(TEXT("Voice guard duration is 0.8 seconds"), FMath::IsNearlyEqual(NoiseDefaults->GetVoiceNoiseDuration(), 0.8f));
	TestTrue(TEXT("Voice guard refresh is 0.8 seconds"), FMath::IsNearlyEqual(NoiseDefaults->GetVoiceNoiseRefreshInterval(), 0.8f));
	TestTrue(TEXT("Voice full-volume radius is 250 cm"), FMath::IsNearlyEqual(CharacterDefaults->GetVoiceFullVolumeRadius(), 250.0f));
	TestTrue(TEXT("Voice falloff is 1250 cm"), FMath::IsNearlyEqual(CharacterDefaults->GetVoiceFalloffDistance(), 1250.0f));
	TestTrue(TEXT("Voice becomes inaudible at 1500 cm"),
		FMath::IsNearlyEqual(CharacterDefaults->GetVoiceFullVolumeRadius() + CharacterDefaults->GetVoiceFalloffDistance(), 1500.0f));

	TestEqual(TEXT("Voice priority is above footstep"), UHeistGuardNoiseReactionComponent::ResolveCandidatePriority(EHeistSoundPingType::Voice), 3);
	TestEqual(TEXT("Footstep priority remains lowest"), UHeistGuardNoiseReactionComponent::ResolveCandidatePriority(EHeistSoundPingType::Footstep), 4);
	TestEqual(TEXT("Coin priority remains above voice"), UHeistGuardNoiseReactionComponent::ResolveCandidatePriority(EHeistSoundPingType::CoinImpact), 2);
	TestTrue(TEXT("Voice gameplay tag is registered"), FHeistGameplayTags::Get().Event_SoundPing_Voice.IsValid());
	TestEqual(TEXT("Voice gameplay tag name"), FHeistGameplayTags::Get().Event_SoundPing_Voice.ToString(), FString(TEXT("Event.SoundPing.Voice")));

	return true;
}

#endif
