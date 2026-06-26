#include "Character/Components/HeistActionComponent.h"

#include "Character/Components/HeistInventoryComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerState.h"
#include "Data/HeistGameBalanceDataAsset.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Engine/World.h"
#include "Inventory/HeistInventoryTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "World/Actors/Escape/HeistVentActor.h"
#include "World/Actors/Trap/HeistTrapActor.h"

#pragma region Construction

UHeistActionComponent::UHeistActionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

#pragma endregion

#pragma region Lifecycle

void UHeistActionComponent::BeginPlay()
{
	Super::BeginPlay();

	SetComponentTickEnabled(false);

	AActor* OwnerActor = GetOwner();
	if (IsValid(OwnerActor) && OwnerActor->HasAuthority())
	{
		OwnerActor->OnTakeAnyDamage.AddDynamic(this, &UHeistActionComponent::HandleOwnerTakeAnyDamage);
	}
}

void UHeistActionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AActor* OwnerActor = GetOwner();
	if (IsValid(OwnerActor))
	{
		OwnerActor->OnTakeAnyDamage.RemoveDynamic(this, &UHeistActionComponent::HandleOwnerTakeAnyDamage);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EscapeCastTimerHandle);
		World->GetTimerManager().ClearTimer(TrapPlacementCastTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UHeistActionComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEscapeCastActive && !bTrapPlacementCastActive)
	{
		SetComponentTickEnabled(false);
		return;
	}

	const AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	if (!IsValid(HeistCharacter))
	{
		CancelEscapeCast(TEXT("InvalidCastState"));
		CancelTrapPlacementCast(TEXT("InvalidCastState"));
		return;
	}

	if (bEscapeCastActive && !PendingEscapeVent.IsValid())
	{
		CancelEscapeCast(TEXT("InvalidCastState"));
		return;
	}

	if (bEscapeCastActive && HasMovedBeyondEscapeCastTolerance())
	{
		CancelEscapeCast(TEXT("Movement"));
		return;
	}

	if (bTrapPlacementCastActive && HasMovedBeyondTrapPlacementCastTolerance())
	{
		CancelTrapPlacementCast(TEXT("Movement"));
		return;
	}

	if (bEscapeCastActive && !PendingEscapeVent->CanUseVent(HeistCharacter))
	{
		CancelEscapeCast(TEXT("VentUnavailable"));
	}
}

#pragma endregion

#pragma region EscapeCast

bool UHeistActionComponent::TryBeginEscapeRequest(AHeistVentActor* TargetVentActor)
{
	AActor* OwnerActor = GetOwner();
	const AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(OwnerActor);
	const AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter)
		? HeistCharacter->GetPlayerState<AHeistPlayerState>()
		: nullptr;
	if (!IsValid(OwnerActor)
		|| !OwnerActor->HasAuthority()
		|| !IsValid(HeistPlayerState)
		|| HeistPlayerState->IsEscaped()
		|| !IsValid(TargetVentActor)
		|| bEscapeCastActive
		|| PendingEscapeVent.IsValid())
	{
		return false;
	}

	PendingEscapeVent = TargetVentActor;
	bEscapeCastActive = true;
	EscapeCastStartLocation = OwnerActor->GetActorLocation();

	const float EscapeCastDurationSeconds = ResolveEscapeCastDurationSeconds();
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const float ServerWorldTime = IsValid(HeistGameState)
		? HeistGameState->GetServerWorldTimeSeconds()
		: (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	EscapeCastEndServerTime = ServerWorldTime + EscapeCastDurationSeconds;

	SetComponentTickEnabled(true);
	OwnerActor->ForceNetUpdate();

	UHeistDebugFunctionLibrary::DebugEscapeCastStarted(
		this,
		OwnerActor,
		TargetVentActor,
		EscapeCastDurationSeconds,
		EscapeCastEndServerTime);

	if (EscapeCastDurationSeconds <= 0.0f)
	{
		HandleEscapeCastTimerElapsed();
	}
	else if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			EscapeCastTimerHandle,
			this,
			&UHeistActionComponent::HandleEscapeCastTimerElapsed,
			EscapeCastDurationSeconds,
			false);
	}
	else
	{
		CancelEscapeCast(TEXT("MissingWorld"));
		return false;
	}

	return true;
}

bool UHeistActionComponent::IsGameplayCastActive() const
{
	return bEscapeCastActive || bTrapPlacementCastActive;
}

bool UHeistActionComponent::HasPendingEscapeRequest() const
{
	return bEscapeCastActive || PendingEscapeVent.IsValid();
}

AHeistVentActor* UHeistActionComponent::GetPendingEscapeVent() const
{
	return PendingEscapeVent.Get();
}

void UHeistActionComponent::ClearPendingEscapeRequest()
{
	const AActor* OwnerActor = GetOwner();
	if (IsValid(OwnerActor) && OwnerActor->HasAuthority())
	{
		ClearEscapeCastState();
	}
}

bool UHeistActionComponent::IsEscapeCastActive() const
{
	return bEscapeCastActive;
}

float UHeistActionComponent::GetEscapeCastEndServerTime() const
{
	return EscapeCastEndServerTime;
}

FHeistEscapeCastCompleted& UHeistActionComponent::GetEscapeCastCompletedDelegate()
{
	return EscapeCastCompletedDelegate;
}

void UHeistActionComponent::OnRep_EscapeCastActive()
{
	UHeistDebugFunctionLibrary::DebugEscapeCastStateReplicated(
		this,
		GetOwner(),
		bEscapeCastActive,
		EscapeCastEndServerTime);
}

void UHeistActionComponent::HandleOwnerTakeAnyDamage(
	AActor*,
	float Damage,
	const UDamageType*,
	AController*,
	AActor*)
{
	if (bEscapeCastActive && Damage > 0.0f)
	{
		CancelEscapeCast(TEXT("Damage"));
	}

	if (bTrapPlacementCastActive && Damage > 0.0f)
	{
		CancelTrapPlacementCast(TEXT("Damage"));
	}
}

#pragma endregion

#pragma region TrapPlacementCast

bool UHeistActionComponent::TryBeginTrapPlacementRequest(
	const FName SourceItemId,
	const int32 SourceInstanceId,
	const FVector& TargetWorldLocation,
	const float CastDurationSeconds,
	const float EffectDurationSeconds,
	const TSubclassOf<AHeistTrapActor> TrapActorClass,
	const bool bConsumeSourceItem)
{
	AActor* OwnerActor = GetOwner();
	const AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(OwnerActor);
	const AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter)
		? HeistCharacter->GetPlayerState<AHeistPlayerState>()
		: nullptr;
	if (!IsValid(OwnerActor)
		|| !OwnerActor->HasAuthority()
		|| !IsValid(HeistPlayerState)
		|| HeistPlayerState->IsEscaped()
		|| SourceItemId.IsNone()
		|| !IsValid(TrapActorClass)
		|| EffectDurationSeconds <= 0.0f
		|| bEscapeCastActive
		|| bTrapPlacementCastActive)
	{
		return false;
	}

	PendingTrapItemId = SourceItemId;
	PendingTrapSourceInstanceId = SourceInstanceId;
	PendingTrapTargetWorldLocation = TargetWorldLocation;
	PendingTrapEffectDurationSeconds = FMath::Max(0.0f, EffectDurationSeconds);
	PendingTrapActorClass = TrapActorClass;
	bPendingTrapConsumesSourceItem = bConsumeSourceItem;
	bTrapPlacementCastActive = true;
	TrapPlacementCastStartLocation = OwnerActor->GetActorLocation();

	const float SafeCastDurationSeconds = FMath::Max(0.0f, CastDurationSeconds);
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const float ServerWorldTime = IsValid(HeistGameState)
		? HeistGameState->GetServerWorldTimeSeconds()
		: (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	TrapPlacementCastEndServerTime = ServerWorldTime + SafeCastDurationSeconds;

	SetComponentTickEnabled(true);
	OwnerActor->ForceNetUpdate();

	UHeistDebugFunctionLibrary::DebugTrapPlacementCastStarted(
		this,
		OwnerActor,
		SourceItemId,
		TargetWorldLocation,
		SafeCastDurationSeconds,
		TrapPlacementCastEndServerTime);

	if (SafeCastDurationSeconds <= 0.0f)
	{
		HandleTrapPlacementCastTimerElapsed();
	}
	else if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			TrapPlacementCastTimerHandle,
			this,
			&UHeistActionComponent::HandleTrapPlacementCastTimerElapsed,
			SafeCastDurationSeconds,
			false);
	}
	else
	{
		CancelTrapPlacementCast(TEXT("MissingWorld"));
		return false;
	}

	return true;
}

bool UHeistActionComponent::IsTrapPlacementCastActive() const
{
	return bTrapPlacementCastActive;
}

FHeistTrapPlacementCastCompleted& UHeistActionComponent::GetTrapPlacementCastCompletedDelegate()
{
	return TrapPlacementCastCompletedDelegate;
}

void UHeistActionComponent::OnRep_TrapPlacementCastActive()
{
	UHeistDebugFunctionLibrary::DebugTrapPlacementCastStateReplicated(
		this,
		GetOwner(),
		bTrapPlacementCastActive,
		TrapPlacementCastEndServerTime);
}

#pragma endregion

#pragma region Replication

void UHeistActionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UHeistActionComponent, bEscapeCastActive);
	DOREPLIFETIME(UHeistActionComponent, EscapeCastEndServerTime);
	DOREPLIFETIME(UHeistActionComponent, bTrapPlacementCastActive);
	DOREPLIFETIME(UHeistActionComponent, TrapPlacementCastEndServerTime);
}

#pragma endregion

#pragma region InternalHelpers

float UHeistActionComponent::ResolveEscapeCastDurationSeconds() const
{
	const UWorld* World = GetWorld();
	const AHeistGameMode* HeistGameMode = World ? World->GetAuthGameMode<AHeistGameMode>() : nullptr;
	if (IsValid(HeistGameMode))
	{
		return HeistGameMode->GetEscapeCastTimeSeconds();
	}

	return FMath::Max(0.0f, GetDefault<UHeistGameBalanceDataAsset>()->EscapeCastTime);
}

bool UHeistActionComponent::HasMovedBeyondEscapeCastTolerance() const
{
	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return true;
	}

	return FVector::DistSquared2D(OwnerActor->GetActorLocation(), EscapeCastStartLocation)
		> FMath::Square(EscapeCastMovementCancelDistance);
}

bool UHeistActionComponent::HasMovedBeyondTrapPlacementCastTolerance() const
{
	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return true;
	}

	return FVector::DistSquared2D(OwnerActor->GetActorLocation(), TrapPlacementCastStartLocation)
		> FMath::Square(TrapPlacementMovementCancelDistance);
}

void UHeistActionComponent::HandleEscapeCastTimerElapsed()
{
	AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	AHeistVentActor* TargetVentActor = PendingEscapeVent.Get();
	AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter)
		? HeistCharacter->GetPlayerState<AHeistPlayerState>()
		: nullptr;
	if (!bEscapeCastActive
		|| !IsValid(HeistCharacter)
		|| !IsValid(HeistPlayerState)
		|| !IsValid(TargetVentActor)
		|| HasMovedBeyondEscapeCastTolerance()
		|| !TargetVentActor->CanUseVent(HeistCharacter))
	{
		CancelEscapeCast(TEXT("CompletionValidationFailed"));
		return;
	}

	if (!HeistPlayerState->MarkEscaped())
	{
		CancelEscapeCast(TEXT("EscapeCommitRejected"));
		return;
	}

	ClearEscapeCastState();

	UHeistDebugFunctionLibrary::DebugEscapeCastCompleted(this, HeistCharacter, TargetVentActor);

	EscapeCastCompletedDelegate.Broadcast(HeistCharacter, TargetVentActor);
}

void UHeistActionComponent::HandleTrapPlacementCastTimerElapsed()
{
	AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	UHeistInventoryComponent* InventoryComponent = IsValid(HeistCharacter)
		? HeistCharacter->GetInventoryComponent()
		: nullptr;
	if (!bTrapPlacementCastActive
		|| !IsValid(HeistCharacter)
		|| !IsValid(InventoryComponent)
		|| PendingTrapItemId.IsNone()
		|| !IsValid(PendingTrapActorClass)
		|| HasMovedBeyondTrapPlacementCastTolerance())
	{
		CancelTrapPlacementCast(TEXT("CompletionValidationFailed"));
		return;
	}

	if (bPendingTrapConsumesSourceItem)
	{
		FHeistInventoryItem SourceItem;
		if (PendingTrapSourceInstanceId == INDEX_NONE
			|| !InventoryComponent->TryGetItem(PendingTrapSourceInstanceId, SourceItem)
			|| SourceItem.ItemId != PendingTrapItemId)
		{
			CancelTrapPlacementCast(TEXT("SourceItemInvalid"));
			return;
		}
	}

	const FTransform SpawnTransform(FRotator::ZeroRotator, PendingTrapTargetWorldLocation);
	AHeistTrapActor* TrapActor = GetWorld()->SpawnActorDeferred<AHeistTrapActor>(
		PendingTrapActorClass,
		SpawnTransform,
		HeistCharacter,
		HeistCharacter,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!IsValid(TrapActor))
	{
		CancelTrapPlacementCast(TEXT("TrapSpawnFailed"));
		return;
	}

	TrapActor->InitializeTrap(HeistCharacter, PendingTrapItemId, PendingTrapEffectDurationSeconds);
	AHeistTrapActor* SpawnedTrap = Cast<AHeistTrapActor>(UGameplayStatics::FinishSpawningActor(TrapActor, SpawnTransform));
	if (!IsValid(SpawnedTrap))
	{
		CancelTrapPlacementCast(TEXT("TrapFinishSpawnFailed"));
		return;
	}

	if (bPendingTrapConsumesSourceItem)
	{
		FHeistInventoryItem RemovedItem;
		if (!InventoryComponent->TryRemoveItem(PendingTrapSourceInstanceId, RemovedItem)
			|| RemovedItem.ItemId != PendingTrapItemId)
		{
			SpawnedTrap->Destroy();
			CancelTrapPlacementCast(TEXT("SourceItemRemoveFailed"));
			return;
		}
	}

	const FName CompletedItemId = PendingTrapItemId;
	ClearTrapPlacementCastState();

	UHeistDebugFunctionLibrary::DebugTrapPlaced(
		this,
		HeistCharacter,
		SpawnedTrap,
		CompletedItemId,
		SpawnedTrap->GetActorLocation());

	TrapPlacementCastCompletedDelegate.Broadcast(HeistCharacter, SpawnedTrap);
}

void UHeistActionComponent::CancelEscapeCast(const TCHAR* Reason)
{
	if (!bEscapeCastActive)
	{
		return;
	}

	const FString CharacterName = GetNameSafe(GetOwner());
	const FString VentName = GetNameSafe(PendingEscapeVent.Get());
	ClearEscapeCastState();

	UHeistDebugFunctionLibrary::DebugEscapeCastCancelled(this, CharacterName, VentName, Reason);
}

void UHeistActionComponent::CancelTrapPlacementCast(const TCHAR* Reason)
{
	if (!bTrapPlacementCastActive)
	{
		return;
	}

	const FString CharacterName = GetNameSafe(GetOwner());
	const FName ItemId = PendingTrapItemId;
	ClearTrapPlacementCastState();

	UHeistDebugFunctionLibrary::DebugTrapPlacementCastCancelled(this, CharacterName, ItemId, Reason);
}

void UHeistActionComponent::ClearEscapeCastState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EscapeCastTimerHandle);
	}

	PendingEscapeVent.Reset();
	bEscapeCastActive = false;
	EscapeCastEndServerTime = 0.0f;
	SetComponentTickEnabled(false);

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
}

void UHeistActionComponent::ClearTrapPlacementCastState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TrapPlacementCastTimerHandle);
	}

	PendingTrapItemId = NAME_None;
	PendingTrapSourceInstanceId = INDEX_NONE;
	PendingTrapTargetWorldLocation = FVector::ZeroVector;
	PendingTrapEffectDurationSeconds = 0.0f;
	PendingTrapActorClass = nullptr;
	bPendingTrapConsumesSourceItem = false;
	bTrapPlacementCastActive = false;
	TrapPlacementCastEndServerTime = 0.0f;

	if (!bEscapeCastActive)
	{
		SetComponentTickEnabled(false);
	}

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
}

#pragma endregion
