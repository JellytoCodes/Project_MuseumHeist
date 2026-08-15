#if WITH_DEV_AUTOMATION_TESTS

#include "Data/HeistGameBalanceDataAsset.h"
#include "Data/HeistMapPresentationDataTypes.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"
#include "UI/Widgets/HeistFloorPlanMapWidget.h"

namespace
{
FHeistMapPresentationRow MakeValidFloorPlanRow()
{
	FHeistMapPresentationRow Row;
	Row.MapId = TEXT("M01");
	Row.MapDisplayName = FText::FromString(TEXT("Test Museum"));
	Row.FloorPlanTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Test/M01/T_FloorPlan_M01.T_FloorPlan_M01")));
	Row.WorldMin = FVector2D(-100.0, -200.0);
	Row.WorldMax = FVector2D(300.0, 600.0);
	Row.MapNorthAxis = EHeistMapNorthAxis::PositiveY;

	FHeistMapZoneAnchor Zone;
	Zone.ZoneId = TEXT("Zone_Gallery");
	Zone.DisplayName = FText::FromString(TEXT("Gallery"));
	Zone.WorldLocation = FVector2D::ZeroVector;
	Row.ZoneAnchors.Add(Zone);
	Row.ContractTargetGalleryZoneId = Zone.ZoneId;

	FHeistMapExitAnchor Exit;
	Exit.ExitId = TEXT("Exit_Main");
	Exit.DisplayName = FText::FromString(TEXT("Exit"));
	Exit.WorldLocation = FVector2D(100.0, 500.0);
	Row.DefaultExitAnchors.Add(Exit);
	return Row;
}

bool IsNearlyEqualUV(const FVector2D& Actual, const FVector2D& Expected)
{
	return Actual.Equals(Expected, UE_KINDA_SMALL_NUMBER);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistWeek7FloorPlanDataTest, "ProjectMuseumHeist.W7.FloorPlan.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistWeek7FloorPlanDataTest::RunTest(const FString& Parameters)
{
	FHeistMapPresentationRow ValidRow = MakeValidFloorPlanRow();
	FString FailureReason;
	TestTrue(TEXT("A complete map presentation definition is valid"), ValidRow.IsRuntimeDefinitionValid(&FailureReason));
	TestTrue(TEXT("A valid definition has no failure reason"), FailureReason.IsEmpty());

	FHeistMapPresentationRow InvalidBounds = ValidRow;
	InvalidBounds.WorldMax = InvalidBounds.WorldMin;
	TestFalse(TEXT("Runtime bounds inference is not accepted"), InvalidBounds.IsRuntimeDefinitionValid(&FailureReason));
	TestEqual(TEXT("Invalid explicit bounds report a stable reason"), FailureReason, FString(TEXT("InvalidWorldBounds")));

	FHeistMapPresentationRow MissingGallery = ValidRow;
	MissingGallery.ContractTargetGalleryZoneId = TEXT("Zone_Missing");
	TestFalse(TEXT("A target-gallery anchor is required"), MissingGallery.IsRuntimeDefinitionValid(&FailureReason));
	TestEqual(TEXT("Missing target-gallery anchor reports a stable reason"), FailureReason, FString(TEXT("TargetGalleryZoneNotFound")));

	const UHeistGameBalanceDataAsset* BalanceData = GetDefault<UHeistGameBalanceDataAsset>();
	UDataTable* MapTable = IsValid(BalanceData) ? BalanceData->MapPresentationDataTable.LoadSynchronous() : nullptr;
	TestNotNull(TEXT("DT_MapPresentation loads from the balance-data contract"), MapTable);
	TestTrue(TEXT("DT_MapPresentation uses FHeistMapPresentationRow"), IsValid(MapTable) && MapTable->GetRowStruct() == FHeistMapPresentationRow::StaticStruct());
	if (!IsValid(MapTable) || MapTable->GetRowStruct() != FHeistMapPresentationRow::StaticStruct())
	{
		return false;
	}

	for (const FName MapId : {FName(TEXT("M01")), FName(TEXT("M02")), FName(TEXT("M03"))})
	{
		const FHeistMapPresentationRow* Row = MapTable->FindRow<FHeistMapPresentationRow>(MapId, TEXT("FHeistWeek7FloorPlanDataTest"), false);
		TestNotNull(*FString::Printf(TEXT("%s has a presentation row"), *MapId.ToString()), Row);
		if (Row != nullptr)
		{
			FString RowFailureReason;
			TestEqual(*FString::Printf(TEXT("%s row MapId matches its key"), *MapId.ToString()), Row->MapId, MapId);
			TestTrue(*FString::Printf(TEXT("%s presentation definition is valid"), *MapId.ToString()), Row->IsRuntimeDefinitionValid(&RowFailureReason));
			TestNotNull(*FString::Printf(TEXT("%s floor-plan texture loads"), *MapId.ToString()), Row->FloorPlanTexture.LoadSynchronous());
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistWeek7FloorPlanProjectionTest, "ProjectMuseumHeist.W7.FloorPlan.Projection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistWeek7FloorPlanProjectionTest::RunTest(const FString& Parameters)
{
	FHeistMapPresentationRow Row = MakeValidFloorPlanRow();
	TestTrue(TEXT("Positive-Y north maps world minimum to lower-left"),
		IsNearlyEqualUV(Row.ProjectWorldLocationToMapUV(Row.WorldMin), FVector2D(0.0, 1.0)));
	TestTrue(TEXT("Positive-Y north maps world maximum to upper-right"),
		IsNearlyEqualUV(Row.ProjectWorldLocationToMapUV(Row.WorldMax), FVector2D(1.0, 0.0)));
	TestTrue(TEXT("Projection center remains centered"),
		IsNearlyEqualUV(Row.ProjectWorldLocationToMapUV((Row.WorldMin + Row.WorldMax) * 0.5), FVector2D(0.5, 0.5)));

	const FVector2D QuarterWorld(Row.WorldMin.X + (Row.WorldMax.X - Row.WorldMin.X) * 0.25,
		Row.WorldMin.Y + (Row.WorldMax.Y - Row.WorldMin.Y) * 0.75);
	Row.MapNorthAxis = EHeistMapNorthAxis::PositiveY;
	TestTrue(TEXT("Positive-Y orientation is stable"), IsNearlyEqualUV(Row.ProjectWorldLocationToMapUV(QuarterWorld), FVector2D(0.25, 0.25)));
	Row.MapNorthAxis = EHeistMapNorthAxis::PositiveX;
	TestTrue(TEXT("Positive-X orientation is stable"), IsNearlyEqualUV(Row.ProjectWorldLocationToMapUV(QuarterWorld), FVector2D(0.25, 0.75)));
	Row.MapNorthAxis = EHeistMapNorthAxis::NegativeX;
	TestTrue(TEXT("Negative-X orientation is stable"), IsNearlyEqualUV(Row.ProjectWorldLocationToMapUV(QuarterWorld), FVector2D(0.75, 0.25)));
	Row.MapNorthAxis = EHeistMapNorthAxis::NegativeY;
	TestTrue(TEXT("Negative-Y orientation is stable"), IsNearlyEqualUV(Row.ProjectWorldLocationToMapUV(QuarterWorld), FVector2D(0.75, 0.75)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistWeek7FloorPlanPolicyTest, "ProjectMuseumHeist.W7.FloorPlan.Policy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistWeek7FloorPlanPolicyTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("An unobserved painting target does not reveal its exact case"),
		UHeistFloorPlanMapWidget::ShouldShowExactPaintingTarget(EHeistDisplayCaseState::Secured, false));
	TestTrue(TEXT("An observed painting target reveals its exact case"),
		UHeistFloorPlanMapWidget::ShouldShowExactPaintingTarget(EHeistDisplayCaseState::Observed, false));
	TestFalse(TEXT("A secured painting target no longer exposes its case"),
		UHeistFloorPlanMapWidget::ShouldShowExactPaintingTarget(EHeistDisplayCaseState::Observed, true));

	TestFalse(TEXT("An unobserved object target does not reveal its exact case"),
		UHeistFloorPlanMapWidget::ShouldShowExactObjectTarget(EHeistObjectAssemblyState::Secured, false));
	TestTrue(TEXT("An observed object target reveals its exact case"),
		UHeistFloorPlanMapWidget::ShouldShowExactObjectTarget(EHeistObjectAssemblyState::Observed, false));
	TestFalse(TEXT("A secured object target no longer exposes its case"),
		UHeistFloorPlanMapWidget::ShouldShowExactObjectTarget(EHeistObjectAssemblyState::Observed, true));

	const EHeistFloorPlanMarkerType AllowedMarkerTypes[] = {
		EHeistFloorPlanMarkerType::LocalPlayer,
		EHeistFloorPlanMarkerType::Teammate,
		EHeistFloorPlanMarkerType::Exit,
		EHeistFloorPlanMarkerType::Zone,
		EHeistFloorPlanMarkerType::TargetGallery,
		EHeistFloorPlanMarkerType::DiscoveredTarget,
		EHeistFloorPlanMarkerType::DroppedOriginal,
		EHeistFloorPlanMarkerType::EscapedTeammate,
		EHeistFloorPlanMarkerType::ArrestedTeammate,
	};
	for (const EHeistFloorPlanMarkerType MarkerType : AllowedMarkerTypes)
	{
		TestTrue(TEXT("Every declared marker category is allowed by the v1 policy"),
			UHeistFloorPlanMapWidget::IsAllowedMarkerType(MarkerType));
	}
	TestFalse(TEXT("The marker policy has no implicit Guard/Sight/SoundPing/hidden-loot category"),
		UHeistFloorPlanMapWidget::IsAllowedMarkerType(EHeistFloorPlanMarkerType::MAX));
	TestFalse(TEXT("Unknown marker values remain forbidden even if the enum grows"),
		UHeistFloorPlanMapWidget::IsAllowedMarkerType(static_cast<EHeistFloorPlanMarkerType>(255)));
	return true;
}

#endif
