#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "World/Actors/Loot/HeistPaintingDisplayCaseActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistCanvasMaterialTransitionTest, "ProjectMuseumHeist.Forgery.CanvasMaterialTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistCanvasMaterialTransitionTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AHeistPaintingDisplayCaseActor* Case = World->SpawnActor<AHeistPaintingDisplayCaseActor>();
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Assets/Art/SurfaceForgery/Materials/Canvas/MI_HeistCanvas_01"));
	if (!TestNotNull(TEXT("Canvas material exists"), Material)) return false;
	Case->OriginalPaintingMaterial = Material;
	Case->ReplicaPaintingMaterial = Material;
	Case->OriginalPaintingBaselineMaterial = Material;
	Case->OriginalVisualComponent->SetMaterial(0, Material);
	Case->OriginalReferenceImage = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture"));
	Case->OriginalVisualTemplateId = TEXT("CanvasTest");
	Case->OriginalVisualRevision = 1;
	Case->bContractExhibitActive = true;
	Case->RefreshOriginalPaintingVisual();
	UMaterialInterface* Original = Case->OriginalVisualComponent->GetMaterial(0);
	TestNotNull(TEXT("Original image material built"), Case->OriginalPaintingDynamicMaterial.Get());
	const FTransform AuthoredTransform = Case->OriginalVisualComponent->GetRelativeTransform();
	Case->bHasCommittedForgeryResult = true;
	Case->CommittedForgeryResult.SimilarityScore = 80;
	Case->CommittedForgeryRevision = 1;
	Case->ReplicaPaintingData.Resolution = 256;
	Case->ReplicaPaintingData.Palette = {FColor::Red, FColor::Blue};
	Case->ReplicaPaintingData.PackedPaletteIndices.Init(0x12, 256 * 256 / 2);
	Case->ReplicaPaintingData.Revision = 1;
	Case->DisplayCaseState = EHeistDisplayCaseState::ReplicaReady;
	Case->RefreshReplicaWorldVisual();
	TestTrue(TEXT("Preview keeps original material"), Case->OriginalVisualComponent->GetMaterial(0) == Original);
	Case->CommittedForgeryResult.bReplicaPlaced = true;
	Case->DisplayCaseState = EHeistDisplayCaseState::ReplicaPlaced;
	Case->RefreshPlaceholderVisualState();
	UMaterialInterface* Replica = Case->OriginalVisualComponent->GetMaterial(0);
	TestTrue(TEXT("Commit changes material on the same canvas"), Replica == Case->ReplicaPaintingDynamicMaterial && Replica != Original);
	Case->RefreshOriginalPaintingVisual();
	TestTrue(TEXT("Late original revision does not overwrite committed replica"), Case->OriginalVisualComponent->GetMaterial(0) == Replica);
	Case->DisplayCaseState = EHeistDisplayCaseState::Secured;
	Case->bHasCommittedForgeryResult = false;
	Case->RefreshPlaceholderVisualState();
	Case->RefreshReplicaWorldVisual();
	TestTrue(TEXT("Reset restores cached original material"), Case->OriginalVisualComponent->GetMaterial(0) == Original);
	TestTrue(TEXT("All transitions preserve authored geometry"), Case->OriginalVisualComponent->GetRelativeTransform().Equals(AuthoredTransform));
	World->DestroyActor(Case);
	return true;
}

#endif
