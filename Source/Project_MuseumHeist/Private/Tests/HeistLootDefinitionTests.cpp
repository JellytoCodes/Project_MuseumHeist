#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Character/Components/HeistInventoryComponent.h"
#include "Components/Image.h"
#include "Data/HeistArtifactDataTypes.h"
#include "Data/HeistContractDataTypes.h"
#include "Data/HeistGameBalanceDataAsset.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Inventory/HeistItemDataTypes.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "UI/DragDrop/HeistInventoryDragDropOperation.h"

namespace
{
bool HaveSameMaterialPaths(const FHeistLootDataRow& Left, const FHeistLootDataRow& Right)
{
	if (Left.WorldMaterials.Num() != Right.WorldMaterials.Num())
	{
		return false;
	}

	for (int32 MaterialIndex = 0; MaterialIndex < Left.WorldMaterials.Num(); ++MaterialIndex)
	{
		if (Left.WorldMaterials[MaterialIndex].ToSoftObjectPath() != Right.WorldMaterials[MaterialIndex].ToSoftObjectPath())
		{
			return false;
		}
	}

	return true;
}

bool HaveSameWorldVisualSignature(const FHeistLootDataRow& Left, const FHeistLootDataRow& Right)
{
	return Left.WorldMesh.ToSoftObjectPath() == Right.WorldMesh.ToSoftObjectPath() && HaveSameMaterialPaths(Left, Right) &&
		Left.WorldVisualRelativeTransform.Equals(Right.WorldVisualRelativeTransform, 0.0);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistInventoryGridContractTest, "ProjectMuseumHeist.Inventory.GridContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistInventoryGridContractTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Inventory column count is five"), UHeistInventoryComponent::GridColumnCount, 5);
	TestEqual(TEXT("Inventory row count is five"), UHeistInventoryComponent::GridRowCount, 5);

	FHeistForgeryTemplateRow TemplateDefinition;
	FIntPoint GridSize = FIntPoint::ZeroValue;
	TemplateDefinition.AllowedPalette.SetNum(HeistSurfaceForgeryInventory::EasyPaletteCount);
	TestTrue(TEXT("Easy Surface difficulty resolves an inventory footprint"), HeistSurfaceForgeryInventory::TryResolveGridSize(TemplateDefinition, GridSize));
	TestEqual(TEXT("Easy Painting Original uses 1x2"), GridSize, FIntPoint(1, 2));

	TemplateDefinition.AllowedPalette.SetNum(HeistSurfaceForgeryInventory::MediumPaletteCount);
	TestTrue(TEXT("Medium Surface difficulty resolves an inventory footprint"), HeistSurfaceForgeryInventory::TryResolveGridSize(TemplateDefinition, GridSize));
	TestEqual(TEXT("Medium Painting Original uses 2x2"), GridSize, FIntPoint(2, 2));

	TemplateDefinition.AllowedPalette.SetNum(HeistSurfaceForgeryInventory::HardPaletteCount);
	TestTrue(TEXT("Hard Surface difficulty resolves an inventory footprint"), HeistSurfaceForgeryInventory::TryResolveGridSize(TemplateDefinition, GridSize));
	TestEqual(TEXT("Hard Painting Original uses 3x2"), GridSize, FIntPoint(3, 2));

	TemplateDefinition.AllowedPalette.SetNum(3);
	TestFalse(TEXT("Unsupported Surface difficulty is rejected"), HeistSurfaceForgeryInventory::TryResolveGridSize(TemplateDefinition, GridSize));
	TestEqual(TEXT("Rejected Surface difficulty clears the footprint"), GridSize, FIntPoint::ZeroValue);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistArrestConfiscationPayloadContractTest, "ProjectMuseumHeist.Inventory.ArrestConfiscationPayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistArrestConfiscationPayloadContractTest::RunTest(const FString& Parameters)
{
	FHeistInventoryItem LooseLoot;
	LooseLoot.InstanceId = 1;
	LooseLoot.ItemId = FName(TEXT("Loot_Test"));
	LooseLoot.Quantity = 2;
	LooseLoot.Weight = 1.5f;

	FHeistInventoryItem Original;
	Original.InstanceId = 2;
	Original.ItemId = FName(TEXT("Artifact_Test"));
	Original.Quantity = 1;
	Original.BaseGridSize = FIntPoint(2, 2);
	Original.Weight = 4.0f;
	Original.ContractValue = 1000;
	Original.bOriginalArtifact = true;
	Original.SourceDisplayCase = GetMutableDefault<AActor>();

	FHeistArrestConfiscationPayload Payload;
	Payload.ConfiscatedItems = {LooseLoot, Original};
	Payload.LooseLootValue = 400;
	Payload.LooseLootWeight = 3.0f;
	TestTrue(TEXT("Loose Loot and Original form a confiscation payload"), Payload.HasConfiscatedItems());
	TestEqual(TEXT("Only the Original counts as an Original"), Payload.GetOriginalItemCount(), 1);
	TestEqual(TEXT("Loose quantity expands to two world actors and Original to one"), Payload.GetWorldActorCount(), 3);
	TestTrue(TEXT("Confiscation total weight combines Loose Loot and Original"), FMath::IsNearlyEqual(Payload.GetTotalWeight(), 7.0f));

	FHeistArrestConfiscationPayload MatchingPayload = Payload;
	TestTrue(TEXT("An unchanged server preview matches at commit"), Payload.Matches(MatchingPayload));
	MatchingPayload.ConfiscatedItems[0].Quantity = 1;
	TestFalse(TEXT("A changed inventory quantity invalidates commit"), Payload.Matches(MatchingPayload));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistInventoryDragVisualContractTest, "ProjectMuseumHeist.Inventory.DragVisualContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistInventoryDragVisualContractTest::RunTest(const FString& Parameters)
{
	UHeistInventoryDragDropOperation* DragOperation = NewObject<UHeistInventoryDragDropOperation>();
	UImage* DragVisualImage = NewObject<UImage>(DragOperation);
	DragOperation->SetupDragOperation(7, FIntPoint(1, 2), DragVisualImage);

	TestEqual(TEXT("Drag operation keeps the item instance"), DragOperation->InstanceId, 7);
	TestEqual(TEXT("Drag operation keeps the source grid position"), DragOperation->SourceGridPosition, FIntPoint(1, 2));
	TestTrue(TEXT("Drag operation uses the supplied image as its visual"), DragOperation->DefaultDragVisual == DragVisualImage);
	TestFalse(TEXT("Drag starts as an inventory move"), DragOperation->bWorldDropPreview);

	const FLinearColor InventoryMoveColor = DragVisualImage->GetColorAndOpacity();
	DragOperation->SetWorldDropPreview(true);
	TestTrue(TEXT("Leaving the inventory enables world-drop preview"), DragOperation->bWorldDropPreview);
	TestFalse(TEXT("World-drop preview changes the drag image tint"), DragVisualImage->GetColorAndOpacity().Equals(InventoryMoveColor));

	DragOperation->SetWorldDropPreview(false);
	TestFalse(TEXT("Returning to the grid clears world-drop preview"), DragOperation->bWorldDropPreview);
	TestTrue(TEXT("Returning to the grid restores the move tint"), DragVisualImage->GetColorAndOpacity().Equals(InventoryMoveColor));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistLootDefinitionTest, "ProjectMuseumHeist.Loot.Definition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistLootDefinitionTest::RunTest(const FString& Parameters)
{
	const UHeistGameBalanceDataAsset* BalanceData = GetDefault<UHeistGameBalanceDataAsset>();
	TestNotNull(TEXT("Default balance data is available"), BalanceData);
	if (!IsValid(BalanceData))
	{
		return false;
	}

	UDataTable* ItemDataTable = BalanceData->ItemDataTable.LoadSynchronous();
	UDataTable* LootDataTable = BalanceData->LootDataTable.LoadSynchronous();
	UDataTable* ArtifactDataTable = BalanceData->ArtifactDataTable.LoadSynchronous();
	UDataTable* ContractDataTable = BalanceData->ContractDataTable.LoadSynchronous();
	TestNotNull(TEXT("DT_ItemData loads"), ItemDataTable);
	TestNotNull(TEXT("DT_LootData loads"), LootDataTable);
	TestNotNull(TEXT("DT_ArtifactData loads"), ArtifactDataTable);
	TestNotNull(TEXT("DT_ContractData loads"), ContractDataTable);
	if (!IsValid(ItemDataTable) || !IsValid(LootDataTable) || !IsValid(ArtifactDataTable) || !IsValid(ContractDataTable))
	{
		return false;
	}

	const UScriptStruct* ItemRowStruct = ItemDataTable->GetRowStruct();
	const UScriptStruct* LootRowStruct = LootDataTable->GetRowStruct();
	const UScriptStruct* ArtifactRowStruct = ArtifactDataTable->GetRowStruct();
	const UScriptStruct* ContractRowStruct = ContractDataTable->GetRowStruct();
	TestTrue(TEXT("DT_ItemData uses the item row schema"), ItemRowStruct == FHeistItemDataRow::StaticStruct());
	TestTrue(TEXT("DT_LootData uses the loot row schema"), LootRowStruct == FHeistLootDataRow::StaticStruct());
	TestTrue(TEXT("DT_ArtifactData uses the artifact row schema"), ArtifactRowStruct == FHeistArtifactDataRow::StaticStruct());
	TestTrue(TEXT("DT_ContractData uses the contract row schema"), ContractRowStruct == FHeistContractDataRow::StaticStruct());
	if (ItemDataTable->GetRowStruct() != FHeistItemDataRow::StaticStruct() || LootDataTable->GetRowStruct() != FHeistLootDataRow::StaticStruct() ||
		ArtifactDataTable->GetRowStruct() != FHeistArtifactDataRow::StaticStruct() || ContractDataTable->GetRowStruct() != FHeistContractDataRow::StaticStruct())
	{
		return false;
	}

	const TArray<FName> ExpectedLootIds = {
		FName(TEXT("Loot_RoyalCrown")), FName(TEXT("Loot_Painting")), FName(TEXT("Loot_AncientSword")), FName(TEXT("Loot_GoldenVase")), FName(TEXT("Loot_JewelNecklace"))};
	TestTrue(TEXT("DT_LootData contains at least the five required v1 loose-loot definitions"), LootDataTable->GetRowNames().Num() >= ExpectedLootIds.Num());

	int32 AvailableLootItemCount = 0;
	for (const FName ItemRowName : ItemDataTable->GetRowNames())
	{
		const FHeistItemDataRow* ItemDefinition = ItemDataTable->FindRow<FHeistItemDataRow>(ItemRowName, TEXT("FHeistLootDefinitionTest::CountItems"), false);
		AvailableLootItemCount += ItemDefinition != nullptr && ItemDefinition->bAvailableInV1 && ItemDefinition->ItemType == EHeistItemType::Loot ? 1 : 0;
	}
	TestTrue(TEXT("DT_ItemData contains at least five available v1 loose-loot rows"), AvailableLootItemCount >= ExpectedLootIds.Num());

	TSet<int32> UniqueValues;
	TSet<FString> UniqueGridSizes;
	TArray<float> UniqueWeights;
	TArray<float> UniqueSpawnWeights;
	TArray<const FHeistLootDataRow*> PriorVisualDefinitions;
	for (const FName LootId : ExpectedLootIds)
	{
		const FString LootLabel = LootId.ToString();
		const FHeistItemDataRow* ItemDefinition = ItemDataTable->FindRow<FHeistItemDataRow>(LootId, TEXT("FHeistLootDefinitionTest::ItemPair"), false);
		const FHeistLootDataRow* LootDefinition = LootDataTable->FindRow<FHeistLootDataRow>(LootId, TEXT("FHeistLootDefinitionTest::LootPair"), false);
		TestNotNull(*FString::Printf(TEXT("%s has an item row"), *LootLabel), ItemDefinition);
		TestNotNull(*FString::Printf(TEXT("%s has a loot row"), *LootLabel), LootDefinition);
		if (ItemDefinition == nullptr || LootDefinition == nullptr)
		{
			continue;
		}

		TestEqual(*FString::Printf(TEXT("%s item row id matches its row name"), *LootLabel), ItemDefinition->ItemId, LootId);
		TestEqual(*FString::Printf(TEXT("%s loot row id matches its row name"), *LootLabel), LootDefinition->ItemId, LootId);
		TestEqual(*FString::Printf(TEXT("%s is a loose-loot item"), *LootLabel), ItemDefinition->ItemType, EHeistItemType::Loot);
		TestTrue(*FString::Printf(TEXT("%s is available in v1"), *LootLabel), ItemDefinition->bAvailableInV1);
		TestTrue(*FString::Printf(TEXT("%s has a positive value"), *LootLabel), LootDefinition->ScoreValue > 0);
		TestTrue(*FString::Printf(TEXT("%s has a valid grid size"), *LootLabel), ItemDefinition->GridSize.X > 0 && ItemDefinition->GridSize.Y > 0);
		TestTrue(*FString::Printf(TEXT("%s has a positive finite weight"), *LootLabel), FMath::IsFinite(ItemDefinition->Weight) && ItemDefinition->Weight > 0.0f);
		TestTrue(*FString::Printf(TEXT("%s has a positive finite spawn weight"), *LootLabel), FMath::IsFinite(LootDefinition->SpawnWeight) && LootDefinition->SpawnWeight > 0.0f);

		TestFalse(*FString::Printf(TEXT("%s value is strictly unique"), *LootLabel), UniqueValues.Contains(LootDefinition->ScoreValue));
		UniqueValues.Add(LootDefinition->ScoreValue);
		const FString GridSignature = FString::Printf(TEXT("%dx%d"), ItemDefinition->GridSize.X, ItemDefinition->GridSize.Y);
		TestFalse(*FString::Printf(TEXT("%s grid size is strictly unique"), *LootLabel), UniqueGridSizes.Contains(GridSignature));
		UniqueGridSizes.Add(GridSignature);
		TestFalse(*FString::Printf(TEXT("%s weight is strictly unique"), *LootLabel), UniqueWeights.Contains(ItemDefinition->Weight));
		UniqueWeights.Add(ItemDefinition->Weight);
		TestFalse(*FString::Printf(TEXT("%s spawn weight is strictly unique"), *LootLabel), UniqueSpawnWeights.Contains(LootDefinition->SpawnWeight));
		UniqueSpawnWeights.Add(LootDefinition->SpawnWeight);

		TestFalse(*FString::Printf(TEXT("%s has a world mesh reference"), *LootLabel), LootDefinition->WorldMesh.IsNull());
		UStaticMesh* WorldMesh = LootDefinition->WorldMesh.LoadSynchronous();
		TestNotNull(*FString::Printf(TEXT("%s world mesh loads"), *LootLabel), WorldMesh);
		TestFalse(*FString::Printf(TEXT("%s world visual transform has no NaN"), *LootLabel), LootDefinition->WorldVisualRelativeTransform.ContainsNaN());
		for (int32 MaterialIndex = 0; MaterialIndex < LootDefinition->WorldMaterials.Num(); ++MaterialIndex)
		{
			const TSoftObjectPtr<UMaterialInterface>& MaterialReference = LootDefinition->WorldMaterials[MaterialIndex];
			TestFalse(*FString::Printf(TEXT("%s material %d has a valid reference"), *LootLabel, MaterialIndex), MaterialReference.IsNull());
			UMaterialInterface* Material = MaterialReference.LoadSynchronous();
			TestNotNull(*FString::Printf(TEXT("%s material %d loads"), *LootLabel, MaterialIndex), Material);
		}

		for (const FHeistLootDataRow* PriorDefinition : PriorVisualDefinitions)
		{
			TestFalse(*FString::Printf(TEXT("%s has a unique mesh/material/transform visual signature"), *LootLabel),
				PriorDefinition != nullptr && HaveSameWorldVisualSignature(*LootDefinition, *PriorDefinition));
		}
		PriorVisualDefinitions.Add(LootDefinition);
	}

	for (const FName LootRowName : LootDataTable->GetRowNames())
	{
		const FHeistLootDataRow* LootDefinition =
			LootDataTable->FindRow<FHeistLootDataRow>(LootRowName, TEXT("FHeistLootDefinitionTest::LootPairCheck"), false);
		const FHeistItemDataRow* ItemDefinition =
			ItemDataTable->FindRow<FHeistItemDataRow>(LootRowName, TEXT("FHeistLootDefinitionTest::OrphanCheck"), false);
		TestNotNull(*FString::Printf(TEXT("%s has a loot definition"), *LootRowName.ToString()), LootDefinition);
		TestNotNull(*FString::Printf(TEXT("%s has no orphan loot extension"), *LootRowName.ToString()), ItemDefinition);
		if (LootDefinition != nullptr && ItemDefinition != nullptr)
		{
			TestEqual(*FString::Printf(TEXT("%s loot row id matches its row name"), *LootRowName.ToString()), LootDefinition->ItemId, LootRowName);
			TestEqual(*FString::Printf(TEXT("%s item row id matches its row name"), *LootRowName.ToString()), ItemDefinition->ItemId, LootRowName);
			TestEqual(*FString::Printf(TEXT("%s paired item is loose loot"), *LootRowName.ToString()), ItemDefinition->ItemType, EHeistItemType::Loot);
			TestTrue(*FString::Printf(TEXT("%s paired item is available in v1"), *LootRowName.ToString()), ItemDefinition->bAvailableInV1);
		}
	}

	for (const FName ItemRowName : ItemDataTable->GetRowNames())
	{
		const FHeistItemDataRow* ItemDefinition =
			ItemDataTable->FindRow<FHeistItemDataRow>(ItemRowName, TEXT("FHeistLootDefinitionTest::ItemPairCheck"), false);
		if (ItemDefinition != nullptr && ItemDefinition->bAvailableInV1 && ItemDefinition->ItemType == EHeistItemType::Loot)
		{
			TestNotNull(*FString::Printf(TEXT("%s available loose-loot item has a loot extension"), *ItemRowName.ToString()),
				LootDataTable->FindRow<FHeistLootDataRow>(ItemRowName, TEXT("FHeistLootDefinitionTest::MissingExtensionCheck"), false));
		}
	}

	int32 MaximumArtifactValue = INDEX_NONE;
	for (const FName ArtifactRowName : ArtifactDataTable->GetRowNames())
	{
		const FHeistArtifactDataRow* ArtifactDefinition =
			ArtifactDataTable->FindRow<FHeistArtifactDataRow>(ArtifactRowName, TEXT("FHeistLootDefinitionTest::ArtifactQuota"), false);
		TestNotNull(*FString::Printf(TEXT("%s has an artifact definition"), *ArtifactRowName.ToString()), ArtifactDefinition);
		if (ArtifactDefinition != nullptr)
		{
			TestEqual(*FString::Printf(TEXT("%s artifact id matches its row name"), *ArtifactRowName.ToString()), ArtifactDefinition->ArtifactId, ArtifactRowName);
			TestTrue(*FString::Printf(TEXT("%s has a positive artifact value"), *ArtifactRowName.ToString()), ArtifactDefinition->ArtifactValue > 0);
			MaximumArtifactValue = FMath::Max(MaximumArtifactValue, ArtifactDefinition->ArtifactValue);
		}
	}
	TestTrue(TEXT("At least one required-target artifact value is defined"), MaximumArtifactValue > 0);

	int32 MinimumSoloBaseQuota = MAX_int32;
	for (const FName ContractRowName : ContractDataTable->GetRowNames())
	{
		const FHeistContractDataRow* ContractDefinition =
			ContractDataTable->FindRow<FHeistContractDataRow>(ContractRowName, TEXT("FHeistLootDefinitionTest::ContractQuota"), false);
		TestNotNull(*FString::Printf(TEXT("%s has a contract definition"), *ContractRowName.ToString()), ContractDefinition);
		if (ContractDefinition != nullptr)
		{
			TestEqual(*FString::Printf(TEXT("%s contract id matches its row name"), *ContractRowName.ToString()), ContractDefinition->ContractId, ContractRowName);
			TestTrue(*FString::Printf(TEXT("%s has a positive solo base quota"), *ContractRowName.ToString()), ContractDefinition->BaseLootValueQuota > 0);
			TestEqual(*FString::Printf(TEXT("%s one-player quota resolves to its base quota"), *ContractRowName.ToString()), ContractDefinition->ResolveLootValueQuota(1),
				ContractDefinition->BaseLootValueQuota);
			MinimumSoloBaseQuota = FMath::Min(MinimumSoloBaseQuota, ContractDefinition->BaseLootValueQuota);
		}
	}
	TestTrue(TEXT("At least one solo contract quota is defined"), MinimumSoloBaseQuota > 0 && MinimumSoloBaseQuota < MAX_int32);
	TestTrue(TEXT("The highest required-target ArtifactValue remains below the solo BaseLootValueQuota"),
		MaximumArtifactValue > 0 && MinimumSoloBaseQuota < MAX_int32 && MaximumArtifactValue < MinimumSoloBaseQuota);

	return true;
}

#endif
