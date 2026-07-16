#include "World/Actors/Loot/HeistDisplayCaseActor.h"

#include "Core/HeistLogChannels.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerState.h"
#include "Net/UnrealNetwork.h"

AHeistDisplayCaseActor::AHeistDisplayCaseActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void AHeistDisplayCaseActor::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		BoundGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
		if (BoundGameState.IsValid())
		{
			MatchPhaseChangedHandle = BoundGameState->GetMatchPhaseChangedDelegate().AddUObject(
				this,
				&AHeistDisplayCaseActor::HandleMatchPhaseChanged);
		}
	}
}

void AHeistDisplayCaseActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindSessionOwnerDelegate();
	if (BoundGameState.IsValid() && MatchPhaseChangedHandle.IsValid())
	{
		BoundGameState->GetMatchPhaseChangedDelegate().Remove(MatchPhaseChangedHandle);
	}
	MatchPhaseChangedHandle.Reset();
	BoundGameState.Reset();

	Super::EndPlay(EndPlayReason);
}

bool AHeistDisplayCaseActor::CanInteract(const AActor* Interactor) const
{
	return Super::CanInteract(Interactor)
		&& DisplayCaseState == EHeistDisplayCaseState::Secured
		&& !bSessionLocked;
}

#pragma region StateMachine

EHeistDisplayCaseState AHeistDisplayCaseActor::GetDisplayCaseState() const
{
	return DisplayCaseState;
}

bool AHeistDisplayCaseActor::CanTransitionToDisplayCaseState(const EHeistDisplayCaseState NewState) const
{
	EHeistDisplayCaseState ExpectedNextState = DisplayCaseState;
	return TryGetNextDisplayCaseState(DisplayCaseState, ExpectedNextState)
		&& ExpectedNextState == NewState;
}

bool AHeistDisplayCaseActor::TryTransitionToDisplayCaseState(const EHeistDisplayCaseState NewState)
{
	if (!HasAuthority())
	{
		UE_LOG(
			LogHeistNetwork,
			Warning,
			TEXT("Display case transition rejected: Case=%s Current=%s Requested=%s Reason=NotAuthority"),
			*GetNameSafe(this),
			*UEnum::GetValueAsString(DisplayCaseState),
			*UEnum::GetValueAsString(NewState));
		return false;
	}

	if (!CanTransitionToDisplayCaseState(NewState))
	{
		UE_LOG(
			LogHeistNetwork,
			Warning,
			TEXT("Display case transition rejected: Case=%s Current=%s Requested=%s Reason=IllegalTransition"),
			*GetNameSafe(this),
			*UEnum::GetValueAsString(DisplayCaseState),
			*UEnum::GetValueAsString(NewState));
		return false;
	}

	const EHeistDisplayCaseState PreviousState = DisplayCaseState;
	DisplayCaseState = NewState;
	HandleDisplayCaseStateChanged(PreviousState);
	ForceNetUpdate();
	return true;
}

bool AHeistDisplayCaseActor::TryAdvanceDisplayCaseState()
{
	EHeistDisplayCaseState NextState = DisplayCaseState;
	return TryGetNextDisplayCaseState(DisplayCaseState, NextState)
		&& TryTransitionToDisplayCaseState(NextState);
}

void AHeistDisplayCaseActor::OnRep_DisplayCaseState(const EHeistDisplayCaseState PreviousState)
{
	HandleDisplayCaseStateChanged(PreviousState);
}

bool AHeistDisplayCaseActor::TryGetNextDisplayCaseState(
	const EHeistDisplayCaseState CurrentState,
	EHeistDisplayCaseState& OutNextState)
{
	switch (CurrentState)
	{
	case EHeistDisplayCaseState::Secured:
		OutNextState = EHeistDisplayCaseState::Observed;
		return true;
	case EHeistDisplayCaseState::Observed:
		OutNextState = EHeistDisplayCaseState::ForgeryInProgress;
		return true;
	case EHeistDisplayCaseState::ForgeryInProgress:
		OutNextState = EHeistDisplayCaseState::ReplicaReady;
		return true;
	case EHeistDisplayCaseState::ReplicaReady:
		OutNextState = EHeistDisplayCaseState::ReplicaPlaced;
		return true;
	case EHeistDisplayCaseState::ReplicaPlaced:
		OutNextState = EHeistDisplayCaseState::OriginalAvailable;
		return true;
	case EHeistDisplayCaseState::OriginalAvailable:
		OutNextState = EHeistDisplayCaseState::OriginalRemoved;
		return true;
	default:
		return false;
	}
}

void AHeistDisplayCaseActor::HandleDisplayCaseStateChanged(const EHeistDisplayCaseState PreviousState)
{
	UE_LOG(
		LogHeistNetwork,
		Log,
		TEXT("Display case state changed: Case=%s Previous=%s New=%s Authority=%s"),
		*GetNameSafe(this),
		*UEnum::GetValueAsString(PreviousState),
		*UEnum::GetValueAsString(DisplayCaseState),
		HasAuthority() ? TEXT("true") : TEXT("false"));

	OnDisplayCaseStateChanged.Broadcast(PreviousState, DisplayCaseState);
}

#pragma endregion

#pragma region Session

AHeistPlayerState* AHeistDisplayCaseActor::GetSessionOwner() const
{
	return SessionOwner.Get();
}

bool AHeistDisplayCaseActor::IsSessionLocked() const
{
	return bSessionLocked;
}

int32 AHeistDisplayCaseActor::GetSessionRevision() const
{
	return SessionRevision;
}

float AHeistDisplayCaseActor::GetMaximumSessionDistance() const
{
	return MaximumSessionDistance;
}

bool AHeistDisplayCaseActor::TryBeginSession(AHeistPlayerState* RequestingPlayerState)
{
	if (!HasAuthority())
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Display case session begin rejected: Case=%s Reason=NotAuthority"), *GetNameSafe(this));
		return false;
	}

	if (bSessionLocked)
	{
		UE_LOG(
			LogHeistNetwork,
			Warning,
			TEXT("Display case session begin rejected: Case=%s Requester=%s Owner=%s Reason=AlreadyLocked"),
			*GetNameSafe(this),
			*GetNameSafe(RequestingPlayerState),
			*GetNameSafe(SessionOwner.Get()));
		return false;
	}

	FName RejectReason = NAME_None;
	if (!ValidateSessionRequest(RequestingPlayerState, RejectReason))
	{
		UE_LOG(
			LogHeistNetwork,
			Warning,
			TEXT("Display case session begin rejected: Case=%s Requester=%s Reason=%s"),
			*GetNameSafe(this),
			*GetNameSafe(RequestingPlayerState),
			*RejectReason.ToString());
		return false;
	}

	SessionOwner = RequestingPlayerState;
	bSessionLocked = true;
	++SessionRevision;
	SessionOwnerArrestChangedHandle = RequestingPlayerState->GetArrestStateChangedDelegate().AddUObject(
		this,
		&AHeistDisplayCaseActor::HandleSessionOwnerArrestStateChanged);
	ForceNetUpdate();
	BroadcastSessionSnapshot(TEXT("ServerBegin"), FName(TEXT("BeginAccepted")));
	return true;
}

bool AHeistDisplayCaseActor::TryCancelSession(AHeistPlayerState* RequestingPlayerState)
{
	if (!HasAuthority())
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Display case session cancel rejected: Case=%s Reason=NotAuthority"), *GetNameSafe(this));
		return false;
	}

	if (!bSessionLocked || !IsValid(SessionOwner.Get()))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Display case session cancel rejected: Case=%s Reason=NotLocked"), *GetNameSafe(this));
		return false;
	}

	if (SessionOwner.Get() != RequestingPlayerState)
	{
		UE_LOG(
			LogHeistNetwork,
			Warning,
			TEXT("Display case session cancel rejected: Case=%s Requester=%s Owner=%s Reason=NotSessionOwner"),
			*GetNameSafe(this),
			*GetNameSafe(RequestingPlayerState),
			*GetNameSafe(SessionOwner.Get()));
		return false;
	}

	ClearSession(FName(TEXT("OwnerCancelled")));
	return true;
}

bool AHeistDisplayCaseActor::CancelSessionForOwner(AHeistPlayerState* ExpectedOwner, const FName Reason)
{
	if (!HasAuthority() || !bSessionLocked || SessionOwner.Get() != ExpectedOwner)
	{
		return false;
	}

	ClearSession(Reason);
	return true;
}

bool AHeistDisplayCaseActor::ValidateSessionRequest(
	AHeistPlayerState* RequestingPlayerState,
	FName& OutRejectReason) const
{
	OutRejectReason = NAME_None;
	if (!IsValid(RequestingPlayerState))
	{
		OutRejectReason = FName(TEXT("MissingPlayerState"));
		return false;
	}

	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistGameState))
	{
		OutRejectReason = FName(TEXT("MissingGameState"));
		return false;
	}

	const bool bPlayerBelongsToMatch = HeistGameState->PlayerArray.ContainsByPredicate(
		[RequestingPlayerState](const TObjectPtr<APlayerState>& CandidatePlayerState)
		{
			return CandidatePlayerState.Get() == RequestingPlayerState;
		});
	if (!bPlayerBelongsToMatch)
	{
		OutRejectReason = FName(TEXT("PlayerStateNotInMatch"));
		return false;
	}

	if (HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame)
	{
		OutRejectReason = FName(TEXT("MatchPhaseNotInGame"));
		return false;
	}

	if (RequestingPlayerState->IsArrested())
	{
		OutRejectReason = FName(TEXT("PlayerArrested"));
		return false;
	}

	if (RequestingPlayerState->IsEscaped())
	{
		OutRejectReason = FName(TEXT("PlayerEscaped"));
		return false;
	}

	const APawn* RequestingPawn = RequestingPlayerState->GetPawn();
	if (!IsValid(RequestingPawn))
	{
		OutRejectReason = FName(TEXT("MissingPawn"));
		return false;
	}

	if (FVector::DistSquared(RequestingPawn->GetActorLocation(), GetActorLocation())
		> FMath::Square(MaximumSessionDistance))
	{
		OutRejectReason = FName(TEXT("OutOfRange"));
		return false;
	}

	return true;
}

void AHeistDisplayCaseActor::ClearSession(const FName Reason)
{
	AHeistPlayerState* PreviousOwner = SessionOwner.Get();
	UnbindSessionOwnerDelegate();
	SessionOwner = nullptr;
	bSessionLocked = false;
	++SessionRevision;
	ForceNetUpdate();

	UE_LOG(
		LogHeistNetwork,
		Log,
		TEXT("Display case session cleared: Case=%s PreviousOwner=%s Reason=%s Revision=%d"),
		*GetNameSafe(this),
		*GetNameSafe(PreviousOwner),
		*Reason.ToString(),
		SessionRevision);
	BroadcastSessionSnapshot(TEXT("ServerClear"), Reason);
}

void AHeistDisplayCaseActor::UnbindSessionOwnerDelegate()
{
	if (IsValid(SessionOwner.Get()) && SessionOwnerArrestChangedHandle.IsValid())
	{
		SessionOwner->GetArrestStateChangedDelegate().Remove(SessionOwnerArrestChangedHandle);
	}
	SessionOwnerArrestChangedHandle.Reset();
}

void AHeistDisplayCaseActor::OnRep_SessionRevision()
{
	BroadcastSessionSnapshot(TEXT("Replicated"), NAME_None);
}

void AHeistDisplayCaseActor::BroadcastSessionSnapshot(const TCHAR* ChangeSource, const FName Reason)
{
	OnDisplayCaseSessionChanged.Broadcast(SessionOwner.Get(), bSessionLocked, SessionRevision);
	UE_LOG(
		LogHeistNetwork,
		Log,
		TEXT("Display case session %s: Case=%s Owner=%s OwnerPlayerId=%d Locked=%s Revision=%d Reason=%s Authority=%s"),
		ChangeSource,
		*GetNameSafe(this),
		*GetNameSafe(SessionOwner.Get()),
		IsValid(SessionOwner.Get()) ? SessionOwner->HeistPlayerId : INDEX_NONE,
		bSessionLocked ? TEXT("true") : TEXT("false"),
		SessionRevision,
		Reason.IsNone() ? TEXT("None") : *Reason.ToString(),
		HasAuthority() ? TEXT("true") : TEXT("false"));
}

void AHeistDisplayCaseActor::HandleSessionOwnerArrestStateChanged(const bool bArrested)
{
	if (HasAuthority() && bSessionLocked && bArrested)
	{
		ClearSession(FName(TEXT("OwnerArrested")));
	}
}

void AHeistDisplayCaseActor::HandleMatchPhaseChanged(
	const EHeistMatchPhase PreviousMatchPhase,
	const EHeistMatchPhase NewMatchPhase)
{
	if (HasAuthority() && bSessionLocked && PreviousMatchPhase != NewMatchPhase)
	{
		ClearSession(FName(TEXT("MatchPhaseChanged")));
	}
}

#pragma endregion

#pragma region Replication

void AHeistDisplayCaseActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeistDisplayCaseActor, DisplayCaseState);
	DOREPLIFETIME(AHeistDisplayCaseActor, SessionOwner);
	DOREPLIFETIME(AHeistDisplayCaseActor, bSessionLocked);
	DOREPLIFETIME(AHeistDisplayCaseActor, SessionRevision);
}

#pragma endregion
