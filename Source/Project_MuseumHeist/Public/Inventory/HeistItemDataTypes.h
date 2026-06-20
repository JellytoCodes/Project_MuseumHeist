#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"

#include "HeistItemDataTypes.generated.h"

class AActor;
class USkeletalMesh;
class USoundBase;
class UStaticMesh;
class UTexture2D;

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistItemDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag CategoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EHeistItemType ItemType = EHeistItemType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FIntPoint GridSize = FIntPoint(1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Weight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Icon;
};

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistLootDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag CategoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EHeistLootGrade LootGrade = EHeistLootGrade::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Weight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 ScoreValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UStaticMesh> WorldMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Icon;
};

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistUsableItemDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag CategoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EHeistUseType UseType = EHeistUseType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EHeistTargetType TargetType = EHeistTargetType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftClassPtr<AActor> SpawnedActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Icon;
};

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistSoundPingDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName SoundPingId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag SoundTypeTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> MarkerIcon;
};

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistGuardDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName GuardId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag GuardTypeTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float PatrolSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ChaseSpeed = 0.0f;
};

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistLootSpawnRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName LootSpawnId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag LootCategoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MinimumLootCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaximumLootCount = 0;
};

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistVentDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName VentId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EHeistZoneId SourceZone = EHeistZoneId::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EHeistZoneId DestinationZone = EHeistZoneId::None;
};

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistCustomizationRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName CustomizationId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag CustomizationTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<USkeletalMesh> CharacterMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> PreviewImage;
};

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistUITextRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName TextId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayText;
};
