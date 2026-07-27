#include "World/Actors/Loot/HeistObjectDisplayCaseActor.h"

#include "Core/HeistGameState.h"
#include "Core/HeistPlayerState.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

AHeistObjectDisplayCaseActor::AHeistObjectDisplayCaseActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void AHeistObjectDisplayCaseActor::BeginPlay()
{
	Super::BeginPlay();

	BoundGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (HasAuthority() && BoundGameState.IsValid())
	{
		MatchPhaseChangedHandle = BoundGameState->GetMatchPhaseChangedDelegate().AddUObject(this, &AHeistObjectDisplayCaseActor::HandleMatchPhaseChanged);
	}
}

void AHeistObjectDisplayCaseActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && bSessionLocked)
	{
		ClearSession(FName(TEXT("CaseEndPlay")));
	}

	UnbindSessionOwnerDelegate();
	if (BoundGameState.IsValid() && MatchPhaseChangedHandle.IsValid())
	{
		BoundGameState->GetMatchPhaseChangedDelegate().Remove(MatchPhaseChangedHandle);
	}
	MatchPhaseChangedHandle.Reset();
	BoundGameState.Reset();

	Super::EndPlay(EndPlayReason);
}

void AHeistObjectDisplayCaseActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeistObjectDisplayCaseActor, AssemblyState);
	DOREPLIFETIME(AHeistObjectDisplayCaseActor, AssemblyRevision);
	DOREPLIFETIME(AHeistObjectDisplayCaseActor, AssemblyReplicaData);
	DOREPLIFETIME(AHeistObjectDisplayCaseActor, SessionOwner);
	DOREPLIFETIME(AHeistObjectDisplayCaseActor, bSessionLocked);
}

FName AHeistObjectDisplayCaseActor::GetObjectCaseId() const
{
	return ObjectCaseId;
}

FName AHeistObjectDisplayCaseActor::GetTargetArtifactId() const
{
	return TargetObjectArtifactId;
}

FName AHeistObjectDisplayCaseActor::GetObjectFamilyId() const
{
	return ObjectFamilyId;
}

EHeistObjectAssemblyState AHeistObjectDisplayCaseActor::GetAssemblyState() const
{
	return AssemblyState;
}

int32 AHeistObjectDisplayCaseActor::GetAssemblyRevision() const
{
	return AssemblyRevision;
}

FHeistObjectAssemblyReplicaData AHeistObjectDisplayCaseActor::GetAssemblyReplicaData() const
{
	return AssemblyReplicaData;
}

bool AHeistObjectDisplayCaseActor::InitializeObjectIdentity(const FName InObjectCaseId, const FName InTargetArtifactId, const FName InObjectFamilyId)
{
	if (!HasAuthority() || bSessionLocked || AssemblyState != EHeistObjectAssemblyState::Secured || InObjectCaseId.IsNone() || InTargetArtifactId.IsNone() || InObjectFamilyId.IsNone())
	{
		BroadcastAssemblySnapshot(FName(TEXT("IdentityInitialize")), FName(TEXT("InvalidIdentityOrState")), false);
		return false;
	}

	ObjectCaseId = InObjectCaseId;
	TargetObjectArtifactId = InTargetArtifactId;
	ObjectFamilyId = InObjectFamilyId;
	++AssemblyRevision;
	ForceNetUpdate();
	BroadcastAssemblySnapshot(FName(TEXT("IdentityInitialize")), FName(TEXT("Accepted")), true);
	return true;
}

bool AHeistObjectDisplayCaseActor::TryTransitionToAssemblyState(const EHeistObjectAssemblyState NewState)
{
	if (!HasAuthority() || !IsAssemblyStateTransitionAllowed(NewState))
	{
		BroadcastAssemblySnapshot(FName(TEXT("StateTransition")), FName(TEXT("TransitionRejected")), false);
		return false;
	}

	AssemblyState = NewState;
	++AssemblyRevision;
	ForceNetUpdate();
	BroadcastAssemblySnapshot(FName(TEXT("StateTransition")), FName(TEXT("Accepted")), true);
	return true;
}

AHeistPlayerState* AHeistObjectDisplayCaseActor::GetSessionOwner() const
{
	return SessionOwner.Get();
}

bool AHeistObjectDisplayCaseActor::IsSessionLocked() const
{
	return bSessionLocked;
}

float AHeistObjectDisplayCaseActor::GetMaximumSessionDistance() const
{
	return MaximumSessionDistance;
}

bool AHeistObjectDisplayCaseActor::TryBeginSession(AHeistPlayerState* RequestingPlayerState)
{
	FName RejectReason = NAME_None;
	if (!ValidateSessionRequest(RequestingPlayerState, RejectReason))
	{
		BroadcastAssemblySnapshot(FName(TEXT("SessionBegin")), RejectReason, false);
		return false;
	}

	SessionOwner = RequestingPlayerState;
	bSessionLocked = true;
	SessionOwnerArrestChangedHandle = RequestingPlayerState->GetArrestStateChangedDelegate().AddUObject(this, &AHeistObjectDisplayCaseActor::HandleSessionOwnerArrestStateChanged);
	++AssemblyRevision;
	ForceNetUpdate();
	BroadcastAssemblySnapshot(FName(TEXT("SessionBegin")), FName(TEXT("Accepted")), true);
	return true;
}

bool AHeistObjectDisplayCaseActor::CancelSessionForOwner(AHeistPlayerState* ExpectedOwner, const FName Reason)
{
	if (!HasAuthority() || !bSessionLocked || SessionOwner.Get() != ExpectedOwner)
	{
		return false;
	}

	ClearSession(Reason.IsNone() ? FName(TEXT("OwnerCancelled")) : Reason);
	return true;
}

bool AHeistObjectDisplayCaseActor::CanInteract(const AActor* /*Interactor*/) const
{
	// TASK-W5-013 binds the owner-only UI request. Until then, validated
	// session entry is exposed through C++ and development debug commands.
	return false;
}

void AHeistObjectDisplayCaseActor::SetObjectIdentityForLegacyMigration(const FName InObjectCaseId, const FName InTargetArtifactId, const FName InObjectFamilyId)
{
	if (!InObjectCaseId.IsNone())
	{
		ObjectCaseId = InObjectCaseId;
	}
	if (!InTargetArtifactId.IsNone())
	{
		TargetObjectArtifactId = InTargetArtifactId;
	}
	if (!InObjectFamilyId.IsNone())
	{
		ObjectFamilyId = InObjectFamilyId;
	}
}

void AHeistObjectDisplayCaseActor::OnRep_AssemblyState()
{
	BP_ObjectAssemblySnapshotChanged();
}

void AHeistObjectDisplayCaseActor::OnRep_AssemblyRevision()
{
	BP_ObjectAssemblySnapshotChanged();
}

void AHeistObjectDisplayCaseActor::OnRep_AssemblyReplicaData()
{
	BP_ObjectAssemblySnapshotChanged();
}

void AHeistObjectDisplayCaseActor::OnRep_SessionSnapshot()
{
	OnObjectAssemblySessionChanged.Broadcast(SessionOwner.Get(), bSessionLocked, AssemblyRevision);
	BP_ObjectAssemblySnapshotChanged();
}

bool AHeistObjectDisplayCaseActor::ValidateSessionRequest(AHeistPlayerState* RequestingPlayerState, FName& OutRejectReason) const
{
	OutRejectReason = NAME_None;
	if (!HasAuthority() || !IsValid(RequestingPlayerState))
	{
		OutRejectReason = FName(TEXT("InvalidAuthorityContext"));
		return false;
	}
	if (bSessionLocked || IsValid(SessionOwner.Get()))
	{
		OutRejectReason = FName(TEXT("CaseSessionLocked"));
		return false;
	}
	if (ObjectCaseId.IsNone() || TargetObjectArtifactId.IsNone() || ObjectFamilyId.IsNone())
	{
		OutRejectReason = FName(TEXT("MissingObjectIdentity"));
		return false;
	}
	if (AssemblyState != EHeistObjectAssemblyState::Secured && AssemblyState != EHeistObjectAssemblyState::Observed)
	{
		OutRejectReason = FName(TEXT("InvalidAssemblyState"));
		return false;
	}
	if (RequestingPlayerState->IsArrested() || RequestingPlayerState->IsEscaped())
	{
		OutRejectReason = FName(TEXT("PlayerStateBlocked"));
		return false;
	}

	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistGameState) || HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame)
	{
		OutRejectReason = FName(TEXT("MatchNotInGame"));
		return false;
	}

	const APawn* RequestingPawn = RequestingPlayerState->GetPawn();
	if (!IsValid(RequestingPawn) || FVector::DistSquared(RequestingPawn->GetActorLocation(), GetActorLocation()) > FMath::Square(MaximumSessionDistance))
	{
		OutRejectReason = FName(TEXT("OutOfRange"));
		return false;
	}

	return true;
}

bool AHeistObjectDisplayCaseActor::IsAssemblyStateTransitionAllowed(const EHeistObjectAssemblyState NewState) const
{
	if (NewState == AssemblyState)
	{
		return false;
	}

	switch (AssemblyState)
	{
	case EHeistObjectAssemblyState::Secured:
		return NewState == EHeistObjectAssemblyState::Observed || NewState == EHeistObjectAssemblyState::Failed;
	case EHeistObjectAssemblyState::Observed:
		return NewState == EHeistObjectAssemblyState::AssemblyInProgress || NewState == EHeistObjectAssemblyState::Failed;
	case EHeistObjectAssemblyState::AssemblyInProgress:
		return NewState == EHeistObjectAssemblyState::Observed || NewState == EHeistObjectAssemblyState::ReplicaReady || NewState == EHeistObjectAssemblyState::Failed;
	case EHeistObjectAssemblyState::ReplicaReady:
		return NewState == EHeistObjectAssemblyState::ReplicaPlaced || NewState == EHeistObjectAssemblyState::Failed;
	case EHeistObjectAssemblyState::ReplicaPlaced:
		return NewState == EHeistObjectAssemblyState::OriginalAvailable || NewState == EHeistObjectAssemblyState::Inspecting || NewState == EHeistObjectAssemblyState::Failed;
	case EHeistObjectAssemblyState::OriginalAvailable:
		return NewState == EHeistObjectAssemblyState::OriginalRemoved || NewState == EHeistObjectAssemblyState::Inspecting || NewState == EHeistObjectAssemblyState::Failed;
	case EHeistObjectAssemblyState::OriginalRemoved:
		return NewState == EHeistObjectAssemblyState::Inspecting || NewState == EHeistObjectAssemblyState::Completed || NewState == EHeistObjectAssemblyState::Failed;
	case EHeistObjectAssemblyState::Inspecting:
		return NewState == EHeistObjectAssemblyState::Completed || NewState == EHeistObjectAssemblyState::Suspected || NewState == EHeistObjectAssemblyState::Alarmed ||
			   NewState == EHeistObjectAssemblyState::Failed;
	case EHeistObjectAssemblyState::Suspected:
		return NewState == EHeistObjectAssemblyState::Alarmed || NewState == EHeistObjectAssemblyState::Completed || NewState == EHeistObjectAssemblyState::Failed;
	case EHeistObjectAssemblyState::Completed:
	case EHeistObjectAssemblyState::Alarmed:
	case EHeistObjectAssemblyState::Failed:
	default:
		return false;
	}
}

void AHeistObjectDisplayCaseActor::ClearSession(const FName Reason)
{
	AHeistPlayerState* PreviousOwner = SessionOwner.Get();
	UnbindSessionOwnerDelegate();
	SessionOwner = nullptr;
	bSessionLocked = false;
	if (AssemblyState == EHeistObjectAssemblyState::AssemblyInProgress)
	{
		AssemblyState = EHeistObjectAssemblyState::Observed;
	}
	++AssemblyRevision;
	ForceNetUpdate();
	OnObjectAssemblySessionChanged.Broadcast(nullptr, false, AssemblyRevision);
	UHeistDebugFunctionLibrary::DebugObjectAssemblyCaseSessionCleared(this, PreviousOwner, Reason);
	BP_ObjectAssemblySnapshotChanged();
}

void AHeistObjectDisplayCaseActor::UnbindSessionOwnerDelegate()
{
	if (IsValid(SessionOwner.Get()) && SessionOwnerArrestChangedHandle.IsValid())
	{
		SessionOwner->GetArrestStateChangedDelegate().Remove(SessionOwnerArrestChangedHandle);
	}
	SessionOwnerArrestChangedHandle.Reset();
}

void AHeistObjectDisplayCaseActor::BroadcastAssemblySnapshot(const FName EventName, const FName Reason, const bool bResult)
{
	OnObjectAssemblySessionChanged.Broadcast(SessionOwner.Get(), bSessionLocked, AssemblyRevision);
	UHeistDebugFunctionLibrary::DebugObjectAssemblyCaseSnapshot(this, EventName, Reason, bResult);
	BP_ObjectAssemblySnapshotChanged();
}

void AHeistObjectDisplayCaseActor::HandleSessionOwnerArrestStateChanged(const bool bArrested)
{
	if (HasAuthority() && bSessionLocked && bArrested)
	{
		ClearSession(FName(TEXT("OwnerArrested")));
	}
}

void AHeistObjectDisplayCaseActor::HandleMatchPhaseChanged(const EHeistMatchPhase PreviousMatchPhase, const EHeistMatchPhase NewMatchPhase)
{
	if (HasAuthority() && bSessionLocked && PreviousMatchPhase != NewMatchPhase && NewMatchPhase != EHeistMatchPhase::InGame)
	{
		ClearSession(FName(TEXT("MatchPhaseChanged")));
	}
}
