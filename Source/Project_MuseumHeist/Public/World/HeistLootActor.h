#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "Engine/DataTable.h"
#include "World/HeistInteractableActor.h"

#include "HeistLootActor.generated.h"

UCLASS()
class PROJECT_MUSEUMHEIST_API AHeistLootActor : public AHeistInteractableActor
{
	GENERATED_BODY()

#pragma region Construction

public:
	AHeistLootActor();

#pragma endregion

#pragma region Lifecycle

protected:
	virtual void BeginPlay() override;

#pragma endregion

#pragma region LootData

public:
	FName GetLootRowId() const;
	void InitializeLootData(UDataTable* InLootDataTable, FName InLootRowId);
	int32 GetScoreValue() const;
	float GetWeightValue() const;
	EHeistLootGrade GetLootGrade() const;
	bool IsLootAvailable() const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist|Loot", meta = (AllowPrivateAccess = "true"))
	FDataTableRowHandle LootDataRow;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Loot", meta = (AllowPrivateAccess = "true"))
	FName LootRowId = NAME_None;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Loot", meta = (AllowPrivateAccess = "true"))
	EHeistLootGrade LootGrade = EHeistLootGrade::OneStar;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Loot", meta = (AllowPrivateAccess = "true"))
	int32 ScoreValue = 0;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Loot", meta = (AllowPrivateAccess = "true"))
	float WeightValue = 0.0f;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Heist|Loot", meta = (AllowPrivateAccess = "true"))
	bool bIsAvailable = true;

#pragma endregion

#pragma region Interaction

public:
	virtual bool CanInteract(const AActor* Interactor) const override;

#pragma endregion

#pragma region Replication

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

#pragma region LootPickup

public:
	bool TryReserveForPickup(AActor* Requester);
	bool CommitPickupReservation(AActor* Requester);
	void ReleasePickupReservation(AActor* Requester);

private:
	TWeakObjectPtr<AActor> PickupReservationOwner;

#pragma endregion

#pragma region InternalHelpers

private:
	void ResolveLootData();
	void ApplyFallbackLootData();

#pragma endregion
};
