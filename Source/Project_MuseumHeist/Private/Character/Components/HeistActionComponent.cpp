#include "Character/Components/HeistActionComponent.h"

#include "Character/Components/HeistInventoryComponent.h"
#include "Character/Components/HeistForgeryComponent.h"
#include "Character/Components/HeistObjectAssemblyComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerState.h"
#include "Data/HeistGameBalanceDataAsset.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "World/Actors/Loot/HeistPaintingDisplayCaseActor.h"
#include "World/Actors/Loot/HeistObjectDisplayCaseActor.h"
#include "World/Actors/Escape/HeistVentActor.h"

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
		World->GetTimerManager().ClearTimer(ObservationCastTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UHeistActionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEscapeCastActive && !bObservationCastActive)
	{
		SetComponentTickEnabled(false);
		return;
	}

	const AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	if (!IsValid(HeistCharacter))
	{
		CancelEscapeCast(TEXT("InvalidCastState"));
		CancelObservationCast(TEXT("InvalidCastState"));
		return;
	}

	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistGameState) || HeistGameState->AreWorldInteractionsRestricted())
	{
		CancelEscapeCast(TEXT("WorldRestricted"));
		CancelObservationCast(TEXT("WorldRestricted"));
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

	if (bEscapeCastActive && HeistGameState->GetAlertRevision() != EscapeCastStartAlertRevision)
	{
		CancelEscapeCast(TEXT("Alert"));
		return;
	}

	if (bObservationCastActive)
	{
		AActor* TargetActor = PendingObservationTarget.Get();
		const AHeistPlayerState* HeistPlayerState = HeistCharacter->GetPlayerState<AHeistPlayerState>();
		const AHeistPaintingDisplayCaseActor* PaintingCase = Cast<AHeistPaintingDisplayCaseActor>(TargetActor);
		const AHeistObjectDisplayCaseActor* ObjectCase = Cast<AHeistObjectDisplayCaseActor>(TargetActor);
		const bool bSessionValid = IsValid(HeistPlayerState) &&
			((IsValid(PaintingCase) && PaintingCase->IsSessionLocked() && PaintingCase->GetSessionOwner() == HeistPlayerState) ||
			 (IsValid(ObjectCase) && ObjectCase->IsSessionLocked() && ObjectCase->GetSessionOwner() == HeistPlayerState));
		if (!bSessionValid)
		{
			CancelObservationCast(TEXT("SessionInvalid"));
			return;
		}

		if (HasMovedBeyondObservationCastTolerance())
		{
			CancelObservationCast(TEXT("Movement"));
			return;
		}
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
	const AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter) ? HeistCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || !IsValid(HeistPlayerState) || HeistPlayerState->IsEscaped() || HeistPlayerState->IsArrested() || !IsValid(TargetVentActor) || bEscapeCastActive ||
		IsGameplayCastActive() || PendingEscapeVent.IsValid() || !IsValid(HeistGameState) || HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame ||
		!HeistGameState->IsContractInitialized() || HeistGameState->AreWorldInteractionsRestricted())
	{
		return false;
	}

	PendingEscapeVent = TargetVentActor;
	bEscapeCastActive = true;
	EscapeCastStartLocation = OwnerActor->GetActorLocation();
	EscapeCastStartAlertRevision = HeistGameState->GetAlertRevision();

	const float EscapeCastDurationSeconds = ResolveEscapeCastDurationSeconds();
	const float ServerWorldTime = IsValid(HeistGameState) ? HeistGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	EscapeCastEndServerTime = ServerWorldTime + EscapeCastDurationSeconds;

	SetComponentTickEnabled(true);
	OwnerActor->ForceNetUpdate();
	ActionStateChangedDelegate.Broadcast();

	UHeistDebugFunctionLibrary::DebugEscapeCastStarted(this, OwnerActor, TargetVentActor, EscapeCastDurationSeconds, EscapeCastEndServerTime);

	if (EscapeCastDurationSeconds <= 0.0f)
	{
		HandleEscapeCastTimerElapsed();
	}
	else if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(EscapeCastTimerHandle, this, &UHeistActionComponent::HandleEscapeCastTimerElapsed, EscapeCastDurationSeconds, false);
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
	return bEscapeCastActive || bObservationCastActive;
}

void UHeistActionComponent::CancelGameplayActions(const TCHAR* Reason)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	CancelEscapeCast(Reason ? Reason : TEXT("Cancelled"));
	CancelObservationCast(Reason ? Reason : TEXT("Cancelled"));
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

FHeistActionStateChanged& UHeistActionComponent::GetActionStateChangedDelegate()
{
	return ActionStateChangedDelegate;
}

void UHeistActionComponent::OnRep_EscapeCastActive()
{
	ActionStateChangedDelegate.Broadcast();
	UHeistDebugFunctionLibrary::DebugEscapeCastStateReplicated(this, GetOwner(), bEscapeCastActive, EscapeCastEndServerTime);
}

void UHeistActionComponent::HandleOwnerTakeAnyDamage(AActor*, float Damage, const UDamageType*, AController*, AActor*)
{
	if (bEscapeCastActive && Damage > 0.0f)
	{
		CancelEscapeCast(TEXT("Damage"));
	}

	if (bObservationCastActive && Damage > 0.0f)
	{
		CancelObservationCast(TEXT("Damage"));
	}
}

#pragma endregion

#pragma region ObservationCast

bool UHeistActionComponent::TryBeginObservationRequest(AHeistPaintingDisplayCaseActor* TargetDisplayCase)
{
	AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter) ? HeistCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	UHeistForgeryComponent* ForgeryComponent = IsValid(HeistCharacter) ? HeistCharacter->GetForgeryComponent() : nullptr;
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const bool bAlertDanger = IsValid(HeistGameState) &&
		(HeistGameState->GetAlertLevel() == EHeistAlertLevel::Alarmed || HeistGameState->GetAlertLevel() == EHeistAlertLevel::Lockdown);
	if (!IsValid(HeistCharacter) || !HeistCharacter->HasAuthority() || !IsValid(HeistPlayerState) || !IsValid(TargetDisplayCase) || !IsValid(ForgeryComponent) || IsGameplayCastActive() ||
		TargetDisplayCase->GetDisplayCaseState() != EHeistDisplayCaseState::Secured || !IsValid(HeistGameState) || HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame ||
		HeistGameState->AreWorldInteractionsRestricted() || bAlertDanger)
	{
		return false;
	}

	float IgnoredTemplateObservationDuration = 0.0f;
	if (!ForgeryComponent->TryPrepareForgeryTemplate(TargetDisplayCase, IgnoredTemplateObservationDuration))
	{
		return false;
	}

	if (!TargetDisplayCase->TryBeginSession(HeistPlayerState))
	{
		ForgeryComponent->ClearPreparedForgeryTemplate(FName(TEXT("ObservationSessionRejected")));
		return false;
	}

	return BeginObservationCast(HeistCharacter, TargetDisplayCase);
}

bool UHeistActionComponent::TryBeginObservationRequest(AHeistObjectDisplayCaseActor* TargetDisplayCase)
{
	AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter) ? HeistCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	UHeistForgeryComponent* ForgeryComponent = IsValid(HeistCharacter) ? HeistCharacter->GetForgeryComponent() : nullptr;
	UHeistObjectAssemblyComponent* ObjectAssemblyComponent = IsValid(HeistCharacter) ? HeistCharacter->GetObjectAssemblyComponent() : nullptr;
	UHeistInventoryComponent* InventoryComponent = IsValid(HeistCharacter) ? HeistCharacter->GetInventoryComponent() : nullptr;
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const bool bAlertDanger = IsValid(HeistGameState) &&
		(HeistGameState->GetAlertLevel() == EHeistAlertLevel::Alarmed || HeistGameState->GetAlertLevel() == EHeistAlertLevel::Lockdown);
	if (!IsValid(HeistCharacter) || !HeistCharacter->HasAuthority() || !IsValid(HeistPlayerState) || !IsValid(TargetDisplayCase) || !IsValid(ObjectAssemblyComponent) ||
		IsGameplayCastActive() || TargetDisplayCase->GetAssemblyState() != EHeistObjectAssemblyState::Secured || TargetDisplayCase->IsSessionLocked() ||
		ObjectAssemblyComponent->IsSessionActive() || ObjectAssemblyComponent->HasPendingReplicaReview() ||
		(IsValid(ForgeryComponent) && ForgeryComponent->IsSessionActive()) || (IsValid(InventoryComponent) && InventoryComponent->IsInventoryOpen()) ||
		!IsValid(HeistGameState) || HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame || HeistGameState->AreWorldInteractionsRestricted() || bAlertDanger)
	{
		return false;
	}

	if (!TargetDisplayCase->TryBeginSession(HeistPlayerState))
	{
		return false;
	}

	return BeginObservationCast(HeistCharacter, TargetDisplayCase);
}

bool UHeistActionComponent::BeginObservationCast(AHeistPlayerCharacter* HeistCharacter, AActor* TargetActor)
{
	if (!IsValid(HeistCharacter) || !HeistCharacter->HasAuthority() || !IsValid(TargetActor))
	{
		return false;
	}

	PendingObservationTarget = TargetActor;
	bObservationCastActive = true;
	bObservationReferenceAvailable = TargetActor->IsA<AHeistPaintingDisplayCaseActor>();
	ObservationCastStartLocation = HeistCharacter->GetActorLocation();

	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const float ServerWorldTime = IsValid(HeistGameState) ? HeistGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	const float SafeDurationSeconds = FMath::Max(0.0f, ObservationCastDurationSeconds);
	ObservationCastEndServerTime = ServerWorldTime + SafeDurationSeconds;

	SetComponentTickEnabled(true);
	HeistCharacter->ForceNetUpdate();
	ActionStateChangedDelegate.Broadcast();
	UHeistDebugFunctionLibrary::DebugObservationCastStarted(this, HeistCharacter, TargetActor, SafeDurationSeconds, ObservationCastEndServerTime);

	if (SafeDurationSeconds <= 0.0f)
	{
		HandleObservationCastTimerElapsed();
	}
	else if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ObservationCastTimerHandle, this, &UHeistActionComponent::HandleObservationCastTimerElapsed, SafeDurationSeconds, false);
	}
	else
	{
		CancelObservationCast(TEXT("MissingWorld"));
		return false;
	}

	return true;
}

void UHeistActionComponent::CancelObservationRequest(const TCHAR* Reason)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		CancelObservationCast(Reason ? Reason : TEXT("OwnerCancelled"));
	}
}

bool UHeistActionComponent::IsObservationCastActive() const
{
	return bObservationCastActive;
}

float UHeistActionComponent::GetObservationCastEndServerTime() const
{
	return ObservationCastEndServerTime;
}

bool UHeistActionComponent::ShouldShowObservationReference() const
{
	return bObservationReferenceAvailable;
}

AActor* UHeistActionComponent::GetPendingObservationTarget() const
{
	return PendingObservationTarget.Get();
}

FHeistObservationCastCompleted& UHeistActionComponent::GetObservationCastCompletedDelegate()
{
	return ObservationCastCompletedDelegate;
}

void UHeistActionComponent::OnRep_ObservationCastActive()
{
	ActionStateChangedDelegate.Broadcast();
	UHeistDebugFunctionLibrary::DebugObservationCastStateReplicated(this, GetOwner(), bObservationCastActive, ObservationCastEndServerTime);
}

#pragma endregion

#pragma region Replication

void UHeistActionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UHeistActionComponent, bEscapeCastActive);
	DOREPLIFETIME(UHeistActionComponent, EscapeCastEndServerTime);
	DOREPLIFETIME(UHeistActionComponent, bObservationCastActive);
	DOREPLIFETIME(UHeistActionComponent, ObservationCastEndServerTime);
	DOREPLIFETIME_CONDITION(UHeistActionComponent, bObservationReferenceAvailable, COND_OwnerOnly);
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

	return FVector::DistSquared2D(OwnerActor->GetActorLocation(), EscapeCastStartLocation) > FMath::Square(EscapeCastMovementCancelDistance);
}

bool UHeistActionComponent::HasMovedBeyondObservationCastTolerance() const
{
	const AActor* OwnerActor = GetOwner();
	return !IsValid(OwnerActor) || FVector::DistSquared2D(OwnerActor->GetActorLocation(), ObservationCastStartLocation) > FMath::Square(ObservationCastMovementCancelDistance);
}

void UHeistActionComponent::HandleEscapeCastTimerElapsed()
{
	AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	AHeistVentActor* TargetVentActor = PendingEscapeVent.Get();
	AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter) ? HeistCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	UHeistInventoryComponent* InventoryComponent = IsValid(HeistCharacter) ? HeistCharacter->GetInventoryComponent() : nullptr;
	AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!bEscapeCastActive || !IsValid(HeistCharacter) || !IsValid(HeistPlayerState) || !IsValid(TargetVentActor) || HasMovedBeyondEscapeCastTolerance() ||
		!IsValid(InventoryComponent) || !IsValid(HeistGameState) || !HeistGameState->IsContractInitialized() ||
		HeistGameState->GetAlertRevision() != EscapeCastStartAlertRevision || !TargetVentActor->CanUseVent(HeistCharacter))
	{
		CancelEscapeCast(TEXT("CompletionValidationFailed"));
		return;
	}

	FHeistPlayerDepositPayload DepositPreview;
	const TCHAR* DepositRejectReason = nullptr;
	if (!InventoryComponent->TryBuildPlayerDepositPayload(DepositPreview, DepositRejectReason, EHeistDepositScope::LooseLootOnly))
	{
		CancelEscapeCast(DepositRejectReason != nullptr ? DepositRejectReason : TEXT("DepositPreviewRejected"));
		return;
	}
	const EHeistDepositScope DepositScope = DepositPreview.HasDeposit() ? EHeistDepositScope::LooseLootOnly : EHeistDepositScope::FullEscape;
	const bool bMidRunSettlement = DepositScope == EHeistDepositScope::LooseLootOnly;
	if (!bMidRunSettlement && !InventoryComponent->TryBuildPlayerDepositPayload(DepositPreview, DepositRejectReason, EHeistDepositScope::FullEscape))
	{
		CancelEscapeCast(DepositRejectReason != nullptr ? DepositRejectReason : TEXT("DepositPreviewRejected"));
		return;
	}

	const FHeistContractSnapshot ContractSnapshot = HeistGameState->GetContractSnapshot();
	for (const FHeistInventoryItem& OriginalItem : DepositPreview.OriginalArtifacts)
	{
		AActor* SourceDisplayCase = OriginalItem.SourceDisplayCase.Get();
		AHeistPaintingDisplayCaseActor* PaintingSourceCase = Cast<AHeistPaintingDisplayCaseActor>(SourceDisplayCase);
		AHeistObjectDisplayCaseActor* ObjectSourceCase = Cast<AHeistObjectDisplayCaseActor>(SourceDisplayCase);
		const FName SourceCaseId = IsValid(PaintingSourceCase) ? PaintingSourceCase->GetDisplayCaseId()
												: (IsValid(ObjectSourceCase) ? ObjectSourceCase->GetObjectCaseId() : NAME_None);
		const bool bMatchesRequiredTarget = OriginalItem.ItemId == ContractSnapshot.RequiredTargetArtifactId && SourceCaseId == ContractSnapshot.RequiredTargetCaseId;
		const bool bRequiredFlagConsistent = OriginalItem.bRequiredTarget == bMatchesRequiredTarget;
		const bool bSourceCanCommit =
			(IsValid(PaintingSourceCase) && PaintingSourceCase->CanCommitOriginalDepositForCarrier(HeistPlayerState, OriginalItem)) ||
			(IsValid(ObjectSourceCase) && ObjectSourceCase->CanCommitOriginalDepositForCarrier(HeistPlayerState, OriginalItem));
		if (!bRequiredFlagConsistent || !bSourceCanCommit)
		{
			CancelEscapeCast(!bRequiredFlagConsistent ? TEXT("RequiredTargetDepositMismatch") : TEXT("OriginalDepositSourceRejected"));
			return;
		}
	}

	if (DepositPreview.HasDeposit())
	{
		if (!HeistGameState->RefreshContractCarriedValue())
		{
			CancelEscapeCast(TEXT("ContractCarriedRefreshRejected"));
			return;
		}

		if (!HeistGameState->CanCommitPlayerDeposit(HeistPlayerState, DepositPreview.GetTotalValue(), DepositPreview.ContainsRequiredTarget(), DepositRejectReason))
		{
			CancelEscapeCast(DepositRejectReason != nullptr ? DepositRejectReason : TEXT("ContractDepositRejected"));
			return;
		}

		FHeistPlayerDepositPayload CommittedDeposit;
		if (!InventoryComponent->TryCommitPlayerDeposit(HeistPlayerState, DepositPreview, CommittedDeposit, DepositRejectReason, DepositScope))
		{
			CancelEscapeCast(DepositRejectReason != nullptr ? DepositRejectReason : TEXT("InventoryDepositRejected"));
			return;
		}

		for (const FHeistInventoryItem& OriginalItem : CommittedDeposit.OriginalArtifacts)
		{
			AActor* SourceDisplayCase = OriginalItem.SourceDisplayCase.Get();
			AHeistPaintingDisplayCaseActor* PaintingSourceCase = Cast<AHeistPaintingDisplayCaseActor>(SourceDisplayCase);
			AHeistObjectDisplayCaseActor* ObjectSourceCase = Cast<AHeistObjectDisplayCaseActor>(SourceDisplayCase);
			const bool bOriginalSourceCommitted =
				(IsValid(PaintingSourceCase) && PaintingSourceCase->CommitOriginalDepositForCarrier(HeistPlayerState, OriginalItem)) ||
				(IsValid(ObjectSourceCase) && ObjectSourceCase->CommitOriginalDepositForCarrier(HeistPlayerState, OriginalItem));
			checkf(bOriginalSourceCommitted, TEXT("Validated Original deposit source must commit after inventory deposit."));
		}
		const bool bContractDepositCommitted =
			HeistGameState->CommitPlayerDeposit(HeistPlayerState, CommittedDeposit.GetTotalValue(), CommittedDeposit.ContainsRequiredTarget());
		checkf(bContractDepositCommitted, TEXT("Validated Contract deposit must commit after inventory deposit."));
	}

	if (bMidRunSettlement)
	{
		ClearEscapeCastState();
		UHeistDebugFunctionLibrary::Message(
			this,
			FString::Printf(TEXT("Vent settlement committed: PlayerId=%d ItemCount=%d DepositValue=%d SecuredTotal=%d Escaped=false Authority=true Result=PASS"),
				HeistPlayerState->HeistPlayerId, DepositPreview.LooseLootItemCount, DepositPreview.LooseLootValue, HeistGameState->GetContractSnapshot().SecuredValue));
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

void UHeistActionComponent::HandleObservationCastTimerElapsed()
{
	AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter) ? HeistCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	AActor* TargetActor = PendingObservationTarget.Get();
	if (!bObservationCastActive || !IsValid(HeistCharacter) || !IsValid(HeistPlayerState) || !IsValid(TargetActor) || HasMovedBeyondObservationCastTolerance())
	{
		CancelObservationCast(TEXT("CompletionValidationFailed"));
		return;
	}

	if (AHeistPaintingDisplayCaseActor* PaintingCase = Cast<AHeistPaintingDisplayCaseActor>(TargetActor))
	{
		if (!PaintingCase->IsSessionLocked() || PaintingCase->GetSessionOwner() != HeistPlayerState || PaintingCase->GetDisplayCaseState() != EHeistDisplayCaseState::Secured ||
			!PaintingCase->TryTransitionToDisplayCaseState(EHeistDisplayCaseState::Observed))
		{
			CancelObservationCast(TEXT("ObservationCommitRejected"));
			return;
		}

		UHeistForgeryComponent* ForgeryComponent = HeistCharacter->GetForgeryComponent();
		if (!IsValid(ForgeryComponent) || !ForgeryComponent->TryBeginForgerySession(PaintingCase))
		{
			CancelObservationCast(TEXT("ForgeryHandoffRejected"));
			return;
		}
	}
	else if (AHeistObjectDisplayCaseActor* ObjectCase = Cast<AHeistObjectDisplayCaseActor>(TargetActor))
	{
		UHeistObjectAssemblyComponent* ObjectAssemblyComponent = HeistCharacter->GetObjectAssemblyComponent();
		if (!ObjectCase->IsSessionLocked() || ObjectCase->GetSessionOwner() != HeistPlayerState || ObjectCase->GetAssemblyState() != EHeistObjectAssemblyState::Secured ||
			!IsValid(ObjectAssemblyComponent) || !ObjectAssemblyComponent->TryBeginAssemblySession(ObjectCase))
		{
			CancelObservationCast(TEXT("AssemblyHandoffRejected"));
			return;
		}
	}
	else
	{
		CancelObservationCast(TEXT("UnsupportedObservationTarget"));
		return;
	}

	ClearObservationCastState();
	UHeistDebugFunctionLibrary::DebugObservationCastCompleted(this, HeistCharacter, TargetActor);
	ObservationCastCompletedDelegate.Broadcast(HeistCharacter, TargetActor);
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

void UHeistActionComponent::CancelObservationCast(const TCHAR* Reason)
{
	if (!bObservationCastActive)
	{
		return;
	}

	AActor* TargetActor = PendingObservationTarget.Get();
	AHeistPaintingDisplayCaseActor* PaintingCase = Cast<AHeistPaintingDisplayCaseActor>(TargetActor);
	AHeistObjectDisplayCaseActor* ObjectCase = Cast<AHeistObjectDisplayCaseActor>(TargetActor);
	AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter) ? HeistCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	const FString CharacterName = GetNameSafe(HeistCharacter);
	const FString DisplayCaseName = GetNameSafe(TargetActor);
	ClearObservationCastState();

	if (IsValid(PaintingCase) && IsValid(HeistCharacter) && IsValid(HeistCharacter->GetForgeryComponent()))
	{
		HeistCharacter->GetForgeryComponent()->ClearPreparedForgeryTemplate(FName(Reason ? Reason : TEXT("ObservationCancelled")));
	}

	if (IsValid(PaintingCase) && IsValid(HeistPlayerState) && PaintingCase->GetSessionOwner() == HeistPlayerState)
	{
		PaintingCase->TryCancelSession(HeistPlayerState);
	}
	else if (IsValid(ObjectCase) && IsValid(HeistPlayerState) && ObjectCase->GetSessionOwner() == HeistPlayerState)
	{
		ObjectCase->CancelSessionForOwner(HeistPlayerState, FName(Reason ? Reason : TEXT("ObservationCancelled")));
	}

	UHeistDebugFunctionLibrary::DebugObservationCastCancelled(this, CharacterName, DisplayCaseName, Reason);
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
	EscapeCastStartAlertRevision = INDEX_NONE;

	if (!bObservationCastActive)
	{
		SetComponentTickEnabled(false);
	}

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}

	ActionStateChangedDelegate.Broadcast();
}

void UHeistActionComponent::ClearObservationCastState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ObservationCastTimerHandle);
	}

	PendingObservationTarget.Reset();
	bObservationCastActive = false;
	ObservationCastEndServerTime = 0.0f;
	bObservationReferenceAvailable = false;

	if (!bEscapeCastActive)
	{
		SetComponentTickEnabled(false);
	}

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}

	ActionStateChangedDelegate.Broadcast();
}

#pragma endregion
