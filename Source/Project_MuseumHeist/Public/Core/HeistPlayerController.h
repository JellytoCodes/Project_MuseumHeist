#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "HeistPlayerController.generated.h"

class AHeistPlayerCharacter;
class AHeistLootActor;
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> MoveInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> InteractInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> GameplayInputMappingContext;

#pragma endregion

#pragma region Interaction

private:
	void HandleInteractPressed();

#pragma endregion

#pragma region Networking

private:
	UFUNCTION(Server, Reliable)
	void Server_RequestLootPickup(AHeistLootActor* TargetLootActor);

#pragma endregion

#pragma region InternalHelpers

private:
	void LogLootPickupRejected(const AHeistLootActor* TargetLootActor, const TCHAR* Reason, float Distance = -1.0f) const;

#pragma endregion

#pragma region Cursor

public:
	bool GetCursorWorldHit(FHitResult& OutHitResult) const;
	bool GetCursorWorldLocation(FVector& OutWorldLocation) const;

private:
	void ConfigureMouseCursorDefaults();

#pragma endregion

};
