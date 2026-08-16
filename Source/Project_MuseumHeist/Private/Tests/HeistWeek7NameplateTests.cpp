#if WITH_DEV_AUTOMATION_TESTS

#include "Character/HeistPlayerCharacter.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Core/HeistPlayerState.h"
#include "Core/HeistTypes.h"
#include "Misc/AutomationTest.h"
#include "UI/Widgets/HeistNameplateWidget.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistWeek7NameplatePresentationTest, "ProjectMuseumHeist.W7.NameplatePresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistWeek7NameplatePresentationTest::RunTest(const FString& Parameters)
{
	const EHeistCrewStatus Statuses[] = {
		EHeistCrewStatus::Active,
		EHeistCrewStatus::Forging,
		EHeistCrewStatus::Assembling,
		EHeistCrewStatus::CarryingOriginal,
		EHeistCrewStatus::Heavy,
		EHeistCrewStatus::Stunned,
		EHeistCrewStatus::Arrested,
		EHeistCrewStatus::Escaped,
	};

	TSet<FString> UniqueGlyphs;
	TSet<uint32> UniqueColors;
	for (const EHeistCrewStatus Status : Statuses)
	{
		const FString Glyph = HeistCrewStatus::ToIconGlyph(Status).ToString();
		const FLinearColor Color = HeistCrewStatus::GetPresentationColor(Status);
		TestEqual(FString::Printf(TEXT("%s uses one Korean glyph"), *UEnum::GetValueAsString(Status)), Glyph.Len(), 1);
		TestTrue(FString::Printf(TEXT("%s presentation color is finite"), *UEnum::GetValueAsString(Status)),
			FMath::IsFinite(Color.R) && FMath::IsFinite(Color.G) && FMath::IsFinite(Color.B) && FMath::IsFinite(Color.A));
		TestTrue(FString::Printf(TEXT("%s presentation color is opaque"), *UEnum::GetValueAsString(Status)), FMath::IsNearlyEqual(Color.A, 1.0f));
		UniqueGlyphs.Add(Glyph);
		UniqueColors.Add(Color.ToFColor(true).ToPackedARGB());
	}
	TestEqual(TEXT("All eight CrewStatus glyphs are distinct"), UniqueGlyphs.Num(), static_cast<int32>(UE_ARRAY_COUNT(Statuses)));
	TestEqual(TEXT("All eight CrewStatus colors are distinct"), UniqueColors.Num(), static_cast<int32>(UE_ARRAY_COUNT(Statuses)));

	const FProperty* PlayerColorProperty = FindFProperty<FProperty>(AHeistPlayerState::StaticClass(), TEXT("PlayerColor"));
	const FProperty* CrewStatusProperty = FindFProperty<FProperty>(AHeistPlayerState::StaticClass(), TEXT("CrewStatus"));
	TestNotNull(TEXT("PlayerColor property exists"), PlayerColorProperty);
	TestNotNull(TEXT("CrewStatus property exists"), CrewStatusProperty);
	if (PlayerColorProperty)
	{
		TestTrue(TEXT("PlayerColor is replicated"), PlayerColorProperty->HasAnyPropertyFlags(CPF_Net));
		TestEqual(TEXT("PlayerColor uses its presentation refresh RepNotify"), PlayerColorProperty->RepNotifyFunc, FName(TEXT("OnRep_PlayerColor")));
	}
	if (CrewStatusProperty)
	{
		TestTrue(TEXT("CrewStatus is replicated"), CrewStatusProperty->HasAnyPropertyFlags(CPF_Net));
		TestEqual(TEXT("CrewStatus uses its presentation refresh RepNotify"), CrewStatusProperty->RepNotifyFunc, FName(TEXT("OnRep_CrewStatus")));
	}

	const FObjectPropertyBase* BadgeProperty = FindFProperty<FObjectPropertyBase>(UHeistNameplateWidget::StaticClass(), TEXT("CrewStatusBadge"));
	const FObjectPropertyBase* IconTextProperty = FindFProperty<FObjectPropertyBase>(UHeistNameplateWidget::StaticClass(), TEXT("CrewStatusIconText"));
	TestNotNull(TEXT("Nameplate exposes optional CrewStatusBadge WBP binding"), BadgeProperty);
	TestNotNull(TEXT("Nameplate exposes optional CrewStatusIconText WBP binding"), IconTextProperty);
	if (BadgeProperty)
	{
		TestTrue(TEXT("CrewStatusBadge binding requires UBorder"), BadgeProperty->PropertyClass->IsChildOf(UBorder::StaticClass()));
#if WITH_METADATA
		TestTrue(TEXT("CrewStatusBadge is optional for native fallback"), BadgeProperty->HasMetaData(TEXT("BindWidgetOptional")));
#endif
	}
	if (IconTextProperty)
	{
		TestTrue(TEXT("CrewStatusIconText binding requires UTextBlock"), IconTextProperty->PropertyClass->IsChildOf(UTextBlock::StaticClass()));
#if WITH_METADATA
		TestTrue(TEXT("CrewStatusIconText is optional for native fallback"), IconTextProperty->HasMetaData(TEXT("BindWidgetOptional")));
#endif
	}

	const AHeistPlayerCharacter* CharacterCDO = GetDefault<AHeistPlayerCharacter>();
	TestNotNull(TEXT("Player character CDO exists"), CharacterCDO);
	UWidgetComponent* NameplateComponent = nullptr;
	if (CharacterCDO)
	{
		TInlineComponentArray<UWidgetComponent*> WidgetComponents(const_cast<AHeistPlayerCharacter*>(CharacterCDO));
		for (UWidgetComponent* Candidate : WidgetComponents)
		{
			if (IsValid(Candidate) && Candidate->GetWidgetClass() && Candidate->GetWidgetClass()->IsChildOf(UHeistNameplateWidget::StaticClass()))
			{
				NameplateComponent = Candidate;
				break;
			}
		}
	}
	TestNotNull(TEXT("Player character binds a Heist Nameplate widget component"), NameplateComponent);
	if (NameplateComponent)
	{
		TestEqual(TEXT("Nameplate is rendered in screen space"), NameplateComponent->GetWidgetSpace(), EWidgetSpace::Screen);
		TestTrue(TEXT("Nameplate has OwnerNoSee as a second self-hide guard"), NameplateComponent->bOwnerNoSee != 0);
	}
	TestFalse(TEXT("Locally controlled character does not display its Nameplate"), UHeistNameplateWidget::ShouldDisplayForLocalControl(true));
	TestTrue(TEXT("Remote character displays its Nameplate"), UHeistNameplateWidget::ShouldDisplayForLocalControl(false));

	const UHeistNameplateWidget* NameplateCDO = GetDefault<UHeistNameplateWidget>();
	TestNotNull(TEXT("Nameplate widget CDO exists"), NameplateCDO);
	if (NameplateCDO)
	{
		const float NearOpacity = NameplateCDO->CalculateDistanceOpacity(1000.0f);
		const float MidFadeOpacity = NameplateCDO->CalculateDistanceOpacity(2250.0f);
		const float FarOpacity = NameplateCDO->CalculateDistanceOpacity(2500.0f);
		TestTrue(TEXT("Near Nameplate remains fully opaque"), FMath::IsNearlyEqual(NearOpacity, 1.0f));
		TestTrue(TEXT("Nameplate is half opaque at the middle of its 500 cm fade band"), FMath::IsNearlyEqual(MidFadeOpacity, 0.5f));
		TestTrue(TEXT("Nameplate is hidden at its 2500 cm maximum distance"), FMath::IsNearlyZero(FarOpacity));
		TestTrue(TEXT("Nameplate remains hidden beyond maximum distance"), FMath::IsNearlyZero(NameplateCDO->CalculateDistanceOpacity(3000.0f)));
	}

	return true;
}

#endif
