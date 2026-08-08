#include "Character/Components/HeistObjectAssemblyComponent.h"

#include "Character/Components/HeistForgeryComponent.h"
#include "Character/Components/HeistInventoryComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerState.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "World/Actors/Loot/HeistObjectDisplayCaseActor.h"

namespace
{
constexpr int32 ObjectAssemblyEntryWireBytes = sizeof(FName) * 3 + sizeof(uint8);

bool IsMaterialSelectionResolved(const FHeistObjectAssemblyPartRow& PartDefinition, const FName MaterialId)
{
	if (PartDefinition.AllowedMaterialIds.IsEmpty())
	{
		return MaterialId.IsNone();
	}
	const TSoftObjectPtr<UMaterialInterface>* MaterialAsset = PartDefinition.MaterialVariants.Find(MaterialId);
	return !MaterialId.IsNone() && PartDefinition.AllowedMaterialIds.Contains(MaterialId) && MaterialAsset != nullptr && !MaterialAsset->IsNull();
}
}

UHeistObjectAssemblyComponent::UHeistObjectAssemblyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UHeistObjectAssemblyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetOwner() && GetOwner()->HasAuthority() && (bSessionActive || IsValid(ActiveDisplayCase.Get())))
	{
		ClearSession(FName(TEXT("OwnerEndPlay")), true, false);
	}
	else if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SessionTimeoutTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

bool UHeistObjectAssemblyComponent::TryBeginAssemblySession(AHeistObjectDisplayCaseActor* TargetDisplayCase, const float DurationSeconds)
{
	AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter) ? HeistCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	if (!IsValid(HeistCharacter) || !HeistCharacter->HasAuthority() || !IsValid(HeistPlayerState) || !IsValid(TargetDisplayCase))
	{
		BroadcastSessionSnapshot(FName(TEXT("SessionBegin")), FName(TEXT("InvalidAuthorityContext")), false);
		return false;
	}
	if (bSessionActive || IsValid(ActiveDisplayCase.Get()))
	{
		BroadcastSessionSnapshot(FName(TEXT("SessionBegin")), FName(TEXT("SessionAlreadyActive")), false);
		return false;
	}
	if (const UHeistForgeryComponent* ForgeryComponent = HeistCharacter->GetForgeryComponent(); IsValid(ForgeryComponent) && ForgeryComponent->IsSessionActive())
	{
		BroadcastSessionSnapshot(FName(TEXT("SessionBegin")), FName(TEXT("SurfaceForgeryActive")), false);
		return false;
	}
	if (const UHeistInventoryComponent* InventoryComponent = HeistCharacter->GetInventoryComponent(); IsValid(InventoryComponent) && InventoryComponent->IsInventoryOpen())
	{
		BroadcastSessionSnapshot(FName(TEXT("SessionBegin")), FName(TEXT("InventoryOpen")), false);
		return false;
	}
	const bool bObservationLockOwned = TargetDisplayCase->IsSessionLocked() && TargetDisplayCase->GetSessionOwner() == HeistPlayerState;
	if (TargetDisplayCase->IsSessionLocked() && !bObservationLockOwned)
	{
		BroadcastSessionSnapshot(FName(TEXT("SessionBegin")), FName(TEXT("CaseSessionLocked")), false);
		return false;
	}

	FName RejectReason = NAME_None;
	if (!TryPrepareTemplate(TargetDisplayCase, RejectReason))
	{
		if (bObservationLockOwned)
		{
			TargetDisplayCase->CancelSessionForOwner(HeistPlayerState, FName(TEXT("AssemblyTemplateRejected")));
		}
		BroadcastSessionSnapshot(FName(TEXT("SessionBegin")), RejectReason, false);
		return false;
	}
	if (DurationSeconds > 0.0f && !FMath::IsWithinInclusive(DurationSeconds, 25.0f, 35.0f))
	{
		if (bObservationLockOwned)
		{
			TargetDisplayCase->CancelSessionForOwner(HeistPlayerState, FName(TEXT("AssemblyDurationRejected")));
		}
		ResetPreparedTemplate();
		BroadcastSessionSnapshot(FName(TEXT("SessionBegin")), FName(TEXT("DurationOutsideContract")), false);
		return false;
	}
	if (!bObservationLockOwned && !TargetDisplayCase->TryBeginSession(HeistPlayerState))
	{
		ResetPreparedTemplate();
		BroadcastSessionSnapshot(FName(TEXT("SessionBegin")), FName(TEXT("CaseSessionAcquireFailed")), false);
		return false;
	}

	if (TargetDisplayCase->GetAssemblyState() == EHeistObjectAssemblyState::Secured &&
		!TargetDisplayCase->TryTransitionToAssemblyState(EHeistObjectAssemblyState::Observed))
	{
		TargetDisplayCase->CancelSessionForOwner(HeistPlayerState, FName(TEXT("ObservationTransitionFailed")));
		ResetPreparedTemplate();
		BroadcastSessionSnapshot(FName(TEXT("SessionBegin")), FName(TEXT("ObservationTransitionFailed")), false);
		return false;
	}
	if (!TargetDisplayCase->TryTransitionToAssemblyState(EHeistObjectAssemblyState::AssemblyInProgress))
	{
		TargetDisplayCase->CancelSessionForOwner(HeistPlayerState, FName(TEXT("AssemblyTransitionFailed")));
		ResetPreparedTemplate();
		BroadcastSessionSnapshot(FName(TEXT("SessionBegin")), FName(TEXT("AssemblyTransitionFailed")), false);
		return false;
	}
	ActiveDisplayCase = TargetDisplayCase;
	ActiveDisplayCase->OnObjectAssemblySessionChanged.AddDynamic(this, &UHeistObjectAssemblyComponent::HandleDisplayCaseSessionChanged);
	bSessionActive = true;
	LastCleanupReason = NAME_None;
	ResetPayloadState(true);
	ResetAuthoritativeResult();

	const float SafeDurationSeconds = DurationSeconds > 0.0f ? DurationSeconds : PreparedTemplate.AssemblyDuration;
	ActiveSessionDurationSeconds = SafeDurationSeconds;
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	SessionStartServerTime = IsValid(HeistGameState) ? HeistGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	SessionEndServerTime = SessionStartServerTime + SafeDurationSeconds;
	++SessionRevision;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(SessionTimeoutTimerHandle, this, &UHeistObjectAssemblyComponent::HandleSessionTimeout, SafeDurationSeconds, false);
	}

	HeistCharacter->ForceNetUpdate();
	BroadcastSessionSnapshot(FName(TEXT("SessionBegin")), FName(TEXT("Accepted")), true);
	return true;
}

bool UHeistObjectAssemblyComponent::TrySubmitAssemblyPayload(const TArray<FHeistObjectAssemblyEntry>& Entries, const int32 ClientSessionRevision)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	FName RejectReason = NAME_None;
	int32 PayloadBytes = 0;
	if (!ValidatePayload(Entries, ClientSessionRevision, RejectReason, PayloadBytes))
	{
		RecordPayloadValidation(false, RejectReason, Entries.Num(), PayloadBytes);
		if (RejectReason == FName(TEXT("SessionExpired")))
		{
			ClearSession(FName(TEXT("Timeout")), true, false);
		}
		return false;
	}

	FHeistObjectAssemblyResult CalculatedResult;
	if (!CalculateDeterministicScore(Entries, CalculatedResult))
	{
		RecordPayloadValidation(false, FName(TEXT("ScoreCalculationFailed")), Entries.Num(), PayloadBytes);
		return false;
	}

	ValidatedEntries = Entries;
	RecordPayloadValidation(true, FName(TEXT("Accepted")), Entries.Num(), PayloadBytes);

	AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter) ? HeistCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	AHeistObjectDisplayCaseActor* SubmittedDisplayCase = ActiveDisplayCase.Get();
	bHandlingCaseSessionCallback = true;
	const bool bReplicaCommitted =
		IsValid(SubmittedDisplayCase) && IsValid(HeistPlayerState) && SubmittedDisplayCase->TryCommitAssemblyReplica(HeistPlayerState, CalculatedResult, Entries);
	bHandlingCaseSessionCallback = false;
	if (!bReplicaCommitted)
	{
		RecordPayloadValidation(false, FName(TEXT("ReplicaPlacementRejected")), Entries.Num(), PayloadBytes);
		return false;
	}

	AuthoritativeResult = SubmittedDisplayCase->GetCommittedAssemblyResult();
	bHasAuthoritativeResult = true;
	++ScoreRevision;
	GetOwner()->ForceNetUpdate();
	UHeistDebugFunctionLibrary::DebugObjectAssemblyScoreCommitted(this, AuthoritativeResult);
	EnterPendingReplicaReview();
	return true;
}

bool UHeistObjectAssemblyComponent::TryConfirmReplicaSwap()
{
	AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter) ? HeistCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	AHeistObjectDisplayCaseActor* TargetDisplayCase = ActiveDisplayCase.Get();
	if (!IsValid(HeistCharacter) || !HeistCharacter->HasAuthority() || !HasPendingReplicaReview() || !IsValid(HeistPlayerState) || !IsValid(TargetDisplayCase))
	{
		return false;
	}

	int32 AddedInventoryInstanceId = INDEX_NONE;
	FName RejectReason = NAME_None;
	bHandlingCaseSessionCallback = true;
	const bool bCommitted = TargetDisplayCase->TryCommitReplicaSwapAndTakeOriginal(HeistPlayerState, AddedInventoryInstanceId, RejectReason);
	bHandlingCaseSessionCallback = false;
	if (!bCommitted)
	{
		BroadcastSessionSnapshot(FName(TEXT("ServerReplicaSwap")), RejectReason.IsNone() ? FName(TEXT("CommitRejected")) : RejectReason, false);
		return false;
	}

	AuthoritativeResult = TargetDisplayCase->GetCommittedAssemblyResult();
	HeistPlayerState->RecordAssemblyContribution(AuthoritativeResult.QualityScore);
	++ScoreRevision;
	HeistCharacter->ForceNetUpdate();
	UHeistDebugFunctionLibrary::DebugObjectAssemblyScoreCommitted(this, AuthoritativeResult);
	CompleteSuccessfulSession();
	return true;
}

bool UHeistObjectAssemblyComponent::TryRestartAssemblyFromPreview()
{
	AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter) ? HeistCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	AHeistObjectDisplayCaseActor* TargetDisplayCase = ActiveDisplayCase.Get();
	if (!IsValid(HeistCharacter) || !HeistCharacter->HasAuthority() || !HasPendingReplicaReview() || !IsValid(HeistPlayerState) || !IsValid(TargetDisplayCase))
	{
		return false;
	}

	FName RejectReason = NAME_None;
	bHandlingCaseSessionCallback = true;
	const bool bRestarted = TargetDisplayCase->TryRestartAssemblyFromPreview(HeistPlayerState, RejectReason);
	bHandlingCaseSessionCallback = false;
	if (!bRestarted)
	{
		return false;
	}

	ResetPayloadState(true);
	ResetAuthoritativeResult();
	++ScoreRevision;
	bSessionActive = true;
	const float SafeDurationSeconds = FMath::Max(1.0f, ActiveSessionDurationSeconds > 0.0f ? ActiveSessionDurationSeconds : PreparedTemplate.AssemblyDuration);
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const float ServerWorldTime = IsValid(HeistGameState) ? HeistGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	SessionStartServerTime = ServerWorldTime;
	SessionEndServerTime = ServerWorldTime + SafeDurationSeconds;
	++SessionRevision;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(SessionTimeoutTimerHandle, this, &UHeistObjectAssemblyComponent::HandleSessionTimeout, SafeDurationSeconds, false);
	}
	HeistCharacter->ForceNetUpdate();
	BroadcastSessionSnapshot(FName(TEXT("ServerReassemble")), FName(TEXT("ReplicaPreviewDiscarded")), true);
	return true;
}

bool UHeistObjectAssemblyComponent::CancelAssemblySession(const FName Reason)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || (!bSessionActive && !IsValid(ActiveDisplayCase.Get())))
	{
		return false;
	}

	ClearSession(Reason.IsNone() ? FName(TEXT("OwnerCancelled")) : Reason, true, false);
	return true;
}

bool UHeistObjectAssemblyComponent::ForceTimeoutForDebug()
{
#if !UE_BUILD_SHIPPING
	if (GetOwner() && GetOwner()->HasAuthority() && bSessionActive)
	{
		HandleSessionTimeout();
		return true;
	}
#endif
	return false;
}

bool UHeistObjectAssemblyComponent::IsSessionActive() const
{
	return bSessionActive;
}

bool UHeistObjectAssemblyComponent::HasPendingReplicaReview() const
{
	const AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	const AHeistObjectDisplayCaseActor* TargetDisplayCase = ActiveDisplayCase.Get();
	return !bSessionActive && bHasAuthoritativeResult && !AuthoritativeResult.bReplicaPlaced && IsValid(HeistCharacter) && IsValid(TargetDisplayCase) &&
		TargetDisplayCase->IsReplicaReviewReadyFor(HeistCharacter);
}

float UHeistObjectAssemblyComponent::GetSessionEndServerTime() const
{
	return SessionEndServerTime;
}

int32 UHeistObjectAssemblyComponent::GetSessionRevision() const
{
	return SessionRevision;
}

AHeistObjectDisplayCaseActor* UHeistObjectAssemblyComponent::GetActiveDisplayCase() const
{
	return ActiveDisplayCase.Get();
}

FName UHeistObjectAssemblyComponent::GetActiveArtifactId() const
{
	return ActiveArtifactId;
}

FName UHeistObjectAssemblyComponent::GetActiveTemplateId() const
{
	return ActiveTemplateId;
}

FName UHeistObjectAssemblyComponent::GetActiveFamilyId() const
{
	return ActiveFamilyId;
}

FName UHeistObjectAssemblyComponent::GetLastCleanupReason() const
{
	return LastCleanupReason;
}

bool UHeistObjectAssemblyComponent::WasLastPayloadAccepted() const
{
	return bLastPayloadAccepted;
}

FName UHeistObjectAssemblyComponent::GetLastPayloadReason() const
{
	return LastPayloadReason;
}

int32 UHeistObjectAssemblyComponent::GetPayloadValidationRevision() const
{
	return PayloadValidationRevision;
}

int32 UHeistObjectAssemblyComponent::GetValidatedEntryCount() const
{
	return ValidatedEntryCount;
}

int32 UHeistObjectAssemblyComponent::GetValidatedPayloadBytes() const
{
	return ValidatedPayloadBytes;
}

const TArray<FHeistObjectAssemblyEntry>& UHeistObjectAssemblyComponent::GetValidatedEntries() const
{
	return ValidatedEntries;
}

bool UHeistObjectAssemblyComponent::HasAuthoritativeResult() const
{
	return bHasAuthoritativeResult;
}

const FHeistObjectAssemblyResult& UHeistObjectAssemblyComponent::GetAuthoritativeResult() const
{
	return AuthoritativeResult;
}

int32 UHeistObjectAssemblyComponent::GetScoreRevision() const
{
	return ScoreRevision;
}

const FHeistObjectAssemblyTemplateRow* UHeistObjectAssemblyComponent::GetPreparedTemplateForDebug() const
{
	return PreparedTemplate.TemplateId.IsNone() ? nullptr : &PreparedTemplate;
}

FHeistObjectAssemblySessionStateChanged& UHeistObjectAssemblyComponent::GetSessionStateChangedDelegate()
{
	return SessionStateChangedDelegate;
}

bool UHeistObjectAssemblyComponent::TryPrepareTemplate(AHeistObjectDisplayCaseActor* TargetDisplayCase, FName& OutRejectReason)
{
	OutRejectReason = NAME_None;
	AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	FHeistArtifactDataRow ArtifactDefinition;
	FHeistObjectAssemblyTemplateRow TemplateDefinition;
	if (!IsValid(HeistGameMode) || !IsValid(TargetDisplayCase) ||
		!HeistGameMode->TryGetArtifactDefinition(TargetDisplayCase->GetTargetArtifactId(), ArtifactDefinition) ||
		ArtifactDefinition.ForgeryType != EHeistForgeryType::Assembly || ArtifactDefinition.ForgeryTemplateId.IsNone() ||
		!HeistGameMode->TryGetObjectAssemblyTemplateDefinition(ArtifactDefinition.ForgeryTemplateId, TemplateDefinition))
	{
		OutRejectReason = FName(TEXT("ArtifactOrTemplateLookupFailed"));
		return false;
	}
	if (TemplateDefinition.FamilyId != TargetDisplayCase->GetObjectFamilyId())
	{
		OutRejectReason = FName(TEXT("CaseTemplateFamilyMismatch"));
		return false;
	}

	const float WeightTotal = TemplateDefinition.RequiredPartWeight + TemplateDefinition.SocketTopologyWeight + TemplateDefinition.OrientationWeight + TemplateDefinition.MaterialWeight;
	if (!FMath::IsFinite(WeightTotal) || WeightTotal <= 0.0f || !FMath::IsWithinInclusive(TemplateDefinition.RequiredParts.Num(), 3, 5))
	{
		OutRejectReason = FName(TEXT("InvalidTemplateScoreContract"));
		return false;
	}

	TMap<FName, FHeistObjectAssemblyPartRow> ResolvedPartDefinitions;
	TArray<FName> ReferencedPartIds;
	ReferencedPartIds.Reserve(1 + TemplateDefinition.RequiredParts.Num() + TemplateDefinition.DecoyPartIds.Num());
	ReferencedPartIds.Add(TemplateDefinition.CorePartId);
	for (const FHeistObjectAssemblyEntry& RequiredPart : TemplateDefinition.RequiredParts)
	{
		ReferencedPartIds.Add(RequiredPart.PartId);
	}
	ReferencedPartIds.Append(TemplateDefinition.DecoyPartIds);

	for (const FName PartId : ReferencedPartIds)
	{
		FHeistObjectAssemblyPartRow PartDefinition;
		if (PartId.IsNone() || ResolvedPartDefinitions.Contains(PartId) || !HeistGameMode->TryGetObjectAssemblyPartDefinition(PartId, PartDefinition) ||
			PartDefinition.FamilyId != TemplateDefinition.FamilyId)
		{
			OutRejectReason = FName(TEXT("PartDefinitionLookupFailed"));
			return false;
		}
		ResolvedPartDefinitions.Add(PartId, PartDefinition);
	}

	for (const FHeistObjectAssemblyEntry& RequiredPart : TemplateDefinition.RequiredParts)
	{
		const FHeistObjectAssemblyPartRow* PartDefinition = ResolvedPartDefinitions.Find(RequiredPart.PartId);
		const bool bMaterialValid = PartDefinition != nullptr && IsMaterialSelectionResolved(*PartDefinition, RequiredPart.MaterialId);
		if (PartDefinition == nullptr || !PartDefinition->CompatibleSocketIds.Contains(RequiredPart.SocketId) ||
			!PartDefinition->AllowedOrientationSteps.Contains(RequiredPart.QuantizedOrientation) || !bMaterialValid)
		{
			OutRejectReason = FName(TEXT("RequiredPartContractInvalid"));
			return false;
		}
	}

	ResetPreparedTemplate();
	PreparedTemplate = TemplateDefinition;
	PreparedPartDefinitions = MoveTemp(ResolvedPartDefinitions);
	ActiveArtifactId = ArtifactDefinition.ArtifactId;
	ActiveTemplateId = TemplateDefinition.TemplateId;
	ActiveFamilyId = TemplateDefinition.FamilyId;
	return true;
}

bool UHeistObjectAssemblyComponent::ValidateActiveSession(FName& OutRejectReason) const
{
	OutRejectReason = NAME_None;
	const AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	const AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter) ? HeistCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	if (!IsValid(HeistCharacter) || !HeistCharacter->HasAuthority() || !IsValid(HeistPlayerState) || !bSessionActive || !IsValid(ActiveDisplayCase.Get()))
	{
		OutRejectReason = FName(TEXT("InactiveOrInvalidSession"));
		return false;
	}
	if (!ActiveDisplayCase->IsSessionLocked() || ActiveDisplayCase->GetSessionOwner() != HeistPlayerState ||
		ActiveDisplayCase->GetAssemblyState() != EHeistObjectAssemblyState::AssemblyInProgress)
	{
		OutRejectReason = FName(TEXT("CaseOwnershipOrStateMismatch"));
		return false;
	}
	if (!FMath::IsWithinInclusive(PreparedTemplate.AssemblyDuration, 25.0f, 35.0f) || !FMath::IsWithinInclusive(ActiveSessionDurationSeconds, 25.0f, 35.0f))
	{
		OutRejectReason = FName(TEXT("DurationOutsideContract"));
		return false;
	}
	if (HeistPlayerState->IsArrested() || HeistPlayerState->IsEscaped())
	{
		OutRejectReason = FName(TEXT("PlayerStateBlocked"));
		return false;
	}
	if (FVector::DistSquared(HeistCharacter->GetActorLocation(), ActiveDisplayCase->GetActorLocation()) > FMath::Square(ActiveDisplayCase->GetMaximumSessionDistance()))
	{
		OutRejectReason = FName(TEXT("OutOfRange"));
		return false;
	}
	return true;
}

bool UHeistObjectAssemblyComponent::ValidatePayload(const TArray<FHeistObjectAssemblyEntry>& Entries, const int32 ClientSessionRevision, FName& OutRejectReason,
													 int32& OutPayloadBytes) const
{
	OutPayloadBytes = sizeof(int32) + Entries.Num() * ObjectAssemblyEntryWireBytes;
	if (!ValidateActiveSession(OutRejectReason))
	{
		return false;
	}
	if (ClientSessionRevision != SessionRevision)
	{
		OutRejectReason = FName(TEXT("SessionRevisionMismatch"));
		return false;
	}

	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const float ServerWorldTime = IsValid(HeistGameState) ? HeistGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	if (SessionEndServerTime <= 0.0f || ServerWorldTime > SessionEndServerTime + SubmissionTimeoutToleranceSeconds)
	{
		OutRejectReason = FName(TEXT("SessionExpired"));
		return false;
	}
	if (Entries.IsEmpty())
	{
		OutRejectReason = FName(TEXT("MissingEntries"));
		return false;
	}
	if (Entries.Num() > MaximumPayloadEntries || Entries.Num() > PreparedTemplate.RequiredParts.Num() + PreparedTemplate.DecoyPartIds.Num())
	{
		OutRejectReason = FName(TEXT("PartCountExceeded"));
		return false;
	}
	if (OutPayloadBytes > MaximumPayloadBytes)
	{
		OutRejectReason = FName(TEXT("PayloadTooLarge"));
		return false;
	}

	TSet<FName> SubmittedPartIds;
	TSet<FName> SubmittedSocketIds;
	for (const FHeistObjectAssemblyEntry& Entry : Entries)
	{
		if (Entry.PartId.IsNone() || Entry.SocketId.IsNone())
		{
			OutRejectReason = FName(TEXT("MissingEntryIdentity"));
			return false;
		}
		if (SubmittedPartIds.Contains(Entry.PartId))
		{
			OutRejectReason = FName(TEXT("DuplicatePart"));
			return false;
		}
		if (SubmittedSocketIds.Contains(Entry.SocketId))
		{
			OutRejectReason = FName(TEXT("DuplicateSocket"));
			return false;
		}
		SubmittedPartIds.Add(Entry.PartId);
		SubmittedSocketIds.Add(Entry.SocketId);

		const FHeistObjectAssemblyPartRow* PartDefinition = PreparedPartDefinitions.Find(Entry.PartId);
		if (PartDefinition == nullptr)
		{
			FHeistObjectAssemblyPartRow SubmittedPartDefinition;
			const AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
			if (IsValid(HeistGameMode) && HeistGameMode->TryGetObjectAssemblyPartDefinition(Entry.PartId, SubmittedPartDefinition) &&
				SubmittedPartDefinition.FamilyId != ActiveFamilyId)
			{
				OutRejectReason = FName(TEXT("PartFamilyMismatch"));
				return false;
			}
			OutRejectReason = FName(TEXT("PartNotInTemplatePool"));
			return false;
		}
		if (PartDefinition->FamilyId != ActiveFamilyId)
		{
			OutRejectReason = FName(TEXT("PartFamilyMismatch"));
			return false;
		}
		if (Entry.PartId == PreparedTemplate.CorePartId)
		{
			OutRejectReason = FName(TEXT("CorePartSubmitted"));
			return false;
		}
		const bool bRequiredPart = PreparedTemplate.RequiredParts.ContainsByPredicate(
			[&Entry](const FHeistObjectAssemblyEntry& RequiredPart) { return RequiredPart.PartId == Entry.PartId; });
		if (!bRequiredPart && !PreparedTemplate.DecoyPartIds.Contains(Entry.PartId))
		{
			OutRejectReason = FName(TEXT("PartNotApproved"));
			return false;
		}
		if (!PartDefinition->CompatibleSocketIds.Contains(Entry.SocketId))
		{
			OutRejectReason = FName(TEXT("SocketIncompatible"));
			return false;
		}
		if (!PartDefinition->AllowedOrientationSteps.Contains(Entry.QuantizedOrientation))
		{
			OutRejectReason = FName(TEXT("OrientationNotAllowed"));
			return false;
		}
		const bool bMaterialValid = IsMaterialSelectionResolved(*PartDefinition, Entry.MaterialId);
		if (!bMaterialValid)
		{
			OutRejectReason = FName(TEXT("MaterialNotAllowed"));
			return false;
		}
	}

	OutRejectReason = FName(TEXT("Accepted"));
	return true;
}

bool UHeistObjectAssemblyComponent::CalculateDeterministicScore(const TArray<FHeistObjectAssemblyEntry>& Entries, FHeistObjectAssemblyResult& OutResult) const
{
	OutResult = FHeistObjectAssemblyResult();
	const int32 RequiredPartCount = PreparedTemplate.RequiredParts.Num();
	const float WeightTotal = PreparedTemplate.RequiredPartWeight + PreparedTemplate.SocketTopologyWeight + PreparedTemplate.OrientationWeight + PreparedTemplate.MaterialWeight;
	if (RequiredPartCount <= 0 || !FMath::IsFinite(WeightTotal) || WeightTotal <= 0.0f)
	{
		return false;
	}

	TMap<FName, const FHeistObjectAssemblyEntry*> SubmittedByPartId;
	for (const FHeistObjectAssemblyEntry& Entry : Entries)
	{
		SubmittedByPartId.Add(Entry.PartId, &Entry);
	}

	int32 MatchedRequiredParts = 0;
	int32 CorrectSockets = 0;
	int32 CorrectOrientations = 0;
	int32 CorrectMaterials = 0;
	for (const FHeistObjectAssemblyEntry& RequiredPart : PreparedTemplate.RequiredParts)
	{
		const FHeistObjectAssemblyEntry* const* SubmittedEntryPtr = SubmittedByPartId.Find(RequiredPart.PartId);
		const FHeistObjectAssemblyEntry* SubmittedEntry = SubmittedEntryPtr != nullptr ? *SubmittedEntryPtr : nullptr;
		if (SubmittedEntry == nullptr)
		{
			continue;
		}

		++MatchedRequiredParts;
		CorrectSockets += SubmittedEntry->SocketId == RequiredPart.SocketId ? 1 : 0;
		CorrectOrientations += SubmittedEntry->QuantizedOrientation == RequiredPart.QuantizedOrientation ? 1 : 0;
		CorrectMaterials += SubmittedEntry->MaterialId == RequiredPart.MaterialId ? 1 : 0;
	}

	const float RequiredPartScore = 100.0f * static_cast<float>(MatchedRequiredParts) / static_cast<float>(RequiredPartCount);
	const float SocketTopologyScore = 100.0f * static_cast<float>(CorrectSockets) / static_cast<float>(RequiredPartCount);
	const float OrientationScore = 100.0f * static_cast<float>(CorrectOrientations) / static_cast<float>(RequiredPartCount);
	const float MaterialScore = 100.0f * static_cast<float>(CorrectMaterials) / static_cast<float>(RequiredPartCount);
	const float Completeness = FMath::Clamp(static_cast<float>(MatchedRequiredParts) / static_cast<float>(RequiredPartCount), 0.0f, 1.0f);
	const float WeightedScore = (RequiredPartScore * PreparedTemplate.RequiredPartWeight + SocketTopologyScore * PreparedTemplate.SocketTopologyWeight +
								 OrientationScore * PreparedTemplate.OrientationWeight + MaterialScore * PreparedTemplate.MaterialWeight) /
								WeightTotal;

	const int32 ExtraPartCount = FMath::Max(0, Entries.Num() - MatchedRequiredParts);
	const bool bExtraPartCapTriggered = ExtraPartCount > 0;
	float QualityScore = FMath::Clamp(WeightedScore * Completeness, 0.0f, 100.0f);
	if (bExtraPartCapTriggered)
	{
		QualityScore = FMath::Min(QualityScore, PreparedTemplate.ExtraPartScoreCap);
	}

	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const float ServerWorldTime = IsValid(HeistGameState) ? HeistGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : SessionStartServerTime);
	OutResult.ArtifactId = ActiveArtifactId;
	OutResult.TemplateId = ActiveTemplateId;
	OutResult.QualityScore = QualityScore;
	OutResult.RequiredPartScore = RequiredPartScore;
	OutResult.SocketTopologyScore = SocketTopologyScore;
	OutResult.OrientationScore = OrientationScore;
	OutResult.MaterialScore = MaterialScore;
	OutResult.Completeness = Completeness;
	OutResult.bExtraPartCapTriggered = bExtraPartCapTriggered;
	OutResult.CompletionTime = FMath::Clamp(ServerWorldTime - SessionStartServerTime, 0.0f, ActiveSessionDurationSeconds);
	OutResult.bReplicaPlaced = false;

	return !OutResult.ArtifactId.IsNone() && !OutResult.TemplateId.IsNone() && FMath::IsFinite(OutResult.QualityScore) &&
		   FMath::IsWithinInclusive(OutResult.QualityScore, 0.0f, 100.0f) && FMath::IsWithinInclusive(OutResult.Completeness, 0.0f, 1.0f);
}

void UHeistObjectAssemblyComponent::RecordPayloadValidation(const bool bAccepted, const FName Reason, const int32 EntryCount, const int32 PayloadBytes)
{
	bLastPayloadAccepted = bAccepted;
	LastPayloadReason = Reason;
	ValidatedEntryCount = bAccepted ? EntryCount : 0;
	ValidatedPayloadBytes = PayloadBytes;
	++PayloadValidationRevision;
	if (GetOwner())
	{
		GetOwner()->ForceNetUpdate();
	}
	UHeistDebugFunctionLibrary::DebugObjectAssemblyPayloadValidation(this, bAccepted, Reason, EntryCount, PayloadBytes);
}

void UHeistObjectAssemblyComponent::ResetPayloadState(const bool bResetLastValidation)
{
	ValidatedEntries.Reset();
	ValidatedEntryCount = 0;
	ValidatedPayloadBytes = 0;
	if (bResetLastValidation)
	{
		bLastPayloadAccepted = false;
		LastPayloadReason = NAME_None;
	}
}

void UHeistObjectAssemblyComponent::ResetPreparedTemplate()
{
	PreparedTemplate = FHeistObjectAssemblyTemplateRow();
	PreparedPartDefinitions.Reset();
	ActiveArtifactId = NAME_None;
	ActiveTemplateId = NAME_None;
	ActiveFamilyId = NAME_None;
}

void UHeistObjectAssemblyComponent::ResetAuthoritativeResult()
{
	bHasAuthoritativeResult = false;
	AuthoritativeResult = FHeistObjectAssemblyResult();
}

void UHeistObjectAssemblyComponent::EnterPendingReplicaReview()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SessionTimeoutTimerHandle);
	}
	bSessionActive = false;
	SessionEndServerTime = 0.0f;
	++SessionRevision;
	if (GetOwner())
	{
		GetOwner()->ForceNetUpdate();
	}
	BroadcastSessionSnapshot(FName(TEXT("ServerPreviewReady")), FName(TEXT("ReplicaReadyGameplayRestored")), true);
}

void UHeistObjectAssemblyComponent::CompleteSuccessfulSession()
{
	ClearSession(FName(TEXT("ReplicaSwapCommitted")), false, true);
}

void UHeistObjectAssemblyComponent::HandleSessionTimeout()
{
	if (GetOwner() && GetOwner()->HasAuthority() && bSessionActive)
	{
		ClearSession(FName(TEXT("Timeout")), true, false);
	}
}

void UHeistObjectAssemblyComponent::ClearSession(const FName Reason, const bool bReleaseCaseLock, const bool bPreserveResult)
{
	AHeistObjectDisplayCaseActor* PreviousDisplayCase = ActiveDisplayCase.Get();
	AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter) ? HeistCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SessionTimeoutTimerHandle);
	}
	if (IsValid(PreviousDisplayCase))
	{
		PreviousDisplayCase->OnObjectAssemblySessionChanged.RemoveDynamic(this, &UHeistObjectAssemblyComponent::HandleDisplayCaseSessionChanged);
	}

	bHandlingCaseSessionCallback = true;
	if (bReleaseCaseLock && IsValid(PreviousDisplayCase) && IsValid(HeistPlayerState))
	{
		PreviousDisplayCase->CancelSessionForOwner(HeistPlayerState, Reason);
	}
	bHandlingCaseSessionCallback = false;

	ActiveDisplayCase = nullptr;
	bSessionActive = false;
	SessionEndServerTime = 0.0f;
	SessionStartServerTime = 0.0f;
	ActiveSessionDurationSeconds = 0.0f;
	LastCleanupReason = Reason;
	if (!bPreserveResult)
	{
		ResetPayloadState(false);
	}
	ResetPreparedTemplate();
	if (!bPreserveResult)
	{
		ResetAuthoritativeResult();
	}
	++SessionRevision;
	if (GetOwner())
	{
		GetOwner()->ForceNetUpdate();
	}
	UHeistDebugFunctionLibrary::DebugObjectAssemblySessionCleared(this, PreviousDisplayCase, Reason, bPreserveResult);
	SessionStateChangedDelegate.Broadcast();
}

void UHeistObjectAssemblyComponent::BroadcastSessionSnapshot(const FName EventName, const FName Reason, const bool bResult)
{
	UHeistDebugFunctionLibrary::DebugObjectAssemblySessionSnapshot(this, EventName, Reason, bResult);
	SessionStateChangedDelegate.Broadcast();
}

void UHeistObjectAssemblyComponent::HandleDisplayCaseSessionChanged(AHeistPlayerState* SessionOwner, const bool bLocked, const int32 /*Revision*/)
{
	if (bHandlingCaseSessionCallback || !GetOwner() || !GetOwner()->HasAuthority() || !IsValid(ActiveDisplayCase.Get()))
	{
		return;
	}

	const AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	const AHeistPlayerState* OwnerPlayerState = IsValid(HeistCharacter) ? HeistCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	if (!bLocked || SessionOwner != OwnerPlayerState)
	{
		ClearSession(FName(TEXT("CaseSessionReleased")), false, false);
	}
}

void UHeistObjectAssemblyComponent::OnRep_SessionRevision()
{
	SessionStateChangedDelegate.Broadcast();
}

void UHeistObjectAssemblyComponent::OnRep_PayloadValidationRevision()
{
	UHeistDebugFunctionLibrary::DebugObjectAssemblyPayloadValidation(this, bLastPayloadAccepted, LastPayloadReason, ValidatedEntryCount, ValidatedPayloadBytes);
	SessionStateChangedDelegate.Broadcast();
}

void UHeistObjectAssemblyComponent::OnRep_ScoreRevision()
{
	UHeistDebugFunctionLibrary::DebugObjectAssemblyScoreCommitted(this, AuthoritativeResult);
	SessionStateChangedDelegate.Broadcast();
}

void UHeistObjectAssemblyComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UHeistObjectAssemblyComponent, ActiveDisplayCase, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistObjectAssemblyComponent, bSessionActive, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistObjectAssemblyComponent, SessionEndServerTime, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistObjectAssemblyComponent, SessionRevision, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistObjectAssemblyComponent, ActiveArtifactId, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistObjectAssemblyComponent, ActiveTemplateId, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistObjectAssemblyComponent, ActiveFamilyId, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistObjectAssemblyComponent, bLastPayloadAccepted, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistObjectAssemblyComponent, LastPayloadReason, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistObjectAssemblyComponent, PayloadValidationRevision, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistObjectAssemblyComponent, ValidatedEntryCount, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistObjectAssemblyComponent, ValidatedPayloadBytes, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistObjectAssemblyComponent, bHasAuthoritativeResult, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistObjectAssemblyComponent, AuthoritativeResult, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHeistObjectAssemblyComponent, ScoreRevision, COND_OwnerOnly);
}
