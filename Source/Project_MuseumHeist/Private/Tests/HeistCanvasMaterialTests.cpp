#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "World/Actors/Loot/HeistPaintingDisplayCaseActor.h"
#include "Components/BoxComponent.h"
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
	UStaticMeshComponent* Panel = NewObject<UStaticMeshComponent>(Case, TEXT("ActivationSecurityPanel"));
	Case->AddInstanceComponent(Panel);
	Panel->SetupAttachment(Case->GetRootComponent());
	Panel->ComponentTags.Add(TEXT("HeistExhibitSecurityPanel"));
	Panel->RegisterComponent();
	UStaticMeshComponent* Frame = NewObject<UStaticMeshComponent>(Case, TEXT("AuthoredPhysicalFrame"));
	Case->AddInstanceComponent(Frame);
	Frame->SetupAttachment(Case->GetRootComponent());
	Frame->RegisterComponent();
	Frame->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	UBoxComponent* Interaction = Case->FindComponentByClass<UBoxComponent>();
	if (!TestNotNull(TEXT("Painting has its real interaction box"), Interaction)) return false;
	Case->bContractExhibitActive = false;
	Case->OnRep_ContractExhibitActive();
	TestFalse(TEXT("Inactive exhibit stays visible"), Case->IsHidden());
	TestTrue(TEXT("Inactive artwork retains its assigned image"), Case->OriginalVisualComponent->GetMaterial(0) == Original);
	TestTrue(TEXT("Inactive canvas stays visible"), Case->OriginalVisualComponent->IsVisible() && !Case->OriginalVisualComponent->bHiddenInGame);
	TestFalse(TEXT("Inactive exhibit hides only the security panel"), Panel->IsVisible());
	TestEqual(TEXT("Inactive interaction is disabled"), Interaction->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestTrue(TEXT("Inactive exhibit preserves authored physical presentation"), Case->GetActorEnableCollision() && Frame->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics);
	// Revision, reference and activation can arrive separately. Final image must win.
	++Case->OriginalVisualRevision;
	Case->OnRep_OriginalVisualRevision();
	UTexture2D* LateReference = UTexture2D::CreateTransient(4, 8);
	if (!TestNotNull(TEXT("Late image fixture exists"), LateReference)) return false;
	Case->OriginalReferenceImage = LateReference;
	Case->OnRep_OriginalVisualRevision();
	if (!TestNotNull(TEXT("Inactive received image builds a material"), Case->OriginalPaintingDynamicMaterial.Get())) return false;
	TestTrue(TEXT("Late reference updates the inactive image at the same revision"),
		Case->OriginalPaintingDynamicMaterial->K2_GetTextureParameterValue(Case->OriginalPaintingTextureParameter) == LateReference);
	TestEqual(TEXT("New image preserves its aspect ratio metadata"), Case->OriginalPaintingDynamicMaterial->K2_GetScalarParameterValue(TEXT("PaintingAspect")), .5f);
	Case->bContractExhibitActive = true;
	Case->OnRep_ContractExhibitActive();
	TestTrue(TEXT("Late activation restores the security panel"), Panel->IsVisible() && !Panel->bHiddenInGame);
	TestEqual(TEXT("Late activation restores query-only interaction"), Interaction->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
	TestTrue(TEXT("Late activation preserves the received image"), Case->OriginalVisualComponent->GetMaterial(0) == Case->OriginalPaintingDynamicMaterial);
	Case->OriginalReferenceImage.Reset();
	Case->OnRep_OriginalVisualRevision();
	Case->OriginalReferenceImage = LateReference;
	Case->OnRep_OriginalVisualRevision();
	if (!TestNotNull(TEXT("Reference after activation rebuilds the material"), Case->OriginalPaintingDynamicMaterial.Get())) return false;
	TestTrue(TEXT("Reference after activation restores the same image"),
		Case->OriginalPaintingDynamicMaterial->K2_GetTextureParameterValue(Case->OriginalPaintingTextureParameter) == LateReference);
	TestTrue(TEXT("All transitions preserve authored geometry"), Case->OriginalVisualComponent->GetRelativeTransform().Equals(AuthoredTransform));
	World->DestroyActor(Case);
	return true;
}

#endif
