#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "GameFramework/PlayerController.h"

#include "HeistPlayerController.generated.h"

class AHeistPlayerCharacter;
class AHeistDisplayCaseActor;
class AHeistGuardCharacter;
class AHeistLootActor;
class AHeistPlayerState;
class AHeistThrowableProjectile;
class AHeistVentActor;
class UHeistForgeryComponent;
class UHeistInventoryComponent;
class UInputAction;
class UInputMappingContext;
struct FHitResult;
struct FInputActionValue;

DECLARE_MULTICAST_DELEGATE_TwoParams(FHeistPopupFeedbackRequested, const FText&, float);

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnRep_Pawn() override;
	virtual void OnRep_PlayerState() override;
	virtual void SetupInputComponent() override;

private:
	void RefreshLocalHUDPresentation();

#pragma endregion

#pragma region Input

private:
	void HandleLookInput(const FInputActionValue& InputValue);
	void HandleMoveInput(const FInputActionValue& InputValue);
	void HandleInventoryToggle();
	void HandleForgeryCancel();
	void UpdateFlashlightAimDirection();
	void RefreshLocalForgeryInputBinding();
	void UnbindLocalForgeryInputState();
	void HandleForgerySessionStateChanged();
	void RefreshLocalInputModeFromPawn();
	void ApplyLocalInputMode(EHeistInputMode NewInputMode);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> MoveInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> LookInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> InteractInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> InventoryInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ForgeryCancelInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> GameplayInputMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> InventoryInputMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> ForgeryInputMappingContext;

	TWeakObjectPtr<UHeistForgeryComponent> BoundForgeryComponent;
	EHeistInputMode LocalInputMode = EHeistInputMode::Gameplay;
	bool bLocalForgerySessionActive = false;

public:
	void HandleInventoryOpenStateChanged(bool bInventoryOpen);
	void HandleArrestStateChanged(bool bArrested);
	EHeistInputMode GetLocalInputMode() const;
	bool IsLocalInputMappingContextActive(EHeistInputMode InputMode) const;
	int32 GetActiveHeistInputMappingContextCount() const;
	bool IsLocalInputModeContractSatisfied() const;

#pragma endregion

#pragma region Interaction

private:
	void HandleInteractPressed();
	void HandleInteractReleased();
	bool bLocalObservationInputHeld = false;

#pragma endregion

#pragma region GameplayRequests

public:
	UFUNCTION(BlueprintCallable, Category = "Heist|Inventory")
	void RequestSetInventoryOpen(bool bInventoryOpen);

	UFUNCTION(BlueprintCallable, Category = "Heist|Forgery")
	void RequestCancelForgery();

	UFUNCTION(BlueprintCallable, Category = "Heist|Inventory")
	void RequestMoveInventoryItem(int32 InstanceId, FIntPoint TargetGridPosition);

	UFUNCTION(BlueprintCallable, Category = "Heist|Inventory")
	void RequestRotateInventoryItem(int32 InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Inventory")
	void RequestDropInventoryItem(int32 InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Objective")
	void RequestTakeOriginal(AHeistDisplayCaseActor* TargetDisplayCase);

	UFUNCTION(BlueprintCallable, Category = "Heist|Objective")
	void RequestDropCarriedOriginal();

	UFUNCTION(BlueprintCallable, Category = "Heist|Inventory")
	void RequestAssignQuickSlot(EHeistQuickSlotType SlotType, int32 InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Heist|Inventory")
	void RequestClearQuickSlot(EHeistQuickSlotType SlotType);

	UFUNCTION(BlueprintCallable, Category = "Heist|QuickSlot")
	void RequestUseQuickSlot(EHeistQuickSlotType SlotType);

	bool TryBuildCameraForwardAim(float Distance, FVector& OutViewLocation, FVector& OutCameraForward, FVector& OutTargetWorldLocation) const;

private:
	UFUNCTION(Server, Reliable)
	void Server_RequestLootPickup(AHeistLootActor* TargetLootActor);

	UFUNCTION(Server, Reliable)
	void Server_RequestEscape(AHeistVentActor* TargetVentActor);

	UFUNCTION(Server, Reliable)
	void Server_RequestObservation(AHeistDisplayCaseActor* TargetDisplayCase);

	UFUNCTION(Server, Reliable)
	void Server_CancelObservation();

	UFUNCTION(Server, Reliable)
	void Server_SetInventoryOpen(bool bInventoryOpen);

	UFUNCTION(Server, Reliable)
	void Server_CancelForgery();

	UFUNCTION(Server, Reliable)
	void Server_RequestMoveInventoryItem(int32 InstanceId, FIntPoint TargetGridPosition);

	UFUNCTION(Server, Reliable)
	void Server_RequestRotateInventoryItem(int32 InstanceId);

	UFUNCTION(Server, Reliable)
	void Server_RequestDropInventoryItem(int32 InstanceId);

	UFUNCTION(Server, Reliable)
	void Server_RequestTakeOriginal(AHeistDisplayCaseActor* TargetDisplayCase);

	UFUNCTION(Server, Reliable)
	void Server_RequestDropCarriedOriginal();

	UFUNCTION(Server, Reliable)
	void Server_RequestAssignQuickSlot(EHeistQuickSlotType SlotType, int32 InstanceId);

	UFUNCTION(Server, Reliable)
	void Server_RequestClearQuickSlot(EHeistQuickSlotType SlotType);

	UFUNCTION(Server, Reliable)
	void Server_RequestUseQuickSlot(EHeistQuickSlotType SlotType, FVector TargetWorldLocation);

	UFUNCTION(Server, Unreliable)
	void Server_UpdateFlashlightAimDirection(FVector_NetQuantizeNormal ClientCameraForward);

#pragma endregion

#pragma region Debug

public:
	void DebugRequestAddInventoryItem(FName ItemId);
	void DebugRequestThrowCoinAtWorldLocation(FVector TargetWorldLocation);
	void DebugRequestSpawnGuard(float Distance);
	void DebugRequestSetNearestGuardState(EHeistGuardState GuardState, float DurationSeconds);
	void DebugRequestEvaluateNearestGuardSight();
	void DebugRequestSetNearestGuardAutomaticSight(bool bEnabled);
	void DebugRequestReportGuardNoise(float Distance);
	void DebugRequestRebuildResults();
	void DebugRequestSeedResult(int32 Score, bool bEscaped, float EscapeTimeSeconds);
	void DebugRequestFeedbackTest();
	void DebugRequestFillInventoryForFeedback(FName ItemId);
	void DebugRequestSetArrested(bool bArrested);
	void DebugRequestDumpArrestState();
	void DebugRequestSetFootstepWeight(float TotalLootWeight);
	void DebugRequestDumpDifficultyBaseline();

private:
	UFUNCTION(Server, Reliable)
	void Server_DebugRequestAddInventoryItem(FName ItemId);

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestThrowCoinAtWorldLocation(FVector TargetWorldLocation);

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestSpawnGuard(float Distance);

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestSetNearestGuardState(EHeistGuardState GuardState, float DurationSeconds);

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestEvaluateNearestGuardSight();

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestSetNearestGuardAutomaticSight(bool bEnabled);

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestReportGuardNoise(float Distance);

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestRebuildResults();

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestSeedResult(int32 Score, bool bEscaped, float EscapeTimeSeconds);

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestSetArrested(bool bArrested);

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestDumpArrestState();

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestSetFootstepWeight(float TotalLootWeight);

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestDumpDifficultyBaseline();

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestFeedbackTest();

	UFUNCTION(Server, Reliable)
	void Server_DebugRequestFillInventoryForFeedback(FName ItemId);

#pragma endregion

#pragma region Feedback

public:
	FHeistPopupFeedbackRequested& GetPopupFeedbackRequestedDelegate();

private:
	UFUNCTION(Client, Reliable)
	void Client_ReceivePopupFeedback(const FText& Message, float DurationSeconds);

	void SendPopupFeedback(const FText& Message, float DurationSeconds = 2.0f);
	void SendPopupFeedbackForRejection(const TCHAR* RequestName, const TCHAR* Reason);
	static FText ResolvePopupFeedbackText(const TCHAR* RequestName, const TCHAR* Reason);

	FHeistPopupFeedbackRequested PopupFeedbackRequestedDelegate;

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
		const TCHAR*& OutRejectReason) const;
	AHeistGuardCharacter* FindNearestGuard() const;
	bool TrySpawnThrowableProjectile(
		const FHeistGameplayRequestContext& RequestContext,
		FName ItemId,
		const FVector& TargetWorldLocation,
		bool bDebugBypassInventory,
		AHeistThrowableProjectile*& OutProjectile,
		const TCHAR*& OutRejectReason) const;
	static FName GetExpectedQuickSlotItemId(EHeistQuickSlotType SlotType);

	void LogLootPickupRejected(const AHeistLootActor* TargetLootActor, const TCHAR* Reason, float Distance = -1.0f);
	void LogEscapeRequestRejected(const AHeistVentActor* TargetVentActor, const TCHAR* Reason, float Distance = -1.0f);
	void LogInventoryRequestRejected(const TCHAR* RequestName, int32 InstanceId, const TCHAR* Reason);
	void LogThrowableUseRejected(EHeistQuickSlotType SlotType, FName ItemId, const TCHAR* Reason);

#pragma endregion

};
