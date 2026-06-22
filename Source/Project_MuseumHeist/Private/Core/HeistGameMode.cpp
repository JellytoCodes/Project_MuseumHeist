#include "Core/HeistGameMode.h"

#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameState.h"
#include "Core/HeistHUD.h"
#include "Core/HeistLogChannels.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistPlayerState.h"
#include "Data/HeistGameBalanceDataAsset.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Inventory/HeistItemDataTypes.h"
#include "Inventory/HeistInventoryTypes.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "World/HeistLootActor.h"

#pragma region InternalHelpers

namespace
{
	const FLinearColor VerificationPlayerColors[] =
	{
		FLinearColor::Red,
		FLinearColor::Green,
		FLinearColor::Blue,
		FLinearColor::Yellow
	};
}

#pragma endregion

#pragma region Construction

AHeistGameMode::AHeistGameMode()
{
	PlayerControllerClass = AHeistPlayerController::StaticClass();
	PlayerStateClass = AHeistPlayerState::StaticClass();
	GameStateClass = AHeistGameState::StaticClass();
	HUDClass = AHeistHUD::StaticClass();
	DefaultPawnClass = AHeistPlayerCharacter::StaticClass();
}

#pragma endregion

#pragma region Lifecycle

void AHeistGameMode::StartPlay()
{
	Super::StartPlay();
	ValidateItemDataTable();
	StartEscapePhaseTimer();
}

void AHeistGameMode::RestartPlayer(AController* NewPlayer)
{
	AHeistPlayerState* HeistPlayerState = NewPlayer ? NewPlayer->GetPlayerState<AHeistPlayerState>() : nullptr;
	if (HeistPlayerState && HeistPlayerState->HeistPlayerId == INDEX_NONE)
	{
		const int32 AssignedPlayerId = NextHeistPlayerId++;
		const int32 ColorIndex = (AssignedPlayerId - 1) % UE_ARRAY_COUNT(VerificationPlayerColors);
		HeistPlayerState->InitializeVerificationIdentity(AssignedPlayerId, VerificationPlayerColors[ColorIndex]);
	}

	Super::RestartPlayer(NewPlayer);
}

#pragma endregion

#pragma region Balance

UDataTable* AHeistGameMode::GetItemDataTable() const
{
	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset)
		? GameBalanceDataAsset.Get()
		: GetDefault<UHeistGameBalanceDataAsset>();

	return ResolvedBalanceData->ItemDataTable.LoadSynchronous();
}

bool AHeistGameMode::TryGetItemDefinition(
	const FName ItemId,
	FHeistItemDataRow& OutItemDefinition) const
{
	OutItemDefinition = FHeistItemDataRow();

	if (ItemId.IsNone())
	{
		UE_LOG(LogHeistInventory, Warning, TEXT("Item definition lookup rejected: Reason=MissingItemId"));
		return false;
	}

	const UDataTable* ItemDataTable = GetItemDataTable();
	if (!IsValid(ItemDataTable))
	{
		UE_LOG(
			LogHeistInventory,
			Warning,
			TEXT("Item definition lookup rejected: ItemId=%s Reason=MissingItemDataTable"),
			*ItemId.ToString());
		return false;
	}

	if (ItemDataTable->GetRowStruct() != FHeistItemDataRow::StaticStruct())
	{
		UE_LOG(
			LogHeistInventory,
			Error,
			TEXT("Item definition lookup rejected: ItemId=%s Reason=InvalidRowStruct Table=%s RowStruct=%s"),
			*ItemId.ToString(),
			*GetNameSafe(ItemDataTable),
			*GetNameSafe(ItemDataTable->GetRowStruct()));
		return false;
	}

	const FHeistItemDataRow* ItemDefinition = ItemDataTable->FindRow<FHeistItemDataRow>(
		ItemId,
		TEXT("AHeistGameMode::TryGetItemDefinition"),
		false);
	if (!ItemDefinition)
	{
		UE_LOG(
			LogHeistInventory,
			Warning,
			TEXT("Item definition lookup rejected: ItemId=%s Reason=MissingRow Table=%s"),
			*ItemId.ToString(),
			*GetNameSafe(ItemDataTable));
		return false;
	}

	if (ItemDefinition->ItemId != ItemId)
	{
		UE_LOG(
			LogHeistInventory,
			Error,
			TEXT("Item definition lookup rejected: RowName=%s RowItemId=%s Reason=RowNameItemIdMismatch"),
			*ItemId.ToString(),
			*ItemDefinition->ItemId.ToString());
		return false;
	}

	if (ItemDefinition->ItemType == EHeistItemType::None
		|| ItemDefinition->GridSize.X <= 0
		|| ItemDefinition->GridSize.Y <= 0
		|| ItemDefinition->Weight < 0.0f)
	{
		UE_LOG(
			LogHeistInventory,
			Error,
			TEXT("Item definition lookup rejected: ItemId=%s Reason=InvalidDefinition Type=%d Grid=%dx%d Weight=%.2f"),
			*ItemId.ToString(),
			static_cast<int32>(ItemDefinition->ItemType),
			ItemDefinition->GridSize.X,
			ItemDefinition->GridSize.Y,
			ItemDefinition->Weight);
		return false;
	}

	OutItemDefinition = *ItemDefinition;
	return true;
}

bool AHeistGameMode::TryGetLootDefinition(
	const FName ItemId,
	FHeistLootDataRow& OutLootDefinition) const
{
	OutLootDefinition = FHeistLootDataRow();
	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset)
		? GameBalanceDataAsset.Get()
		: GetDefault<UHeistGameBalanceDataAsset>();
	const UDataTable* LootDataTable = ResolvedBalanceData->LootDataTable.LoadSynchronous();
	if (!IsValid(LootDataTable) || LootDataTable->GetRowStruct() != FHeistLootDataRow::StaticStruct())
	{
		return false;
	}

	const FHeistLootDataRow* LootDefinition = LootDataTable->FindRow<FHeistLootDataRow>(
		ItemId,
		TEXT("AHeistGameMode::TryGetLootDefinition"),
		false);
	if (LootDefinition == nullptr || LootDefinition->ItemId != ItemId)
	{
		return false;
	}

	OutLootDefinition = *LootDefinition;
	return true;
}

bool AHeistGameMode::TrySpawnDroppedLoot(
	const FHeistLootDropRequest& DropRequest,
	AHeistLootActor*& OutDroppedLootActor) const
{
	OutDroppedLootActor = nullptr;
	if (!HasAuthority() || DropRequest.ItemId.IsNone() || !IsValid(DropRequest.DroppedBy))
	{
		return false;
	}

	FHeistItemDataRow ItemDefinition;
	FHeistLootDataRow LootDefinition;
	if (!TryGetItemDefinition(DropRequest.ItemId, ItemDefinition)
		|| ItemDefinition.ItemType != EHeistItemType::Loot
		|| !TryGetLootDefinition(DropRequest.ItemId, LootDefinition))
	{
		return false;
	}

	UClass* LootActorClass = ItemDefinition.WorldLootActorClass.LoadSynchronous();
	if (!IsValid(LootActorClass) || !LootActorClass->IsChildOf(AHeistLootActor::StaticClass()))
	{
		return false;
	}

	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset)
		? GameBalanceDataAsset.Get()
		: GetDefault<UHeistGameBalanceDataAsset>();
	UDataTable* LootDataTable = ResolvedBalanceData->LootDataTable.LoadSynchronous();
	const FTransform SpawnTransform(FRotator::ZeroRotator, FVector(DropRequest.DropOrigin));
	AHeistLootActor* DroppedLootActor = GetWorld()->SpawnActorDeferred<AHeistLootActor>(
		LootActorClass,
		SpawnTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!IsValid(DroppedLootActor))
	{
		return false;
	}

	DroppedLootActor->InitializeLootData(LootDataTable, DropRequest.ItemId);
	OutDroppedLootActor = Cast<AHeistLootActor>(UGameplayStatics::FinishSpawningActor(DroppedLootActor, SpawnTransform));
	return IsValid(OutDroppedLootActor);
}

void AHeistGameMode::ValidateItemDataTable() const
{
	if (!HasAuthority())
	{
		return;
	}

	const UDataTable* ItemDataTable = GetItemDataTable();
	if (!IsValid(ItemDataTable))
	{
		UE_LOG(LogHeistInventory, Error, TEXT("Item data validation completed: Result=FAIL Reason=MissingItemDataTable"));
		return;
	}

	const TArray<FName> RowNames = ItemDataTable->GetRowNames();
	if (RowNames.IsEmpty())
	{
		UE_LOG(
			LogHeistInventory,
			Error,
			TEXT("Item data validation completed: Table=%s TotalRows=0 ValidRows=0 InvalidRows=0 Result=FAIL Reason=EmptyTable"),
			*GetNameSafe(ItemDataTable));
		return;
	}

	int32 ValidRowCount = 0;
	for (const FName RowName : RowNames)
	{
		FHeistItemDataRow ItemDefinition;
		if (TryGetItemDefinition(RowName, ItemDefinition))
		{
			++ValidRowCount;
		}
	}

	const int32 InvalidRowCount = RowNames.Num() - ValidRowCount;
	if (InvalidRowCount > 0)
	{
		UE_LOG(
			LogHeistInventory,
			Error,
			TEXT("Item data validation completed: Table=%s TotalRows=%d ValidRows=%d InvalidRows=%d Result=FAIL"),
			*GetNameSafe(ItemDataTable),
			RowNames.Num(),
			ValidRowCount,
			InvalidRowCount);
		return;
	}

	UE_LOG(
		LogHeistInventory,
		Log,
		TEXT("Item data validation completed: Table=%s TotalRows=%d ValidRows=%d InvalidRows=0 Result=PASS"),
		*GetNameSafe(ItemDataTable),
		RowNames.Num(),
		ValidRowCount);
}

#pragma endregion

#pragma region EscapePhase

float AHeistGameMode::GetEscapeCastTimeSeconds() const
{
	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset)
		? GameBalanceDataAsset.Get()
		: GetDefault<UHeistGameBalanceDataAsset>();

	return FMath::Max(0.0f, ResolvedBalanceData->EscapeCastTime);
}

void AHeistGameMode::StartEscapePhaseTimer()
{
	if (!HasAuthority())
	{
		return;
	}

	AHeistGameState* HeistGameState = GetGameState<AHeistGameState>();
	if (!IsValid(HeistGameState))
	{
		UE_LOG(LogHeist, Warning, TEXT("Escape phase timer skipped: Reason=MissingGameState"));
		return;
	}

	FTimerManager& TimerManager = GetWorldTimerManager();
	if (TimerManager.IsTimerActive(EscapePhaseTimerHandle) || HeistGameState->IsEscapePhaseOpen())
	{
		return;
	}

	const float EscapePhaseDelaySeconds = ResolveEscapePhaseDelaySeconds();
	HeistGameState->InitializeEscapePhase(EscapePhaseDelaySeconds);

	if (EscapePhaseDelaySeconds <= 0.0f)
	{
		HeistGameState->OpenEscapePhase();
		return;
	}

	TimerManager.SetTimer(
		EscapePhaseTimerHandle,
		this,
		&AHeistGameMode::HandleEscapePhaseTimerElapsed,
		EscapePhaseDelaySeconds,
		false);

	UE_LOG(
		LogHeist,
		Log,
		TEXT("Escape phase timer started: Delay=%.2f BalanceData=%s"),
		EscapePhaseDelaySeconds,
		*GetNameSafe(GameBalanceDataAsset));
}

void AHeistGameMode::HandleEscapePhaseTimerElapsed()
{
	if (AHeistGameState* HeistGameState = GetGameState<AHeistGameState>())
	{
		HeistGameState->OpenEscapePhase();
	}
	else
	{
		UE_LOG(LogHeist, Warning, TEXT("Escape phase open skipped: Reason=MissingGameState"));
	}
}

float AHeistGameMode::ResolveEscapePhaseDelaySeconds() const
{
	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset)
		? GameBalanceDataAsset.Get()
		: GetDefault<UHeistGameBalanceDataAsset>();

	return FMath::Max(0.0f, ResolvedBalanceData->VentUnlockTime);
}

#pragma endregion
