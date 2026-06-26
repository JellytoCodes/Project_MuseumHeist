#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "GameFramework/PlayerController.h"

#include "HeistPlayerController.generated.h"

class AHeistPlayerCharacter;
class AHeistLootActor;
class AHeistPlayerState;
class AHeistTrapActor;
class AHeistThrowableProjectile;
class AHeistVentActor;
class UHeistInventoryComponent;
class UInputAction;
class UInputMappingContext;
struct FHeistItemDataRow;
struct FHeistUsableItemDataRow;
struct FHitResult;
struct FInputActionValue;

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistPlayerController : public APlayerController
{
	GENERATED_BODY()

#pragma region Construction

public:
	AHeistPlayerController();

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

#pragma endregion

#pragma region Input

private:
	void HandleMoveInput(const FInputActionValue& InputValue);
	void HandleInventoryToggle();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> MoveInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> InteractInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> InventoryInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> GameplayInputMappingContext;

#pragma endregion

#pragma region Interaction

private:
	void HandleInteractPressed();

#pragma endregion

#pragma region Networking

public:
	UFUNCTION(BlueprintCallable, Category = "Heist|Inventory")
	void RequestSetInventoryOpen(bool bInventoryOpen);

	UFUNCTION(BlueprintCallable, Category = "Heist|Inventory")
	void RequestMoveInventoryItem(int32 InstanceId, FIntPoint TargetGridPosition);

	UFUNCTION(BlueprintCallable, Category = "Heist|Inventory")
	void RequestRotateInventoryItem(int32 InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Inventory")
	void RequestDropInventoryItem(int32 InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Inventory")
	void RequestAssignQuickSlot(EHeistQuickSlotType SlotType, int32 InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Inventory")
	void RequestClearQuickSlot(EHeistQuickSlotType SlotType);

	UFUNCTION(BlueprintCallable, Category = "Heist|QuickSlot")
	void RequestUseQuickSlotAtWorldLocation(EHeistQuickSlotType SlotType, FVector TargetWorldLocation);

	void DebugRequestThrowCoinAtWorldLocation(FVector TargetWorldLocation);
	void DebugRequestThrowSmokeAtWorldLocation(FVector TargetWorldLocation);
	void DebugRequestPlaceGlueTrapAtWorldLocation(FVector TargetWorldLocation);

private:
	UFUNCTION(Server, Reliable)
	void Server_RequestLootPickup(AHeistLootActor* TargetLootActor);

	UFUNCTION(Server, Reliable)
	void Server_RequestEscape(AHeistVentActor* TargetVentActor);

	UFUNCTION(Server, Reliable)
	void Server_SetInventoryOpen(bool bInventoryOpen);

	UFUNCTION(Server, Reliable)
	void Server_RequestMoveInventoryItem(int32 InstanceId, FIntPoint TargetGridPosition);

	UFUNCTION(Server, Reliable)
	void Server_RequestRotateInventoryItem(int32 InstanceId);

	UFUNCTION(Server, Reliable)
	void Server_RequestDropInventoryItem(int32 InstanceId);

	UFUNCTION(Server, Reliable)
	void Server_RequestAssignQuickSlot(EHeistQuickSlotType SlotType, int32 InstanceId);

	UFUNCTION(Server, Reliable)
	void Server_RequestClearQuickSlot(EHeistQuickSlotType SlotType);

	UFUNCTION(Server, Reliable)
	void Server_RequestUseQuickSlotAtWorldLocation(EHeistQuickSlotType SlotType, FVector TargetWorldLocation);

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestThrowCoinAtWorldLocation(FVector TargetWorldLocation);

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestThrowSmokeAtWorldLocation(FVector TargetWorldLocation);

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestPlaceGlueTrapAtWorldLocation(FVector TargetWorldLocation);

#pragma endregion

#pragma region InternalHelpers

private:
	struct FHeistGameplayRequestContext
	{
		AHeistPlayerCharacter* Character = nullptr;
		AHeistPlayerState* PlayerState = nullptr;
		UHeistInventoryComponent* InventoryComponent = nullptr;
	};

	bool TryBuildGameplayRequestContext(
		FHeistGameplayRequestContext& OutContext,
		const TCHAR*& OutRejectReason) const;
	bool TryBuildInventoryMutationRequestContext(
		FHeistGameplayRequestContext& OutContext,
		const TCHAR*& OutRejectReason) const;

	bool TryResolveQuickSlotItem(
		const FHeistGameplayRequestContext& RequestContext,
		EHeistQuickSlotType SlotType,
		FName& OutItemId,
		int32& OutInstanceId,
		const TCHAR*& OutRejectReason) const;
	bool TrySpawnThrowableProjectile(
		const FHeistGameplayRequestContext& RequestContext,
		FName ItemId,
		const FVector& TargetWorldLocation,
		bool bDebugBypassInventory,
		AHeistThrowableProjectile*& OutProjectile,
		const TCHAR*& OutRejectReason) const;
	bool TryBeginTrapPlacement(
		const FHeistGameplayRequestContext& RequestContext,
		FName ItemId,
		int32 SourceInstanceId,
		const FVector& TargetWorldLocation,
		bool bDebugBypassInventory,
		const TCHAR*& OutRejectReason) const;
	static FName GetExpectedQuickSlotItemId(EHeistQuickSlotType SlotType);

	void LogLootPickupRejected(const AHeistLootActor* TargetLootActor, const TCHAR* Reason, float Distance = -1.0f) const;
	void LogEscapeRequestRejected(const AHeistVentActor* TargetVentActor, const TCHAR* Reason, float Distance = -1.0f) const;
	void LogInventoryRequestRejected(const TCHAR* RequestName, int32 InstanceId, const TCHAR* Reason) const;
	void LogThrowableUseRejected(EHeistQuickSlotType SlotType, FName ItemId, const TCHAR* Reason) const;

#pragma endregion

#pragma region Cursor

public:
	bool GetCursorWorldHit(FHitResult& OutHitResult) const;
	bool GetCursorWorldLocation(FVector& OutWorldLocation) const;

private:
	void ConfigureMouseCursorDefaults();

#pragma endregion

};
