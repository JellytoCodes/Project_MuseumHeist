#if WITH_DEV_AUTOMATION_TESTS

#include "Character/HeistPlayerCharacter.h"
#include "Components/AudioComponent.h"
#include "Misc/AutomationTest.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistWeek7StatusEffectSlotsTest, "ProjectMuseumHeist.W7.StatusEffectSlots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistWeek7StatusEffectSlotsTest::RunTest(const FString& Parameters)
{
	struct FSlotExpectation
	{
		const TCHAR* PropertyName;
		UClass* ExpectedClass;
	};

	const FSlotExpectation SlotExpectations[] = {
		{TEXT("ForgingStatusVFX"), UNiagaraSystem::StaticClass()},
		{TEXT("AssemblingStatusVFX"), UNiagaraSystem::StaticClass()},
		{TEXT("CarryingOriginalStatusVFX"), UNiagaraSystem::StaticClass()},
		{TEXT("HeavyStatusVFX"), UNiagaraSystem::StaticClass()},
		{TEXT("StunnedStatusVFX"), UNiagaraSystem::StaticClass()},
		{TEXT("ArrestedStatusVFX"), UNiagaraSystem::StaticClass()},
		{TEXT("EscapedStatusBurstVFX"), UNiagaraSystem::StaticClass()},
		{TEXT("ForgingStatusSound"), USoundBase::StaticClass()},
		{TEXT("AssemblingStatusSound"), USoundBase::StaticClass()},
		{TEXT("CarryingOriginalStatusSound"), USoundBase::StaticClass()},
		{TEXT("HeavyStatusSound"), USoundBase::StaticClass()},
		{TEXT("StunnedStatusSound"), USoundBase::StaticClass()},
		{TEXT("ArrestedStatusSound"), USoundBase::StaticClass()},
		{TEXT("EscapedStatusSound"), USoundBase::StaticClass()},
	};

	for (const FSlotExpectation& Expectation : SlotExpectations)
	{
		const FString PropertyLabel(Expectation.PropertyName);
		const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(AHeistPlayerCharacter::StaticClass(), Expectation.PropertyName);
		TestNotNull(FString::Printf(TEXT("%s asset slot exists"), *PropertyLabel), Property);
		if (!Property)
		{
			continue;
		}

		TestTrue(FString::Printf(TEXT("%s accepts the expected asset class"), *PropertyLabel), Property->PropertyClass->IsChildOf(Expectation.ExpectedClass));
		TestTrue(FString::Printf(TEXT("%s is editable in Blueprint Class Defaults"), *PropertyLabel), Property->HasAnyPropertyFlags(CPF_Edit));
		TestTrue(FString::Printf(TEXT("%s is defaults-only"), *PropertyLabel), Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance));
		TestTrue(FString::Printf(TEXT("%s is Blueprint-visible"), *PropertyLabel), Property->HasAnyPropertyFlags(CPF_BlueprintVisible));
		TestTrue(FString::Printf(TEXT("%s is Blueprint-read-only"), *PropertyLabel), Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly));
		TestFalse(FString::Printf(TEXT("%s is not replicated gameplay state"), *PropertyLabel), Property->HasAnyPropertyFlags(CPF_Net));
	}

	const AHeistPlayerCharacter* NativeCharacterCDO = GetDefault<AHeistPlayerCharacter>();
	TestNotNull(TEXT("Native player character CDO exists"), NativeCharacterCDO);
	if (NativeCharacterCDO)
	{
		TestTrue(TEXT("Native player character owns reusable status effect components"), NativeCharacterCDO->AreCrewStatusEffectComponentsReadyForDebug());
		TestTrue(TEXT("Native player character starts with clean status effect presentation"), NativeCharacterCDO->IsCrewStatusEffectPresentationCleanForDebug());
	}

	const FObjectPropertyBase* VFXComponentProperty = FindFProperty<FObjectPropertyBase>(AHeistPlayerCharacter::StaticClass(), TEXT("CrewStatusVFXComponent"));
	const FObjectPropertyBase* AudioComponentProperty = FindFProperty<FObjectPropertyBase>(AHeistPlayerCharacter::StaticClass(), TEXT("CrewStatusTransitionAudioComponent"));
	TestNotNull(TEXT("Reusable CrewStatus VFX component property exists"), VFXComponentProperty);
	TestNotNull(TEXT("Reusable CrewStatus transition audio component property exists"), AudioComponentProperty);
	if (VFXComponentProperty)
	{
		TestTrue(TEXT("CrewStatus VFX component uses Niagara"), VFXComponentProperty->PropertyClass->IsChildOf(UNiagaraComponent::StaticClass()));
		TestFalse(TEXT("CrewStatus VFX component pointer is not replicated"), VFXComponentProperty->HasAnyPropertyFlags(CPF_Net));
	}
	if (AudioComponentProperty)
	{
		TestTrue(TEXT("CrewStatus transition audio component uses UAudioComponent"), AudioComponentProperty->PropertyClass->IsChildOf(UAudioComponent::StaticClass()));
		TestFalse(TEXT("CrewStatus transition audio component pointer is not replicated"), AudioComponentProperty->HasAnyPropertyFlags(CPF_Net));
	}

	if (NativeCharacterCDO && VFXComponentProperty && AudioComponentProperty)
	{
		const UNiagaraComponent* VFXComponent = Cast<UNiagaraComponent>(VFXComponentProperty->GetObjectPropertyValue_InContainer(NativeCharacterCDO));
		const UAudioComponent* AudioComponent = Cast<UAudioComponent>(AudioComponentProperty->GetObjectPropertyValue_InContainer(NativeCharacterCDO));
		TestNotNull(TEXT("Native CDO owns CrewStatusVFXComponent"), VFXComponent);
		TestNotNull(TEXT("Native CDO owns CrewStatusTransitionAudioComponent"), AudioComponent);
		if (VFXComponent)
		{
			TestFalse(TEXT("CrewStatus VFX does not auto-activate"), VFXComponent->bAutoActivate != 0);
			TestFalse(TEXT("CrewStatus VFX component does not replicate independently"), VFXComponent->GetIsReplicated());
		}
		if (AudioComponent)
		{
			TestFalse(TEXT("CrewStatus transition audio does not auto-activate"), AudioComponent->bAutoActivate != 0);
			TestFalse(TEXT("CrewStatus transition audio component does not replicate independently"), AudioComponent->GetIsReplicated());
		}
	}

	UClass* BlueprintCharacterClass = LoadClass<AHeistPlayerCharacter>(nullptr, TEXT("/Game/Blueprints/Player/BP_HeistPlayerCharacter.BP_HeistPlayerCharacter_C"));
	TestNotNull(TEXT("BP_HeistPlayerCharacter loads with native status effect slots"), BlueprintCharacterClass);
	if (BlueprintCharacterClass)
	{
		const AHeistPlayerCharacter* BlueprintCharacterCDO = Cast<AHeistPlayerCharacter>(BlueprintCharacterClass->GetDefaultObject());
		TestNotNull(TEXT("BP_HeistPlayerCharacter CDO exists"), BlueprintCharacterCDO);
		if (BlueprintCharacterCDO)
		{
			TestTrue(TEXT("BP_HeistPlayerCharacter inherits reusable status effect components"), BlueprintCharacterCDO->AreCrewStatusEffectComponentsReadyForDebug());
		}
	}

	return true;
}

#endif
