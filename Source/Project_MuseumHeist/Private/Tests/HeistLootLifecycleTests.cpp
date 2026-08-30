#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "AI/HeistGuardCharacter.h"
#include "AI/HeistGuardStateComponent.h"
#include "Character/Components/HeistInteractionComponent.h"
#include "Character/Components/HeistInventoryComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HeistGameInstance.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistPlayerState.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Inventory/HeistItemDataTypes.h"
#include "Inventory/HeistInventoryTypes.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "World/Actors/Escape/HeistVentActor.h"
#include "World/Actors/Loot/HeistLootActor.h"
#include "World/Spawn/HeistLootSpawnPoint.h"

namespace HeistLootLifecycleTest
{
constexpr int32 LootCarrierPlayerId = 2;
constexpr float CrossWorldLootLocationToleranceCm = 5.0f;
const FName MatchSpawnedLooseLootTag(TEXT("HeistMatchSpawnedLooseLoot"));

struct FHeistLootLifecycleFixture
{
	FName RowId = NAME_None;
	FName OriginalActorName = NAME_None;
	FName DroppedActorName = NAME_None;
	FVector OriginalSpawnLocation = FVector::ZeroVector;
	FVector DroppedSpawnLocation = FVector::ZeroVector;
	FHeistItemDataRow ItemDefinition;
	FHeistLootDataRow LootDefinition;
	int32 InitialInstanceId = INDEX_NONE;
	int32 RepickedInstanceId = INDEX_NONE;
};

struct FHeistLootLifecycleAutomationState
{
	bool bAborted = false;
	bool bCapturedPlaySettings = false;
	EPlayNetMode OriginalNetMode = EPlayNetMode::PIE_Standalone;
	bool bOriginalRunUnderOneProcess = true;
	int32 OriginalClientCount = 1;
	bool bMatchStartSupplyValidated = false;
	bool bRuntimeFixturesSpawned = false;
	FString LastCaptureWaitReason;
	TArray<FHeistLootLifecycleFixture> Fixtures;
	FName PendingFeedbackRowId = NAME_None;
	int32 PendingFeedbackValue = 0;
	TMap<FName, int32> FeedbackCounts;
	TArray<FString> UnexpectedFeedback;
	FDelegateHandle FeedbackHandle;
	TWeakObjectPtr<AHeistPlayerController> FeedbackController;
};

const TArray<FName>& GetLifecycleLootRowIds()
{
	// Largest first keeps the production first-fit placement deterministic within the 5x5 grid.
	static const TArray<FName> RowIds = {
		FName(TEXT("Loot_Painting")), FName(TEXT("Loot_RoyalCrown")), FName(TEXT("Loot_AncientSword")), FName(TEXT("Loot_GoldenVase")),
		FName(TEXT("Loot_JewelNecklace"))};
	return RowIds;
}

TArray<UWorld*> GetLootLifecyclePIEWorlds()
{
	TArray<UWorld*> Worlds;
	if (!IsValid(GEngine))
	{
		return Worlds;
	}

	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		UWorld* World = WorldContext.World();
		if (WorldContext.WorldType == EWorldType::PIE && IsValid(World))
		{
			Worlds.Add(World);
		}
	}
	Worlds.Sort([](const UWorld& Left, const UWorld& Right)
	{
		if (Left.GetNetMode() != Right.GetNetMode())
		{
			return static_cast<uint8>(Left.GetNetMode()) < static_cast<uint8>(Right.GetNetMode());
		}
		return Left.GetName() < Right.GetName();
	});
	return Worlds;
}

UWorld* GetLootLifecycleServerWorld()
{
	for (UWorld* World : GetLootLifecyclePIEWorlds())
	{
		if (IsValid(World) && (World->GetNetMode() == NM_ListenServer || World->GetNetMode() == NM_Standalone))
		{
			return World;
		}
	}
	return nullptr;
}

bool SetAllLootLifecycleLobbyPlayersReady(UWorld* ServerWorld)
{
	AHeistGameState* GameState = IsValid(ServerWorld) ? ServerWorld->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(GameState))
	{
		return false;
	}
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (AHeistPlayerState* HeistPlayerState = Cast<AHeistPlayerState>(PlayerState))
		{
			HeistPlayerState->SetLobbyReady(true);
		}
	}
	return GameState->AreAllConnectedPlayersLobbyReady();
}

AHeistPlayerController* GetLootLifecycleLocalHeistPlayerController(UWorld* World)
{
	if (!IsValid(World))
	{
		return nullptr;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		AHeistPlayerController* PlayerController = Cast<AHeistPlayerController>(It->Get());
		if (IsValid(PlayerController) && PlayerController->IsLocalController())
		{
			return PlayerController;
		}
	}
	return nullptr;
}

AHeistPlayerState* FindHeistPlayerStateById(UWorld* World, const int32 PlayerId)
{
	const AHeistGameState* GameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(GameState))
	{
		return nullptr;
	}
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		AHeistPlayerState* HeistPlayerState = Cast<AHeistPlayerState>(PlayerState);
		if (IsValid(HeistPlayerState) && HeistPlayerState->HeistPlayerId == PlayerId)
		{
			return HeistPlayerState;
		}
	}
	return nullptr;
}

AHeistPlayerController* GetServerPlayerControllerById(const int32 PlayerId)
{
	UWorld* ServerWorld = GetLootLifecycleServerWorld();
	if (!IsValid(ServerWorld))
	{
		return nullptr;
	}
	for (FConstPlayerControllerIterator It = ServerWorld->GetPlayerControllerIterator(); It; ++It)
	{
		AHeistPlayerController* PlayerController = Cast<AHeistPlayerController>(It->Get());
		const AHeistPlayerState* PlayerState = IsValid(PlayerController) ? PlayerController->GetPlayerState<AHeistPlayerState>() : nullptr;
		if (IsValid(PlayerState) && PlayerState->HeistPlayerId == PlayerId)
		{
			return PlayerController;
		}
	}
	return nullptr;
}

AHeistPlayerController* GetOwningPlayerControllerById(const int32 PlayerId)
{
	for (UWorld* World : GetLootLifecyclePIEWorlds())
	{
		AHeistPlayerController* PlayerController = GetLootLifecycleLocalHeistPlayerController(World);
		const AHeistPlayerState* PlayerState = IsValid(PlayerController) ? PlayerController->GetPlayerState<AHeistPlayerState>() : nullptr;
		if (IsValid(PlayerState) && PlayerState->HeistPlayerId == PlayerId)
		{
			return PlayerController;
		}
	}
	return nullptr;
}

AHeistPlayerCharacter* FindHeistCharacterById(UWorld* World, const int32 PlayerId)
{
	if (!IsValid(World))
	{
		return nullptr;
	}
	for (TActorIterator<AHeistPlayerCharacter> It(World); It; ++It)
	{
		const AHeistPlayerState* PlayerState = IsValid(*It) ? It->GetPlayerState<AHeistPlayerState>() : nullptr;
		if (IsValid(PlayerState) && PlayerState->HeistPlayerId == PlayerId)
		{
			return *It;
		}
	}
	return nullptr;
}

AHeistLootActor* FindLootActorByName(UWorld* World, const FName ActorName)
{
	if (!IsValid(World) || ActorName.IsNone())
	{
		return nullptr;
	}
	for (TActorIterator<AHeistLootActor> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetFName() == ActorName)
		{
			return *It;
		}
	}
	return nullptr;
}

AHeistLootActor* FindLootActorByRowAndLocation(UWorld* World, const FName RowId, const FVector& ServerSpawnLocation)
{
	if (!IsValid(World) || RowId.IsNone())
	{
		return nullptr;
	}

	AHeistLootActor* ClosestMatch = nullptr;
	float ClosestDistanceSquared = FMath::Square(CrossWorldLootLocationToleranceCm);
	for (TActorIterator<AHeistLootActor> It(World); It; ++It)
	{
		AHeistLootActor* LootActor = *It;
		if (!IsValid(LootActor) || LootActor->GetLootRowId() != RowId)
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared(LootActor->GetActorLocation(), ServerSpawnLocation);
		if (DistanceSquared <= ClosestDistanceSquared)
		{
			ClosestMatch = LootActor;
			ClosestDistanceSquared = DistanceSquared;
		}
	}
	return ClosestMatch;
}

AHeistLootActor* FindAvailableLootActorByRow(UWorld* World, const FName RowId, const FName ExcludedActorName = NAME_None)
{
	if (!IsValid(World) || RowId.IsNone())
	{
		return nullptr;
	}
	for (TActorIterator<AHeistLootActor> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetLootRowId() == RowId && It->GetFName() != ExcludedActorName && !It->ActorHasTag(MatchSpawnedLooseLootTag) && It->IsLootAvailable())
		{
			return *It;
		}
	}
	return nullptr;
}

AHeistVentActor* FindVentActor(UWorld* World)
{
	if (!IsValid(World))
	{
		return nullptr;
	}
	for (TActorIterator<AHeistVentActor> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}
	return nullptr;
}

bool TeleportServerPlayerIntoInteraction(const int32 PlayerId, AActor* TargetActor)
{
	AHeistPlayerController* PlayerController = GetServerPlayerControllerById(PlayerId);
	AHeistPlayerCharacter* Character = IsValid(PlayerController) ? PlayerController->GetPawn<AHeistPlayerCharacter>() : nullptr;
	if (!IsValid(Character) || !IsValid(TargetActor))
	{
		return false;
	}
	const USphereComponent* InteractionSphere = TargetActor->FindComponentByClass<USphereComponent>();
	const FVector Destination = IsValid(InteractionSphere) ? InteractionSphere->GetComponentLocation() : TargetActor->GetActorLocation();
	Character->SetActorLocation(Destination, false, nullptr, ETeleportType::TeleportPhysics);
	Character->ForceNetUpdate();
	return true;
}

bool IsServerPlayerOverlapping(const int32 PlayerId, const AActor* TargetActor)
{
	const AHeistPlayerCharacter* Character = FindHeistCharacterById(GetLootLifecycleServerWorld(), PlayerId);
	const UHeistInteractionComponent* InteractionComponent = IsValid(Character) ? Character->GetInteractionComponent() : nullptr;
	return IsValid(InteractionComponent) && IsValid(TargetActor) && InteractionComponent->IsActorOverlappingInteractionArea(TargetActor);
}

template <typename TTargetActor>
bool InvokeSingleActorServerRPC(AHeistPlayerController* PlayerController, const FName FunctionName, TTargetActor* TargetActor)
{
	if (!IsValid(PlayerController) || !IsValid(TargetActor))
	{
		return false;
	}
	UFunction* Function = PlayerController->FindFunction(FunctionName);
	if (!IsValid(Function))
	{
		UE_LOG(LogTemp, Error, TEXT("W6-011 RPC lookup failed: Controller=%s Function=%s"), *GetNameSafe(PlayerController), *FunctionName.ToString());
		return false;
	}
	struct FActorRPCParameters
	{
		TTargetActor* Target = nullptr;
	};
	if (Function->ParmsSize != sizeof(FActorRPCParameters))
	{
		UE_LOG(LogTemp, Error, TEXT("W6-011 RPC parameter mismatch: Controller=%s Function=%s ReflectedBytes=%d TestBytes=%d"), *GetNameSafe(PlayerController),
			*FunctionName.ToString(), static_cast<int32>(Function->ParmsSize), static_cast<int32>(sizeof(FActorRPCParameters)));
		return false;
	}
	FActorRPCParameters Parameters;
	Parameters.Target = TargetActor;
	PlayerController->ProcessEvent(Function, &Parameters);
	return true;
}

bool AreLootLifecycleWorldsReady(const EHeistMatchPhase ExpectedPhase, const bool bRequirePawn)
{
	const TArray<UWorld*> Worlds = GetLootLifecyclePIEWorlds();
	if (Worlds.Num() != 2 || !IsValid(GetLootLifecycleServerWorld()))
	{
		return false;
	}
	TSet<int32> LocalPlayerIds;
	for (UWorld* World : Worlds)
	{
		const AHeistGameState* GameState = World->GetGameState<AHeistGameState>();
		const AHeistPlayerController* LocalPlayerController = GetLootLifecycleLocalHeistPlayerController(World);
		const AHeistPlayerState* LocalPlayerState = IsValid(LocalPlayerController) ? LocalPlayerController->GetPlayerState<AHeistPlayerState>() : nullptr;
		if (!IsValid(GameState) || GameState->GetMatchPhase() != ExpectedPhase || GameState->PlayerArray.Num() != 2 || !IsValid(LocalPlayerController) ||
			!IsValid(LocalPlayerState) || LocalPlayerState->HeistPlayerId < 1 || LocalPlayerState->HeistPlayerId > 2 ||
			(bRequirePawn && !IsValid(LocalPlayerController->GetPawn())))
		{
			return false;
		}
		LocalPlayerIds.Add(LocalPlayerState->HeistPlayerId);
	}
	return LocalPlayerIds.Num() == 2;
}

bool IsLootWorldVisualResolved(const AHeistLootActor* LootActor, const FHeistItemDataRow& ItemDefinition, const FHeistLootDataRow& LootDefinition,
	const bool bExpectedAvailable)
{
	if (!IsValid(LootActor) || LootActor->GetLootRowId() != LootDefinition.ItemId || LootActor->IsLootAvailable() != bExpectedAvailable ||
		LootActor->GetScoreValue() != LootDefinition.ScoreValue || !FMath::IsNearlyEqual(LootActor->GetWeightValue(), ItemDefinition.Weight))
	{
		return false;
	}
	UClass* SharedLootClass = LoadClass<AHeistLootActor>(nullptr, TEXT("/Game/Blueprints/World/Actors/Loot/BP_Loot.BP_Loot_C"));
	const UStaticMeshComponent* VisualMesh = LootActor->FindComponentByClass<UStaticMeshComponent>();
	const USphereComponent* InteractionSphere = LootActor->FindComponentByClass<USphereComponent>();
	UStaticMesh* ExpectedMesh = LootDefinition.WorldMesh.LoadSynchronous();
	if (!IsValid(SharedLootClass) || !LootActor->IsA(SharedLootClass) || !IsValid(VisualMesh) || !IsValid(InteractionSphere) || !IsValid(ExpectedMesh) ||
		VisualMesh->GetStaticMesh() != ExpectedMesh || !VisualMesh->GetRelativeTransform().Equals(LootDefinition.WorldVisualRelativeTransform, KINDA_SMALL_NUMBER) ||
		VisualMesh->IsVisible() != bExpectedAvailable || (InteractionSphere->GetCollisionEnabled() != ECollisionEnabled::NoCollision) != bExpectedAvailable)
	{
		return false;
	}
	for (int32 MaterialIndex = 0; MaterialIndex < LootDefinition.WorldMaterials.Num(); ++MaterialIndex)
	{
		UMaterialInterface* ExpectedMaterial = LootDefinition.WorldMaterials[MaterialIndex].LoadSynchronous();
		if (!IsValid(ExpectedMaterial) || VisualMesh->GetMaterial(MaterialIndex) != ExpectedMaterial)
		{
			return false;
		}
	}
	return true;
}

const FHeistInventoryItem* FindInventoryItemByRow(const UHeistInventoryComponent* InventoryComponent, const FName RowId)
{
	if (!IsValid(InventoryComponent))
	{
		return nullptr;
	}
	for (const FHeistInventoryFastArrayItem& Entry : InventoryComponent->GetReplicatedInventory().Items)
	{
		if (Entry.InventoryItem.ItemId == RowId)
		{
			return &Entry.InventoryItem;
		}
	}
	return nullptr;
}

bool IsInventoryLayoutValid(const UHeistInventoryComponent* InventoryComponent)
{
	if (!IsValid(InventoryComponent))
	{
		return false;
	}
	TArray<bool> OccupiedCells;
	OccupiedCells.Init(false, UHeistInventoryComponent::GridColumnCount * UHeistInventoryComponent::GridRowCount);
	for (const FHeistInventoryFastArrayItem& Entry : InventoryComponent->GetReplicatedInventory().Items)
	{
		const FHeistInventoryItem& Item = Entry.InventoryItem;
		const FIntPoint Size = Item.GetPlacedSize();
		if (Item.GridPosition.X < 0 || Item.GridPosition.Y < 0 || Size.X <= 0 || Size.Y <= 0 ||
			Item.GridPosition.X + Size.X > UHeistInventoryComponent::GridColumnCount || Item.GridPosition.Y + Size.Y > UHeistInventoryComponent::GridRowCount)
		{
			return false;
		}
		for (int32 Row = Item.GridPosition.Y; Row < Item.GridPosition.Y + Size.Y; ++Row)
		{
			for (int32 Column = Item.GridPosition.X; Column < Item.GridPosition.X + Size.X; ++Column)
			{
				const int32 CellIndex = Row * UHeistInventoryComponent::GridColumnCount + Column;
				if (OccupiedCells[CellIndex])
				{
					return false;
				}
				OccupiedCells[CellIndex] = true;
			}
		}
	}
	return true;
}

int32 GetExpectedValueThroughFixture(const TSharedRef<FHeistLootLifecycleAutomationState>& State, const int32 LastFixtureIndex)
{
	int32 TotalValue = 0;
	for (int32 FixtureIndex = 0; FixtureIndex <= LastFixtureIndex && State->Fixtures.IsValidIndex(FixtureIndex); ++FixtureIndex)
	{
		TotalValue += State->Fixtures[FixtureIndex].LootDefinition.ScoreValue;
	}
	return TotalValue;
}

float GetExpectedWeightThroughFixture(const TSharedRef<FHeistLootLifecycleAutomationState>& State, const int32 LastFixtureIndex)
{
	float TotalWeight = 0.0f;
	for (int32 FixtureIndex = 0; FixtureIndex <= LastFixtureIndex && State->Fixtures.IsValidIndex(FixtureIndex); ++FixtureIndex)
	{
		TotalWeight += State->Fixtures[FixtureIndex].ItemDefinition.Weight;
	}
	return TotalWeight;
}

bool DoesInventoryMatchHeldFixtures(const UHeistInventoryComponent* InventoryComponent, const TSharedRef<FHeistLootLifecycleAutomationState>& State,
	const int32 LastHeldFixtureIndex)
{
	const int32 ExpectedItemCount = LastHeldFixtureIndex + 1;
	if (!IsValid(InventoryComponent) || InventoryComponent->GetReplicatedInventory().Items.Num() != ExpectedItemCount || !IsInventoryLayoutValid(InventoryComponent))
	{
		return false;
	}
	for (int32 FixtureIndex = 0; FixtureIndex <= LastHeldFixtureIndex; ++FixtureIndex)
	{
		if (!State->Fixtures.IsValidIndex(FixtureIndex))
		{
			return false;
		}
		const FHeistLootLifecycleFixture& Fixture = State->Fixtures[FixtureIndex];
		const FHeistInventoryItem* InventoryItem = FindInventoryItemByRow(InventoryComponent, Fixture.RowId);
		if (InventoryItem == nullptr || InventoryItem->IsOriginalArtifact() || InventoryItem->BaseGridSize != Fixture.ItemDefinition.GridSize ||
			!FMath::IsNearlyEqual(InventoryItem->Weight, Fixture.ItemDefinition.Weight))
		{
			return false;
		}
	}
	return true;
}

bool AreHeldLootStateAndContractReplicated(const TSharedRef<FHeistLootLifecycleAutomationState>& State, const int32 LastHeldFixtureIndex)
{
	const int32 ExpectedValue = GetExpectedValueThroughFixture(State, LastHeldFixtureIndex);
	const float ExpectedWeight = GetExpectedWeightThroughFixture(State, LastHeldFixtureIndex);
	UWorld* ServerWorld = GetLootLifecycleServerWorld();
	AHeistPlayerCharacter* ServerCharacter = FindHeistCharacterById(ServerWorld, LootCarrierPlayerId);
	AHeistPlayerController* OwningPlayerController = GetOwningPlayerControllerById(LootCarrierPlayerId);
	AHeistPlayerCharacter* OwningCharacter = IsValid(OwningPlayerController) ? FindHeistCharacterById(OwningPlayerController->GetWorld(), LootCarrierPlayerId) : nullptr;
	const UHeistInventoryComponent* ServerInventory = IsValid(ServerCharacter) ? ServerCharacter->GetInventoryComponent() : nullptr;
	const UHeistInventoryComponent* OwningInventory = IsValid(OwningCharacter) ? OwningCharacter->GetInventoryComponent() : nullptr;
	if (!DoesInventoryMatchHeldFixtures(ServerInventory, State, LastHeldFixtureIndex) || !DoesInventoryMatchHeldFixtures(OwningInventory, State, LastHeldFixtureIndex))
	{
		return false;
	}
	for (UWorld* World : GetLootLifecyclePIEWorlds())
	{
		const AHeistPlayerState* PlayerState = FindHeistPlayerStateById(World, LootCarrierPlayerId);
		const AHeistGameState* GameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
		if (!IsValid(PlayerState) || PlayerState->GetTotalLootScore() != ExpectedValue || !FMath::IsNearlyEqual(PlayerState->GetTotalLootWeight(), ExpectedWeight) ||
			!IsValid(GameState) || GameState->GetContractSnapshot().CarriedValue != ExpectedValue)
		{
			return false;
		}
	}
	return true;
}

bool AreDroppedLootStateAndContractReplicated(const TSharedRef<FHeistLootLifecycleAutomationState>& State, const int32 FixtureIndex)
{
	if (!State->Fixtures.IsValidIndex(FixtureIndex))
	{
		return false;
	}
	FHeistLootLifecycleFixture& Fixture = State->Fixtures[FixtureIndex];
	const int32 PreviousFixtureIndex = FixtureIndex - 1;
	const int32 ExpectedValue = GetExpectedValueThroughFixture(State, PreviousFixtureIndex);
	const float ExpectedWeight = GetExpectedWeightThroughFixture(State, PreviousFixtureIndex);
	UWorld* ServerWorld = GetLootLifecycleServerWorld();
	AHeistPlayerCharacter* ServerCharacter = FindHeistCharacterById(ServerWorld, LootCarrierPlayerId);
	AHeistPlayerController* OwningPlayerController = GetOwningPlayerControllerById(LootCarrierPlayerId);
	AHeistPlayerCharacter* OwningCharacter = IsValid(OwningPlayerController) ? FindHeistCharacterById(OwningPlayerController->GetWorld(), LootCarrierPlayerId) : nullptr;
	const UHeistInventoryComponent* ServerInventory = IsValid(ServerCharacter) ? ServerCharacter->GetInventoryComponent() : nullptr;
	const UHeistInventoryComponent* OwningInventory = IsValid(OwningCharacter) ? OwningCharacter->GetInventoryComponent() : nullptr;
	if (!DoesInventoryMatchHeldFixtures(ServerInventory, State, PreviousFixtureIndex) || !DoesInventoryMatchHeldFixtures(OwningInventory, State, PreviousFixtureIndex))
	{
		return false;
	}

	AHeistLootActor* DroppedLootActor = FindAvailableLootActorByRow(ServerWorld, Fixture.RowId, Fixture.OriginalActorName);
	if (!IsValid(DroppedLootActor))
	{
		return false;
	}
	Fixture.DroppedActorName = DroppedLootActor->GetFName();
	Fixture.DroppedSpawnLocation = DroppedLootActor->GetActorLocation();
	for (UWorld* World : GetLootLifecyclePIEWorlds())
	{
		const AHeistPlayerState* PlayerState = FindHeistPlayerStateById(World, LootCarrierPlayerId);
		const AHeistGameState* GameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
		const AHeistLootActor* OriginalLootActor = FindLootActorByRowAndLocation(World, Fixture.RowId, Fixture.OriginalSpawnLocation);
		const AHeistLootActor* ReplicatedDroppedLootActor = FindLootActorByRowAndLocation(World, Fixture.RowId, Fixture.DroppedSpawnLocation);
		if (!IsValid(PlayerState) || PlayerState->GetTotalLootScore() != ExpectedValue || !FMath::IsNearlyEqual(PlayerState->GetTotalLootWeight(), ExpectedWeight) ||
			!IsValid(GameState) || GameState->GetContractSnapshot().CarriedValue != ExpectedValue ||
			!IsLootWorldVisualResolved(OriginalLootActor, Fixture.ItemDefinition, Fixture.LootDefinition, false) ||
			!IsLootWorldVisualResolved(ReplicatedDroppedLootActor, Fixture.ItemDefinition, Fixture.LootDefinition, true))
		{
			return false;
		}
	}
	return true;
}

bool IsPickedActorAndFeedbackReplicated(const TSharedRef<FHeistLootLifecycleAutomationState>& State, const int32 FixtureIndex,
	const FVector& ServerSpawnLocation, const int32 ExpectedFeedbackCount, const bool bCaptureInitialInstanceId)
{
	if (!State->Fixtures.IsValidIndex(FixtureIndex) || State->FeedbackCounts.FindRef(State->Fixtures[FixtureIndex].RowId) < ExpectedFeedbackCount ||
		!AreHeldLootStateAndContractReplicated(State, FixtureIndex))
	{
		return false;
	}
	FHeistLootLifecycleFixture& Fixture = State->Fixtures[FixtureIndex];
	for (UWorld* World : GetLootLifecyclePIEWorlds())
	{
		if (!IsLootWorldVisualResolved(FindLootActorByRowAndLocation(World, Fixture.RowId, ServerSpawnLocation), Fixture.ItemDefinition,
			Fixture.LootDefinition, false))
		{
			return false;
		}
	}

	const AHeistPlayerCharacter* ServerCharacter = FindHeistCharacterById(GetLootLifecycleServerWorld(), LootCarrierPlayerId);
	const UHeistInventoryComponent* ServerInventory = IsValid(ServerCharacter) ? ServerCharacter->GetInventoryComponent() : nullptr;
	const FHeistInventoryItem* InventoryItem = FindInventoryItemByRow(ServerInventory, Fixture.RowId);
	if (InventoryItem == nullptr)
	{
		return false;
	}
	if (bCaptureInitialInstanceId)
	{
		Fixture.InitialInstanceId = InventoryItem->InstanceId;
	}
	else
	{
		Fixture.RepickedInstanceId = InventoryItem->InstanceId;
		if (Fixture.InitialInstanceId == INDEX_NONE || Fixture.RepickedInstanceId == Fixture.InitialInstanceId)
		{
			return false;
		}
	}
	return true;
}

bool CaptureLifecycleFixtures(FAutomationTestBase* Test, const TSharedRef<FHeistLootLifecycleAutomationState>& State)
{
	const auto WaitFor = [State](FString Reason)
	{
		if (State->LastCaptureWaitReason != Reason)
		{
			State->LastCaptureWaitReason = MoveTemp(Reason);
			UE_LOG(LogTemp, Display, TEXT("W6-011 fixture capture waiting: %s"), *State->LastCaptureWaitReason);
		}
		return false;
	};
	UWorld* ServerWorld = GetLootLifecycleServerWorld();
	AHeistGameMode* GameMode = IsValid(ServerWorld) ? ServerWorld->GetAuthGameMode<AHeistGameMode>() : nullptr;
	const AHeistGameState* ServerGameState = IsValid(ServerWorld) ? ServerWorld->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(GameMode) || !IsValid(ServerGameState) || !ServerGameState->IsContractInitialized() || ServerGameState->GetContractSnapshot().MapId != FName(TEXT("M01")))
	{
		return WaitFor(TEXT("contract initialization"));
	}
	bool bFoundGuard = false;
	for (TActorIterator<AHeistGuardCharacter> It(ServerWorld); It; ++It)
	{
		if (UHeistGuardStateComponent* GuardState = IsValid(*It) ? It->GetGuardStateComponent() : nullptr; IsValid(GuardState))
		{
			if (GuardState->GetGuardState() != EHeistGuardState::Disabled)
			{
				GuardState->SetDisabled(true);
			}
			bFoundGuard |= GuardState->GetGuardState() == EHeistGuardState::Disabled;
		}
	}
	if (!bFoundGuard)
	{
		return WaitFor(TEXT("guard disable"));
	}

	AHeistPlayerCharacter* FixtureOwner = FindHeistCharacterById(ServerWorld, LootCarrierPlayerId);
	if (!IsValid(FixtureOwner))
	{
		return WaitFor(TEXT("fixture owner"));
	}

	if (!State->bMatchStartSupplyValidated)
	{
		int32 ExpectedVaultLootCount = 0;
		int32 ExpectedExhibitionLootCount = 0;
		GameMode->GetMatchStartLooseLootCounts(ExpectedVaultLootCount, ExpectedExhibitionLootCount);
		const int32 ExpectedMatchStartLootCount = ExpectedVaultLootCount + ExpectedExhibitionLootCount;
		TArray<AHeistLootActor*> MatchStartLootActors;
		int32 EnabledSpawnPointCount = 0;
		for (TActorIterator<AHeistLootActor> It(ServerWorld); It; ++It)
		{
			AHeistLootActor* LootActor = *It;
			if (IsValid(LootActor) && LootActor->ActorHasTag(FName(TEXT("HeistMatchSpawnedLooseLoot"))) && LootActor->IsLootAvailable())
			{
				MatchStartLootActors.Add(LootActor);
			}
		}
		for (TActorIterator<AHeistLootSpawnPoint> It(ServerWorld); It; ++It)
		{
			EnabledSpawnPointCount += IsValid(*It) && It->IsSpawnEnabled() ? 1 : 0;
		}
		if (ExpectedMatchStartLootCount <= 0 || MatchStartLootActors.Num() != ExpectedMatchStartLootCount || EnabledSpawnPointCount < ExpectedMatchStartLootCount)
		{
			return WaitFor(FString::Printf(TEXT("match supply count expected=%d actors=%d spawnpoints=%d"), ExpectedMatchStartLootCount,
				MatchStartLootActors.Num(), EnabledSpawnPointCount));
		}

		int32 VaultLootCount = 0;
		int32 ExhibitionLootCount = 0;
		for (AHeistLootActor* ServerLootActor : MatchStartLootActors)
		{
			FHeistItemDataRow ItemDefinition;
			FHeistLootDataRow LootDefinition;
			const FName RowId = ServerLootActor->GetLootRowId();
			if (!GameMode->TryGetItemDefinition(RowId, ItemDefinition) || !GameMode->TryGetLootDefinition(RowId, LootDefinition) ||
				ItemDefinition.ItemType != EHeistItemType::Loot || !ItemDefinition.bAvailableInV1 || LootDefinition.ScoreValue <= 0)
			{
				return WaitFor(FString::Printf(TEXT("match supply definition row=%s"), *RowId.ToString()));
			}
			if (LootDefinition.SpawnCategory == EHeistSpawnCategory::VaultFixed)
			{
				++VaultLootCount;
			}
			else if (LootDefinition.SpawnCategory == EHeistSpawnCategory::ExhibitionRoom)
			{
				++ExhibitionLootCount;
			}
			else
			{
				return WaitFor(FString::Printf(TEXT("match supply category row=%s"), *RowId.ToString()));
			}
			int32 MatchingSpawnPointCount = 0;
			for (TActorIterator<AHeistLootSpawnPoint> It(ServerWorld); It; ++It)
			{
				AHeistLootSpawnPoint* SpawnPoint = *It;
				if (IsValid(SpawnPoint) && SpawnPoint->GetSpawnCategory() == LootDefinition.SpawnCategory &&
					FVector::DistSquared(SpawnPoint->GetActorLocation(), ServerLootActor->GetActorLocation()) <= 1.0f)
				{
					++MatchingSpawnPointCount;
					if (!SpawnPoint->IsOccupied() || SpawnPoint->CanSpawnCategory(LootDefinition.SpawnCategory))
					{
						return WaitFor(FString::Printf(TEXT("match supply occupancy row=%s spawn=%s"), *RowId.ToString(), *GetNameSafe(SpawnPoint)));
					}
				}
			}
			if (MatchingSpawnPointCount != 1)
			{
				return WaitFor(FString::Printf(TEXT("match supply location row=%s matches=%d"), *RowId.ToString(), MatchingSpawnPointCount));
			}
			for (UWorld* World : GetLootLifecyclePIEWorlds())
			{
				const AHeistLootActor* ReplicatedLootActor = FindLootActorByRowAndLocation(World, RowId, ServerLootActor->GetActorLocation());
				if (!IsLootWorldVisualResolved(ReplicatedLootActor, ItemDefinition, LootDefinition, true))
				{
					const UStaticMeshComponent* VisualMesh = IsValid(ReplicatedLootActor) ? ReplicatedLootActor->FindComponentByClass<UStaticMeshComponent>() : nullptr;
					const USphereComponent* InteractionSphere = IsValid(ReplicatedLootActor) ? ReplicatedLootActor->FindComponentByClass<USphereComponent>() : nullptr;
					UStaticMesh* ExpectedMesh = LootDefinition.WorldMesh.LoadSynchronous();
					return WaitFor(FString::Printf(
						TEXT("match supply visual row=%s actor=%s world=%s actualRow=%s score=%d/%d weight=%.2f/%.2f class=%s mesh=%s/%s transform=%s visible=%s collision=%d available=%s"),
						*RowId.ToString(), *ServerLootActor->GetName(), *GetNameSafe(World),
						IsValid(ReplicatedLootActor) ? *ReplicatedLootActor->GetLootRowId().ToString() : TEXT("Invalid"),
						IsValid(ReplicatedLootActor) ? ReplicatedLootActor->GetScoreValue() : -1, LootDefinition.ScoreValue,
						IsValid(ReplicatedLootActor) ? ReplicatedLootActor->GetWeightValue() : -1.0f, ItemDefinition.Weight,
						*GetNameSafe(IsValid(ReplicatedLootActor) ? ReplicatedLootActor->GetClass() : nullptr),
						*GetNameSafe(IsValid(VisualMesh) ? VisualMesh->GetStaticMesh() : nullptr), *GetNameSafe(ExpectedMesh),
						IsValid(VisualMesh) && VisualMesh->GetRelativeTransform().Equals(LootDefinition.WorldVisualRelativeTransform, KINDA_SMALL_NUMBER)
							? TEXT("PASS") : TEXT("FAIL"),
						IsValid(VisualMesh) && VisualMesh->IsVisible() ? TEXT("true") : TEXT("false"),
						IsValid(InteractionSphere) ? static_cast<int32>(InteractionSphere->GetCollisionEnabled()) : -1,
						IsValid(ReplicatedLootActor) && ReplicatedLootActor->IsLootAvailable() ? TEXT("true") : TEXT("false")));
				}
			}
		}
		if (VaultLootCount != ExpectedVaultLootCount || ExhibitionLootCount != ExpectedExhibitionLootCount)
		{
			return WaitFor(FString::Printf(TEXT("match supply categories vault=%d/%d exhibition=%d/%d"), VaultLootCount, ExpectedVaultLootCount,
				ExhibitionLootCount, ExpectedExhibitionLootCount));
		}

		State->bMatchStartSupplyValidated = true;
		State->LastCaptureWaitReason.Reset();
		Test->AddInfo(FString::Printf(TEXT("W6-011 match-start supply: Map=M01 Players=2 Loot=%d Vault=%d Exhibition=%d SpawnPointCategory=PASS HostClientVisual=PASS"),
			ExpectedMatchStartLootCount, VaultLootCount, ExhibitionLootCount));
		return false;
	}

	TArray<FHeistLootLifecycleFixture> Fixtures = State->Fixtures;
	int32 TotalGridArea = 0;
	int32 TotalValue = 0;
	float TotalWeight = 0.0f;
	if (!State->bRuntimeFixturesSpawned)
	{
		Fixtures.Reset(GetLifecycleLootRowIds().Num());
		TArray<TWeakObjectPtr<AHeistLootActor>> SpawnedFixtures;
		for (int32 RowIndex = 0; RowIndex < GetLifecycleLootRowIds().Num(); ++RowIndex)
		{
			const FName RowId = GetLifecycleLootRowIds()[RowIndex];
			FHeistLootLifecycleFixture Fixture;
			Fixture.RowId = RowId;
			if (!GameMode->TryGetItemDefinition(RowId, Fixture.ItemDefinition) || !GameMode->TryGetLootDefinition(RowId, Fixture.LootDefinition) ||
				Fixture.ItemDefinition.ItemId != RowId || Fixture.LootDefinition.ItemId != RowId || Fixture.ItemDefinition.ItemType != EHeistItemType::Loot ||
				!Fixture.ItemDefinition.bAvailableInV1 || Fixture.ItemDefinition.GridSize.X <= 0 || Fixture.ItemDefinition.GridSize.Y <= 0 ||
				!FMath::IsFinite(Fixture.ItemDefinition.Weight) || Fixture.ItemDefinition.Weight <= 0.0f || Fixture.LootDefinition.ScoreValue <= 0)
			{
				for (const TWeakObjectPtr<AHeistLootActor>& SpawnedFixture : SpawnedFixtures)
				{
					if (SpawnedFixture.IsValid())
					{
						SpawnedFixture->Destroy();
					}
				}
				return WaitFor(FString::Printf(TEXT("fixture definition row=%s"), *RowId.ToString()));
			}

			FHeistLootDropRequest DropRequest;
			DropRequest.DroppedBy = FixtureOwner;
			DropRequest.ItemId = RowId;
			DropRequest.DropOrigin = FixtureOwner->GetActorLocation() + FVector(150.0f + 125.0f * RowIndex, 150.0f, 20.0f);
			AHeistLootActor* ServerLootActor = nullptr;
			if (!GameMode->TrySpawnDroppedLoot(DropRequest, ServerLootActor) || !IsValid(ServerLootActor))
			{
				for (const TWeakObjectPtr<AHeistLootActor>& SpawnedFixture : SpawnedFixtures)
				{
					if (SpawnedFixture.IsValid())
					{
						SpawnedFixture->Destroy();
					}
				}
				return WaitFor(FString::Printf(TEXT("fixture spawn row=%s"), *RowId.ToString()));
			}
			ServerLootActor->ForceNetUpdate();
			SpawnedFixtures.Add(ServerLootActor);
			Fixture.OriginalActorName = ServerLootActor->GetFName();
			Fixture.OriginalSpawnLocation = ServerLootActor->GetActorLocation();
			Fixtures.Add(MoveTemp(Fixture));
		}
		State->Fixtures = Fixtures;
		State->bRuntimeFixturesSpawned = true;
		return WaitFor(TEXT("fixture replication"));
	}

	for (FHeistLootLifecycleFixture& Fixture : Fixtures)
	{
		AHeistLootActor* ServerLootActor = FindLootActorByName(ServerWorld, Fixture.OriginalActorName);
		if (!IsValid(ServerLootActor) || !ServerLootActor->IsLootAvailable() || ServerLootActor->GetLootRowId() != Fixture.RowId)
		{
			return WaitFor(FString::Printf(TEXT("fixture server state row=%s actor=%s"), *Fixture.RowId.ToString(), *Fixture.OriginalActorName.ToString()));
		}
		for (UWorld* World : GetLootLifecyclePIEWorlds())
		{
			if (!IsLootWorldVisualResolved(FindLootActorByRowAndLocation(World, Fixture.RowId, Fixture.OriginalSpawnLocation), Fixture.ItemDefinition,
				Fixture.LootDefinition, true))
			{
				return WaitFor(FString::Printf(TEXT("fixture visual row=%s actor=%s world=%s"), *Fixture.RowId.ToString(),
					*Fixture.OriginalActorName.ToString(), *GetNameSafe(World)));
			}
		}
		TotalGridArea += Fixture.ItemDefinition.GridSize.X * Fixture.ItemDefinition.GridSize.Y;
		TotalValue += Fixture.LootDefinition.ScoreValue;
		TotalWeight += Fixture.ItemDefinition.Weight;
	}
	const int32 GridCapacity = UHeistInventoryComponent::GridColumnCount * UHeistInventoryComponent::GridRowCount;
	if (Fixtures.Num() != 5 || TotalGridArea > GridCapacity)
	{
		return WaitFor(FString::Printf(TEXT("fixture inventory rows=%d area=%d/%d"), Fixtures.Num(), TotalGridArea, GridCapacity));
	}

	AHeistPlayerController* FeedbackController = GetOwningPlayerControllerById(LootCarrierPlayerId);
	if (!IsValid(FeedbackController))
	{
		return WaitFor(TEXT("feedback controller"));
	}
	State->Fixtures = MoveTemp(Fixtures);
	State->LastCaptureWaitReason.Reset();
	State->FeedbackController = FeedbackController;
	const TSharedPtr<FHeistLootLifecycleAutomationState> SharedState = State;
	const TWeakPtr<FHeistLootLifecycleAutomationState> WeakState = SharedState;
	State->FeedbackHandle = FeedbackController->GetPopupFeedbackRequestedDelegate().AddLambda([WeakState](const FText& Message, const float DurationSeconds)
	{
		const TSharedPtr<FHeistLootLifecycleAutomationState> PinnedState = WeakState.Pin();
		if (!PinnedState.IsValid())
		{
			return;
		}
		const FText ExpectedMessage = FText::Format(NSLOCTEXT("HeistFeedback", "LootPickupAccepted", "전리품 획득 +{0}"), FText::AsNumber(PinnedState->PendingFeedbackValue));
		if (!PinnedState->PendingFeedbackRowId.IsNone() && DurationSeconds > 0.0f && Message.ToString() == ExpectedMessage.ToString())
		{
			++PinnedState->FeedbackCounts.FindOrAdd(PinnedState->PendingFeedbackRowId);
			return;
		}
		PinnedState->UnexpectedFeedback.Add(FString::Printf(TEXT("PendingRow=%s Expected=%s Actual=%s Duration=%.2f"),
			*PinnedState->PendingFeedbackRowId.ToString(), *ExpectedMessage.ToString(), *Message.ToString(), DurationSeconds));
	});

	Test->AddInfo(FString::Printf(TEXT("W6-011 row lifecycle fixture: Map=M01 Players=2 Rows=5 GridArea=%d/%d Value=%d Weight=%.1f SharedShell=BP_Loot HostClientVisual=PASS"),
		TotalGridArea, GridCapacity, TotalValue, TotalWeight));
	return true;
}

class FHeistLootLifecycleWaitCommand final : public IAutomationLatentCommand
{
  public:
	FHeistLootLifecycleWaitCommand(FAutomationTestBase* InTest, const TSharedRef<FHeistLootLifecycleAutomationState>& InState, FString InDescription,
		TFunction<bool()> InPredicate, const double InTimeoutSeconds)
		: Test(InTest), State(InState), Description(MoveTemp(InDescription)), Predicate(MoveTemp(InPredicate)), TimeoutSeconds(InTimeoutSeconds)
	{
	}

	virtual bool Update() override
	{
		if (State->bAborted)
		{
			return true;
		}
		if (StartSeconds <= 0.0)
		{
			StartSeconds = FPlatformTime::Seconds();
		}
		if (Predicate())
		{
			Test->AddInfo(FString::Printf(TEXT("W6-011 wait: %s"), *Description));
			return true;
		}
		if (FPlatformTime::Seconds() - StartSeconds >= TimeoutSeconds)
		{
			Test->AddError(FString::Printf(TEXT("W6-011 wait timed out: %s (%.1fs)"), *Description, TimeoutSeconds));
			State->bAborted = true;
			return true;
		}
		return false;
	}

  private:
	FAutomationTestBase* Test = nullptr;
	TSharedRef<FHeistLootLifecycleAutomationState> State;
	FString Description;
	TFunction<bool()> Predicate;
	double TimeoutSeconds = 0.0;
	double StartSeconds = 0.0;
};

class FHeistLootLifecycleActionCommand final : public IAutomationLatentCommand
{
  public:
	FHeistLootLifecycleActionCommand(FAutomationTestBase* InTest, const TSharedRef<FHeistLootLifecycleAutomationState>& InState, FString InDescription,
		TFunction<bool()> InAction, const bool bInRunAfterAbort = false)
		: Test(InTest), State(InState), Description(MoveTemp(InDescription)), Action(MoveTemp(InAction)), bRunAfterAbort(bInRunAfterAbort)
	{
	}

	virtual bool Update() override
	{
		if (State->bAborted && !bRunAfterAbort)
		{
			return true;
		}
		const bool bPassed = Action();
		if (bPassed)
		{
			Test->AddInfo(FString::Printf(TEXT("W6-011 action: %s"), *Description));
		}
		else
		{
			Test->AddError(FString::Printf(TEXT("W6-011 action failed: %s"), *Description));
			State->bAborted = true;
		}
		return true;
	}

  private:
	FAutomationTestBase* Test = nullptr;
	TSharedRef<FHeistLootLifecycleAutomationState> State;
	FString Description;
	TFunction<bool()> Action;
	bool bRunAfterAbort = false;
};

void AppendLootFixtureLifecycleCommands(FAutomationTestBase* Test, const TSharedRef<FHeistLootLifecycleAutomationState>& State, const int32 FixtureIndex)
{
	Test->AddCommand(new FHeistLootLifecycleActionCommand(Test, State, FString::Printf(TEXT("move client carrier to fixture %d original BP_Loot"), FixtureIndex + 1),
		[State, FixtureIndex]()
	{
		return State->Fixtures.IsValidIndex(FixtureIndex) &&
			TeleportServerPlayerIntoInteraction(LootCarrierPlayerId, FindLootActorByName(GetLootLifecycleServerWorld(), State->Fixtures[FixtureIndex].OriginalActorName));
	}));
	Test->AddCommand(new FHeistLootLifecycleWaitCommand(Test, State, FString::Printf(TEXT("fixture %d original overlap"), FixtureIndex + 1), [State, FixtureIndex]()
	{
		return State->Fixtures.IsValidIndex(FixtureIndex) &&
			IsServerPlayerOverlapping(LootCarrierPlayerId, FindLootActorByName(GetLootLifecycleServerWorld(), State->Fixtures[FixtureIndex].OriginalActorName));
	}, 10.0));
	Test->AddCommand(new FHeistLootLifecycleActionCommand(Test, State, FString::Printf(TEXT("fixture %d initial pickup through owning-client RPC"), FixtureIndex + 1),
		[State, FixtureIndex]()
	{
		if (!State->Fixtures.IsValidIndex(FixtureIndex))
		{
			return false;
		}
		const FHeistLootLifecycleFixture& Fixture = State->Fixtures[FixtureIndex];
		AHeistPlayerController* OwningPlayerController = GetOwningPlayerControllerById(LootCarrierPlayerId);
		AHeistLootActor* LocalLootActor = IsValid(OwningPlayerController)
			? FindLootActorByRowAndLocation(OwningPlayerController->GetWorld(), Fixture.RowId, Fixture.OriginalSpawnLocation)
			: nullptr;
		State->PendingFeedbackRowId = Fixture.RowId;
		State->PendingFeedbackValue = Fixture.LootDefinition.ScoreValue;
		return InvokeSingleActorServerRPC(OwningPlayerController, FName(TEXT("Server_RequestLootPickup")), LocalLootActor);
	}));
	Test->AddCommand(new FHeistLootLifecycleWaitCommand(Test, State,
		FString::Printf(TEXT("fixture %d initial pickup inventory, feedback and unavailable replication"), FixtureIndex + 1), [State, FixtureIndex]()
	{
		return State->Fixtures.IsValidIndex(FixtureIndex) &&
			IsPickedActorAndFeedbackReplicated(State, FixtureIndex, State->Fixtures[FixtureIndex].OriginalSpawnLocation, 1, true);
	}, 15.0));

	Test->AddCommand(new FHeistLootLifecycleActionCommand(Test, State, FString::Printf(TEXT("fixture %d open owning inventory for drop"), FixtureIndex + 1), []()
	{
		AHeistPlayerController* OwningPlayerController = GetOwningPlayerControllerById(LootCarrierPlayerId);
		if (!IsValid(OwningPlayerController))
		{
			return false;
		}
		OwningPlayerController->RequestSetInventoryOpen(true);
		return true;
	}));
	Test->AddCommand(new FHeistLootLifecycleWaitCommand(Test, State, FString::Printf(TEXT("fixture %d inventory open on authority and owner"), FixtureIndex + 1), []()
	{
		const AHeistPlayerCharacter* ServerCharacter = FindHeistCharacterById(GetLootLifecycleServerWorld(), LootCarrierPlayerId);
		AHeistPlayerController* OwningPlayerController = GetOwningPlayerControllerById(LootCarrierPlayerId);
		const AHeistPlayerCharacter* OwningCharacter = IsValid(OwningPlayerController) ? FindHeistCharacterById(OwningPlayerController->GetWorld(), LootCarrierPlayerId) : nullptr;
		const UHeistInventoryComponent* ServerInventory = IsValid(ServerCharacter) ? ServerCharacter->GetInventoryComponent() : nullptr;
		const UHeistInventoryComponent* OwningInventory = IsValid(OwningCharacter) ? OwningCharacter->GetInventoryComponent() : nullptr;
		return IsValid(ServerInventory) && ServerInventory->IsInventoryOpen() && IsValid(OwningInventory) && OwningInventory->IsInventoryOpen();
	}, 10.0));
	Test->AddCommand(new FHeistLootLifecycleActionCommand(Test, State, FString::Printf(TEXT("fixture %d drop through production inventory request"), FixtureIndex + 1),
		[State, FixtureIndex]()
	{
		if (!State->Fixtures.IsValidIndex(FixtureIndex))
		{
			return false;
		}
		AHeistPlayerController* OwningPlayerController = GetOwningPlayerControllerById(LootCarrierPlayerId);
		if (!IsValid(OwningPlayerController) || State->Fixtures[FixtureIndex].InitialInstanceId == INDEX_NONE)
		{
			return false;
		}
		OwningPlayerController->RequestDropInventoryItem(State->Fixtures[FixtureIndex].InitialInstanceId);
		return true;
	}));
	Test->AddCommand(new FHeistLootLifecycleWaitCommand(Test, State,
		FString::Printf(TEXT("fixture %d drop removes inventory and replicates a visible BP_Loot"), FixtureIndex + 1), [State, FixtureIndex]()
	{
		return AreDroppedLootStateAndContractReplicated(State, FixtureIndex);
	}, 15.0));
	Test->AddCommand(new FHeistLootLifecycleActionCommand(Test, State, FString::Printf(TEXT("fixture %d close inventory before re-pickup"), FixtureIndex + 1), []()
	{
		AHeistPlayerController* OwningPlayerController = GetOwningPlayerControllerById(LootCarrierPlayerId);
		if (!IsValid(OwningPlayerController))
		{
			return false;
		}
		OwningPlayerController->RequestSetInventoryOpen(false);
		return true;
	}));
	Test->AddCommand(new FHeistLootLifecycleWaitCommand(Test, State, FString::Printf(TEXT("fixture %d inventory closed on authority and owner"), FixtureIndex + 1), []()
	{
		const AHeistPlayerCharacter* ServerCharacter = FindHeistCharacterById(GetLootLifecycleServerWorld(), LootCarrierPlayerId);
		AHeistPlayerController* OwningPlayerController = GetOwningPlayerControllerById(LootCarrierPlayerId);
		const AHeistPlayerCharacter* OwningCharacter = IsValid(OwningPlayerController) ? FindHeistCharacterById(OwningPlayerController->GetWorld(), LootCarrierPlayerId) : nullptr;
		const UHeistInventoryComponent* ServerInventory = IsValid(ServerCharacter) ? ServerCharacter->GetInventoryComponent() : nullptr;
		const UHeistInventoryComponent* OwningInventory = IsValid(OwningCharacter) ? OwningCharacter->GetInventoryComponent() : nullptr;
		return IsValid(ServerInventory) && !ServerInventory->IsInventoryOpen() && IsValid(OwningInventory) && !OwningInventory->IsInventoryOpen();
	}, 10.0));

	Test->AddCommand(new FHeistLootLifecycleActionCommand(Test, State, FString::Printf(TEXT("move client carrier to fixture %d dropped BP_Loot"), FixtureIndex + 1),
		[State, FixtureIndex]()
	{
		return State->Fixtures.IsValidIndex(FixtureIndex) &&
			TeleportServerPlayerIntoInteraction(LootCarrierPlayerId, FindLootActorByName(GetLootLifecycleServerWorld(), State->Fixtures[FixtureIndex].DroppedActorName));
	}));
	Test->AddCommand(new FHeistLootLifecycleWaitCommand(Test, State, FString::Printf(TEXT("fixture %d dropped loot overlap"), FixtureIndex + 1), [State, FixtureIndex]()
	{
		return State->Fixtures.IsValidIndex(FixtureIndex) &&
			IsServerPlayerOverlapping(LootCarrierPlayerId, FindLootActorByName(GetLootLifecycleServerWorld(), State->Fixtures[FixtureIndex].DroppedActorName));
	}, 10.0));
	Test->AddCommand(new FHeistLootLifecycleActionCommand(Test, State, FString::Printf(TEXT("fixture %d re-pickup through owning-client RPC"), FixtureIndex + 1),
		[State, FixtureIndex]()
	{
		if (!State->Fixtures.IsValidIndex(FixtureIndex))
		{
			return false;
		}
		const FHeistLootLifecycleFixture& Fixture = State->Fixtures[FixtureIndex];
		AHeistPlayerController* OwningPlayerController = GetOwningPlayerControllerById(LootCarrierPlayerId);
		AHeistLootActor* LocalLootActor = IsValid(OwningPlayerController)
			? FindLootActorByRowAndLocation(OwningPlayerController->GetWorld(), Fixture.RowId, Fixture.DroppedSpawnLocation)
			: nullptr;
		State->PendingFeedbackRowId = Fixture.RowId;
		State->PendingFeedbackValue = Fixture.LootDefinition.ScoreValue;
		return InvokeSingleActorServerRPC(OwningPlayerController, FName(TEXT("Server_RequestLootPickup")), LocalLootActor);
	}));
	Test->AddCommand(new FHeistLootLifecycleWaitCommand(Test, State,
		FString::Printf(TEXT("fixture %d re-pickup inventory, second feedback and unavailable replication"), FixtureIndex + 1), [State, FixtureIndex]()
	{
		return State->Fixtures.IsValidIndex(FixtureIndex) &&
			IsPickedActorAndFeedbackReplicated(State, FixtureIndex, State->Fixtures[FixtureIndex].DroppedSpawnLocation, 2, false);
	}, 15.0));
	Test->AddCommand(new FHeistLootLifecycleActionCommand(Test, State, FString::Printf(TEXT("record fixture %d lifecycle evidence"), FixtureIndex + 1),
		[Test, State, FixtureIndex]()
	{
		if (!State->Fixtures.IsValidIndex(FixtureIndex))
		{
			return false;
		}
		const FHeistLootLifecycleFixture& Fixture = State->Fixtures[FixtureIndex];
		Test->AddInfo(FString::Printf(
			TEXT("W6-011 row evidence: Row=%s Value=%d Grid=%dx%d InitialActor=%s DroppedActor=%s InitialInstance=%d RepickedInstance=%d PickupFeedback=2 HostClientVisual=PASS Inventory=PASS DropRespawn=PASS Result=PASS"),
			*Fixture.RowId.ToString(), Fixture.LootDefinition.ScoreValue, Fixture.ItemDefinition.GridSize.X, Fixture.ItemDefinition.GridSize.Y,
			*Fixture.OriginalActorName.ToString(), *Fixture.DroppedActorName.ToString(), Fixture.InitialInstanceId, Fixture.RepickedInstanceId));
		return true;
	}));
}

bool EnqueueTwoPlayerLootLifecycleScenario(FAutomationTestBase* Test)
{
	const TSharedRef<FHeistLootLifecycleAutomationState> State = MakeShared<FHeistLootLifecycleAutomationState>();
	Test->AddCommand(new FEditorLoadMap(TEXT("/Game/Maps/TitleMenuMap")));
	Test->AddCommand(new FHeistLootLifecycleActionCommand(Test, State, TEXT("configure two-player listen-server PIE"), [State]()
	{
		ULevelEditorPlaySettings* PlaySettings = GetMutableDefault<ULevelEditorPlaySettings>();
		if (!IsValid(PlaySettings))
		{
			return false;
		}
		PlaySettings->GetPlayNetMode(State->OriginalNetMode);
		PlaySettings->GetRunUnderOneProcess(State->bOriginalRunUnderOneProcess);
		PlaySettings->GetPlayNumberOfClients(State->OriginalClientCount);
		State->bCapturedPlaySettings = true;
		PlaySettings->SetRunUnderOneProcess(true);
		PlaySettings->SetPlayNetMode(EPlayNetMode::PIE_ListenServer);
		PlaySettings->SetPlayNumberOfClients(2);
		return true;
	}));
	Test->AddCommand(new FStartPIECommand(false));
	Test->AddCommand(new FHeistLootLifecycleWaitCommand(Test, State, TEXT("two title worlds"), []()
	{
		const TArray<UWorld*> Worlds = GetLootLifecyclePIEWorlds();
		if (Worlds.Num() != 2 || !IsValid(GetLootLifecycleServerWorld()))
		{
			return false;
		}
		for (UWorld* World : Worlds)
		{
			if (!IsValid(GetLootLifecycleLocalHeistPlayerController(World)))
			{
				return false;
			}
		}
		return true;
	}, 45.0));
	Test->AddCommand(new FHeistLootLifecycleActionCommand(Test, State, TEXT("create host session"), []()
	{
		UWorld* ServerWorld = GetLootLifecycleServerWorld();
		UHeistGameInstance* GameInstance = IsValid(ServerWorld) ? Cast<UHeistGameInstance>(ServerWorld->GetGameInstance()) : nullptr;
		return IsValid(GameInstance) && GameInstance->RequestHostSession();
	}));
	Test->AddCommand(new FHeistLootLifecycleWaitCommand(Test, State, TEXT("lobby session and two players"), []()
	{
		UWorld* ServerWorld = GetLootLifecycleServerWorld();
		const UHeistGameInstance* GameInstance = IsValid(ServerWorld) ? Cast<UHeistGameInstance>(ServerWorld->GetGameInstance()) : nullptr;
		return AreLootLifecycleWorldsReady(EHeistMatchPhase::Lobby, false) && IsValid(GameInstance) && GameInstance->IsHostingOnlineSession() &&
			GameInstance->HasActiveNamedOnlineSession();
	}, 60.0));
	Test->AddCommand(new FHeistLootLifecycleActionCommand(Test, State, TEXT("select M01"), []()
	{
		AHeistPlayerController* HostPlayerController = GetOwningPlayerControllerById(1);
		if (!IsValid(HostPlayerController))
		{
			return false;
		}
		HostPlayerController->RequestSetLobbyMapSelection(FName(TEXT("M01")));
		return true;
	}));
	Test->AddCommand(new FHeistLootLifecycleWaitCommand(Test, State, TEXT("M01 selection replication"), []()
	{
		UWorld* ServerWorld = GetLootLifecycleServerWorld();
		const UHeistGameInstance* GameInstance = IsValid(ServerWorld) ? Cast<UHeistGameInstance>(ServerWorld->GetGameInstance()) : nullptr;
		return IsValid(GameInstance) && GameInstance->GetSelectedMapId() == FName(TEXT("M01")) && !GameInstance->IsMapSelectionUpdatePending();
	}, 15.0));
	Test->AddCommand(new FHeistLootLifecycleActionCommand(Test, State, TEXT("start M01 gameplay"), []()
	{
		UWorld* ServerWorld = GetLootLifecycleServerWorld();
		UHeistGameInstance* GameInstance = IsValid(ServerWorld) ? Cast<UHeistGameInstance>(ServerWorld->GetGameInstance()) : nullptr;
		return IsValid(GameInstance) && SetAllLootLifecycleLobbyPlayersReady(ServerWorld) && GameInstance->RequestStartSelectedGameplayMap();
	}));
	Test->AddCommand(new FHeistLootLifecycleWaitCommand(Test, State, TEXT("M01 match-start supply and five row lifecycle fixtures"), [Test, State]()
	{
		return AreLootLifecycleWorldsReady(EHeistMatchPhase::InGame, true) && CaptureLifecycleFixtures(Test, State);
	}, 75.0));

	for (int32 FixtureIndex = 0; FixtureIndex < GetLifecycleLootRowIds().Num(); ++FixtureIndex)
	{
		AppendLootFixtureLifecycleCommands(Test, State, FixtureIndex);
	}

	Test->AddCommand(new FHeistLootLifecycleActionCommand(Test, State, TEXT("validate all five rows fit the production 5x5 inventory"), [Test, State]()
	{
		if (!AreHeldLootStateAndContractReplicated(State, State->Fixtures.Num() - 1))
		{
			return false;
		}
		int32 GridArea = 0;
		int32 TotalValue = 0;
		float TotalWeight = 0.0f;
		for (const FHeistLootLifecycleFixture& Fixture : State->Fixtures)
		{
			GridArea += Fixture.ItemDefinition.GridSize.X * Fixture.ItemDefinition.GridSize.Y;
			TotalValue += Fixture.LootDefinition.ScoreValue;
			TotalWeight += Fixture.ItemDefinition.Weight;
		}
		const int32 GridCapacity = UHeistInventoryComponent::GridColumnCount * UHeistInventoryComponent::GridRowCount;
		Test->AddInfo(FString::Printf(TEXT("W6-011 inventory evidence: Rows=5 OccupiedArea=%d Capacity=%d Value=%d Weight=%.1f AuthorityOwnerFastArray=PASS NoOverlap=PASS"),
			GridArea, GridCapacity, TotalValue, TotalWeight));
		return GridArea <= GridCapacity;
	}));
	Test->AddCommand(new FHeistLootLifecycleActionCommand(Test, State, TEXT("open Vent without seeding Result or Contract outcome"), []()
	{
		UWorld* ServerWorld = GetLootLifecycleServerWorld();
		AHeistGameState* GameState = IsValid(ServerWorld) ? ServerWorld->GetGameState<AHeistGameState>() : nullptr;
		if (!IsValid(GameState) || !GameState->HasAuthority())
		{
			return false;
		}
		GameState->OpenEscapePhase();
		return true;
	}));
	Test->AddCommand(new FHeistLootLifecycleWaitCommand(Test, State, TEXT("Vent open on host and client"), []()
	{
		for (UWorld* World : GetLootLifecyclePIEWorlds())
		{
			const AHeistGameState* GameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
			const AHeistVentActor* VentActor = FindVentActor(World);
			if (!IsValid(GameState) || !GameState->IsEscapePhaseOpen() || !IsValid(VentActor) || !VentActor->IsVentActive())
			{
				return false;
			}
		}
		return true;
	}, 15.0));
	Test->AddCommand(new FHeistLootLifecycleActionCommand(Test, State, TEXT("move client carrier to Vent"), []()
	{
		return TeleportServerPlayerIntoInteraction(LootCarrierPlayerId, FindVentActor(GetLootLifecycleServerWorld()));
	}));
	Test->AddCommand(new FHeistLootLifecycleWaitCommand(Test, State, TEXT("client carrier Vent overlap"), []()
	{
		return IsServerPlayerOverlapping(LootCarrierPlayerId, FindVentActor(GetLootLifecycleServerWorld()));
	}, 10.0));
	Test->AddCommand(new FHeistLootLifecycleActionCommand(Test, State, TEXT("request client carrier escape through owning-client RPC"), []()
	{
		AHeistPlayerController* OwningPlayerController = GetOwningPlayerControllerById(LootCarrierPlayerId);
		AHeistVentActor* LocalVent = IsValid(OwningPlayerController) ? FindVentActor(OwningPlayerController->GetWorld()) : nullptr;
		return InvokeSingleActorServerRPC(OwningPlayerController, FName(TEXT("Server_RequestEscape")), LocalVent);
	}));
	Test->AddCommand(new FHeistLootLifecycleWaitCommand(Test, State, TEXT("five-row Vent settlement, inventory clear and active player replication"), [State]()
	{
		const int32 FinalFixtureIndex = State->Fixtures.Num() - 1;
		const int32 ExpectedValue = GetExpectedValueThroughFixture(State, FinalFixtureIndex);
		UWorld* ServerWorld = GetLootLifecycleServerWorld();
		const AHeistPlayerCharacter* ServerCharacter = FindHeistCharacterById(ServerWorld, LootCarrierPlayerId);
		AHeistPlayerController* OwningPlayerController = GetOwningPlayerControllerById(LootCarrierPlayerId);
		const AHeistPlayerCharacter* OwningCharacter = IsValid(OwningPlayerController) ? FindHeistCharacterById(OwningPlayerController->GetWorld(), LootCarrierPlayerId) : nullptr;
		const UHeistInventoryComponent* ServerInventory = IsValid(ServerCharacter) ? ServerCharacter->GetInventoryComponent() : nullptr;
		const UHeistInventoryComponent* OwningInventory = IsValid(OwningCharacter) ? OwningCharacter->GetInventoryComponent() : nullptr;
		if (!IsValid(ServerInventory) || ServerInventory->GetReplicatedInventory().Items.Num() != 0 || !IsValid(OwningInventory) ||
			OwningInventory->GetReplicatedInventory().Items.Num() != 0)
		{
			return false;
		}
		for (UWorld* World : GetLootLifecyclePIEWorlds())
		{
			const AHeistPlayerState* PlayerState = FindHeistPlayerStateById(World, LootCarrierPlayerId);
			const AHeistGameState* GameState = IsValid(World) ? World->GetGameState<AHeistGameState>() : nullptr;
			if (!IsValid(PlayerState) || PlayerState->IsEscaped() || PlayerState->GetTotalLootScore() != 0 || !FMath::IsNearlyZero(PlayerState->GetTotalLootWeight()) ||
				PlayerState->GetContribution().SecuredLootValue != ExpectedValue || !IsValid(GameState) || GameState->GetContractSnapshot().SecuredValue != ExpectedValue ||
				GameState->GetContractSnapshot().CarriedValue != 0)
			{
				return false;
			}
		}
		return true;
	}, 20.0));
	Test->AddCommand(new FHeistLootLifecycleActionCommand(Test, State, TEXT("request final escape after Loose Loot settlement"), []()
	{
		AHeistPlayerController* OwningPlayerController = GetOwningPlayerControllerById(LootCarrierPlayerId);
		AHeistVentActor* LocalVent = IsValid(OwningPlayerController) ? FindVentActor(OwningPlayerController->GetWorld()) : nullptr;
		return InvokeSingleActorServerRPC(OwningPlayerController, FName(TEXT("Server_RequestEscape")), LocalVent);
	}));
	Test->AddCommand(new FHeistLootLifecycleWaitCommand(Test, State, TEXT("final escape commits only after the second Vent interaction"), []()
	{
		for (UWorld* World : GetLootLifecyclePIEWorlds())
		{
			const AHeistPlayerState* PlayerState = FindHeistPlayerStateById(World, LootCarrierPlayerId);
			if (!IsValid(PlayerState) || !PlayerState->IsEscaped())
			{
				return false;
			}
		}
		return true;
	}, 20.0));
	Test->AddCommand(new FHeistLootLifecycleActionCommand(Test, State, TEXT("record W6-011 two-player lifecycle PASS evidence"), [Test, State]()
	{
		const int32 FinalFixtureIndex = State->Fixtures.Num() - 1;
		const int32 ExpectedValue = GetExpectedValueThroughFixture(State, FinalFixtureIndex);
		for (const FName RowId : GetLifecycleLootRowIds())
		{
			if (State->FeedbackCounts.FindRef(RowId) != 2)
			{
				return false;
			}
		}
		if (State->UnexpectedFeedback.Num() > 0)
		{
			for (const FString& UnexpectedMessage : State->UnexpectedFeedback)
			{
				Test->AddError(FString::Printf(TEXT("W6-011 unexpected popup feedback: %s"), *UnexpectedMessage));
			}
			return false;
		}
		Test->AddInfo(FString::Printf(
			TEXT("W6-011 lifecycle gate: Players=2 Map=M01 Rows=5 MatchStartSupply=true SpawnPointCategory=true InitialPickups=5 Drops=5 Repickups=5 PickupFeedback=10 SharedShell=BP_Loot HostClientVisual=true InventoryGrid=5x5 VentSettlement=true SecondInteractionEscape=true Secured=%d InventoryEmpty=true ContractOutcomeSeed=false Result=PASS"),
			ExpectedValue));
		return true;
	}));

	Test->AddCommand(new FHeistLootLifecycleActionCommand(Test, State, TEXT("unbind popup feedback capture"), [State]()
	{
		if (State->FeedbackController.IsValid() && State->FeedbackHandle.IsValid())
		{
			State->FeedbackController->GetPopupFeedbackRequestedDelegate().Remove(State->FeedbackHandle);
		}
		State->FeedbackHandle.Reset();
		State->FeedbackController.Reset();
		return true;
	}, true));
	Test->AddCommand(new FEndPlayMapCommand());
	Test->AddCommand(new FWaitLatentCommand(1.0f));
	Test->AddCommand(new FHeistLootLifecycleActionCommand(Test, State, TEXT("restore editor play settings"), [State]()
	{
		if (!State->bCapturedPlaySettings)
		{
			return true;
		}
		ULevelEditorPlaySettings* PlaySettings = GetMutableDefault<ULevelEditorPlaySettings>();
		if (!IsValid(PlaySettings))
		{
			return false;
		}
		PlaySettings->SetRunUnderOneProcess(State->bOriginalRunUnderOneProcess);
		PlaySettings->SetPlayNetMode(State->OriginalNetMode);
		PlaySettings->SetPlayNumberOfClients(State->OriginalClientCount);
		return true;
	}, true));
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistLootLifecycleTwoPlayerTest, "ProjectMuseumHeist.Loot.LifecycleTwoPlayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistLootLifecycleTwoPlayerTest::RunTest(const FString& Parameters)
{
	return HeistLootLifecycleTest::EnqueueTwoPlayerLootLifecycleScenario(this);
}

#endif
