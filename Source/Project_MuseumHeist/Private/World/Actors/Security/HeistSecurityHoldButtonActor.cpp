#include "World/Actors/Security/HeistSecurityHoldButtonActor.h"

#include "Character/HeistPlayerCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "World/Actors/Security/HeistLaserBarrierActor.h"

AHeistSecurityHoldButtonActor::AHeistSecurityHoldButtonActor()
{
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(10.0f);
}

void AHeistSecurityHoldButtonActor::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(VisualMeshComponent))
	{
		ReleasedVisualScale = VisualMeshComponent->GetRelativeScale3D();
	}
	InteractionCollision->OnComponentEndOverlap.AddDynamic(this, &AHeistSecurityHoldButtonActor::HandleInteractionAreaEndOverlap);
	if (HasAuthority())
	{
		AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
		BoundGameState = HeistGameState;
		if (IsValid(HeistGameState))
		{
			HeistGameState->GetMatchPhaseChangedDelegate().AddUObject(this, &AHeistSecurityHoldButtonActor::HandleMatchPhaseChanged);
		}
	}
	ApplyPresentation();
}

void AHeistSecurityHoldButtonActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		ReleaseHoldInternal(FName(TEXT("ButtonEndPlay")), true);
	}
	if (BoundGameState.IsValid())
	{
		BoundGameState->GetMatchPhaseChangedDelegate().RemoveAll(this);
		BoundGameState.Reset();
	}
	if (IsValid(InteractionCollision))
	{
		InteractionCollision->OnComponentEndOverlap.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

bool AHeistSecurityHoldButtonActor::CanInteract(const AActor* Interactor) const
{
	const AHeistPlayerCharacter* PlayerCharacter = Cast<AHeistPlayerCharacter>(Interactor);
	const AHeistPlayerState* PlayerState = IsValid(PlayerCharacter) ? PlayerCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const bool bConnectedPlayerState = IsValid(HeistGameState) && HeistGameState->PlayerArray.ContainsByPredicate(
		[PlayerState](const TObjectPtr<APlayerState>& Candidate) { return Candidate.Get() == PlayerState; });
	return Super::CanInteract(Interactor) && IsValid(PlayerCharacter) && PlayerCharacter->CanPerformGameplayActions() && IsValid(PlayerState) && !PlayerState->IsArrested() &&
		!PlayerState->IsEscaped() && PlayerState->GetCrewStatus() != EHeistCrewStatus::Stunned && bConnectedPlayerState && IsValid(HeistGameState) &&
		HeistGameState->GetMatchPhase() == EHeistMatchPhase::InGame && !IsValid(HolderPlayerState) &&
		IsValid(LinkedLaserBarrier) && LinkedLaserBarrier->IsBarrierEnabled() && LinkedLaserBarrier->IsBeamActive();
}

void AHeistSecurityHoldButtonActor::Interact(AActor* Interactor)
{
	if (HasAuthority())
	{
		TryBeginHold(Cast<AHeistPlayerCharacter>(Interactor));
	}
}

bool AHeistSecurityHoldButtonActor::TryBeginHold(AHeistPlayerCharacter* RequestingCharacter)
{
	AHeistPlayerState* RequestingPlayerState = IsValid(RequestingCharacter) ? RequestingCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	if (!HasAuthority() || !CanInteract(RequestingCharacter) || !IsValid(RequestingPlayerState) || !InteractionCollision->IsOverlappingActor(RequestingCharacter))
	{
		return false;
	}

	HolderCharacter = RequestingCharacter;
	HolderPlayerState = RequestingPlayerState;
	bBypassActive = false;
	HoldStartServerTime = ResolveServerWorldTimeSeconds();
	const AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	const float HoldDurationSeconds = IsValid(HeistGameMode) ? HeistGameMode->GetSecurityLaserHoldDurationSeconds() : 3.0f;
	HoldEndServerTime = HoldStartServerTime + HoldDurationSeconds;
	++HoldRevision;
	RequestingPlayerState->GetCrewStatusChangedDelegate().RemoveAll(this);
	RequestingPlayerState->GetCrewStatusChangedDelegate().AddUObject(this, &AHeistSecurityHoldButtonActor::HandleHolderCrewStatusChanged);

	const float SafeDuration = FMath::Max(0.01f, HoldEndServerTime - HoldStartServerTime);
	GetWorldTimerManager().SetTimer(HoldCompletionTimerHandle, this, &AHeistSecurityHoldButtonActor::CompleteHold, SafeDuration, false);
	GetWorldTimerManager().SetTimer(HolderValidationTimerHandle, this, &AHeistSecurityHoldButtonActor::ValidateHolder, 0.10f, true, 0.10f);
	ForceNetUpdate();
	ApplyPresentation();
	return true;
}

bool AHeistSecurityHoldButtonActor::TryEndHold(AHeistPlayerState* RequestingPlayerState, const FName Reason)
{
	if (!HasAuthority() || !IsValid(RequestingPlayerState) || HolderPlayerState.Get() != RequestingPlayerState)
	{
		return false;
	}

	ReleaseHoldInternal(Reason.IsNone() ? FName(TEXT("InputReleased")) : Reason, false);
	return true;
}

bool AHeistSecurityHoldButtonActor::ForceReleaseForPlayer(AHeistPlayerState* PlayerState, const FName Reason)
{
	if (!HasAuthority() || !IsValid(PlayerState) || HolderPlayerState.Get() != PlayerState)
	{
		return false;
	}

	ReleaseHoldInternal(Reason.IsNone() ? FName(TEXT("HolderCleanup")) : Reason, false);
	return true;
}

void AHeistSecurityHoldButtonActor::ForceReleaseHold(const FName Reason)
{
	if (HasAuthority())
	{
		ReleaseHoldInternal(Reason.IsNone() ? FName(TEXT("ForcedCleanup")) : Reason, false);
	}
}

bool AHeistSecurityHoldButtonActor::IsHoldActive() const
{
	return IsValid(HolderPlayerState);
}

bool AHeistSecurityHoldButtonActor::IsBypassActive() const
{
	return bBypassActive;
}

float AHeistSecurityHoldButtonActor::GetHoldProgress() const
{
	if (!IsValid(HolderPlayerState))
	{
		return 0.0f;
	}
	if (bBypassActive)
	{
		return 1.0f;
	}

	const float Duration = HoldEndServerTime - HoldStartServerTime;
	return Duration > KINDA_SMALL_NUMBER ? FMath::Clamp((ResolveServerWorldTimeSeconds() - HoldStartServerTime) / Duration, 0.0f, 1.0f) : 0.0f;
}

AHeistPlayerState* AHeistSecurityHoldButtonActor::GetHolderPlayerState() const
{
	return HolderPlayerState.Get();
}

AHeistLaserBarrierActor* AHeistSecurityHoldButtonActor::GetLinkedLaserBarrier() const
{
	return LinkedLaserBarrier.Get();
}

bool AHeistSecurityHoldButtonActor::IsHolderContextValid() const
{
	const AHeistPlayerCharacter* Character = HolderCharacter.Get();
	const AHeistPlayerState* PlayerState = HolderPlayerState.Get();
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const bool bConnectedPlayerState = IsValid(HeistGameState) && HeistGameState->PlayerArray.ContainsByPredicate(
		[PlayerState](const TObjectPtr<APlayerState>& Candidate) { return Candidate.Get() == PlayerState; });
	return HasAuthority() && IsValid(Character) && IsValid(PlayerState) && Character->GetPlayerState<AHeistPlayerState>() == PlayerState && IsValid(Character->GetController()) &&
		Character->CanPerformGameplayActions() && PlayerState->GetCrewStatus() != EHeistCrewStatus::Stunned && bConnectedPlayerState &&
		InteractionCollision->IsOverlappingActor(Character) && IsValid(HeistGameState) &&
		HeistGameState->GetMatchPhase() == EHeistMatchPhase::InGame && IsValid(LinkedLaserBarrier) && LinkedLaserBarrier->IsBarrierEnabled();
}

float AHeistSecurityHoldButtonActor::ResolveServerWorldTimeSeconds() const
{
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	return IsValid(HeistGameState) ? HeistGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
}

void AHeistSecurityHoldButtonActor::CompleteHold()
{
	if (!IsHolderContextValid() || !LinkedLaserBarrier->TryActivateBypass(HolderPlayerState.Get()))
	{
		ReleaseHoldInternal(FName(TEXT("HoldCompletionRejected")), false);
		return;
	}

	bBypassActive = true;
	HoldEndServerTime = ResolveServerWorldTimeSeconds();
	++HoldRevision;
	GetWorldTimerManager().ClearTimer(HoldCompletionTimerHandle);
	ForceNetUpdate();
	ApplyPresentation();
}

void AHeistSecurityHoldButtonActor::ValidateHolder()
{
	if (!IsHolderContextValid())
	{
		ReleaseHoldInternal(FName(TEXT("HolderContextInvalid")), false);
	}
}

void AHeistSecurityHoldButtonActor::ReleaseHoldInternal(const FName, const bool bImmediateBarrierRestore)
{
	if (!HasAuthority())
	{
		return;
	}

	AHeistPlayerState* PreviousHolder = HolderPlayerState.Get();
	const bool bHadHold = IsValid(PreviousHolder) || HolderCharacter.IsValid() || bBypassActive || GetWorldTimerManager().TimerExists(HoldCompletionTimerHandle) ||
		GetWorldTimerManager().TimerExists(HolderValidationTimerHandle);
	GetWorldTimerManager().ClearTimer(HoldCompletionTimerHandle);
	GetWorldTimerManager().ClearTimer(HolderValidationTimerHandle);
	if (IsValid(PreviousHolder))
	{
		PreviousHolder->GetCrewStatusChangedDelegate().RemoveAll(this);
	}

	if (IsValid(LinkedLaserBarrier) && bHadHold)
	{
		if (bImmediateBarrierRestore)
		{
			LinkedLaserBarrier->ForceRestoreDefaultState();
		}
		else if (bBypassActive)
		{
			if (!IsValid(PreviousHolder) || !LinkedLaserBarrier->BeginRearm(PreviousHolder))
			{
				LinkedLaserBarrier->ForceRestoreDefaultState();
			}
		}
	}

	HolderCharacter.Reset();
	HolderPlayerState = nullptr;
	bBypassActive = false;
	HoldStartServerTime = 0.0f;
	HoldEndServerTime = 0.0f;
	if (bHadHold)
	{
		++HoldRevision;
		ForceNetUpdate();
		ApplyPresentation();
	}
}

void AHeistSecurityHoldButtonActor::ApplyPresentation()
{
	if (bAppliedBypassActive == bBypassActive && AppliedHoldRevision == HoldRevision &&
		AppliedHolderPlayerState.Get() == HolderPlayerState.Get())
	{
		return;
	}

	bAppliedBypassActive = bBypassActive;
	AppliedHoldRevision = HoldRevision;
	AppliedHolderPlayerState = HolderPlayerState;
	if (IsValid(VisualMeshComponent))
	{
		FVector ResolvedVisualScale = ReleasedVisualScale;
		if (IsHoldActive())
		{
			ResolvedVisualScale.Z *= FMath::Clamp(HeldVisualScaleZMultiplier, 0.01f, 1.0f);
		}
		VisualMeshComponent->SetRelativeScale3D(ResolvedVisualScale);
	}
	BP_ApplySecurityHoldButtonPresentation(IsHoldActive(), bBypassActive, GetHoldProgress(), HoldRevision, HolderPlayerState.Get());
}

void AHeistSecurityHoldButtonActor::HandleMatchPhaseChanged(const EHeistMatchPhase, const EHeistMatchPhase NewMatchPhase)
{
	if (HasAuthority() && NewMatchPhase != EHeistMatchPhase::InGame)
	{
		ReleaseHoldInternal(FName(TEXT("MatchPhaseChanged")), true);
	}
}

void AHeistSecurityHoldButtonActor::HandleHolderCrewStatusChanged(const EHeistCrewStatus NewStatus)
{
	if (HasAuthority() && (NewStatus == EHeistCrewStatus::Stunned || NewStatus == EHeistCrewStatus::Arrested || NewStatus == EHeistCrewStatus::Escaped))
	{
		ReleaseHoldInternal(FName(TEXT("HolderCrewStatusChanged")), false);
	}
}

void AHeistSecurityHoldButtonActor::HandleInteractionAreaEndOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32)
{
	if (HasAuthority() && OtherActor == HolderCharacter.Get())
	{
		ReleaseHoldInternal(FName(TEXT("HolderLeftInteractionArea")), false);
	}
}

void AHeistSecurityHoldButtonActor::OnRep_HoldState()
{
	ApplyPresentation();
}

void AHeistSecurityHoldButtonActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeistSecurityHoldButtonActor, HolderPlayerState);
	DOREPLIFETIME(AHeistSecurityHoldButtonActor, bBypassActive);
	DOREPLIFETIME(AHeistSecurityHoldButtonActor, HoldStartServerTime);
	DOREPLIFETIME(AHeistSecurityHoldButtonActor, HoldEndServerTime);
	DOREPLIFETIME(AHeistSecurityHoldButtonActor, HoldRevision);
}
