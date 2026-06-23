#include "Debug/HeistDebugFunctionLibrary.h"

#include "Character/Components/HeistInventoryComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistLogChannels.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/HeistInventoryTypes.h"

#pragma region InternalHelpers

namespace
{
	AHeistPlayerController* ResolveHeistPlayerController(APlayerController* PlayerController)
	{
		return Cast<AHeistPlayerController>(PlayerController);
	}

	UHeistInventoryComponent* ResolveInventoryComponent(APlayerController* PlayerController)
	{
		const AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
		const AHeistPlayerCharacter* HeistCharacter = IsValid(HeistPlayerController)
			? HeistPlayerController->GetPawn<AHeistPlayerCharacter>()
			: nullptr;
		return IsValid(HeistCharacter) ? HeistCharacter->GetInventoryComponent() : nullptr;
	}

	const TCHAR* ToQuickSlotText(const EHeistQuickSlotType SlotType)
	{
		switch (SlotType)
		{
		case EHeistQuickSlotType::Coin:
			return TEXT("Coin");
		case EHeistQuickSlotType::SmokeGrenade:
			return TEXT("SmokeGrenade");
		case EHeistQuickSlotType::GlueTrap:
			return TEXT("GlueTrap");
		default:
			return TEXT("None");
		}
	}

	bool TryParseQuickSlotName(const FString& SlotName, EHeistQuickSlotType& OutSlotType)
	{
		const FString NormalizedSlotName = SlotName.TrimStartAndEnd().ToLower();
		if (NormalizedSlotName == TEXT("coin") || NormalizedSlotName == TEXT("q"))
		{
			OutSlotType = EHeistQuickSlotType::Coin;
			return true;
		}

		if (NormalizedSlotName == TEXT("smoke")
			|| NormalizedSlotName == TEXT("smokegrenade")
			|| NormalizedSlotName == TEXT("e"))
		{
			OutSlotType = EHeistQuickSlotType::SmokeGrenade;
			return true;
		}

		if (NormalizedSlotName == TEXT("glue")
			|| NormalizedSlotName == TEXT("gluetrap")
			|| NormalizedSlotName == TEXT("r"))
		{
			OutSlotType = EHeistQuickSlotType::GlueTrap;
			return true;
		}

		OutSlotType = EHeistQuickSlotType::None;
		return false;
	}
}

#pragma endregion

#pragma region Logging

void UHeistDebugFunctionLibrary::Message(
	const UObject* WorldContextObject,
	const FString& Message,
	EHeistDebugLevel Level,
	bool bPrintToScreen,
	float Duration)
{
	const FString ContextName = GetNameSafe(WorldContextObject);
	const FString FormattedMessage = FString::Printf(TEXT("[%s] %s"), *ContextName, *Message);

	switch (Level)
	{
	case EHeistDebugLevel::Warning:
		UE_LOG(LogHeist, Warning, TEXT("%s"), *FormattedMessage);
		break;
	case EHeistDebugLevel::Error:
		UE_LOG(LogHeist, Error, TEXT("%s"), *FormattedMessage);
		break;
	default:
		UE_LOG(LogHeist, Log, TEXT("%s"), *FormattedMessage);
		break;
	}

	if (!bPrintToScreen || GEngine == nullptr)
	{
		return;
	}

	FColor MessageColor = FColor::White;
	if (Level == EHeistDebugLevel::Warning)
	{
		MessageColor = FColor::Yellow;
	}
	else if (Level == EHeistDebugLevel::Error)
	{
		MessageColor = FColor::Red;
	}

	GEngine->AddOnScreenDebugMessage(
		INDEX_NONE,
		FMath::Max(0.0f, Duration),
		MessageColor,
		FormattedMessage);
}

#pragma endregion

#pragma region InventoryDebug

void UHeistDebugFunctionLibrary::DebugInventoryDump(APlayerController* PlayerController)
{
	const UHeistInventoryComponent* InventoryComponent = ResolveInventoryComponent(PlayerController);
	if (!IsValid(InventoryComponent))
	{
		Message(
			PlayerController,
			TEXT("Inventory debug dump failed: missing local Heist inventory component."),
			EHeistDebugLevel::Warning,
			true);
		return;
	}

	const FHeistReplicatedInventory& ReplicatedInventory = InventoryComponent->GetReplicatedInventory();
	Message(
		PlayerController,
		FString::Printf(
			TEXT("Inventory dump: Open=%s Grid=%dx%d Items=%d QuickSlots=%d"),
			InventoryComponent->IsInventoryOpen() ? TEXT("true") : TEXT("false"),
			InventoryComponent->GetGridColumnCount(),
			InventoryComponent->GetGridRowCount(),
			ReplicatedInventory.Items.Num(),
			InventoryComponent->GetQuickSlots().Num()),
		EHeistDebugLevel::Info,
		true,
		6.0f);

	for (const FHeistInventoryFastArrayItem& ItemEntry : ReplicatedInventory.Items)
	{
		const FHeistInventoryItem& Item = ItemEntry.InventoryItem;
		Message(
			PlayerController,
			FString::Printf(
				TEXT("Inventory item: InstanceId=%d ItemId=%s Grid=(%d,%d) Quantity=%d Rotated=%s"),
				Item.InstanceId,
				*Item.ItemId.ToString(),
				Item.GridPosition.X,
				Item.GridPosition.Y,
				Item.Quantity,
				Item.bRotated ? TEXT("true") : TEXT("false")),
			EHeistDebugLevel::Info,
			false);
	}

	for (const FHeistQuickSlotState& QuickSlot : InventoryComponent->GetQuickSlots())
	{
		Message(
			PlayerController,
			FString::Printf(
				TEXT("QuickSlot: Slot=%s InstanceId=%d"),
				ToQuickSlotText(QuickSlot.SlotType),
				QuickSlot.ItemInstanceId),
			EHeistDebugLevel::Info,
			false);
	}
}

void UHeistDebugFunctionLibrary::DebugInventoryOpen(APlayerController* PlayerController, const bool bOpen)
{
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Inventory debug open failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->RequestSetInventoryOpen(bOpen);
	Message(
		PlayerController,
		FString::Printf(TEXT("Inventory debug open requested: Open=%s"), bOpen ? TEXT("true") : TEXT("false")),
		EHeistDebugLevel::Info,
		true);
}

void UHeistDebugFunctionLibrary::DebugInventoryMove(
	APlayerController* PlayerController,
	const int32 InstanceId,
	const int32 GridX,
	const int32 GridY)
{
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Inventory debug move failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->RequestMoveInventoryItem(InstanceId, FIntPoint(GridX, GridY));
	Message(
		PlayerController,
		FString::Printf(TEXT("Inventory debug move requested: InstanceId=%d Grid=(%d,%d)"), InstanceId, GridX, GridY),
		EHeistDebugLevel::Info,
		true);
}

void UHeistDebugFunctionLibrary::DebugInventoryRotate(APlayerController* PlayerController, const int32 InstanceId)
{
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Inventory debug rotate failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->RequestRotateInventoryItem(InstanceId);
	Message(
		PlayerController,
		FString::Printf(TEXT("Inventory debug rotate requested: InstanceId=%d"), InstanceId),
		EHeistDebugLevel::Info,
		true);
}

void UHeistDebugFunctionLibrary::DebugInventoryDrop(APlayerController* PlayerController, const int32 InstanceId)
{
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Inventory debug drop failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->RequestDropInventoryItem(InstanceId);
	Message(
		PlayerController,
		FString::Printf(TEXT("Inventory debug drop requested: InstanceId=%d"), InstanceId),
		EHeistDebugLevel::Info,
		true);
}

void UHeistDebugFunctionLibrary::DebugInventoryAssignQuickSlot(
	APlayerController* PlayerController,
	const FString& SlotName,
	const int32 InstanceId)
{
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Inventory debug assign failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	EHeistQuickSlotType SlotType = EHeistQuickSlotType::None;
	if (!TryParseQuickSlotName(SlotName, SlotType))
	{
		Message(
			PlayerController,
			FString::Printf(TEXT("Inventory debug assign failed: invalid slot '%s'. Use Q/Coin, E/Smoke, or R/Glue."), *SlotName),
			EHeistDebugLevel::Warning,
			true);
		return;
	}

	HeistPlayerController->RequestAssignQuickSlot(SlotType, InstanceId);
	Message(
		PlayerController,
		FString::Printf(
			TEXT("Inventory debug assign requested: Slot=%s InstanceId=%d"),
			ToQuickSlotText(SlotType),
			InstanceId),
		EHeistDebugLevel::Info,
		true);
}

void UHeistDebugFunctionLibrary::DebugInventoryClearQuickSlot(APlayerController* PlayerController, const FString& SlotName)
{
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Inventory debug clear failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	EHeistQuickSlotType SlotType = EHeistQuickSlotType::None;
	if (!TryParseQuickSlotName(SlotName, SlotType))
	{
		Message(
			PlayerController,
			FString::Printf(TEXT("Inventory debug clear failed: invalid slot '%s'. Use Q/Coin, E/Smoke, or R/Glue."), *SlotName),
			EHeistDebugLevel::Warning,
			true);
		return;
	}

	HeistPlayerController->RequestClearQuickSlot(SlotType);
	Message(
		PlayerController,
		FString::Printf(TEXT("Inventory debug clear requested: Slot=%s"), ToQuickSlotText(SlotType)),
		EHeistDebugLevel::Info,
		true);
}

void UHeistDebugFunctionLibrary::DebugInventoryInvalidMove(APlayerController* PlayerController, const int32 InstanceId)
{
	AHeistPlayerController* HeistPlayerController = ResolveHeistPlayerController(PlayerController);
	if (!IsValid(HeistPlayerController))
	{
		Message(PlayerController, TEXT("Inventory debug invalid move failed: invalid Heist player controller."), EHeistDebugLevel::Warning, true);
		return;
	}

	HeistPlayerController->RequestMoveInventoryItem(InstanceId, FIntPoint(-1, -1));
	Message(
		PlayerController,
		FString::Printf(TEXT("Inventory debug invalid move requested: InstanceId=%d Grid=(-1,-1)"), InstanceId),
		EHeistDebugLevel::Info,
		true);
}

#pragma endregion
