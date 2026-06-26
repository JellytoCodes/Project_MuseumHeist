#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "HeistSmokeCloudActor.generated.h"

class AHeistPlayerCharacter;
class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistSmokeCloudActor : public AActor
{
	GENERATED_BODY()

#pragma region Construction

public:
	AHeistSmokeCloudActor();

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#pragma endregion

#pragma region Smoke

public:
	void InitializeSmokeCloud(AHeistPlayerCharacter* InOwningCharacter, FName InSourceItemId, float InDurationSeconds, float InSmokeRadius = 300.0f);
	bool IsActorInsideSmoke(const AActor* Actor) const;
	bool IsLocationInsideSmoke(const FVector& WorldLocation) const;
	bool BlocksAISight() const;
	float GetRemainingLifetimeSeconds() const;

	static bool IsAISightBlockedBySmoke(const UObject* WorldContextObject, const FVector& FromLocation, const FVector& ToLocation, AHeistSmokeCloudActor*& OutBlockingSmokeCloud);

private:
	UFUNCTION()
	void HandleSmokeOverlapBegin(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleSmokeOverlapEnd(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	void ApplySmokeToActor(AActor* Actor);
	void ClearSmokeFromActorIfUncovered(AActor* Actor);
	void ApplySmokeToCurrentOverlaps();
	bool HasOtherSmokeCloudCoveringActor(const AActor* Actor) const;
	void UpdateSmokeRadius();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Smoke", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> SmokeCollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> VisualMeshComponent;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Smoke", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AHeistPlayerCharacter> OwningCharacter;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Smoke", meta = (AllowPrivateAccess = "true"))
	FName SourceItemId = NAME_None;

	UPROPERTY(ReplicatedUsing = OnRep_SmokeCloudState, EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Smoke", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float SmokeRadius = 300.0f;

	UPROPERTY(ReplicatedUsing = OnRep_SmokeCloudState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Heist|Smoke", meta = (AllowPrivateAccess = "true"))
	float EndServerTime = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_SmokeCloudState, EditDefaultsOnly, BlueprintReadOnly, Category = "Heist|Smoke", meta = (AllowPrivateAccess = "true"))
	bool bBlocksAISight = true;

	UFUNCTION()
	void OnRep_SmokeCloudState();

#pragma endregion

#pragma region Replication

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion
};
