#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"

#include "HeistItemDataTypes.generated.h"

class AActor;
class AHeistLootActor;
class USkeletalMesh;
class USoundBase;
class UStaticMesh;
class UTexture2D;

#pragma region Inventory

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
	bool bCanRotate = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bCanUseQuickSlot = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bAvailableInV1 = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftClassPtr<AHeistLootActor> WorldLootActorClass;
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
	EHeistLootGrade LootGrade = EHeistLootGrade::OneStar;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Weight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 ScoreValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EHeistSpawnCategory SpawnCategory = EHeistSpawnCategory::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float SpawnWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bCanDropOnStun = false;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", Units = "s"))
	float Cooldown = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", Units = "s"))
	float CastTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", Units = "s"))
	float Duration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", Units = "cm/s"))
	float ProjectileSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftClassPtr<AActor> SpawnedActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Icon;
};

#pragma endregion

#pragma region AI

USTRUCT(BlueprintType)
struct PROJECT_MUSEUMHEIST_API FHeistSoundPingDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName SoundPingId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag SoundPingTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EHeistSoundPingType PingType = EHeistSoundPingType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", Units = "cm"))
	float Radius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", Units = "s"))
	float Duration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", Units = "s"))
	float RefreshInterval = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bShowDirectionOnly = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bAffectsGuards = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bAffectsPlayers = true;

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
	FName GuardProfileId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag GuardTypeTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", Units = "cm"))
	float SightRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", ClampMax = "360.0", Units = "deg"))
	float SightAngle = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", ClampMax = "360.0", Units = "deg"))
	float InvestigateSightAngle = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", Units = "cm/s"))
	float PatrolSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", Units = "cm/s"))
	float ChaseSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", Units = "s"))
	float StunDuration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", Units = "s"))
	float InvestigateDuration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", Units = "cm"))
	float AggroResetDistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.01", Units = "s"))
	float SightUpdateInterval = 0.0f;
};

#pragma endregion

#pragma region World

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

#pragma endregion

#pragma region UI

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

#pragma endregion
