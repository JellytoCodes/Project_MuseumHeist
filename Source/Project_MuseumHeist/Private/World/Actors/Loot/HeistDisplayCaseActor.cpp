#include "World/Actors/Loot/HeistDisplayCaseActor.h"

#include "Character/HeistPlayerCharacter.h"
#include "Character/Components/HeistInventoryComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistLogChannels.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerState.h"
#include "Data/HeistArtifactDataTypes.h"
#include "Net/UnrealNetwork.h"

const FName AHeistDisplayCaseActor::OriginalVisualComponentTag(TEXT("OriginalVisual"));
const FName AHeistDisplayCaseActor::ReplicaVisualComponentTag(TEXT("ReplicaVisual"));

AHeistDisplayCaseActor::AHeistDisplayCaseActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	OriginalVisualComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OriginalVisualComponent"));
	OriginalVisualComponent->SetupAttachment(RootComponent);
	OriginalVisualComponent->ComponentTags.Add(OriginalVisualComponentTag);
	OriginalVisualComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OriginalVisualComponent->SetGenerateOverlapEvents(false);
	OriginalVisualComponent->SetCanEverAffectNavigation(false);

	ReplicaVisualComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ReplicaVisualComponent"));
	ReplicaVisualComponent->SetupAttachment(RootComponent);
	ReplicaVisualComponent->ComponentTags.Add(ReplicaVisualComponentTag);
	ReplicaVisualComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ReplicaVisualComponent->SetGenerateOverlapEvents(false);
	ReplicaVisualComponent->SetCanEverAffectNavigation(false);
	ReplicaVisualComponent->SetVisibility(false);
	ReplicaVisualComponent->SetHiddenInGame(true);
}

void AHeistDisplayCaseActor::BeginPlay()
{
	Super::BeginPlay();

	RefreshPlaceholderVisualState();

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
	if (HasAuthority() && bSessionLocked)
	{
		ClearSession(FName(TEXT("CaseEndPlay")));
	}

	UnbindSessionOwnerDelegate();
	UnbindOriginalCarrierDelegate();
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
		&& !bSessionLocked
		&& (DisplayCaseState == EHeistDisplayCaseState::Secured
			|| DisplayCaseState == EHeistDisplayCaseState::OriginalAvailable);
}

#pragma region StateMachine

EHeistDisplayCaseState AHeistDisplayCaseActor::GetDisplayCaseState() const
{
	return DisplayCaseState;
}

bool AHeistDisplayCaseActor::ShouldDisplayOriginalPlaceholder() const
{
	return ShouldDisplayOriginalPlaceholderForState(DisplayCaseState);
}

bool AHeistDisplayCaseActor::ShouldDisplayReplicaPlaceholder() const
{
	return ShouldDisplayReplicaPlaceholderForState(DisplayCaseState);
}

void AHeistDisplayCaseActor::GetPlaceholderVisualDebugState(
	bool& OutExpectedOriginalVisible,
	bool& OutExpectedReplicaVisible,
	int32& OutOriginalComponentCount,
	int32& OutReplicaComponentCount,
	bool& OutComponentsMatchExpectedState) const
{
	OutExpectedOriginalVisible = ShouldDisplayOriginalPlaceholder();
	OutExpectedReplicaVisible = ShouldDisplayReplicaPlaceholder();
	OutOriginalComponentCount = 0;
	OutReplicaComponentCount = 0;
	OutComponentsMatchExpectedState = true;

	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(this);
	for (const UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!IsValid(PrimitiveComponent))
		{
			continue;
		}

		if (PrimitiveComponent->ComponentHasTag(OriginalVisualComponentTag))
		{
			++OutOriginalComponentCount;
			OutComponentsMatchExpectedState &= PrimitiveComponent->IsVisible() == OutExpectedOriginalVisible;
		}

		if (PrimitiveComponent->ComponentHasTag(ReplicaVisualComponentTag))
		{
			++OutReplicaComponentCount;
			OutComponentsMatchExpectedState &= PrimitiveComponent->IsVisible() == OutExpectedReplicaVisible;
		}
	}

	OutComponentsMatchExpectedState &= OutOriginalComponentCount > 0 && OutReplicaComponentCount > 0;
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

bool AHeistDisplayCaseActor::ResetForgerySessionState(const FName Reason)
{
	if (!HasAuthority()
		|| (DisplayCaseState != EHeistDisplayCaseState::Observed
			&& DisplayCaseState != EHeistDisplayCaseState::ForgeryInProgress))
	{
		return false;
	}

	const EHeistDisplayCaseState PreviousState = DisplayCaseState;
	DisplayCaseState = EHeistDisplayCaseState::Secured;
	HandleDisplayCaseStateChanged(PreviousState);
	ForceNetUpdate();

	UE_LOG(
		LogHeistNetwork,
		Log,
		TEXT("Display case forgery state reset: Case=%s Previous=%s New=%s Reason=%s"),
		*GetNameSafe(this),
		*UEnum::GetValueAsString(PreviousState),
		*UEnum::GetValueAsString(DisplayCaseState),
		Reason.IsNone() ? TEXT("None") : *Reason.ToString());
	return true;
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

bool AHeistDisplayCaseActor::ShouldDisplayOriginalPlaceholderForState(const EHeistDisplayCaseState State)
{
	switch (State)
	{
	case EHeistDisplayCaseState::Secured:
	case EHeistDisplayCaseState::Observed:
	case EHeistDisplayCaseState::ForgeryInProgress:
	case EHeistDisplayCaseState::ReplicaReady:
	case EHeistDisplayCaseState::ReplicaPlaced:
	case EHeistDisplayCaseState::OriginalAvailable:
		return true;
	default:
		return false;
	}
}

bool AHeistDisplayCaseActor::ShouldDisplayReplicaPlaceholderForState(const EHeistDisplayCaseState State)
{
	switch (State)
	{
	case EHeistDisplayCaseState::ReplicaPlaced:
	case EHeistDisplayCaseState::OriginalAvailable:
	case EHeistDisplayCaseState::OriginalRemoved:
	case EHeistDisplayCaseState::Inspecting:
	case EHeistDisplayCaseState::Completed:
	case EHeistDisplayCaseState::Suspected:
	case EHeistDisplayCaseState::Alarmed:
	case EHeistDisplayCaseState::Failed:
		return true;
	default:
		return false;
	}
}

void AHeistDisplayCaseActor::HandleDisplayCaseStateChanged(const EHeistDisplayCaseState PreviousState)
{
	RefreshPlaceholderVisualState();

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

void AHeistDisplayCaseActor::RefreshPlaceholderVisualState()
{
	const bool bOriginalVisible = ShouldDisplayOriginalPlaceholder();
	const bool bReplicaVisible = ShouldDisplayReplicaPlaceholder();
	int32 OriginalComponentCount = 0;
	int32 ReplicaComponentCount = 0;

	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(this);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!IsValid(PrimitiveComponent))
		{
			continue;
		}

		if (PrimitiveComponent->ComponentHasTag(OriginalVisualComponentTag))
		{
			++OriginalComponentCount;
			PrimitiveComponent->SetVisibility(bOriginalVisible, true);
			PrimitiveComponent->SetHiddenInGame(!bOriginalVisible, true);
		}

		if (PrimitiveComponent->ComponentHasTag(ReplicaVisualComponentTag))
		{
			++ReplicaComponentCount;
			PrimitiveComponent->SetVisibility(bReplicaVisible, true);
			PrimitiveComponent->SetHiddenInGame(!bReplicaVisible, true);
		}
	}

	BP_ApplyPlaceholderVisualState(DisplayCaseState, bOriginalVisible, bReplicaVisible);

	UE_LOG(
		LogHeistNetwork,
		Log,
		TEXT("Display case placeholder visual applied: Case=%s State=%s OriginalVisible=%s ReplicaVisible=%s OriginalComponents=%d ReplicaComponents=%d Authority=%s Result=%s"),
		*GetNameSafe(this),
		*UEnum::GetValueAsString(DisplayCaseState),
		bOriginalVisible ? TEXT("true") : TEXT("false"),
		bReplicaVisible ? TEXT("true") : TEXT("false"),
		OriginalComponentCount,
		ReplicaComponentCount,
		HasAuthority() ? TEXT("true") : TEXT("false"),
		OriginalComponentCount > 0 && ReplicaComponentCount > 0 ? TEXT("PASS") : TEXT("MISSING_COMPONENTS"));
}

#pragma endregion

#pragma region OriginalCarry

FName AHeistDisplayCaseActor::GetTargetArtifactId() const
{
	return TargetArtifactId;
}

FName AHeistDisplayCaseActor::GetDisplayCaseId() const
{
	return DisplayCaseId;
}

AHeistPlayerState* AHeistDisplayCaseActor::GetOriginalCarrier() const
{
	return OriginalCarrier.Get();
}

int32 AHeistDisplayCaseActor::GetOriginalCarryRevision() const
{
	return OriginalCarryRevision;
}

bool AHeistDisplayCaseActor::TryTakeOriginal(AHeistPlayerState* RequestingPlayerState)
{
	if (!HasAuthority())
	{
		UE_LOG(
			LogHeistNetwork,
			Warning,
			TEXT("Original carry rejected: Case=%s Reason=NotAuthority"),
			*GetNameSafe(this));
		return false;
	}

	float ArtifactWeight = 0.0f;
	FName RejectReason = NAME_None;
	if (!ValidateOriginalTakeRequest(RequestingPlayerState, ArtifactWeight, RejectReason))
	{
		UE_LOG(
			LogHeistNetwork,
			Warning,
			TEXT("Original carry rejected: Case=%s Artifact=%s Requester=%s Reason=%s"),
			*GetNameSafe(this),
			*TargetArtifactId.ToString(),
			*GetNameSafe(RequestingPlayerState),
			*RejectReason.ToString());
		return false;
	}

	AHeistPlayerCharacter* PlayerCharacter =
		Cast<AHeistPlayerCharacter>(RequestingPlayerState->GetPawn());
	UHeistInventoryComponent* InventoryComponent = IsValid(PlayerCharacter)
		? PlayerCharacter->GetInventoryComponent()
		: nullptr;
	check(IsValid(InventoryComponent));

	if (!InventoryComponent->TryBeginOriginalCarry(
		RequestingPlayerState,
		TargetArtifactId,
		ArtifactWeight,
		this))
	{
		UE_LOG(
			LogHeistNetwork,
			Warning,
			TEXT("Original carry rejected: Case=%s Artifact=%s Requester=%s Reason=CarryEntryCommitFailed"),
			*GetNameSafe(this),
			*TargetArtifactId.ToString(),
			*GetNameSafe(RequestingPlayerState));
		return false;
	}

	OriginalCarrier = RequestingPlayerState;
	OriginalCarrierArrestChangedHandle =
		RequestingPlayerState->GetArrestStateChangedDelegate().AddUObject(
			this,
			&AHeistDisplayCaseActor::HandleOriginalCarrierArrestStateChanged);
	if (!TryTransitionToDisplayCaseState(EHeistDisplayCaseState::OriginalRemoved))
	{
		UnbindOriginalCarrierDelegate();
		OriginalCarrier = nullptr;
		FHeistOriginalCarryEntry RolledBackEntry;
		const bool bRolledBack = InventoryComponent->TryEndOriginalCarry(
			RequestingPlayerState,
			this,
			RolledBackEntry);
		checkf(bRolledBack, TEXT("Failed OriginalRemoved transition must roll back carry weight."));
		return false;
	}

	++OriginalCarryRevision;
	SyncObjectiveCarrierCandidate(RequestingPlayerState);
	ForceNetUpdate();
	BroadcastOriginalCarrySnapshot(TEXT("ServerTake"), FName(TEXT("TakeAccepted")));
	return true;
}

bool AHeistDisplayCaseActor::ReleaseOriginalForCarrier(
	AHeistPlayerState* ExpectedCarrier,
	const FName Reason)
{
	if (!HasAuthority()
		|| DisplayCaseState != EHeistDisplayCaseState::OriginalRemoved
		|| !IsValid(OriginalCarrier.Get())
		|| OriginalCarrier.Get() != ExpectedCarrier)
	{
		return false;
	}

	AHeistPlayerCharacter* PlayerCharacter =
		Cast<AHeistPlayerCharacter>(ExpectedCarrier->GetPawn());
	UHeistInventoryComponent* InventoryComponent = IsValid(PlayerCharacter)
		? PlayerCharacter->GetInventoryComponent()
		: nullptr;
	FHeistOriginalCarryEntry ReleasedEntry;
	const bool bCarryEntryReleased = IsValid(InventoryComponent)
		&& InventoryComponent->TryEndOriginalCarry(
			ExpectedCarrier,
			this,
			ReleasedEntry);
	const bool bAllowMissingInventoryCleanup =
		Reason == FName(TEXT("OwnerDisconnected"))
		|| Reason == FName(TEXT("OwnerArrested"));
	if (!bCarryEntryReleased
		&& (IsValid(InventoryComponent) || !bAllowMissingInventoryCleanup))
	{
		UE_LOG(
			LogHeistNetwork,
			Warning,
			TEXT("Original drop rejected: Case=%s Carrier=%s Reason=CarryEntryReleaseFailed"),
			*GetNameSafe(this),
			*GetNameSafe(ExpectedCarrier));
		return false;
	}

	const EHeistDisplayCaseState PreviousState = DisplayCaseState;
	DisplayCaseState = EHeistDisplayCaseState::OriginalAvailable;
	HandleDisplayCaseStateChanged(PreviousState);
	UnbindOriginalCarrierDelegate();
	OriginalCarrier = nullptr;
	++OriginalCarryRevision;
	SyncObjectiveCarrierCandidate(nullptr);
	ForceNetUpdate();
	BroadcastOriginalCarrySnapshot(TEXT("ServerRelease"), Reason);
	return true;
}

bool AHeistDisplayCaseActor::ValidateOriginalTakeRequest(
	AHeistPlayerState* RequestingPlayerState,
	float& OutArtifactWeight,
	FName& OutRejectReason) const
{
	OutArtifactWeight = 0.0f;
	OutRejectReason = NAME_None;
	if (!IsValid(RequestingPlayerState))
	{
		OutRejectReason = FName(TEXT("MissingPlayerState"));
		return false;
	}
	if (DisplayCaseState != EHeistDisplayCaseState::OriginalAvailable)
	{
		OutRejectReason = FName(TEXT("OriginalNotAvailable"));
		return false;
	}
	if (bSessionLocked)
	{
		OutRejectReason = FName(TEXT("CaseSessionLocked"));
		return false;
	}
	if (IsValid(OriginalCarrier.Get()))
	{
		OutRejectReason = FName(TEXT("OriginalAlreadyCarried"));
		return false;
	}

	const AHeistGameState* HeistGameState =
		GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistGameState)
		|| HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame)
	{
		OutRejectReason = FName(TEXT("MatchPhaseNotInGame"));
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
	if ((!HeistGameState->GetActiveTargetArtifactId().IsNone()
			&& HeistGameState->GetActiveTargetArtifactId() != TargetArtifactId)
		|| (!HeistGameState->GetActiveTargetCaseId().IsNone()
			&& HeistGameState->GetActiveTargetCaseId() != DisplayCaseId))
	{
		OutRejectReason = FName(TEXT("NotActiveTargetCase"));
		return false;
	}
	if (RequestingPlayerState->IsArrested() || RequestingPlayerState->IsEscaped())
	{
		OutRejectReason = FName(TEXT("PlayerStateBlocked"));
		return false;
	}

	AHeistPlayerCharacter* PlayerCharacter =
		Cast<AHeistPlayerCharacter>(RequestingPlayerState->GetPawn());
	UHeistInventoryComponent* InventoryComponent = IsValid(PlayerCharacter)
		? PlayerCharacter->GetInventoryComponent()
		: nullptr;
	if (!IsValid(PlayerCharacter) || !IsValid(InventoryComponent))
	{
		OutRejectReason = FName(TEXT("MissingCharacterOrInventory"));
		return false;
	}
	if (FVector::DistSquared(PlayerCharacter->GetActorLocation(), GetActorLocation())
		> FMath::Square(MaximumSessionDistance))
	{
		OutRejectReason = FName(TEXT("OutOfRange"));
		return false;
	}
	if (InventoryComponent->IsCarryingOriginal())
	{
		OutRejectReason = FName(TEXT("AlreadyCarryingOriginal"));
		return false;
	}

	const AHeistGameMode* HeistGameMode =
		GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	FHeistArtifactDataRow ArtifactDefinition;
	if (!IsValid(HeistGameMode)
		|| !HeistGameMode->TryGetArtifactDefinition(TargetArtifactId, ArtifactDefinition))
	{
		OutRejectReason = FName(TEXT("InvalidArtifactDefinition"));
		return false;
	}

	OutArtifactWeight = ArtifactDefinition.Weight;
	if (!RequestingPlayerState->CanAddLootScoreAndWeight(0, OutArtifactWeight))
	{
		OutRejectReason = FName(TEXT("InvalidCarryWeight"));
		return false;
	}
	return true;
}

void AHeistDisplayCaseActor::SyncObjectiveCarrierCandidate(AHeistPlayerState* Carrier)
{
	AHeistGameState* HeistGameState =
		GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistGameState))
	{
		return;
	}
	if ((!HeistGameState->GetActiveTargetArtifactId().IsNone()
			&& HeistGameState->GetActiveTargetArtifactId() != TargetArtifactId)
		|| (!HeistGameState->GetActiveTargetCaseId().IsNone()
			&& HeistGameState->GetActiveTargetCaseId() != DisplayCaseId))
	{
		return;
	}

	const FName ObjectiveArtifactId = HeistGameState->GetActiveTargetArtifactId().IsNone()
		? TargetArtifactId
		: HeistGameState->GetActiveTargetArtifactId();
	const FName ObjectiveCaseId = HeistGameState->GetActiveTargetCaseId().IsNone()
		? DisplayCaseId
		: HeistGameState->GetActiveTargetCaseId();
	const EHeistObjectiveState ObjectiveState =
		HeistGameState->GetObjectiveState() == EHeistObjectiveState::Inactive
			? EHeistObjectiveState::InProgress
			: HeistGameState->GetObjectiveState();
	HeistGameState->SetObjectiveSnapshot(
		ObjectiveArtifactId,
		ObjectiveCaseId,
		ObjectiveState,
		Carrier);
}

void AHeistDisplayCaseActor::UnbindOriginalCarrierDelegate()
{
	if (IsValid(OriginalCarrier.Get()) && OriginalCarrierArrestChangedHandle.IsValid())
	{
		OriginalCarrier->GetArrestStateChangedDelegate().Remove(
			OriginalCarrierArrestChangedHandle);
	}
	OriginalCarrierArrestChangedHandle.Reset();
}

void AHeistDisplayCaseActor::BroadcastOriginalCarrySnapshot(
	const TCHAR* ChangeSource,
	const FName Reason)
{
	OnOriginalCarryChanged.Broadcast(
		OriginalCarrier.Get(),
		TargetArtifactId,
		OriginalCarryRevision);
	UE_LOG(
		LogHeistNetwork,
		Log,
		TEXT("Original carry %s: Case=%s CaseId=%s Artifact=%s Carrier=%s CarrierPlayerId=%d State=%s Revision=%d Reason=%s Authority=%s"),
		ChangeSource,
		*GetNameSafe(this),
		*DisplayCaseId.ToString(),
		*TargetArtifactId.ToString(),
		*GetNameSafe(OriginalCarrier.Get()),
		IsValid(OriginalCarrier.Get()) ? OriginalCarrier->HeistPlayerId : INDEX_NONE,
		*UEnum::GetValueAsString(DisplayCaseState),
		OriginalCarryRevision,
		Reason.IsNone() ? TEXT("None") : *Reason.ToString(),
		HasAuthority() ? TEXT("true") : TEXT("false"));
}

void AHeistDisplayCaseActor::HandleOriginalCarrierArrestStateChanged(const bool bArrested)
{
	if (HasAuthority() && bArrested && IsValid(OriginalCarrier.Get()))
	{
		ReleaseOriginalForCarrier(
			OriginalCarrier.Get(),
			FName(TEXT("OwnerArrested")));
	}
}

void AHeistDisplayCaseActor::OnRep_OriginalCarryRevision()
{
	BroadcastOriginalCarrySnapshot(TEXT("Replicated"), NAME_None);
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
	ResetForgerySessionState(Reason);
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
	DOREPLIFETIME(AHeistDisplayCaseActor, OriginalCarrier);
	DOREPLIFETIME(AHeistDisplayCaseActor, OriginalCarryRevision);
	DOREPLIFETIME(AHeistDisplayCaseActor, SessionOwner);
	DOREPLIFETIME(AHeistDisplayCaseActor, bSessionLocked);
	DOREPLIFETIME(AHeistDisplayCaseActor, SessionRevision);
}

#pragma endregion
