#include "World/Actors/Loot/HeistObjectDisplayCaseActor.h"
#include "World/Actors/Loot/HeistDroppedOriginalActor.h"

#include "Character/Components/HeistInventoryComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistGameState.h"
#include "Core/HeistLogChannels.h"
#include "Core/HeistPlayerState.h"
#include "Data/HeistArtifactDataTypes.h"
#include "Data/HeistGameBalanceDataAsset.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"

namespace
{
constexpr int32 MaximumReplicaEntryCount = 8;
constexpr float ObjectInspectionDelayExcellentMultiplier = 4.0f;
constexpr float ObjectInspectionDelayGoodMultiplier = 2.0f;
constexpr float ObjectInspectionDelayFairMultiplier = 1.0f;
constexpr float ObjectInspectionDelayPoorMultiplier = 0.5f;

bool ResolveReplicaDefinitions(const FName ArtifactId, const FName FamilyId, FHeistArtifactDataRow& OutArtifact, FHeistObjectAssemblyTemplateRow& OutTemplate,
							   TMap<FName, FHeistObjectAssemblyPartRow>& OutParts, FName& OutRejectReason)
{
	OutArtifact = FHeistArtifactDataRow();
	OutTemplate = FHeistObjectAssemblyTemplateRow();
	OutParts.Reset();
	OutRejectReason = NAME_None;

	const UHeistGameBalanceDataAsset* BalanceData = GetDefault<UHeistGameBalanceDataAsset>();
	UDataTable* ArtifactTable = IsValid(BalanceData) ? BalanceData->ArtifactDataTable.LoadSynchronous() : nullptr;
	UDataTable* TemplateTable = IsValid(BalanceData) ? BalanceData->ObjectAssemblyTemplateDataTable.LoadSynchronous() : nullptr;
	UDataTable* PartTable = IsValid(BalanceData) ? BalanceData->ObjectAssemblyPartDataTable.LoadSynchronous() : nullptr;
	if (!IsValid(ArtifactTable) || !IsValid(TemplateTable) || !IsValid(PartTable) || ArtifactTable->GetRowStruct() != FHeistArtifactDataRow::StaticStruct() ||
		TemplateTable->GetRowStruct() != FHeistObjectAssemblyTemplateRow::StaticStruct() || PartTable->GetRowStruct() != FHeistObjectAssemblyPartRow::StaticStruct())
	{
		OutRejectReason = FName(TEXT("MissingReplicaDataTables"));
		return false;
	}

	const FHeistArtifactDataRow* ArtifactRow = ArtifactTable->FindRow<FHeistArtifactDataRow>(ArtifactId, TEXT("ResolveObjectReplicaArtifact"), false);
	if (ArtifactRow == nullptr || ArtifactRow->ArtifactId != ArtifactId || ArtifactRow->ForgeryType != EHeistForgeryType::Assembly || ArtifactRow->ForgeryTemplateId.IsNone())
	{
		OutRejectReason = FName(TEXT("InvalidReplicaArtifact"));
		return false;
	}

	const FHeistObjectAssemblyTemplateRow* TemplateRow =
		TemplateTable->FindRow<FHeistObjectAssemblyTemplateRow>(ArtifactRow->ForgeryTemplateId, TEXT("ResolveObjectReplicaTemplate"), false);
	if (TemplateRow == nullptr || TemplateRow->TemplateId != ArtifactRow->ForgeryTemplateId || TemplateRow->FamilyId != FamilyId || TemplateRow->CorePartId.IsNone())
	{
		OutRejectReason = FName(TEXT("InvalidReplicaTemplate"));
		return false;
	}

	TArray<FName> ReferencedPartIds;
	ReferencedPartIds.Add(TemplateRow->CorePartId);
	for (const FHeistObjectAssemblyEntry& Entry : TemplateRow->RequiredParts)
	{
		ReferencedPartIds.AddUnique(Entry.PartId);
	}
	for (const FName PartId : TemplateRow->DecoyPartIds)
	{
		ReferencedPartIds.AddUnique(PartId);
	}

	for (const FName PartId : ReferencedPartIds)
	{
		const FHeistObjectAssemblyPartRow* PartRow = PartTable->FindRow<FHeistObjectAssemblyPartRow>(PartId, TEXT("ResolveObjectReplicaPart"), false);
		if (PartRow == nullptr || PartRow->PartId != PartId || PartRow->FamilyId != FamilyId || PartRow->StaticMesh.IsNull())
		{
			OutRejectReason = FName(TEXT("InvalidReplicaPart"));
			OutParts.Reset();
			return false;
		}
		OutParts.Add(PartId, *PartRow);
	}

	OutArtifact = *ArtifactRow;
	OutTemplate = *TemplateRow;
	return true;
}
} // namespace

AHeistObjectDisplayCaseActor::AHeistObjectDisplayCaseActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	ReplicaRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("ObjectReplicaRoot"));
	ReplicaRootComponent->SetupAttachment(RootComponent);
}

void AHeistObjectDisplayCaseActor::BeginPlay()
{
	Super::BeginPlay();

	BoundGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (HasAuthority() && BoundGameState.IsValid())
	{
		MatchPhaseChangedHandle = BoundGameState->GetMatchPhaseChangedDelegate().AddUObject(this, &AHeistObjectDisplayCaseActor::HandleMatchPhaseChanged);
	}

	if (AssemblyReplicaData.Revision > 0)
	{
		RebuildReplicaComponents();
	}
	RefreshObjectVisualState();
}

void AHeistObjectDisplayCaseActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		if (bSessionLocked)
		{
			ClearSession(FName(TEXT("CaseEndPlay")));
		}
		if (AHeistPlayerState* Carrier = OriginalCarrier.Get(); IsValid(Carrier))
		{
			ReleaseOriginalForCarrier(Carrier, FName(TEXT("CaseEndPlay")));
		}
		ClearInspectionDelayTimer();
		if (AssemblyState == EHeistObjectAssemblyState::Inspecting)
		{
			if (AActor* Guard = InspectingGuardActor.Get(); IsValid(Guard))
			{
				InterruptInspection(Guard, FName(TEXT("CaseEndPlay")));
			}
		}
		bRegisteredForInspection = false;
		InspectingGuardActor.Reset();
		ActiveInspectionScheduleRevision = INDEX_NONE;
	}

	UnbindSessionOwnerDelegate();
	UnbindOriginalCarrierDelegate();
	if (BoundGameState.IsValid() && MatchPhaseChangedHandle.IsValid())
	{
		BoundGameState->GetMatchPhaseChangedDelegate().Remove(MatchPhaseChangedHandle);
	}
	MatchPhaseChangedHandle.Reset();
	BoundGameState.Reset();
	DestroyReplicaComponents();

	Super::EndPlay(EndPlayReason);
}

void AHeistObjectDisplayCaseActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeistObjectDisplayCaseActor, ObjectCaseId);
	DOREPLIFETIME(AHeistObjectDisplayCaseActor, TargetObjectArtifactId);
	DOREPLIFETIME(AHeistObjectDisplayCaseActor, ObjectFamilyId);
	DOREPLIFETIME(AHeistObjectDisplayCaseActor, AssemblyState);
	DOREPLIFETIME(AHeistObjectDisplayCaseActor, AssemblyRevision);
	DOREPLIFETIME(AHeistObjectDisplayCaseActor, AssemblyReplicaData);
	DOREPLIFETIME(AHeistObjectDisplayCaseActor, SessionOwner);
	DOREPLIFETIME(AHeistObjectDisplayCaseActor, bSessionLocked);
	DOREPLIFETIME(AHeistObjectDisplayCaseActor, OriginalCarrier);
	DOREPLIFETIME(AHeistObjectDisplayCaseActor, OriginalCarryRevision);
	DOREPLIFETIME(AHeistObjectDisplayCaseActor, bOriginalSecuredAtExit);
	DOREPLIFETIME(AHeistObjectDisplayCaseActor, ResolvedInspectionDelay);
	DOREPLIFETIME(AHeistObjectDisplayCaseActor, InspectionReadyServerTime);
	DOREPLIFETIME(AHeistObjectDisplayCaseActor, InspectionScoreBand);
	DOREPLIFETIME(AHeistObjectDisplayCaseActor, ResolvedInspectionAlertOutcome);
	DOREPLIFETIME(AHeistObjectDisplayCaseActor, ResolvedInspectionCaseOutcome);
	DOREPLIFETIME(AHeistObjectDisplayCaseActor, InspectionScheduleRevision);
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

bool AHeistObjectDisplayCaseActor::HasCommittedAssemblyResult() const
{
	return bHasCommittedAssemblyResult;
}

FHeistObjectAssemblyResult AHeistObjectDisplayCaseActor::GetCommittedAssemblyResult() const
{
	return CommittedAssemblyResult;
}

bool AHeistObjectDisplayCaseActor::TryCommitAssemblyReplica(AHeistPlayerState* RequestingPlayerState, const FHeistObjectAssemblyResult& AssemblyResult,
															const TArray<FHeistObjectAssemblyEntry>& Entries)
{
	FName RejectReason = NAME_None;
	if (!HasAuthority() || !ValidateReplicaCommit(RequestingPlayerState, AssemblyResult, Entries, RejectReason))
	{
		UHeistDebugFunctionLibrary::DebugObjectAssemblyReplicaCommit(this, RequestingPlayerState, AssemblyResult, Entries.Num(),
																   RejectReason.IsNone() ? FName(TEXT("NotAuthority")) : RejectReason, false);
		return false;
	}
	if (!ResolveInspectionSchedule(AssemblyResult, RejectReason))
	{
		UHeistDebugFunctionLibrary::DebugObjectAssemblyReplicaCommit(this, RequestingPlayerState, AssemblyResult, Entries.Num(), RejectReason, false);
		return false;
	}

	CommittedAssemblyResult = AssemblyResult;
	CommittedAssemblyResult.bReplicaPlaced = true;
	bHasCommittedAssemblyResult = true;
	AssemblyReplicaData.Entries = Entries;
	++AssemblyReplicaData.Revision;

	const bool bTransitionsCommitted = TryTransitionToAssemblyState(EHeistObjectAssemblyState::ReplicaReady) &&
		TryTransitionToAssemblyState(EHeistObjectAssemblyState::ReplicaPlaced) && TryTransitionToAssemblyState(EHeistObjectAssemblyState::OriginalAvailable);
	if (!bTransitionsCommitted)
	{
		bHasCommittedAssemblyResult = false;
		CommittedAssemblyResult = FHeistObjectAssemblyResult();
		AssemblyReplicaData = FHeistObjectAssemblyReplicaData();
		UHeistDebugFunctionLibrary::DebugObjectAssemblyReplicaCommit(this, RequestingPlayerState, AssemblyResult, Entries.Num(), FName(TEXT("StateTransitionFailed")), false);
		return false;
	}

	StartInspectionDelayTimer();
	RefreshInspectionRegistration();
	UHeistDebugFunctionLibrary::DebugObjectAssemblyInspection(this, nullptr, FName(TEXT("Schedule")), FName(TEXT("Accepted")), true);
	RebuildReplicaComponents();
	ClearSession(FName(TEXT("AssemblyCompleted")));
	ForceNetUpdate();
	UHeistDebugFunctionLibrary::DebugObjectAssemblyReplicaCommit(this, RequestingPlayerState, CommittedAssemblyResult, Entries.Num(), FName(TEXT("Accepted")), true);
	return true;
}

void AHeistObjectDisplayCaseActor::GetReplicaComponentDebugState(int32& OutReplicaRevision, int32& OutExpectedEntryCount, int32& OutBuiltPartCount, int32& OutUnresolvedSocketCount,
																 bool& OutCoreReady, bool& OutContractPassed) const
{
	OutReplicaRevision = AssemblyReplicaData.Revision;
	OutExpectedEntryCount = AssemblyReplicaData.Entries.Num();
	OutBuiltPartCount = ReplicaPartComponents.Num();
	OutUnresolvedSocketCount = UnresolvedReplicaSocketCount;
	OutCoreReady = IsValid(ReplicaCoreComponent) && IsValid(ReplicaCoreComponent->GetStaticMesh());
	OutContractPassed = OutReplicaRevision > 0 && AppliedReplicaRevision == OutReplicaRevision && OutCoreReady && OutBuiltPartCount == OutExpectedEntryCount &&
		ReplicaPartComponents.ContainsByPredicate([](const TObjectPtr<UStaticMeshComponent>& Component) { return !IsValid(Component) || !IsValid(Component->GetStaticMesh()); }) == false;
}

bool AHeistObjectDisplayCaseActor::ForceReplicaRebuildForDebug()
{
#if !UE_BUILD_SHIPPING
	if (AssemblyReplicaData.Revision > 0)
	{
		RebuildReplicaComponents();
		int32 ReplicaRevision = 0;
		int32 ExpectedEntryCount = 0;
		int32 BuiltPartCount = 0;
		int32 UnresolvedSocketCount = 0;
		bool bCoreReady = false;
		bool bContractPassed = false;
		GetReplicaComponentDebugState(ReplicaRevision, ExpectedEntryCount, BuiltPartCount, UnresolvedSocketCount, bCoreReady, bContractPassed);
		return bContractPassed;
	}
#endif
	return false;
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
	RefreshObjectVisualState();
	RefreshInspectionRegistration();
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

AHeistPlayerState* AHeistObjectDisplayCaseActor::GetOriginalCarrier() const
{
	return OriginalCarrier.Get();
}

int32 AHeistObjectDisplayCaseActor::GetOriginalCarryRevision() const
{
	return OriginalCarryRevision;
}

bool AHeistObjectDisplayCaseActor::IsOriginalSecuredAtExit() const
{
	return bOriginalSecuredAtExit;
}

bool AHeistObjectDisplayCaseActor::TryTakeOriginal(AHeistPlayerState* RequestingPlayerState)
{
	int32 ArtifactValue = 0;
	float ArtifactWeight = 0.0f;
	bool bRequiredTarget = false;
	FName RejectReason = NAME_None;
	if (!HasAuthority() || !ValidateOriginalTakeRequest(RequestingPlayerState, ArtifactValue, ArtifactWeight, bRequiredTarget, RejectReason))
	{
		BroadcastOriginalCarrySnapshot(FName(TEXT("Take")), RejectReason.IsNone() ? FName(TEXT("NotAuthority")) : RejectReason, false);
		return false;
	}

	AHeistPlayerCharacter* PlayerCharacter = Cast<AHeistPlayerCharacter>(RequestingPlayerState->GetPawn());
	UHeistInventoryComponent* InventoryComponent = IsValid(PlayerCharacter) ? PlayerCharacter->GetInventoryComponent() : nullptr;
	check(IsValid(InventoryComponent));
	if (!InventoryComponent->TryBeginOriginalCarry(RequestingPlayerState, TargetObjectArtifactId, ArtifactValue, ArtifactWeight, bRequiredTarget, this))
	{
		BroadcastOriginalCarrySnapshot(FName(TEXT("Take")), FName(TEXT("CarryEntryCommitFailed")), false);
		return false;
	}

	OriginalCarrier = RequestingPlayerState;
	OriginalCarrierArrestChangedHandle =
		RequestingPlayerState->GetArrestStateChangedDelegate().AddUObject(this, &AHeistObjectDisplayCaseActor::HandleOriginalCarrierArrestStateChanged);
	if (!TryTransitionToAssemblyState(EHeistObjectAssemblyState::OriginalRemoved))
	{
		UnbindOriginalCarrierDelegate();
		OriginalCarrier = nullptr;
		FHeistOriginalCarryEntry RolledBackEntry;
		checkf(InventoryComponent->TryEndOriginalCarry(RequestingPlayerState, this, RolledBackEntry), TEXT("Object original carry rollback must succeed."));
		BroadcastOriginalCarrySnapshot(FName(TEXT("Take")), FName(TEXT("OriginalRemovedTransitionFailed")), false);
		return false;
	}

	++OriginalCarryRevision;
	SyncObjectiveCarrierCandidate(RequestingPlayerState);
	ForceNetUpdate();
	BroadcastOriginalCarrySnapshot(FName(TEXT("Take")), FName(TEXT("Accepted")), true);
	return true;
}

bool AHeistObjectDisplayCaseActor::ReleaseOriginalForCarrier(AHeistPlayerState* ExpectedCarrier, const FName Reason)
{
	const bool bOriginalCanReturn = AssemblyState == EHeistObjectAssemblyState::OriginalRemoved || AssemblyState == EHeistObjectAssemblyState::Inspecting ||
		AssemblyState == EHeistObjectAssemblyState::Completed || AssemblyState == EHeistObjectAssemblyState::Suspected || AssemblyState == EHeistObjectAssemblyState::Alarmed ||
		AssemblyState == EHeistObjectAssemblyState::Failed;
	if (!HasAuthority() || !bOriginalCanReturn || !IsValid(OriginalCarrier.Get()) || OriginalCarrier.Get() != ExpectedCarrier)
	{
		return false;
	}

	AHeistPlayerCharacter* PlayerCharacter = Cast<AHeistPlayerCharacter>(ExpectedCarrier->GetPawn());
	UHeistInventoryComponent* InventoryComponent = IsValid(PlayerCharacter) ? PlayerCharacter->GetInventoryComponent() : nullptr;
	FHeistOriginalCarryEntry ReleasedEntry;
	const bool bCarryEntryReleased = IsValid(InventoryComponent) && InventoryComponent->TryEndOriginalCarry(ExpectedCarrier, this, ReleasedEntry);
	const bool bAllowMissingInventoryCleanup =
		Reason == FName(TEXT("OwnerDisconnected")) || Reason == FName(TEXT("OwnerArrested")) || Reason == FName(TEXT("CaseEndPlay")) || Reason == FName(TEXT("OnlineSessionShutdown"));
	if (!bCarryEntryReleased && (IsValid(InventoryComponent) || !bAllowMissingInventoryCleanup))
	{
		BroadcastOriginalCarrySnapshot(FName(TEXT("Release")), FName(TEXT("CarryEntryReleaseFailed")), false);
		return false;
	}

	AssemblyState = EHeistObjectAssemblyState::OriginalAvailable;
	++AssemblyRevision;
	UnbindOriginalCarrierDelegate();
	OriginalCarrier = nullptr;
	++OriginalCarryRevision;
	SyncObjectiveCarrierCandidate(nullptr);
	RefreshObjectVisualState();
	RefreshInspectionRegistration();
	ForceNetUpdate();
	BroadcastOriginalCarrySnapshot(FName(TEXT("Release")), Reason, true);
	return true;
}

bool AHeistObjectDisplayCaseActor::DropOriginalForCarrier(AHeistPlayerState* ExpectedCarrier, const FName Reason)
{
	const bool bOriginalOutsideCase = AssemblyState == EHeistObjectAssemblyState::OriginalRemoved || AssemblyState == EHeistObjectAssemblyState::Inspecting ||
		AssemblyState == EHeistObjectAssemblyState::Completed || AssemblyState == EHeistObjectAssemblyState::Suspected || AssemblyState == EHeistObjectAssemblyState::Alarmed ||
		AssemblyState == EHeistObjectAssemblyState::Failed;
	if (!HasAuthority() || !bOriginalOutsideCase || !IsValid(ExpectedCarrier) || OriginalCarrier.Get() != ExpectedCarrier)
	{
		return false;
	}

	AHeistPlayerCharacter* PlayerCharacter = Cast<AHeistPlayerCharacter>(ExpectedCarrier->GetPawn());
	UHeistInventoryComponent* InventoryComponent = IsValid(PlayerCharacter) ? PlayerCharacter->GetInventoryComponent() : nullptr;
	FHeistOriginalCarryEntry DropEntry = IsValid(InventoryComponent) ? InventoryComponent->GetOriginalCarryEntry() : FHeistOriginalCarryEntry();
	const bool bHasCommittedCarryEntry = DropEntry.IsValid();
	if (!DropEntry.IsValid())
	{
		const AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
		FHeistArtifactDataRow ArtifactDefinition;
		const bool bAllowRecoveryFallback = Reason == FName(TEXT("OwnerDisconnected")) || Reason == FName(TEXT("OwnerArrested"));
		if (!bAllowRecoveryFallback || !IsValid(HeistGameMode) ||
			!HeistGameMode->TryGetArtifactDefinition(TargetObjectArtifactId, ArtifactDefinition))
		{
			BroadcastOriginalCarrySnapshot(FName(TEXT("WorldDrop")), FName(TEXT("MissingCarryEntry")), false);
			return false;
		}

		const AHeistGameState* HeistGameState = GetWorld()->GetGameState<AHeistGameState>();
		const FHeistContractSnapshot ContractSnapshot = IsValid(HeistGameState) ? HeistGameState->GetContractSnapshot() : FHeistContractSnapshot();
		DropEntry.ArtifactId = TargetObjectArtifactId;
		DropEntry.ArtifactValue = ArtifactDefinition.ArtifactValue;
		DropEntry.Weight = ArtifactDefinition.Weight;
		DropEntry.bRequiredTarget = ContractSnapshot.IsInitialized() && ContractSnapshot.RequiredTargetArtifactId == TargetObjectArtifactId &&
			ContractSnapshot.RequiredTargetCaseId == ObjectCaseId;
		DropEntry.SourceDisplayCase = this;
	}

	if (DropEntry.SourceDisplayCase != this || DropEntry.ArtifactId != TargetObjectArtifactId)
	{
		BroadcastOriginalCarrySnapshot(FName(TEXT("WorldDrop")), FName(TEXT("CarrySourceMismatch")), false);
		return false;
	}

	const FVector DropLocation = IsValid(PlayerCharacter)
		? PlayerCharacter->GetActorLocation() + PlayerCharacter->GetActorForwardVector() * 80.0f + FVector(0.0f, 0.0f, 30.0f)
		: GetActorLocation() + FVector(0.0f, 0.0f, 75.0f);
	const FTransform DropTransform(FRotator::ZeroRotator, DropLocation);
	UClass* SpawnClass = DroppedOriginalActorClass ? DroppedOriginalActorClass.Get() : AHeistDroppedOriginalActor::StaticClass();
	AHeistDroppedOriginalActor* DroppedOriginal =
		GetWorld()->SpawnActorDeferred<AHeistDroppedOriginalActor>(SpawnClass, DropTransform, nullptr, PlayerCharacter, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(DroppedOriginal))
	{
		BroadcastOriginalCarrySnapshot(FName(TEXT("WorldDrop")), FName(TEXT("SpawnFailed")), false);
		return false;
	}

	DroppedOriginal->InitializeDroppedOriginal(DropEntry.ArtifactId, DropEntry.ArtifactValue, DropEntry.Weight, DropEntry.bRequiredTarget, this);
	DroppedOriginal->FinishSpawning(DropTransform);

	if (bHasCommittedCarryEntry)
	{
		check(IsValid(InventoryComponent));
		FHeistOriginalCarryEntry ReleasedEntry;
		if (!InventoryComponent->TryEndOriginalCarry(ExpectedCarrier, this, ReleasedEntry))
		{
			DroppedOriginal->Destroy();
			BroadcastOriginalCarrySnapshot(FName(TEXT("WorldDrop")), FName(TEXT("CarryEntryReleaseFailed")), false);
			return false;
		}
	}

	UnbindOriginalCarrierDelegate();
	OriginalCarrier = nullptr;
	++OriginalCarryRevision;
	SyncObjectiveCarrierCandidate(nullptr);
	ForceNetUpdate();
	BroadcastOriginalCarrySnapshot(FName(TEXT("WorldDrop")), Reason, true);
	UE_LOG(LogHeistNetwork, Log,
		   TEXT("Object original world drop committed: Case=%s CaseId=%s Actor=%s Artifact=%s Value=%d Weight=%.1f Required=%s Reason=%s Revision=%d Authority=true Result=PASS"),
		   *GetNameSafe(this), *ObjectCaseId.ToString(), *GetNameSafe(DroppedOriginal), *DropEntry.ArtifactId.ToString(), DropEntry.ArtifactValue, DropEntry.Weight,
		   DropEntry.bRequiredTarget ? TEXT("true") : TEXT("false"), *Reason.ToString(), OriginalCarryRevision);
	return true;
}

bool AHeistObjectDisplayCaseActor::TryClaimDroppedOriginal(AHeistPlayerState* RequestingPlayerState, AHeistDroppedOriginalActor* DroppedOriginal)
{
	const bool bOriginalOutsideCase = AssemblyState == EHeistObjectAssemblyState::OriginalRemoved || AssemblyState == EHeistObjectAssemblyState::Inspecting ||
		AssemblyState == EHeistObjectAssemblyState::Completed || AssemblyState == EHeistObjectAssemblyState::Suspected || AssemblyState == EHeistObjectAssemblyState::Alarmed ||
		AssemblyState == EHeistObjectAssemblyState::Failed;
	if (!HasAuthority() || !bOriginalOutsideCase || bOriginalSecuredAtExit || IsValid(OriginalCarrier.Get()) || !IsValid(RequestingPlayerState) || RequestingPlayerState->IsArrested() ||
		RequestingPlayerState->IsEscaped() || !IsValid(DroppedOriginal) || DroppedOriginal->GetSourceDisplayCase() != this ||
		DroppedOriginal->GetArtifactId() != TargetObjectArtifactId)
	{
		return false;
	}

	AHeistPlayerCharacter* PlayerCharacter = Cast<AHeistPlayerCharacter>(RequestingPlayerState->GetPawn());
	UHeistInventoryComponent* InventoryComponent = IsValid(PlayerCharacter) ? PlayerCharacter->GetInventoryComponent() : nullptr;
	if (!IsValid(InventoryComponent) || InventoryComponent->IsCarryingOriginal() ||
		!InventoryComponent->TryBeginOriginalCarry(RequestingPlayerState, DroppedOriginal->GetArtifactId(), DroppedOriginal->GetArtifactValue(), DroppedOriginal->GetWeight(),
														 DroppedOriginal->IsRequiredTarget(), this))
	{
		return false;
	}

	OriginalCarrier = RequestingPlayerState;
	OriginalCarrierArrestChangedHandle =
		RequestingPlayerState->GetArrestStateChangedDelegate().AddUObject(this, &AHeistObjectDisplayCaseActor::HandleOriginalCarrierArrestStateChanged);
	++OriginalCarryRevision;
	SyncObjectiveCarrierCandidate(RequestingPlayerState);
	ForceNetUpdate();
	BroadcastOriginalCarrySnapshot(FName(TEXT("WorldPickup")), FName(TEXT("Accepted")), true);
	return true;
}

bool AHeistObjectDisplayCaseActor::CanCommitOriginalDepositForCarrier(const AHeistPlayerState* ExpectedCarrier, const FHeistOriginalCarryEntry& CarryEntry) const
{
	const bool bOriginalOutsideCase = AssemblyState == EHeistObjectAssemblyState::OriginalRemoved || AssemblyState == EHeistObjectAssemblyState::Inspecting ||
		AssemblyState == EHeistObjectAssemblyState::Completed || AssemblyState == EHeistObjectAssemblyState::Suspected || AssemblyState == EHeistObjectAssemblyState::Alarmed ||
		AssemblyState == EHeistObjectAssemblyState::Failed;
	return HasAuthority() && bOriginalOutsideCase && !bOriginalSecuredAtExit && IsValid(ExpectedCarrier) && OriginalCarrier.Get() == ExpectedCarrier && CarryEntry.IsValid() &&
		   CarryEntry.SourceDisplayCase == this && CarryEntry.ArtifactId == TargetObjectArtifactId;
}

bool AHeistObjectDisplayCaseActor::CommitOriginalDepositForCarrier(AHeistPlayerState* ExpectedCarrier, const FHeistOriginalCarryEntry& CarryEntry)
{
	if (!CanCommitOriginalDepositForCarrier(ExpectedCarrier, CarryEntry))
	{
		BroadcastOriginalCarrySnapshot(FName(TEXT("Deposit")), FName(TEXT("InvalidDepositState")), false);
		return false;
	}

	UnbindOriginalCarrierDelegate();
	OriginalCarrier = nullptr;
	bOriginalSecuredAtExit = true;
	++OriginalCarryRevision;
	SyncObjectiveCarrierCandidate(nullptr);
	ForceNetUpdate();
	BroadcastOriginalCarrySnapshot(FName(TEXT("Deposit")), FName(TEXT("DepositedAtSharedExit")), true);
	return true;
}

bool AHeistObjectDisplayCaseActor::IsRegisteredForInspection() const
{
	return bRegisteredForInspection;
}

bool AHeistObjectDisplayCaseActor::IsValidInspectionCandidate() const
{
	const bool bEligibleState = AssemblyState == EHeistObjectAssemblyState::ReplicaPlaced || AssemblyState == EHeistObjectAssemblyState::OriginalAvailable ||
		AssemblyState == EHeistObjectAssemblyState::OriginalRemoved;
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const bool bMatchInGame = IsValid(HeistGameState) && HeistGameState->GetMatchPhase() == EHeistMatchPhase::InGame;
	return bMatchInGame && bRegisteredForInspection && bHasCommittedAssemblyResult && CommittedAssemblyResult.bReplicaPlaced && bEligibleState && HasInspectionDelayElapsed() &&
		!InspectingGuardActor.IsValid() && LastAppliedInspectionScheduleRevision != InspectionScheduleRevision;
}

float AHeistObjectDisplayCaseActor::GetInspectionDelayRemaining() const
{
	if (InspectionReadyServerTime <= 0.0f)
	{
		return 0.0f;
	}
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const float ServerTime = IsValid(HeistGameState) ? HeistGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	return FMath::Max(0.0f, InspectionReadyServerTime - ServerTime);
}

float AHeistObjectDisplayCaseActor::GetResolvedInspectionDelay() const
{
	return ResolvedInspectionDelay;
}

FName AHeistObjectDisplayCaseActor::GetInspectionScoreBand() const
{
	return InspectionScoreBand;
}

EHeistAlertLevel AHeistObjectDisplayCaseActor::GetResolvedInspectionAlertOutcome() const
{
	return ResolvedInspectionAlertOutcome;
}

EHeistObjectAssemblyState AHeistObjectDisplayCaseActor::GetResolvedInspectionCaseOutcome() const
{
	return ResolvedInspectionCaseOutcome;
}

int32 AHeistObjectDisplayCaseActor::GetInspectionScheduleRevision() const
{
	return InspectionScheduleRevision;
}

int32 AHeistObjectDisplayCaseActor::GetInspectionRegistrationRevision() const
{
	return InspectionRegistrationRevision;
}

int32 AHeistObjectDisplayCaseActor::GetInspectionResultApplicationCount() const
{
	return InspectionResultApplicationCount;
}

int32 AHeistObjectDisplayCaseActor::GetInspectionDuplicateBlockCount() const
{
	return InspectionDuplicateBlockCount;
}

bool AHeistObjectDisplayCaseActor::TryBeginInspection(AActor* InspectingGuard)
{
	if (!HasAuthority() || !IsValid(InspectingGuard) || InspectingGuardActor.IsValid() || !IsValidInspectionCandidate())
	{
		UHeistDebugFunctionLibrary::DebugObjectAssemblyInspection(this, InspectingGuard, FName(TEXT("Begin")), FName(TEXT("InvalidOrDuplicateCandidate")), false);
		return false;
	}

	PreInspectionState = AssemblyState;
	InspectingGuardActor = InspectingGuard;
	ActiveInspectionScheduleRevision = InspectionScheduleRevision;
	AssemblyState = EHeistObjectAssemblyState::Inspecting;
	++AssemblyRevision;
	RefreshObjectVisualState();
	RefreshInspectionRegistration();
	ForceNetUpdate();
	UHeistDebugFunctionLibrary::DebugObjectAssemblyInspection(this, InspectingGuard, FName(TEXT("Begin")), FName(TEXT("Accepted")), true);
	return true;
}

bool AHeistObjectDisplayCaseActor::InterruptInspection(AActor* InspectingGuard, const FName Reason)
{
	if (!HasAuthority() || AssemblyState != EHeistObjectAssemblyState::Inspecting || !IsInspectionOwnedBy(InspectingGuard))
	{
		return false;
	}

	AssemblyState = PreInspectionState;
	InspectingGuardActor.Reset();
	ActiveInspectionScheduleRevision = INDEX_NONE;
	++AssemblyRevision;
	RefreshObjectVisualState();
	RefreshInspectionRegistration();
	ForceNetUpdate();
	UHeistDebugFunctionLibrary::DebugObjectAssemblyInspection(this, InspectingGuard, FName(TEXT("Interrupt")), Reason, true);
	return true;
}

bool AHeistObjectDisplayCaseActor::ApplyInspectionResult(AActor* InspectingGuard)
{
	if (HasAuthority() && IsValid(InspectingGuard) && LastAppliedInspectionScheduleRevision == InspectionScheduleRevision)
	{
		++InspectionDuplicateBlockCount;
		UHeistDebugFunctionLibrary::DebugObjectAssemblyInspection(this, InspectingGuard, FName(TEXT("Apply")), FName(TEXT("DuplicateResult")), false);
		return false;
	}

	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!HasAuthority() || !IsValid(HeistGameState) || HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame || AssemblyState != EHeistObjectAssemblyState::Inspecting ||
		!IsInspectionOwnedBy(InspectingGuard) || ActiveInspectionScheduleRevision != InspectionScheduleRevision)
	{
		return false;
	}

	AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	const FName AlertTriggerId(*FString::Printf(TEXT("ObjectInspection_%s_%d"), *ObjectCaseId.ToString(), InspectionScheduleRevision));
	if (!IsValid(HeistGameMode) || !HeistGameMode->RequestAlertEscalation(ResolvedInspectionAlertOutcome, AlertTriggerId))
	{
		UHeistDebugFunctionLibrary::DebugObjectAssemblyInspection(this, InspectingGuard, FName(TEXT("Apply")), FName(TEXT("AlertRequestRejected")), false);
		return false;
	}

	LastAppliedInspectionScheduleRevision = ActiveInspectionScheduleRevision;
	ActiveInspectionScheduleRevision = INDEX_NONE;
	++InspectionResultApplicationCount;
	AssemblyState = ResolvedInspectionCaseOutcome;
	InspectingGuardActor.Reset();
	++AssemblyRevision;
	RefreshObjectVisualState();
	RefreshInspectionRegistration();
	ForceNetUpdate();
	UHeistDebugFunctionLibrary::DebugObjectAssemblyInspection(this, InspectingGuard, FName(TEXT("Apply")), FName(TEXT("Accepted")), true);
	return true;
}

bool AHeistObjectDisplayCaseActor::IsInspectionOwnedBy(const AActor* InspectingGuard) const
{
	return IsValid(InspectingGuard) && InspectingGuardActor.Get() == InspectingGuard;
}

bool AHeistObjectDisplayCaseActor::IsInspectionClaimActive() const
{
	return AssemblyState == EHeistObjectAssemblyState::Inspecting && InspectingGuardActor.IsValid() && ActiveInspectionScheduleRevision == InspectionScheduleRevision;
}

bool AHeistObjectDisplayCaseActor::IsInspectionDelayTimerActive() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) && World->GetTimerManager().TimerExists(InspectionDelayTimerHandle);
}

AActor* AHeistObjectDisplayCaseActor::GetInspectingGuard() const
{
	return InspectingGuardActor.Get();
}

bool AHeistObjectDisplayCaseActor::ForceInspectionReadyForDebug()
{
#if !UE_BUILD_SHIPPING
	if (HasAuthority() && bHasCommittedAssemblyResult && InspectionScheduleRevision > 0)
	{
		ClearInspectionDelayTimer();
		InspectionReadyServerTime = 0.0f;
		RefreshInspectionRegistration();
		ForceNetUpdate();
		return IsValidInspectionCandidate();
	}
#endif
	return false;
}

bool AHeistObjectDisplayCaseActor::CalculateInspectionSchedule(const float QualityScore, const float BaseInspectionDelay, float& OutDelay, FName& OutScoreBand,
															   EHeistAlertLevel& OutAlertOutcome, EHeistObjectAssemblyState& OutCaseOutcome)
{
	OutDelay = 0.0f;
	OutScoreBand = NAME_None;
	OutAlertOutcome = EHeistAlertLevel::Quiet;
	OutCaseOutcome = EHeistObjectAssemblyState::Suspected;
	if (!FMath::IsFinite(QualityScore) || !FMath::IsFinite(BaseInspectionDelay) || !FMath::IsWithinInclusive(QualityScore, 0.0f, 100.0f) || BaseInspectionDelay < 0.0f)
	{
		return false;
	}

	float DelayMultiplier = 0.0f;
	if (QualityScore >= 90.0f)
	{
		OutScoreBand = FName(TEXT("90-100"));
		OutAlertOutcome = EHeistAlertLevel::Quiet;
		OutCaseOutcome = EHeistObjectAssemblyState::Completed;
		DelayMultiplier = ObjectInspectionDelayExcellentMultiplier;
	}
	else if (QualityScore >= 70.0f)
	{
		OutScoreBand = FName(TEXT("70-89"));
		OutAlertOutcome = EHeistAlertLevel::Suspicious;
		OutCaseOutcome = EHeistObjectAssemblyState::Suspected;
		DelayMultiplier = ObjectInspectionDelayGoodMultiplier;
	}
	else if (QualityScore >= 50.0f)
	{
		OutScoreBand = FName(TEXT("50-69"));
		OutAlertOutcome = EHeistAlertLevel::Searching;
		OutCaseOutcome = EHeistObjectAssemblyState::Suspected;
		DelayMultiplier = ObjectInspectionDelayFairMultiplier;
	}
	else if (QualityScore >= 30.0f)
	{
		OutScoreBand = FName(TEXT("30-49"));
		OutAlertOutcome = EHeistAlertLevel::Alarmed;
		OutCaseOutcome = EHeistObjectAssemblyState::Alarmed;
		DelayMultiplier = ObjectInspectionDelayPoorMultiplier;
	}
	else
	{
		OutScoreBand = FName(TEXT("0-29"));
		OutAlertOutcome = EHeistAlertLevel::Alarmed;
		OutCaseOutcome = EHeistObjectAssemblyState::Alarmed;
	}

	OutDelay = BaseInspectionDelay * DelayMultiplier;
	return FMath::IsFinite(OutDelay) && OutDelay >= 0.0f;
}

bool AHeistObjectDisplayCaseActor::CanInteract(const AActor* Interactor) const
{
	const APawn* RequestingPawn = Cast<APawn>(Interactor);
	const AHeistPlayerState* RequestingPlayerState = IsValid(RequestingPawn) ? RequestingPawn->GetPlayerState<AHeistPlayerState>() : nullptr;
	const bool bStateAllowsAssembly = AssemblyState == EHeistObjectAssemblyState::Secured || AssemblyState == EHeistObjectAssemblyState::Observed;
	const bool bStateAllowsOriginalTake = AssemblyState == EHeistObjectAssemblyState::OriginalAvailable;
	const bool bIdentityReady = !ObjectCaseId.IsNone() && !TargetObjectArtifactId.IsNone() && !ObjectFamilyId.IsNone();
	const bool bPlayerReady = IsValid(RequestingPlayerState) && !RequestingPlayerState->IsArrested() && !RequestingPlayerState->IsEscaped();
	const bool bWithinSessionDistance =
		IsValid(RequestingPawn) && FVector::DistSquared(RequestingPawn->GetActorLocation(), GetActorLocation()) <= FMath::Square(MaximumSessionDistance);
	return Super::CanInteract(Interactor) && bIdentityReady && (bStateAllowsAssembly || bStateAllowsOriginalTake) && !bSessionLocked && !IsValid(SessionOwner.Get()) && bPlayerReady &&
		bWithinSessionDistance;
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
	RefreshObjectVisualState();
	BP_ObjectAssemblySnapshotChanged();
}

void AHeistObjectDisplayCaseActor::OnRep_ObjectIdentity()
{
	if (AssemblyReplicaData.Revision > 0)
	{
		RebuildReplicaComponents();
	}
	BP_ObjectAssemblySnapshotChanged();
}

void AHeistObjectDisplayCaseActor::OnRep_AssemblyRevision()
{
	RefreshObjectVisualState();
	BP_ObjectAssemblySnapshotChanged();
}

void AHeistObjectDisplayCaseActor::OnRep_AssemblyReplicaData()
{
	RebuildReplicaComponents();
	BP_ObjectAssemblySnapshotChanged();
}

void AHeistObjectDisplayCaseActor::OnRep_SessionSnapshot()
{
	OnObjectAssemblySessionChanged.Broadcast(SessionOwner.Get(), bSessionLocked, AssemblyRevision);
	BP_ObjectAssemblySnapshotChanged();
}

void AHeistObjectDisplayCaseActor::OnRep_OriginalCarryRevision()
{
	OnObjectOriginalCarryChanged.Broadcast(OriginalCarrier.Get(), TargetObjectArtifactId, OriginalCarryRevision);
	RefreshObjectVisualState();
	BP_ObjectAssemblySnapshotChanged();
}

void AHeistObjectDisplayCaseActor::OnRep_InspectionScheduleRevision()
{
	UHeistDebugFunctionLibrary::DebugObjectAssemblyInspection(this, nullptr, FName(TEXT("ScheduleReplicated")), FName(TEXT("Accepted")), InspectionScheduleRevision > 0);
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

bool AHeistObjectDisplayCaseActor::ValidateReplicaCommit(AHeistPlayerState* RequestingPlayerState, const FHeistObjectAssemblyResult& AssemblyResult,
														  const TArray<FHeistObjectAssemblyEntry>& Entries, FName& OutRejectReason) const
{
	OutRejectReason = NAME_None;
	if (!IsValid(RequestingPlayerState) || bHasCommittedAssemblyResult || AssemblyReplicaData.Revision > 0)
	{
		OutRejectReason = bHasCommittedAssemblyResult ? FName(TEXT("DuplicateReplicaPlacement")) : FName(TEXT("MissingPlayerState"));
		return false;
	}
	if (AssemblyState != EHeistObjectAssemblyState::AssemblyInProgress || !bSessionLocked || SessionOwner.Get() != RequestingPlayerState)
	{
		OutRejectReason = FName(TEXT("CaseOwnershipOrStateMismatch"));
		return false;
	}
	if (!FMath::IsWithinInclusive(Entries.Num(), 1, MaximumReplicaEntryCount) || AssemblyResult.ArtifactId != TargetObjectArtifactId || AssemblyResult.TemplateId.IsNone() ||
		!FMath::IsFinite(AssemblyResult.QualityScore) || !FMath::IsWithinInclusive(AssemblyResult.QualityScore, 0.0f, 100.0f) ||
		!FMath::IsFinite(AssemblyResult.RequiredPartScore) || !FMath::IsWithinInclusive(AssemblyResult.RequiredPartScore, 0.0f, 100.0f) ||
		!FMath::IsFinite(AssemblyResult.SocketTopologyScore) || !FMath::IsWithinInclusive(AssemblyResult.SocketTopologyScore, 0.0f, 100.0f) ||
		!FMath::IsFinite(AssemblyResult.OrientationScore) || !FMath::IsWithinInclusive(AssemblyResult.OrientationScore, 0.0f, 100.0f) ||
		!FMath::IsFinite(AssemblyResult.MaterialScore) || !FMath::IsWithinInclusive(AssemblyResult.MaterialScore, 0.0f, 100.0f) || !FMath::IsFinite(AssemblyResult.Completeness) ||
		!FMath::IsWithinInclusive(AssemblyResult.Completeness, 0.0f, 1.0f) || !FMath::IsFinite(AssemblyResult.CompletionTime) || AssemblyResult.CompletionTime < 0.0f ||
		AssemblyResult.bReplicaPlaced)
	{
		OutRejectReason = FName(TEXT("InvalidAssemblyResult"));
		return false;
	}

	FHeistArtifactDataRow ArtifactDefinition;
	FHeistObjectAssemblyTemplateRow TemplateDefinition;
	TMap<FName, FHeistObjectAssemblyPartRow> PartDefinitions;
	if (!ResolveReplicaDefinitions(TargetObjectArtifactId, ObjectFamilyId, ArtifactDefinition, TemplateDefinition, PartDefinitions, OutRejectReason) ||
		AssemblyResult.TemplateId != TemplateDefinition.TemplateId)
	{
		OutRejectReason = OutRejectReason.IsNone() ? FName(TEXT("TemplateIdentityMismatch")) : OutRejectReason;
		return false;
	}

	TSet<FName> SubmittedPartIds;
	TSet<FName> SubmittedSocketIds;
	for (const FHeistObjectAssemblyEntry& Entry : Entries)
	{
		const FHeistObjectAssemblyPartRow* PartDefinition = PartDefinitions.Find(Entry.PartId);
		const bool bMaterialValid =
			PartDefinition != nullptr && (PartDefinition->AllowedMaterialIds.IsEmpty() ? Entry.MaterialId.IsNone() : PartDefinition->AllowedMaterialIds.Contains(Entry.MaterialId));
		if (Entry.PartId.IsNone() || Entry.SocketId.IsNone() || SubmittedPartIds.Contains(Entry.PartId) || SubmittedSocketIds.Contains(Entry.SocketId) || PartDefinition == nullptr ||
			Entry.PartId == TemplateDefinition.CorePartId || !PartDefinition->CompatibleSocketIds.Contains(Entry.SocketId) ||
			!PartDefinition->AllowedOrientationSteps.Contains(Entry.QuantizedOrientation) || !bMaterialValid)
		{
			OutRejectReason = FName(TEXT("InvalidReplicaEntry"));
			return false;
		}
		SubmittedPartIds.Add(Entry.PartId);
		SubmittedSocketIds.Add(Entry.SocketId);
	}
	return true;
}

void AHeistObjectDisplayCaseActor::RefreshObjectVisualState()
{
	const bool bOriginalVisible = ShouldDisplayOriginalVisual();
	const bool bReplicaVisible = ShouldDisplayReplicaVisual();
	if (IsValid(VisualMeshComponent))
	{
		VisualMeshComponent->SetVisibility(bOriginalVisible, true);
		VisualMeshComponent->SetHiddenInGame(!bOriginalVisible, true);
	}
	if (IsValid(ReplicaRootComponent))
	{
		ReplicaRootComponent->SetVisibility(bReplicaVisible, true);
		SetActorHiddenInGame(false);
	}
}

void AHeistObjectDisplayCaseActor::RebuildReplicaComponents()
{
	DestroyReplicaComponents();
	if (AssemblyReplicaData.Revision <= 0 || AssemblyReplicaData.Entries.IsEmpty())
	{
		RefreshObjectVisualState();
		return;
	}

	FHeistArtifactDataRow ArtifactDefinition;
	FHeistObjectAssemblyTemplateRow TemplateDefinition;
	TMap<FName, FHeistObjectAssemblyPartRow> PartDefinitions;
	FName RejectReason = NAME_None;
	bool bCoreReady = false;
	if (!ResolveReplicaDefinitions(TargetObjectArtifactId, ObjectFamilyId, ArtifactDefinition, TemplateDefinition, PartDefinitions, RejectReason))
	{
		UHeistDebugFunctionLibrary::DebugObjectAssemblyReplicaRebuildEvent(this, AssemblyReplicaData.Entries.Num(), 0, 0, false, AssemblyReplicaData.Revision, false);
		return;
	}

	const FHeistObjectAssemblyPartRow* CoreDefinition = PartDefinitions.Find(TemplateDefinition.CorePartId);
	UStaticMesh* CoreMesh = CoreDefinition != nullptr ? CoreDefinition->StaticMesh.LoadSynchronous() : nullptr;
	if (!IsValid(CoreMesh) || !IsValid(ReplicaRootComponent))
	{
		UHeistDebugFunctionLibrary::DebugObjectAssemblyReplicaRebuildEvent(this, AssemblyReplicaData.Entries.Num(), 0, 0, false, AssemblyReplicaData.Revision, false);
		return;
	}

	ReplicaCoreComponent = NewObject<UStaticMeshComponent>(
		this, MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), FName(TEXT("ObjectReplicaCore"))));
	ReplicaCoreComponent->SetupAttachment(ReplicaRootComponent);
	ReplicaCoreComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ReplicaCoreComponent->SetGenerateOverlapEvents(false);
	ReplicaCoreComponent->SetStaticMesh(CoreMesh);
	ReplicaCoreComponent->ComponentTags.Add(FName(*FString::Printf(TEXT("PartId=%s"), *TemplateDefinition.CorePartId.ToString())));
	AddInstanceComponent(ReplicaCoreComponent);
	ReplicaCoreComponent->RegisterComponent();
	bCoreReady = true;

	for (int32 EntryIndex = 0; EntryIndex < AssemblyReplicaData.Entries.Num(); ++EntryIndex)
	{
		const FHeistObjectAssemblyEntry& Entry = AssemblyReplicaData.Entries[EntryIndex];
		const FHeistObjectAssemblyPartRow* PartDefinition = PartDefinitions.Find(Entry.PartId);
		UStaticMesh* PartMesh = PartDefinition != nullptr ? PartDefinition->StaticMesh.LoadSynchronous() : nullptr;
		if (!IsValid(PartMesh))
		{
			continue;
		}

		UStaticMeshComponent* PartComponent =
			NewObject<UStaticMeshComponent>(this, MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), FName(TEXT("ObjectReplicaPart"))));
		PartComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PartComponent->SetGenerateOverlapEvents(false);
		PartComponent->SetStaticMesh(PartMesh);
		PartComponent->ComponentTags.Add(FName(*FString::Printf(TEXT("PartId=%s"), *Entry.PartId.ToString())));
		PartComponent->ComponentTags.Add(FName(*FString::Printf(TEXT("SocketId=%s"), *Entry.SocketId.ToString())));
		PartComponent->ComponentTags.Add(FName(*FString::Printf(TEXT("MaterialId=%s"), *Entry.MaterialId.ToString())));
		if (ReplicaCoreComponent->DoesSocketExist(Entry.SocketId))
		{
			PartComponent->SetupAttachment(ReplicaCoreComponent, Entry.SocketId);
			PartComponent->SetRelativeRotation(FRotator(0.0f, static_cast<float>(Entry.QuantizedOrientation) * 22.5f, 0.0f));
		}
		else
		{
			++UnresolvedReplicaSocketCount;
			PartComponent->SetupAttachment(ReplicaRootComponent);
			PartComponent->SetRelativeTransform(ResolveFallbackPartTransform(Entry.SocketId, EntryIndex, Entry.QuantizedOrientation));
		}
		AddInstanceComponent(PartComponent);
		PartComponent->RegisterComponent();
		BP_ApplyObjectReplicaPartMaterial(PartComponent, Entry.PartId, Entry.MaterialId);
		ReplicaPartComponents.Add(PartComponent);
	}

	AppliedReplicaRevision = AssemblyReplicaData.Revision;
	RefreshObjectVisualState();
	int32 ReplicaRevision = 0;
	int32 ExpectedEntryCount = 0;
	int32 BuiltPartCount = 0;
	int32 UnresolvedSocketCount = 0;
	bool bDebugCoreReady = false;
	bool bContractPassed = false;
	GetReplicaComponentDebugState(ReplicaRevision, ExpectedEntryCount, BuiltPartCount, UnresolvedSocketCount, bDebugCoreReady, bContractPassed);
	UHeistDebugFunctionLibrary::DebugObjectAssemblyReplicaRebuildEvent(this, ExpectedEntryCount, BuiltPartCount, UnresolvedSocketCount, bCoreReady, ReplicaRevision, bContractPassed);
}

void AHeistObjectDisplayCaseActor::DestroyReplicaComponents()
{
	for (UStaticMeshComponent* PartComponent : ReplicaPartComponents)
	{
		if (IsValid(PartComponent))
		{
			PartComponent->DestroyComponent();
		}
	}
	ReplicaPartComponents.Reset();
	if (IsValid(ReplicaCoreComponent))
	{
		ReplicaCoreComponent->DestroyComponent();
	}
	ReplicaCoreComponent = nullptr;
	AppliedReplicaRevision = 0;
	UnresolvedReplicaSocketCount = 0;
}

FTransform AHeistObjectDisplayCaseActor::ResolveFallbackPartTransform(const FName SocketId, const int32 PlacementIndex, const uint8 QuantizedOrientation) const
{
	const FString SocketName = SocketId.ToString();
	FVector Location;
	FVector Scale(0.55f);
	if (SocketName.Contains(TEXT("Head"), ESearchCase::IgnoreCase))
	{
		Location = FVector(0.0f, 0.0f, 105.0f);
		Scale = FVector(0.65f);
	}
	else if (SocketName.Contains(TEXT("Shoulder_L"), ESearchCase::IgnoreCase))
	{
		Location = FVector(0.0f, -75.0f, 40.0f);
		Scale = FVector(0.32f, 0.32f, 0.85f);
	}
	else if (SocketName.Contains(TEXT("Shoulder_R"), ESearchCase::IgnoreCase))
	{
		Location = FVector(0.0f, 75.0f, 40.0f);
		Scale = FVector(0.32f, 0.32f, 0.85f);
	}
	else if (SocketName.Contains(TEXT("Pedestal"), ESearchCase::IgnoreCase))
	{
		Location = FVector(0.0f, 0.0f, -80.0f);
		Scale = FVector(0.65f);
	}
	else if (SocketName.Contains(TEXT("Handle"), ESearchCase::IgnoreCase))
	{
		Location = FVector(0.0f, 75.0f, 25.0f);
		Scale = FVector(0.45f);
	}
	else
	{
		const float AngleRadians = FMath::DegreesToRadians(static_cast<float>(PlacementIndex) * 72.0f);
		Location = FVector(0.0f, FMath::Cos(AngleRadians) * 85.0f, 25.0f + FMath::Sin(AngleRadians) * 60.0f);
	}
	return FTransform(FRotator(0.0f, static_cast<float>(QuantizedOrientation) * 22.5f, 0.0f), Location, Scale);
}

bool AHeistObjectDisplayCaseActor::ShouldDisplayOriginalVisual() const
{
	return AssemblyState == EHeistObjectAssemblyState::Secured || AssemblyState == EHeistObjectAssemblyState::Observed ||
		AssemblyState == EHeistObjectAssemblyState::AssemblyInProgress || AssemblyState == EHeistObjectAssemblyState::ReplicaReady;
}

bool AHeistObjectDisplayCaseActor::ShouldDisplayReplicaVisual() const
{
	return AssemblyReplicaData.Revision > 0 && AssemblyState != EHeistObjectAssemblyState::Secured && AssemblyState != EHeistObjectAssemblyState::Observed &&
		AssemblyState != EHeistObjectAssemblyState::AssemblyInProgress && AssemblyState != EHeistObjectAssemblyState::ReplicaReady;
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
	RefreshObjectVisualState();
	RefreshInspectionRegistration();
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

bool AHeistObjectDisplayCaseActor::ValidateOriginalTakeRequest(AHeistPlayerState* RequestingPlayerState, int32& OutArtifactValue, float& OutArtifactWeight, bool& bOutRequiredTarget,
														  FName& OutRejectReason) const
{
	OutArtifactValue = 0;
	OutArtifactWeight = 0.0f;
	bOutRequiredTarget = false;
	OutRejectReason = NAME_None;
	if (!IsValid(RequestingPlayerState) || AssemblyState != EHeistObjectAssemblyState::OriginalAvailable || bSessionLocked || IsValid(OriginalCarrier.Get()))
	{
		OutRejectReason = FName(TEXT("InvalidOriginalState"));
		return false;
	}

	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistGameState) || HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame || RequestingPlayerState->IsArrested() || RequestingPlayerState->IsEscaped())
	{
		OutRejectReason = FName(TEXT("PlayerOrMatchBlocked"));
		return false;
	}
	const bool bPlayerBelongsToMatch =
		HeistGameState->PlayerArray.ContainsByPredicate([RequestingPlayerState](const TObjectPtr<APlayerState>& Candidate) { return Candidate.Get() == RequestingPlayerState; });
	if (!bPlayerBelongsToMatch)
	{
		OutRejectReason = FName(TEXT("PlayerStateNotInMatch"));
		return false;
	}
	if ((!HeistGameState->GetActiveTargetArtifactId().IsNone() && HeistGameState->GetActiveTargetArtifactId() != TargetObjectArtifactId) ||
		(!HeistGameState->GetActiveTargetCaseId().IsNone() && HeistGameState->GetActiveTargetCaseId() != ObjectCaseId))
	{
		OutRejectReason = FName(TEXT("NotActiveTargetCase"));
		return false;
	}

	AHeistPlayerCharacter* PlayerCharacter = Cast<AHeistPlayerCharacter>(RequestingPlayerState->GetPawn());
	UHeistInventoryComponent* InventoryComponent = IsValid(PlayerCharacter) ? PlayerCharacter->GetInventoryComponent() : nullptr;
	if (!IsValid(PlayerCharacter) || !IsValid(InventoryComponent) ||
		FVector::DistSquared(PlayerCharacter->GetActorLocation(), GetActorLocation()) > FMath::Square(MaximumSessionDistance) || InventoryComponent->IsCarryingOriginal())
	{
		OutRejectReason = FName(TEXT("CharacterInventoryOrRangeBlocked"));
		return false;
	}

	const AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	FHeistArtifactDataRow ArtifactDefinition;
	if (!IsValid(HeistGameMode) || !HeistGameMode->TryGetArtifactDefinition(TargetObjectArtifactId, ArtifactDefinition))
	{
		OutRejectReason = FName(TEXT("InvalidArtifactDefinition"));
		return false;
	}
	OutArtifactValue = ArtifactDefinition.ArtifactValue;
	OutArtifactWeight = ArtifactDefinition.Weight;
	const FHeistContractSnapshot& ContractSnapshot = HeistGameState->GetContractSnapshot();
	bOutRequiredTarget = ContractSnapshot.IsInitialized()
		? ContractSnapshot.RequiredTargetArtifactId == TargetObjectArtifactId && ContractSnapshot.RequiredTargetCaseId == ObjectCaseId
		: HeistGameState->GetActiveTargetArtifactId() == TargetObjectArtifactId && HeistGameState->GetActiveTargetCaseId() == ObjectCaseId;
	if (!RequestingPlayerState->CanAddLootScoreAndWeight(0, OutArtifactWeight))
	{
		OutRejectReason = FName(TEXT("InvalidCarryWeight"));
		return false;
	}
	return true;
}

void AHeistObjectDisplayCaseActor::SyncObjectiveCarrierCandidate(AHeistPlayerState* Carrier)
{
	AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistGameState) ||
		(!HeistGameState->GetActiveTargetArtifactId().IsNone() && HeistGameState->GetActiveTargetArtifactId() != TargetObjectArtifactId) ||
		(!HeistGameState->GetActiveTargetCaseId().IsNone() && HeistGameState->GetActiveTargetCaseId() != ObjectCaseId))
	{
		return;
	}
	const EHeistObjectiveState ObjectiveState =
		HeistGameState->GetObjectiveState() == EHeistObjectiveState::Inactive ? EHeistObjectiveState::InProgress : HeistGameState->GetObjectiveState();
	HeistGameState->SetObjectiveSnapshot(TargetObjectArtifactId, ObjectCaseId, ObjectiveState, Carrier);
}

void AHeistObjectDisplayCaseActor::UnbindOriginalCarrierDelegate()
{
	if (IsValid(OriginalCarrier.Get()) && OriginalCarrierArrestChangedHandle.IsValid())
	{
		OriginalCarrier->GetArrestStateChangedDelegate().Remove(OriginalCarrierArrestChangedHandle);
	}
	OriginalCarrierArrestChangedHandle.Reset();
}

void AHeistObjectDisplayCaseActor::BroadcastOriginalCarrySnapshot(const FName EventName, const FName Reason, const bool bResult)
{
	OnObjectOriginalCarryChanged.Broadcast(OriginalCarrier.Get(), TargetObjectArtifactId, OriginalCarryRevision);
	UHeistDebugFunctionLibrary::DebugObjectAssemblyOriginalCarry(this, OriginalCarrier.Get(), EventName, Reason, bResult);
	BP_ObjectAssemblySnapshotChanged();
}

bool AHeistObjectDisplayCaseActor::ResolveInspectionSchedule(const FHeistObjectAssemblyResult& AssemblyResult, FName& OutRejectReason)
{
	OutRejectReason = NAME_None;
	const AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	FHeistArtifactDataRow ArtifactDefinition;
	if (!HasAuthority() || !IsValid(HeistGameMode) || !HeistGameMode->TryGetArtifactDefinition(TargetObjectArtifactId, ArtifactDefinition) ||
		!FMath::IsFinite(ArtifactDefinition.BaseInspectionDelay) || ArtifactDefinition.BaseInspectionDelay < 0.0f)
	{
		OutRejectReason = FName(TEXT("InvalidInspectionDelayData"));
		return false;
	}
	if (!CalculateInspectionSchedule(AssemblyResult.QualityScore, ArtifactDefinition.BaseInspectionDelay, ResolvedInspectionDelay, InspectionScoreBand,
									 ResolvedInspectionAlertOutcome, ResolvedInspectionCaseOutcome))
	{
		OutRejectReason = FName(TEXT("InspectionScheduleMappingFailed"));
		return false;
	}
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const float ServerTime = IsValid(HeistGameState) ? HeistGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	InspectionReadyServerTime = ServerTime + ResolvedInspectionDelay;
	++InspectionScheduleRevision;
	return true;
}

void AHeistObjectDisplayCaseActor::StartInspectionDelayTimer()
{
	ClearInspectionDelayTimer();
	if (!HasAuthority() || !IsValid(GetWorld()) || ResolvedInspectionDelay <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	FTimerDelegate TimerDelegate;
	TimerDelegate.BindUObject(this, &AHeistObjectDisplayCaseActor::HandleInspectionDelayExpired, InspectionScheduleRevision, InspectionDelayTimerRevision);
	GetWorld()->GetTimerManager().SetTimer(InspectionDelayTimerHandle, TimerDelegate, ResolvedInspectionDelay, false);
}

void AHeistObjectDisplayCaseActor::ClearInspectionDelayTimer()
{
	++InspectionDelayTimerRevision;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InspectionDelayTimerHandle);
	}
	InspectionDelayTimerHandle.Invalidate();
}

void AHeistObjectDisplayCaseActor::HandleInspectionDelayExpired(const int32 ExpectedScheduleRevision, const int32 ExpectedTimerRevision)
{
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!HasAuthority() || ExpectedScheduleRevision != InspectionScheduleRevision || ExpectedTimerRevision != InspectionDelayTimerRevision || !IsValid(HeistGameState) ||
		HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame)
	{
		UHeistDebugFunctionLibrary::DebugObjectAssemblyInspection(this, nullptr, FName(TEXT("DelayExpired")), FName(TEXT("StaleOrMatchEnded")), false);
		return;
	}
	InspectionDelayTimerHandle.Invalidate();
	RefreshInspectionRegistration();
	UHeistDebugFunctionLibrary::DebugObjectAssemblyInspection(this, nullptr, FName(TEXT("DelayExpired")), FName(TEXT("Accepted")), bRegisteredForInspection);
}

bool AHeistObjectDisplayCaseActor::HasInspectionDelayElapsed() const
{
	return GetInspectionDelayRemaining() <= KINDA_SMALL_NUMBER;
}

void AHeistObjectDisplayCaseActor::RefreshInspectionRegistration()
{
	if (!HasAuthority())
	{
		return;
	}
	const bool bEligibleState = AssemblyState == EHeistObjectAssemblyState::ReplicaPlaced || AssemblyState == EHeistObjectAssemblyState::OriginalAvailable ||
		AssemblyState == EHeistObjectAssemblyState::OriginalRemoved;
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const bool bShouldRegister = IsValid(HeistGameState) && HeistGameState->GetMatchPhase() == EHeistMatchPhase::InGame && bHasCommittedAssemblyResult &&
		CommittedAssemblyResult.bReplicaPlaced && bEligibleState && HasInspectionDelayElapsed();
	if (bRegisteredForInspection != bShouldRegister)
	{
		bRegisteredForInspection = bShouldRegister;
		++InspectionRegistrationRevision;
		UHeistDebugFunctionLibrary::DebugObjectAssemblyInspection(this, InspectingGuardActor.Get(), FName(TEXT("Registration")),
																 bShouldRegister ? FName(TEXT("Registered")) : FName(TEXT("Unregistered")), true);
	}
}

void AHeistObjectDisplayCaseActor::ClearInspectionStateForMatchEnd()
{
	ClearInspectionDelayTimer();
	InspectionReadyServerTime = 0.0f;
	if (AssemblyState == EHeistObjectAssemblyState::Inspecting)
	{
		if (AActor* Guard = InspectingGuardActor.Get(); IsValid(Guard))
		{
			InterruptInspection(Guard, FName(TEXT("MatchEnded")));
		}
		else
		{
			AssemblyState = PreInspectionState;
			InspectingGuardActor.Reset();
			ActiveInspectionScheduleRevision = INDEX_NONE;
			++AssemblyRevision;
		}
	}
	else
	{
		InspectingGuardActor.Reset();
		ActiveInspectionScheduleRevision = INDEX_NONE;
	}
	bRegisteredForInspection = false;
	RefreshObjectVisualState();
}

void AHeistObjectDisplayCaseActor::HandleSessionOwnerArrestStateChanged(const bool bArrested)
{
	if (HasAuthority() && bSessionLocked && bArrested)
	{
		ClearSession(FName(TEXT("OwnerArrested")));
	}
}

void AHeistObjectDisplayCaseActor::HandleOriginalCarrierArrestStateChanged(const bool bArrested)
{
	if (HasAuthority() && bArrested && IsValid(OriginalCarrier.Get()))
	{
		DropOriginalForCarrier(OriginalCarrier.Get(), FName(TEXT("OwnerArrested")));
	}
}

void AHeistObjectDisplayCaseActor::HandleMatchPhaseChanged(const EHeistMatchPhase PreviousMatchPhase, const EHeistMatchPhase NewMatchPhase)
{
	if (!HasAuthority() || PreviousMatchPhase == NewMatchPhase)
	{
		return;
	}
	if (bSessionLocked && NewMatchPhase != EHeistMatchPhase::InGame)
	{
		ClearSession(FName(TEXT("MatchPhaseChanged")));
	}
	if (NewMatchPhase != EHeistMatchPhase::InGame)
	{
		ClearInspectionStateForMatchEnd();
		ForceNetUpdate();
	}
	else
	{
		RefreshInspectionRegistration();
	}
}
