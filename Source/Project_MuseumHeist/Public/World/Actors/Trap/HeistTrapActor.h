#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "HeistTrapActor.generated.h"

class AHeistPlayerCharacter;
class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistTrapActor : public AActor
{
	GENERATED_BODY()

#pragma region Construction

public:
	AHeistTrapActor();

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void BeginPlay() override;

#pragma endregion

#pragma region Trap

public:
	void InitializeTrap(AHeistPlayerCharacter* InOwningCharacter, FName InSourceItemId, float InEffectDurationSeconds);

protected:
	virtual bool HandleAuthorityTrigger(AActor* TriggeringActor);

	AHeistPlayerCharacter* GetOwningCharacter() const;
	FName GetSourceItemId() const;
	float GetEffectDurationSeconds() const;

private:
	UFUNCTION()
	void HandleTrapOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Trap", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> TriggerComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> VisualMeshComponent;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Trap", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AHeistPlayerCharacter> OwningCharacter;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Trap", meta = (AllowPrivateAccess = "true"))
	FName SourceItemId = NAME_None;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Trap", meta = (AllowPrivateAccess = "true"))
	float EffectDurationSeconds = 0.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Trap", meta = (AllowPrivateAccess = "true"))
	bool bTriggered = false;

#pragma endregion

#pragma region Replication

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion
};
