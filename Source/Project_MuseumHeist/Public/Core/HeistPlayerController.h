#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "GameFramework/PlayerController.h"

#include "HeistPlayerController.generated.h"

class AHeistPlayerCharacter;
class AHeistLootActor;
class AHeistPlayerState;
class AHeistVentActor;
class UHeistInventoryComponent;
class UInputAction;
class UInputMappingContext;
struct FHitResult;
struct FInputActionValue;

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistPlayerController : public APlayerController
{
	GENERATED_BODY()

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

	void LogLootPickupRejected(const AHeistLootActor* TargetLootActor, const TCHAR* Reason, float Distance = -1.0f) const;
	void LogEscapeRequestRejected(const AHeistVentActor* TargetVentActor, const TCHAR* Reason, float Distance = -1.0f) const;
	void LogInventoryRequestRejected(const TCHAR* RequestName, int32 InstanceId, const TCHAR* Reason) const;

#pragma endregion

#pragma region Cursor

public:
	bool GetCursorWorldHit(FHitResult& OutHitResult) const;
	bool GetCursorWorldLocation(FVector& OutWorldLocation) const;

private:
	void ConfigureMouseCursorDefaults();

#pragma endregion

};
