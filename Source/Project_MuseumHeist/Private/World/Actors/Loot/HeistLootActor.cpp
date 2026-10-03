#include "World/Actors/Loot/HeistLootActor.h"

#include "Core/HeistGameMode.h"
#include "Core/HeistGameState.h"
#include "Core/HeistGameplayTags.h"
#include "Core/HeistPlayerState.h"
#include "Core/HeistLogChannels.h"
#include "Character/HeistPlayerCharacter.h"
#include "Character/Components/HeistInteractionComponent.h"
#include "Data/HeistGameBalanceDataAsset.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Inventory/HeistItemDataTypes.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#pragma region Construction

AHeistLootActor::AHeistLootActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(20.0f);
	CaseShell = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CaseShell"));
	CaseShell->SetupAttachment(RootComponent);
	CaseShell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CaseShell->SetGenerateOverlapEvents(false);
	CaseShell->SetCanEverAffectNavigation(false);
	CaseShell->SetVisibility(false);
}

#pragma endregion

#pragma region Lifecycle

void AHeistLootActor::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		ResolveLootData();
		CaseGameState = GetWorld()->GetGameState<AHeistGameState>();
		if (bExhibitionPresentation && CaseGameState.IsValid())
		{
			CaseGameState->GetMatchPhaseChangedDelegate().AddUObject(this, &AHeistLootActor::HandleCaseMatchPhaseChanged);
		}
	}
	ResolveLootVisualFromRowId();
	RefreshAvailabilityPresentation();
}

void AHeistLootActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(CaseValidationTimer);
	if (CaseGameState.IsValid()) CaseGameState->GetMatchPhaseChangedDelegate().RemoveAll(this);
	Super::EndPlay(EndPlayReason);
}

#pragma endregion

#pragma region LootData

FName AHeistLootActor::GetLootRowId() const
{
	return LootRowId;
}

void AHeistLootActor::InitializeLootData(UDataTable* InLootDataTable, const FName InLootRowId)
{
	checkf(HasAuthority(), TEXT("Loot data initialization requires authority."));
	checkf(!HasActorBegunPlay(), TEXT("Loot data must be initialized before BeginPlay."));

	LootDataRow.DataTable = InLootDataTable;
	LootDataRow.RowName = InLootRowId;
}

int32 AHeistLootActor::GetScoreValue() const
{
	return ScoreValue;
}

float AHeistLootActor::GetWeightValue() const
{
	return WeightValue;
}

EHeistLootGrade AHeistLootActor::GetLootGrade() const
{
	return LootGrade;
}

bool AHeistLootActor::IsLootAvailable() const
{
	return bIsAvailable && (!bExhibitionPresentation || bExhibitionLootActive) && !PickupReservationOwner.IsValid();
}

bool AHeistLootActor::IsPickupReady() const
{
	return IsLootAvailable() && (!bExhibitionPresentation || bCaseOpen);
}

#pragma endregion

#pragma region ExhibitionCase

void AHeistLootActor::InitializeExhibitionLoot(const bool bInActive, const bool bInLocked)
{
	checkf(HasAuthority(), TEXT("Exhibition loot initialization requires authority."));
	checkf(!HasActorBegunPlay(), TEXT("Exhibition loot must be initialized before BeginPlay."));
	bExhibitionPresentation = true;
	bExhibitionLootActive = bInActive;
	bCaseOpen = !bInLocked;
	CompletedCaseLatches = 0;
}

float AHeistLootActor::GetCaseServerTime() const
{
	const AHeistGameState* State = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	return IsValid(State) ? State->GetServerWorldTimeSeconds() : 0.0f;
}

float AHeistLootActor::GetCaseTimingProgress() const
{
	if (!IsValid(CaseOperator)) return 0.0f;
	return FMath::Fmod(FMath::Max(0.0f, GetCaseServerTime() - CaseRoundStartServerTime) / 6.0f, 1.0f);
}

float AHeistLootActor::GetCaseSuccessWindowWidth() const
{
	return CompletedCaseLatches == 0 ? 0.28f : CompletedCaseLatches == 1 ? 0.20f : 0.14f;
}

void AHeistLootActor::StartCaseRound(const float Now)
{
	const float HalfWidth = GetCaseSuccessWindowWidth() * 0.5f;
	CaseSuccessWindowCenter = FMath::FRandRange(HalfWidth, 1.0f - HalfWidth);
	CaseRoundStartServerTime = Now + 0.5f;
	++CaseRevision;
	ForceNetUpdate();
}

bool AHeistLootActor::TryUseExhibitionCase(AHeistPlayerCharacter* Character, const int32 ExpectedRevision)
{
	if (!HasAuthority() || !bExhibitionPresentation || bCaseOpen || ExpectedRevision != CaseRevision || !CanInteract(Character) ||
		!Character->GetInteractionComponent()->IsActorOverlappingInteractionArea(this)) return false;
	if (IsValid(CaseOperator) && !IsCaseOperatorValid())
	{
		CancelCaseOperation();
		return false;
	}
	AHeistPlayerState* Player = Character->GetPlayerState<AHeistPlayerState>();
	if (IsValid(CaseOperator) && CaseOperator != Player) return false;
	const float Now = GetCaseServerTime();
	if (LastCaseAttemptPlayer == Player && Now - LastCaseAttemptServerTime < 0.15f) return false;
	if (!IsValid(CaseOperator))
	{
		CaseOperator = Player;
		CaseOperationOrigin = Character->GetActorLocation();
		LastCaseAttemptServerTime = Now;
		LastCaseAttemptPlayer = Player;
		StartCaseRound(Now);
		GetWorldTimerManager().SetTimer(CaseValidationTimer, this, &AHeistLootActor::ValidateCaseOperation, 0.1f, true);
		MulticastCaseSound(0);
		return true;
	}
	if (!IsCaseOperatorValid() || Now < CaseRoundStartServerTime) return false;
	const float PingMs = Player->GetPingInMilliseconds();
	const float TransitSeconds = FMath::IsFinite(PingMs) ? FMath::Clamp(PingMs * 0.0005f, 0.0f, 0.25f) : 0.0f;
	const float InputTime = Now - TransitSeconds;
	if (InputTime < CaseRoundStartServerTime) return false;
	LastCaseAttemptServerTime = Now;
	LastCaseAttemptPlayer = Player;
	const float Progress = FMath::Fmod((InputTime - CaseRoundStartServerTime) / 6.0f, 1.0f);
	if (FMath::Abs(Progress - CaseSuccessWindowCenter) <= GetCaseSuccessWindowWidth() * 0.5f)
	{
		++CompletedCaseLatches;
		MulticastCaseSound(0);
		if (CompletedCaseLatches == 3)
		{
			bCaseOpen = true;
			CancelCaseOperation();
			RefreshAvailabilityPresentation();
			MulticastCaseSound(2);
			return true;
		}
	}
	else
	{
		MulticastCaseSound(1);
		FHeistSoundPingEvent Noise;
		Noise.PingType = EHeistSoundPingType::DisplayCaseLock;
		Noise.SoundPingTag = FHeistGameplayTags::Get().Event_SoundPing_DisplayCaseLock;
		Noise.WorldLocation = GetActorLocation();
		Noise.Radius = 2400.0f;
		Noise.Duration = 3.0f;
		Noise.bAffectsGuards = true;
		Noise.ServerTimeSeconds = Now;
		GetWorld()->GetGameState<AHeistGameState>()->ReportSoundPing(Noise);
	}
	StartCaseRound(Now);
	return true;
}

bool AHeistLootActor::IsCaseOperatorValid() const
{
	const AHeistPlayerCharacter* Character = IsValid(CaseOperator) ? Cast<AHeistPlayerCharacter>(CaseOperator->GetPawn()) : nullptr;
	return IsValid(Character) && CanInteract(Character) && Character->GetInteractionComponent()->IsActorOverlappingInteractionArea(this) &&
		FVector::DistSquared(CaseOperationOrigin, Character->GetActorLocation()) <= FMath::Square(48.0f);
}

void AHeistLootActor::ValidateCaseOperation()
{
	if (!IsCaseOperatorValid()) CancelCaseOperation();
}

void AHeistLootActor::CancelExhibitionCaseForPlayer(AHeistPlayerCharacter* Character)
{
	if (HasAuthority() && IsValid(Character) && CaseOperator == Character->GetPlayerState()) CancelCaseOperation();
}

void AHeistLootActor::CancelCaseOperation()
{
	if (!HasAuthority()) return;
	GetWorldTimerManager().ClearTimer(CaseValidationTimer);
	CaseOperator = nullptr;
	CaseRoundStartServerTime = 0.0f;
	++CaseRevision;
	ForceNetUpdate();
}

void AHeistLootActor::HandleCaseMatchPhaseChanged(EHeistMatchPhase Previous, EHeistMatchPhase Current)
{
	if (Current != EHeistMatchPhase::InGame && IsValid(CaseOperator)) CancelCaseOperation();
}

FText AHeistLootActor::GetCasePrompt(const AHeistPlayerCharacter* Character) const
{
	if (bCaseOpen) return NSLOCTEXT("HeistLootCase", "Pickup", "[E] 전리품 줍기");
	if (IsValid(CaseOperator) && (!IsValid(Character) || CaseOperator != Character->GetPlayerState()))
		return FText::Format(NSLOCTEXT("HeistLootCase", "OtherLatches", "동료가 잠금 해제 중 · 걸쇠 {0}/3"), FText::AsNumber(CompletedCaseLatches));
	if (IsValid(CaseOperator))
		return FText::Format(NSLOCTEXT("HeistLootCase", "Latches", "걸쇠 {0}/3 · 초록 구간에서 [E]"), FText::AsNumber(CompletedCaseLatches));
	return NSLOCTEXT("HeistLootCase", "Begin", "[E] 케이스 열기 · 이동하면 중단");
}

void AHeistLootActor::OnRep_CaseState()
{
	RefreshAvailabilityPresentation();
}

void AHeistLootActor::MulticastCaseSound_Implementation(const uint8 Event)
{
	USoundBase* Sound = Event == 1 ? CaseFailureSound.Get() : Event == 2 ? CaseOpenSound.Get() : CaseLatchSound.Get();
	if (IsValid(Sound)) UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(), 1.0f, 1.0f, 0.0f, CaseSoundAttenuation);
}

#pragma endregion

#pragma region Interaction

bool AHeistLootActor::CanInteract(const AActor* Interactor) const
{
	if (!IsLootAvailable() || !Super::CanInteract(Interactor)) return false;
	if (!bExhibitionPresentation) return true;
	const AHeistPlayerCharacter* Character = Cast<AHeistPlayerCharacter>(Interactor);
	const AHeistPlayerState* Player = IsValid(Character) ? Character->GetPlayerState<AHeistPlayerState>() : nullptr;
	const AHeistGameState* State = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	return IsValid(Player) && IsValid(State) && State->GetMatchPhase() == EHeistMatchPhase::InGame && State->PlayerArray.Contains(Player) &&
		Character->CanPerformGameplayActions();
}

#pragma endregion

#pragma region Replication

void AHeistLootActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeistLootActor, LootRowId);
	DOREPLIFETIME(AHeistLootActor, LootGrade);
	DOREPLIFETIME(AHeistLootActor, ScoreValue);
	DOREPLIFETIME(AHeistLootActor, WeightValue);
	DOREPLIFETIME(AHeistLootActor, bIsAvailable);
	DOREPLIFETIME(AHeistLootActor, bExhibitionPresentation);
	DOREPLIFETIME(AHeistLootActor, bExhibitionLootActive);
	DOREPLIFETIME(AHeistLootActor, bCaseOpen);
	DOREPLIFETIME(AHeistLootActor, CompletedCaseLatches);
	DOREPLIFETIME(AHeistLootActor, CaseRevision);
	DOREPLIFETIME(AHeistLootActor, CaseOperator);
	DOREPLIFETIME(AHeistLootActor, CaseRoundStartServerTime);
	DOREPLIFETIME(AHeistLootActor, CaseSuccessWindowCenter);
}

#pragma endregion

#pragma region LootPickup

bool AHeistLootActor::TryReserveForPickup(AActor* Requester)
{
	if (!HasAuthority() || !IsValid(Requester) || !IsPickupReady() || (bExhibitionPresentation && !CanInteract(Requester)))
	{
		return false;
	}

	PickupReservationOwner = Requester;
	return true;
}

bool AHeistLootActor::CommitPickupReservation(AActor* Requester)
{
	if (!HasAuthority() || !IsValid(Requester) || PickupReservationOwner.Get() != Requester || !bIsAvailable)
	{
		return false;
	}

	bIsAvailable = false;
	PickupReservationOwner.Reset();
	RefreshAvailabilityPresentation();
	ForceNetUpdate();
	LootPickupCommittedDelegate.Broadcast(this, Requester);
	return true;
}

void AHeistLootActor::ReleasePickupReservation(AActor* Requester)
{
	if (HasAuthority() && IsValid(Requester) && PickupReservationOwner.Get() == Requester)
	{
		PickupReservationOwner.Reset();
	}
}

FHeistLootPickupCommitted& AHeistLootActor::GetLootPickupCommittedDelegate()
{
	return LootPickupCommittedDelegate;
}

#pragma endregion

#pragma region InternalHelpers

void AHeistLootActor::ResolveLootData()
{
	const FHeistLootDataRow* ResolvedRow = LootDataRow.GetRow<FHeistLootDataRow>(TEXT("AHeistLootActor::ResolveLootData"));

	if (ResolvedRow != nullptr)
	{
		const FName ResolvedItemId = ResolvedRow->ItemId.IsNone() ? LootDataRow.RowName : ResolvedRow->ItemId;
		const AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
		FHeistItemDataRow ItemDefinition;
		if (IsValid(HeistGameMode) && HeistGameMode->TryGetItemDefinition(ResolvedItemId, ItemDefinition) && ItemDefinition.ItemType == EHeistItemType::Loot)
		{
			LootRowId = ResolvedItemId;
			LootGrade = ResolvedRow->LootGrade;
			ScoreValue = ResolvedRow->ScoreValue;
			WeightValue = ItemDefinition.Weight;
			bIsAvailable = true;
			RefreshAvailabilityPresentation();
			return;
		}
	}

	ApplyFallbackLootData();
#if !UE_BUILD_SHIPPING
	UE_LOG(LogHeistInventory, Warning, TEXT("LootDataRow '%s' was not found. Fallback values are active."), *LootDataRow.RowName.ToString());
#endif
}

void AHeistLootActor::ResolveLootVisualFromRowId()
{
	if (LootRowId.IsNone() || !IsValid(VisualMeshComponent))
	{
		return;
	}

	const UHeistGameBalanceDataAsset* BalanceData = GetDefault<UHeistGameBalanceDataAsset>();
	UDataTable* LootDataTable = IsValid(BalanceData) ? BalanceData->LootDataTable.LoadSynchronous() : nullptr;
	const FHeistLootDataRow* LootDefinition = IsValid(LootDataTable) && LootDataTable->GetRowStruct() == FHeistLootDataRow::StaticStruct()
		? LootDataTable->FindRow<FHeistLootDataRow>(LootRowId, TEXT("AHeistLootActor::ResolveLootVisualFromRowId"), false)
		: nullptr;
	if (LootDefinition == nullptr || LootDefinition->ItemId != LootRowId)
	{
		VisualMeshComponent->SetStaticMesh(nullptr);
		UE_LOG(LogHeistInventory, Error, TEXT("Loot visual resolution failed: Actor=%s ItemId=%s Reason=MissingVisualRow"), *GetNameSafe(this), *LootRowId.ToString());
		return;
	}

	ApplyLootVisual(*LootDefinition);
}

void AHeistLootActor::ApplyLootVisual(const FHeistLootDataRow& LootDefinition)
{
	UStaticMesh* ResolvedMesh = LootDefinition.WorldMesh.LoadSynchronous();
	if (!IsValid(ResolvedMesh))
	{
		VisualMeshComponent->SetStaticMesh(nullptr);
		UE_LOG(LogHeistInventory, Error, TEXT("Loot visual resolution failed: Actor=%s ItemId=%s Reason=MissingWorldMesh"), *GetNameSafe(this), *LootDefinition.ItemId.ToString());
		return;
	}

	VisualMeshComponent->SetStaticMesh(ResolvedMesh);
	VisualMeshComponent->SetRelativeTransform(LootDefinition.WorldVisualRelativeTransform);
	VisualMeshComponent->EmptyOverrideMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < LootDefinition.WorldMaterials.Num(); ++MaterialIndex)
	{
		if (UMaterialInterface* Material = LootDefinition.WorldMaterials[MaterialIndex].LoadSynchronous(); IsValid(Material))
		{
			VisualMeshComponent->SetMaterial(MaterialIndex, Material);
		}
	}
}

void AHeistLootActor::ApplyFallbackLootData()
{
	LootRowId = LootDataRow.RowName;
	LootGrade = EHeistLootGrade::OneStar;
	ScoreValue = 0;
	WeightValue = 0.0f;
	bIsAvailable = true;
	RefreshAvailabilityPresentation();
}

void AHeistLootActor::RefreshAvailabilityPresentation()
{
	if (IsValid(VisualMeshComponent))
	{
		VisualMeshComponent->SetVisibility(bIsAvailable, true);
	}
	if (IsValid(CaseShell)) CaseShell->SetVisibility(bExhibitionPresentation && !bCaseOpen, true);

	if (IsValid(InteractionCollision))
	{
		const bool bInteractionEnabled = bIsAvailable && (!bExhibitionPresentation || bExhibitionLootActive);
		InteractionCollision->SetCollisionEnabled(bInteractionEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		InteractionCollision->SetGenerateOverlapEvents(bInteractionEnabled);
	}
}

void AHeistLootActor::OnRep_LootRowId()
{
	ResolveLootVisualFromRowId();
	RefreshAvailabilityPresentation();
}

void AHeistLootActor::OnRep_IsAvailable()
{
	RefreshAvailabilityPresentation();
}

#pragma endregion
