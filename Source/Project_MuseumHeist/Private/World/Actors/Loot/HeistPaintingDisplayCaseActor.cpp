#include "World/Actors/Loot/HeistPaintingDisplayCaseActor.h"

#include "Character/HeistPlayerCharacter.h"
#include "Character/Components/HeistInventoryComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistGameplayTags.h"
#include "Core/HeistLogChannels.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerState.h"
#include "Data/HeistArtifactDataTypes.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "World/Actors/Loot/HeistDroppedOriginalActor.h"

namespace
{
constexpr int32 ReplicaTierPoor = 0;
constexpr int32 ReplicaTierFair = 1;
constexpr int32 ReplicaTierGood = 2;
constexpr int32 ReplicaTierExcellent = 3;
constexpr int32 ReplicaPaintingResolution = 256;
constexpr int32 ReplicaPaintingMaximumPaletteColors = 8;
constexpr uint32 ReplicaPaintingMaximumPackedBytes = (ReplicaPaintingResolution * ReplicaPaintingResolution + 1) / 2;

constexpr int32 ReplicaScorePrimitiveDataIndex = 0;
constexpr int32 ReplicaCoveragePrimitiveDataIndex = 1;
constexpr int32 ReplicaColorAccuracyPrimitiveDataIndex = 2;
constexpr int32 ReplicaTierPrimitiveDataIndex = 3;
constexpr float PaintingReplicaSwapMinimumInspectionDelaySeconds = 3.0f;
}

bool FHeistReplicaPaintingData::NetSerialize(FArchive& Ar, UPackageMap*, bool& bOutSuccess)
{
	static_assert(sizeof(FColor) == 4, "Replica palette network serialization requires four-byte FColor values.");

	const bool bClearedPayload = Resolution == 0 && Palette.IsEmpty() && PackedPaletteIndices.IsEmpty();
	const bool bCompletePayload = Resolution == ReplicaPaintingResolution && FMath::IsWithinInclusive(Palette.Num(), 2, ReplicaPaintingMaximumPaletteColors) &&
		PackedPaletteIndices.Num() == static_cast<int32>(ReplicaPaintingMaximumPackedBytes);
	if (Ar.IsSaving() && !bClearedPayload && !bCompletePayload)
	{
		Ar.SetError();
		bOutSuccess = false;
		return true;
	}

	uint32 SerializedResolution = Ar.IsSaving() ? static_cast<uint32>(Resolution) : 0;
	uint32 SerializedPaletteCount = Ar.IsSaving() ? static_cast<uint32>(Palette.Num()) : 0;
	uint32 SerializedPackedByteCount = Ar.IsSaving() ? static_cast<uint32>(PackedPaletteIndices.Num()) : 0;
	uint32 SerializedRevision = Ar.IsSaving() ? static_cast<uint32>(FMath::Max(0, Revision)) : 0;
	Ar.SerializeIntPacked(SerializedResolution);
	Ar.SerializeIntPacked(SerializedPaletteCount);
	Ar.SerializeIntPacked(SerializedPackedByteCount);
	Ar.SerializeIntPacked(SerializedRevision);

	if (Ar.IsLoading())
	{
		const bool bClearedHeader = SerializedResolution == 0 && SerializedPaletteCount == 0 && SerializedPackedByteCount == 0;
		const bool bCompleteHeader = SerializedResolution == ReplicaPaintingResolution &&
			FMath::IsWithinInclusive(SerializedPaletteCount, 2U, static_cast<uint32>(ReplicaPaintingMaximumPaletteColors)) &&
			SerializedPackedByteCount == ReplicaPaintingMaximumPackedBytes;
		if ((!bClearedHeader && !bCompleteHeader) || SerializedRevision > static_cast<uint32>(MAX_int32))
		{
			Ar.SetError();
			bOutSuccess = false;
			return true;
		}

		Resolution = static_cast<int32>(SerializedResolution);
		Revision = static_cast<int32>(SerializedRevision);
		Palette.SetNumUninitialized(static_cast<int32>(SerializedPaletteCount));
		PackedPaletteIndices.SetNumUninitialized(static_cast<int32>(SerializedPackedByteCount));
	}

	if (!Palette.IsEmpty())
	{
		Ar.Serialize(Palette.GetData(), static_cast<int64>(Palette.Num()) * sizeof(FColor));
	}
	if (!PackedPaletteIndices.IsEmpty())
	{
		Ar.Serialize(PackedPaletteIndices.GetData(), PackedPaletteIndices.Num());
	}

	bOutSuccess = !Ar.IsError();
	return true;
}

const FName AHeistPaintingDisplayCaseActor::OriginalVisualComponentTag(TEXT("OriginalVisual"));
const FName AHeistPaintingDisplayCaseActor::ReplicaVisualComponentTag(TEXT("ReplicaVisual"));

AHeistPaintingDisplayCaseActor::AHeistPaintingDisplayCaseActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	OriginalVisualComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OriginalVisualComponent"));
	OriginalVisualComponent->SetupAttachment(VisualMeshComponent);
	OriginalVisualComponent->ComponentTags.Add(OriginalVisualComponentTag);
	OriginalVisualComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OriginalVisualComponent->SetGenerateOverlapEvents(false);
	OriginalVisualComponent->SetCanEverAffectNavigation(false);

	ReplicaVisualComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ReplicaVisualComponent"));
	ReplicaVisualComponent->SetupAttachment(VisualMeshComponent);
	ReplicaVisualComponent->ComponentTags.Add(ReplicaVisualComponentTag);
	ReplicaVisualComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ReplicaVisualComponent->SetGenerateOverlapEvents(false);
	ReplicaVisualComponent->SetCanEverAffectNavigation(false);
	ReplicaVisualComponent->SetVisibility(false);
	ReplicaVisualComponent->SetHiddenInGame(true);
}

void AHeistPaintingDisplayCaseActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyContractExhibitActiveState();

	CaptureReplicaVisualBaseline();
	OriginalPaintingBaselineMaterial = IsValid(OriginalVisualComponent) ? OriginalVisualComponent->GetMaterial(0) : nullptr;
	RefreshPlaceholderVisualState();
	RefreshInspectionRegistration();
	RefreshReplicaWorldVisual();
	RefreshReplicaReviewWorldFeedback();

	BoundGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (BoundGameState.IsValid())
	{
		SurfaceTemplateSelectionChangedHandle =
			BoundGameState->GetSurfaceTemplateSelectionChangedDelegate().AddUObject(this, &AHeistPaintingDisplayCaseActor::HandleSurfaceTemplateSelectionChanged);
		ObjectiveStateChangedHandle =
			BoundGameState->GetObjectiveStateChangedDelegate().AddUObject(this, &AHeistPaintingDisplayCaseActor::HandleObjectiveStateChanged);
		if (BoundGameState->GetSurfaceTemplateSelectionRevision() > 0)
		{
			HandleSurfaceTemplateSelectionChanged(BoundGameState->GetSurfaceTemplatePoolId(), BoundGameState->GetSelectedSurfaceTemplateId(),
												 BoundGameState->GetSurfaceTemplateSelectionRevision());
		}
		if (HasAuthority())
		{
			MatchPhaseChangedHandle = BoundGameState->GetMatchPhaseChangedDelegate().AddUObject(this, &AHeistPaintingDisplayCaseActor::HandleMatchPhaseChanged);
		}
	}
}

void AHeistPaintingDisplayCaseActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearInspectionDelayTimer();
	if (HasAuthority() && bRegisteredForInspection)
	{
		bRegisteredForInspection = false;
		++InspectionRegistrationRevision;
		UE_LOG(LogHeistNetwork, Log, TEXT("Inspection target registration changed: Case=%s CaseId=%s Registered=false Revision=%d Reason=CaseEndPlay Authority=true"), *GetNameSafe(this),
			   *DisplayCaseId.ToString(), InspectionRegistrationRevision);
	}

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
	if (BoundGameState.IsValid() && SurfaceTemplateSelectionChangedHandle.IsValid())
	{
		BoundGameState->GetSurfaceTemplateSelectionChangedDelegate().Remove(SurfaceTemplateSelectionChangedHandle);
	}
	if (BoundGameState.IsValid() && ObjectiveStateChangedHandle.IsValid())
	{
		BoundGameState->GetObjectiveStateChangedDelegate().Remove(ObjectiveStateChangedHandle);
	}
	MatchPhaseChangedHandle.Reset();
	SurfaceTemplateSelectionChangedHandle.Reset();
	ObjectiveStateChangedHandle.Reset();
	BoundGameState.Reset();
	OriginalPaintingDynamicMaterial = nullptr;
	OriginalPaintingBaselineMaterial = nullptr;
	ResetReplicaPaintingResources();

	Super::EndPlay(EndPlayReason);
}

bool AHeistPaintingDisplayCaseActor::CanInteract(const AActor* Interactor) const
{
	const bool bDefaultInteraction = !bSessionLocked && (DisplayCaseState == EHeistDisplayCaseState::Secured || DisplayCaseState == EHeistDisplayCaseState::OriginalAvailable);
	return bContractExhibitActive && Super::CanInteract(Interactor) && (bDefaultInteraction || IsReplicaReviewReadyFor(Interactor));
}

bool AHeistPaintingDisplayCaseActor::IsContractExhibitActive() const
{
	return bContractExhibitActive;
}

bool AHeistPaintingDisplayCaseActor::SetContractExhibitActive(const bool bActive)
{
	if (!HasAuthority())
	{
		return false;
	}

	bContractExhibitActive = bActive;
	ApplyContractExhibitActiveState();
	ForceNetUpdate();
	return true;
}

void AHeistPaintingDisplayCaseActor::ApplyContractExhibitActiveState()
{
	SetActorHiddenInGame(!bContractExhibitActive);
	SetActorEnableCollision(bContractExhibitActive);
	SetActorTickEnabled(bContractExhibitActive);
}

void AHeistPaintingDisplayCaseActor::OnRep_ContractExhibitActive()
{
	ApplyContractExhibitActiveState();
}

#pragma region StateMachine

EHeistDisplayCaseState AHeistPaintingDisplayCaseActor::GetDisplayCaseState() const
{
	return DisplayCaseState;
}

bool AHeistPaintingDisplayCaseActor::ShouldDisplayOriginalPlaceholder() const
{
	return ShouldDisplayOriginalPlaceholderForState(DisplayCaseState);
}

bool AHeistPaintingDisplayCaseActor::ShouldDisplayReplicaPlaceholder() const
{
	return ShouldDisplayReplicaPlaceholderForState(DisplayCaseState);
}

void AHeistPaintingDisplayCaseActor::GetPlaceholderVisualDebugState(bool& OutExpectedOriginalVisible, bool& OutExpectedReplicaVisible, int32& OutOriginalComponentCount,
																	int32& OutReplicaComponentCount, bool& OutComponentsMatchExpectedState) const
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
			OutComponentsMatchExpectedState &= PrimitiveComponent->IsVisible() == OutExpectedOriginalVisible && (!PrimitiveComponent->bHiddenInGame) == OutExpectedOriginalVisible;
		}

		if (PrimitiveComponent->ComponentHasTag(ReplicaVisualComponentTag))
		{
			++OutReplicaComponentCount;
			OutComponentsMatchExpectedState &= PrimitiveComponent->IsVisible() == OutExpectedReplicaVisible && (!PrimitiveComponent->bHiddenInGame) == OutExpectedReplicaVisible;
		}
	}

	OutComponentsMatchExpectedState &= OutOriginalComponentCount == 1 && OutReplicaComponentCount == 1;
}

bool AHeistPaintingDisplayCaseActor::CanTransitionToDisplayCaseState(const EHeistDisplayCaseState NewState) const
{
	EHeistDisplayCaseState ExpectedNextState = DisplayCaseState;
	return TryGetNextDisplayCaseState(DisplayCaseState, ExpectedNextState) && ExpectedNextState == NewState;
}

bool AHeistPaintingDisplayCaseActor::TryTransitionToDisplayCaseState(const EHeistDisplayCaseState NewState)
{
	if (!HasAuthority())
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Display case transition rejected: Case=%s Current=%s Requested=%s Reason=NotAuthority"), *GetNameSafe(this), *UEnum::GetValueAsString(DisplayCaseState),
			   *UEnum::GetValueAsString(NewState));
		return false;
	}

	if (!CanTransitionToDisplayCaseState(NewState))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Display case transition rejected: Case=%s Current=%s Requested=%s Reason=IllegalTransition"), *GetNameSafe(this),
			   *UEnum::GetValueAsString(DisplayCaseState), *UEnum::GetValueAsString(NewState));
		return false;
	}

	const EHeistDisplayCaseState PreviousState = DisplayCaseState;
	DisplayCaseState = NewState;
	HandleDisplayCaseStateChanged(PreviousState);
	ForceNetUpdate();
	return true;
}

bool AHeistPaintingDisplayCaseActor::TryAdvanceDisplayCaseState()
{
	EHeistDisplayCaseState NextState = DisplayCaseState;
	return TryGetNextDisplayCaseState(DisplayCaseState, NextState) && TryTransitionToDisplayCaseState(NextState);
}

bool AHeistPaintingDisplayCaseActor::ResetForgerySessionState(const FName Reason)
{
	if (!HasAuthority() || (DisplayCaseState != EHeistDisplayCaseState::Observed && DisplayCaseState != EHeistDisplayCaseState::ForgeryInProgress &&
						 DisplayCaseState != EHeistDisplayCaseState::ReplicaReady))
	{
		return false;
	}

	const EHeistDisplayCaseState PreviousState = DisplayCaseState;
	ResetReplicaPreviewData();
	DisplayCaseState = EHeistDisplayCaseState::Secured;
	HandleDisplayCaseStateChanged(PreviousState);
	ForceNetUpdate();

	UE_LOG(LogHeistNetwork, Log, TEXT("Display case forgery state reset: Case=%s Previous=%s New=%s Reason=%s"), *GetNameSafe(this), *UEnum::GetValueAsString(PreviousState),
		   *UEnum::GetValueAsString(DisplayCaseState), Reason.IsNone() ? TEXT("None") : *Reason.ToString());
	return true;
}

void AHeistPaintingDisplayCaseActor::OnRep_DisplayCaseState(const EHeistDisplayCaseState PreviousState)
{
	HandleDisplayCaseStateChanged(PreviousState);
}

bool AHeistPaintingDisplayCaseActor::TryGetNextDisplayCaseState(const EHeistDisplayCaseState CurrentState, EHeistDisplayCaseState& OutNextState)
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

bool AHeistPaintingDisplayCaseActor::ShouldDisplayOriginalPlaceholderForState(const EHeistDisplayCaseState State)
{
	switch (State)
	{
	case EHeistDisplayCaseState::Secured:
	case EHeistDisplayCaseState::Observed:
	case EHeistDisplayCaseState::ForgeryInProgress:
	case EHeistDisplayCaseState::ReplicaReady:
		return true;
	default:
		return false;
	}
}

bool AHeistPaintingDisplayCaseActor::ShouldDisplayReplicaPlaceholderForState(const EHeistDisplayCaseState State)
{
	switch (State)
	{
	case EHeistDisplayCaseState::ReplicaPlaced:
	case EHeistDisplayCaseState::OriginalAvailable:
	case EHeistDisplayCaseState::OriginalRemoved:
	case EHeistDisplayCaseState::Inspecting:
	case EHeistDisplayCaseState::Completed:
	case EHeistDisplayCaseState::Failed:
		return true;
	default:
		return false;
	}
}

void AHeistPaintingDisplayCaseActor::HandleDisplayCaseStateChanged(const EHeistDisplayCaseState PreviousState)
{
	RefreshPlaceholderVisualState();
	RefreshInspectionRegistration();
	RefreshReplicaReviewWorldFeedback();

	UE_LOG(LogHeistNetwork, Log, TEXT("Display case state changed: Case=%s Previous=%s New=%s Authority=%s"), *GetNameSafe(this), *UEnum::GetValueAsString(PreviousState),
		   *UEnum::GetValueAsString(DisplayCaseState), HasAuthority() ? TEXT("true") : TEXT("false"));

	OnDisplayCaseStateChanged.Broadcast(PreviousState, DisplayCaseState);
}

void AHeistPaintingDisplayCaseActor::RefreshPlaceholderVisualState()
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

	UE_LOG(LogHeistNetwork, Log,
		   TEXT("Display case placeholder visual applied: Case=%s State=%s OriginalVisible=%s ReplicaVisible=%s OriginalComponents=%d ReplicaComponents=%d Authority=%s Result=%s"), *GetNameSafe(this),
		   *UEnum::GetValueAsString(DisplayCaseState), bOriginalVisible ? TEXT("true") : TEXT("false"), bReplicaVisible ? TEXT("true") : TEXT("false"), OriginalComponentCount, ReplicaComponentCount,
		   HasAuthority() ? TEXT("true") : TEXT("false"), OriginalComponentCount == 1 && ReplicaComponentCount == 1 ? TEXT("PASS") : TEXT("INVALID_COMPONENT_COUNT"));
}

#pragma endregion

#pragma region OriginalPaintingVisual

FName AHeistPaintingDisplayCaseActor::GetOriginalVisualTemplateId() const
{
	return OriginalVisualTemplateId;
}

int32 AHeistPaintingDisplayCaseActor::GetOriginalVisualRevision() const
{
	return OriginalVisualRevision;
}

void AHeistPaintingDisplayCaseActor::GetOriginalPaintingVisualDebugState(FName& OutTemplateId, int32& OutRevision, bool& OutReferenceLoaded, bool& OutDynamicMaterialBuilt,
																		 bool& OutTextureParameterApplied, bool& OutContractPassed) const
{
	OutTemplateId = OriginalVisualTemplateId;
	OutRevision = OriginalVisualRevision;
	OutReferenceLoaded = OriginalReferenceImage.IsValid();
	OutDynamicMaterialBuilt = IsValid(OriginalPaintingDynamicMaterial);
	OutTextureParameterApplied = bOriginalPaintingTextureParameterApplied;
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	OutContractPassed =
		IsActiveObjectiveTargetCase() && IsValid(HeistGameState) && !OutTemplateId.IsNone() && OutTemplateId == HeistGameState->GetSelectedSurfaceTemplateId() &&
		OutRevision > 0 && OutRevision == HeistGameState->GetSurfaceTemplateSelectionRevision() && AppliedOriginalVisualRevision == OutRevision &&
		OutReferenceLoaded && OutDynamicMaterialBuilt && OutTextureParameterApplied;
}

void AHeistPaintingDisplayCaseActor::HandleSurfaceTemplateSelectionChanged(const FName PoolId, const FName TemplateId, const int32 SelectionRevision)
{
	if (PoolId.IsNone() || TemplateId.IsNone() || SelectionRevision <= 0)
	{
		if (HasAuthority())
		{
			OriginalVisualTemplateId = NAME_None;
			OriginalReferenceImage.Reset();
			OriginalVisualRevision = 0;
			ForceNetUpdate();
		}
		ResetOriginalPaintingVisual();
		return;
	}

	if (!IsActiveObjectiveTargetCase())
	{
		if (HasAuthority() && (!OriginalVisualTemplateId.IsNone() || !OriginalReferenceImage.IsNull() || OriginalVisualRevision > 0))
		{
			OriginalVisualTemplateId = NAME_None;
			OriginalReferenceImage.Reset();
			OriginalVisualRevision = 0;
			ForceNetUpdate();
		}
		ResetOriginalPaintingVisual();
		return;
	}

	if (HasAuthority())
	{
		const AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
		FHeistForgeryTemplateRow TemplateDefinition;
		if (!IsValid(HeistGameMode) || !HeistGameMode->TryGetForgeryTemplateDefinition(TemplateId, TemplateDefinition) ||
			(!TemplateDefinition.SurfacePoolId.IsNone() && TemplateDefinition.SurfacePoolId != PoolId) || TemplateDefinition.ReferenceImage.IsNull())
		{
			return;
		}

		OriginalVisualTemplateId = TemplateId;
		OriginalReferenceImage = TemplateDefinition.ReferenceImage;
		OriginalVisualRevision = SelectionRevision;
		ForceNetUpdate();
	}

	RefreshOriginalPaintingVisual();
}

void AHeistPaintingDisplayCaseActor::HandleObjectiveStateChanged(const FName ActiveTargetArtifactId, const FName ActiveTargetCaseId, const EHeistObjectiveState,
																AHeistPlayerState*)
{
	if (ActiveTargetCaseId == DisplayCaseId && (ActiveTargetArtifactId.IsNone() || ActiveTargetArtifactId == TargetArtifactId) && BoundGameState.IsValid())
	{
		HandleSurfaceTemplateSelectionChanged(BoundGameState->GetSurfaceTemplatePoolId(), BoundGameState->GetSelectedSurfaceTemplateId(),
											 BoundGameState->GetSurfaceTemplateSelectionRevision());
		return;
	}

	HandleSurfaceTemplateSelectionChanged(NAME_None, NAME_None, 0);
}

bool AHeistPaintingDisplayCaseActor::IsActiveObjectiveTargetCase() const
{
	const AHeistGameState* HeistGameState = BoundGameState.IsValid() ? BoundGameState.Get() : (GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr);
	return IsValid(HeistGameState) && HeistGameState->GetActiveTargetCaseId() == DisplayCaseId &&
		   (HeistGameState->GetActiveTargetArtifactId().IsNone() || HeistGameState->GetActiveTargetArtifactId() == TargetArtifactId);
}

void AHeistPaintingDisplayCaseActor::RefreshOriginalPaintingVisual()
{
	if (!IsActiveObjectiveTargetCase() || !IsValid(OriginalVisualComponent) || OriginalVisualTemplateId.IsNone() || OriginalVisualRevision <= 0 || OriginalReferenceImage.IsNull())
	{
		ResetOriginalPaintingVisual();
		return;
	}
	if (AppliedOriginalVisualRevision == OriginalVisualRevision && IsValid(OriginalPaintingDynamicMaterial) && bOriginalPaintingTextureParameterApplied)
	{
		return;
	}
	bOriginalPaintingTextureParameterApplied = false;

	UTexture2D* ReferenceTexture = OriginalReferenceImage.LoadSynchronous();
	UMaterialInterface* MaterialSource = IsValid(OriginalPaintingMaterial) ? OriginalPaintingMaterial.Get()
																		 : (IsValid(ReplicaPaintingMaterial) ? ReplicaPaintingMaterial.Get() : OriginalPaintingBaselineMaterial.Get());
	if (!IsValid(ReferenceTexture) || !IsValid(MaterialSource))
	{
		return;
	}

	OriginalPaintingDynamicMaterial = UMaterialInstanceDynamic::Create(MaterialSource, this);
	if (!IsValid(OriginalPaintingDynamicMaterial))
	{
		return;
	}

	OriginalPaintingDynamicMaterial->SetTextureParameterValue(OriginalPaintingTextureParameter, ReferenceTexture);
	bOriginalPaintingTextureParameterApplied = OriginalPaintingDynamicMaterial->K2_GetTextureParameterValue(OriginalPaintingTextureParameter) == ReferenceTexture;
	if (bOriginalPaintingTextureParameterApplied)
	{
		OriginalVisualComponent->SetMaterial(0, OriginalPaintingDynamicMaterial);
		AppliedOriginalVisualRevision = OriginalVisualRevision;
	}
	BP_ApplyOriginalPaintingVisual(OriginalVisualTemplateId, ReferenceTexture, bOriginalPaintingTextureParameterApplied);
}

void AHeistPaintingDisplayCaseActor::ResetOriginalPaintingVisual()
{
	bOriginalPaintingTextureParameterApplied = false;
	AppliedOriginalVisualRevision = 0;
	OriginalPaintingDynamicMaterial = nullptr;
	if (IsValid(OriginalVisualComponent) && IsValid(OriginalPaintingBaselineMaterial))
	{
		OriginalVisualComponent->SetMaterial(0, OriginalPaintingBaselineMaterial.Get());
	}
	BP_ApplyOriginalPaintingVisual(NAME_None, nullptr, false);
}

void AHeistPaintingDisplayCaseActor::OnRep_OriginalVisualRevision()
{
	if (OriginalVisualRevision > 0)
	{
		RefreshOriginalPaintingVisual();
	}
	else
	{
		ResetOriginalPaintingVisual();
	}
}

#pragma endregion

#pragma region ReplicaPlacement

bool AHeistPaintingDisplayCaseActor::HasCommittedForgeryResult() const
{
	return bHasCommittedForgeryResult && CommittedForgeryResult.bReplicaPlaced;
}

bool AHeistPaintingDisplayCaseActor::HasReplicaPreview() const
{
	return bHasCommittedForgeryResult && !CommittedForgeryResult.bReplicaPlaced && DisplayCaseState == EHeistDisplayCaseState::ReplicaReady;
}

bool AHeistPaintingDisplayCaseActor::IsReplicaReviewReadyFor(const AActor* Interactor) const
{
	const AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(Interactor);
	const AHeistPlayerState* InteractorPlayerState = IsValid(HeistCharacter) ? HeistCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	return IsValid(InteractorPlayerState) && HasReplicaPreview() && bSessionLocked && SessionOwner.Get() == InteractorPlayerState;
}

bool AHeistPaintingDisplayCaseActor::IsUntouchedForWorldFeedback() const
{
	return DisplayCaseState == EHeistDisplayCaseState::Secured && !bSessionLocked && !bHasCommittedForgeryResult;
}

FHeistForgeryResult AHeistPaintingDisplayCaseActor::GetCommittedForgeryResult() const
{
	return CommittedForgeryResult;
}

int32 AHeistPaintingDisplayCaseActor::GetCommittedForgeryRevision() const
{
	return CommittedForgeryRevision;
}

int32 AHeistPaintingDisplayCaseActor::GetReplicaVisualTier() const
{
	return bHasCommittedForgeryResult ? ResolveReplicaVisualTier(CommittedForgeryResult.SimilarityScore) : INDEX_NONE;
}

FName AHeistPaintingDisplayCaseActor::GetReplicaVisualTierName() const
{
	return ResolveReplicaVisualTierName(GetReplicaVisualTier());
}

bool AHeistPaintingDisplayCaseActor::IsReplicaWorldVisualReady() const
{
	bool bReplicaExpectedVisible = false;
	bool bHasReplicaMesh = false;
	int32 ExpectedTier = INDEX_NONE;
	int32 AppliedTier = INDEX_NONE;
	FName TierName = NAME_None;
	bool bUsingTierMaterial = false;
	bool bUsingTransformFallback = false;
	bool bCustomPrimitiveDataApplied = false;
	bool bContractPassed = false;
	GetReplicaWorldVisualDebugState(bReplicaExpectedVisible, bHasReplicaMesh, ExpectedTier, AppliedTier, TierName, bUsingTierMaterial, bUsingTransformFallback, bCustomPrimitiveDataApplied,
									bContractPassed);
	return bContractPassed;
}

void AHeistPaintingDisplayCaseActor::GetReplicaWorldVisualDebugState(bool& OutReplicaExpectedVisible, bool& OutHasReplicaMesh, int32& OutExpectedTier, int32& OutAppliedTier, FName& OutTierName,
																	 bool& OutUsingTierMaterial, bool& OutUsingTransformFallback, bool& OutCustomPrimitiveDataApplied, bool& OutContractPassed) const
{
	OutReplicaExpectedVisible = ShouldDisplayReplicaPlaceholder();
	OutHasReplicaMesh = IsValid(ReplicaVisualComponent) && IsValid(ReplicaVisualComponent->GetStaticMesh());
	OutExpectedTier = GetReplicaVisualTier();
	OutAppliedTier = AppliedReplicaVisualTier;
	OutTierName = ResolveReplicaVisualTierName(OutExpectedTier);
	OutUsingTierMaterial = bUsingReplicaTierMaterial;
	OutUsingTransformFallback = bUsingReplicaTransformFallback;
	OutCustomPrimitiveDataApplied = bReplicaVisualCustomPrimitiveDataApplied;

	const bool bPreviewOrCommittedResultValid = bHasCommittedForgeryResult && FMath::IsWithinInclusive(CommittedForgeryResult.SimilarityScore, 0.0f, 100.0f) &&
		(CommittedForgeryResult.bReplicaPlaced || DisplayCaseState == EHeistDisplayCaseState::ReplicaReady);
	const bool bTierValid = FMath::IsWithinInclusive(OutExpectedTier, ReplicaTierPoor, ReplicaTierExcellent) && OutAppliedTier == OutExpectedTier && !OutTierName.IsNone();
	const bool bVisibleStateMatches =
		IsValid(ReplicaVisualComponent) && ReplicaVisualComponent->IsVisible() == OutReplicaExpectedVisible && (!ReplicaVisualComponent->bHiddenInGame) == OutReplicaExpectedVisible;
	const bool bPresentationPathValid = IsValid(ReplicaPaintingMaterial) ? bReplicaPaintingTextureParameterApplied : OutUsingTierMaterial || OutUsingTransformFallback;
	const bool bStateVisibilityContract = HasReplicaPreview() ? !OutReplicaExpectedVisible : OutReplicaExpectedVisible;

	OutContractPassed = bPreviewOrCommittedResultValid && bStateVisibilityContract && OutHasReplicaMesh && bTierValid && bVisibleStateMatches && bPresentationPathValid && OutCustomPrimitiveDataApplied;
}

bool AHeistPaintingDisplayCaseActor::HasReplicaPaintingData() const
{
	FName RejectReason = NAME_None;
	return ValidateReplicaPaintingData(ReplicaPaintingData, RejectReason) && ReplicaPaintingData.Revision == CommittedForgeryRevision;
}

FHeistReplicaPaintingData AHeistPaintingDisplayCaseActor::GetReplicaPaintingData() const
{
	return ReplicaPaintingData;
}

int32 AHeistPaintingDisplayCaseActor::GetReplicaPaintingRevision() const
{
	return ReplicaPaintingData.Revision;
}

UTexture2D* AHeistPaintingDisplayCaseActor::GetReplicaPaintingTexture() const
{
	return ReplicaPaintingTexture.Get();
}

void AHeistPaintingDisplayCaseActor::GetReplicaPaintingDebugState(int32& OutResolution, int32& OutPaletteColorCount, int32& OutPackedByteCount, int32& OutPaintingRevision, bool& OutTextureBuilt,
																  bool& OutDynamicMaterialBuilt, bool& OutTextureParameterApplied, bool& OutContractPassed) const
{
	OutResolution = ReplicaPaintingData.Resolution;
	OutPaletteColorCount = ReplicaPaintingData.Palette.Num();
	OutPackedByteCount = ReplicaPaintingData.PackedPaletteIndices.Num();
	OutPaintingRevision = ReplicaPaintingData.Revision;
	OutTextureBuilt = IsValid(ReplicaPaintingTexture);
	OutDynamicMaterialBuilt = IsValid(ReplicaPaintingDynamicMaterial);
	OutTextureParameterApplied = bReplicaPaintingTextureParameterApplied;

	FName RejectReason = NAME_None;
	const bool bDataValid = ValidateReplicaPaintingData(ReplicaPaintingData, RejectReason);
	OutContractPassed = bHasCommittedForgeryResult && (CommittedForgeryResult.bReplicaPlaced || DisplayCaseState == EHeistDisplayCaseState::ReplicaReady) && bDataValid &&
		OutPaintingRevision == CommittedForgeryRevision &&
						AppliedReplicaPaintingRevision == OutPaintingRevision && OutTextureBuilt && OutDynamicMaterialBuilt && OutTextureParameterApplied;
}

void AHeistPaintingDisplayCaseActor::CaptureReplicaVisualBaseline()
{
	if (bReplicaVisualBaselineCaptured || !IsValid(ReplicaVisualComponent))
	{
		return;
	}

	ReplicaBaselineRelativeTransform = ReplicaVisualComponent->GetRelativeTransform();
	ReplicaBaselineMaterial = ReplicaVisualComponent->GetMaterial(0);
	bReplicaVisualBaselineCaptured = true;
}

void AHeistPaintingDisplayCaseActor::RefreshReplicaWorldVisual()
{
	if (!IsValid(ReplicaVisualComponent))
	{
		return;
	}

	CaptureReplicaVisualBaseline();
	AppliedReplicaVisualTier = INDEX_NONE;
	bUsingReplicaTierMaterial = false;
	bUsingReplicaTransformFallback = false;
	bReplicaVisualCustomPrimitiveDataApplied = false;
	bReplicaPaintingTextureParameterApplied = false;

	if (!bHasCommittedForgeryResult)
	{
		ResetReplicaPaintingResources();
		if (bReplicaVisualBaselineCaptured)
		{
			ReplicaVisualComponent->SetRelativeTransform(ReplicaBaselineRelativeTransform);
			ReplicaVisualComponent->SetMaterial(0, ReplicaBaselineMaterial.Get());
		}
		return;
	}

	AppliedReplicaVisualTier = ResolveReplicaVisualTier(CommittedForgeryResult.SimilarityScore);
	UMaterialInterface* TierMaterial = ResolveReplicaTierMaterial(AppliedReplicaVisualTier);
	const bool bHasPaintingMaterial = IsValid(ReplicaPaintingMaterial);
	bUsingReplicaTierMaterial = !bHasPaintingMaterial && IsValid(TierMaterial);
	bUsingReplicaTransformFallback = !bHasPaintingMaterial && !bUsingReplicaTierMaterial;

	if (bHasPaintingMaterial)
	{
		ReplicaVisualComponent->SetMaterial(0, ReplicaPaintingMaterial.Get());
		ReplicaVisualComponent->SetRelativeTransform(ReplicaBaselineRelativeTransform);
	}
	else if (bUsingReplicaTierMaterial)
	{
		ReplicaVisualComponent->SetMaterial(0, TierMaterial);
		ReplicaVisualComponent->SetRelativeTransform(ReplicaBaselineRelativeTransform);
	}
	else
	{
		ReplicaVisualComponent->SetMaterial(0, ReplicaBaselineMaterial.Get());

		float RollOffset = 0.0f;
		float UniformScaleMultiplier = 1.0f;
		switch (AppliedReplicaVisualTier)
		{
		case ReplicaTierPoor:
			RollOffset = -12.0f;
			UniformScaleMultiplier = 0.88f;
			break;
		case ReplicaTierFair:
			RollOffset = 8.0f;
			UniformScaleMultiplier = 0.94f;
			break;
		case ReplicaTierGood:
			RollOffset = -5.0f;
			UniformScaleMultiplier = 0.97f;
			break;
		default:
			break;
		}

		FTransform TierTransform = ReplicaBaselineRelativeTransform;
		FRotator TierRotation = TierTransform.Rotator();
		TierRotation.Roll += RollOffset;
		TierTransform.SetRotation(TierRotation.Quaternion());
		TierTransform.SetScale3D(ReplicaBaselineRelativeTransform.GetScale3D() * UniformScaleMultiplier);
		ReplicaVisualComponent->SetRelativeTransform(TierTransform);
	}

	ReplicaVisualComponent->SetCustomPrimitiveDataFloat(ReplicaScorePrimitiveDataIndex, FMath::Clamp(CommittedForgeryResult.SimilarityScore / 100.0f, 0.0f, 1.0f));
	ReplicaVisualComponent->SetCustomPrimitiveDataFloat(ReplicaCoveragePrimitiveDataIndex, FMath::Clamp(CommittedForgeryResult.CoverageScore / 100.0f, 0.0f, 1.0f));
	ReplicaVisualComponent->SetCustomPrimitiveDataFloat(ReplicaColorAccuracyPrimitiveDataIndex, FMath::Clamp(CommittedForgeryResult.ColorAccuracyScore / 100.0f, 0.0f, 1.0f));
	ReplicaVisualComponent->SetCustomPrimitiveDataFloat(ReplicaTierPrimitiveDataIndex, static_cast<float>(AppliedReplicaVisualTier));
	bReplicaVisualCustomPrimitiveDataApplied = true;

	const FName TierName = ResolveReplicaVisualTierName(AppliedReplicaVisualTier);
	BP_ApplyReplicaWorldVisual(AppliedReplicaVisualTier, TierName, CommittedForgeryResult.SimilarityScore, CommittedForgeryResult.CoverageScore, CommittedForgeryResult.ColorAccuracyScore,
							   CommittedForgeryResult.TemplateId, bUsingReplicaTierMaterial);
	// Blueprint may add frame polish or replace a presentation material.
	// Apply the authoritative painting texture last so the submitted image
	// remains the final material on the replica surface.
	RefreshReplicaPaintingTexture();

	UE_LOG(LogHeistNetwork, Log,
		TEXT(
			"Replica world visual applied: Case=%s Template=%s Score=%.2f Coverage=%.2f ColorAccuracy=%.2f Tier=%d TierName=%s TierMaterial=%s TransformFallback=%s CustomPrimitiveData=true PaintingResolution=%d PaintingPalette=%d PaintingBytes=%d PaintingRevision=%d PaintingTexture=%s PaintingMID=%s PaintingParameter=%s Authority=%s Result=PASS"),
		*GetNameSafe(this), *CommittedForgeryResult.TemplateId.ToString(), CommittedForgeryResult.SimilarityScore, CommittedForgeryResult.CoverageScore, CommittedForgeryResult.ColorAccuracyScore,
		AppliedReplicaVisualTier, *TierName.ToString(), bUsingReplicaTierMaterial ? TEXT("true") : TEXT("false"), bUsingReplicaTransformFallback ? TEXT("true") : TEXT("false"),
		ReplicaPaintingData.Resolution, ReplicaPaintingData.Palette.Num(), ReplicaPaintingData.PackedPaletteIndices.Num(), ReplicaPaintingData.Revision,
		IsValid(ReplicaPaintingTexture) ? TEXT("true") : TEXT("false"), IsValid(ReplicaPaintingDynamicMaterial) ? TEXT("true") : TEXT("false"),
		bReplicaPaintingTextureParameterApplied ? TEXT("true") : TEXT("false"), HasAuthority() ? TEXT("true") : TEXT("false"));
}

void AHeistPaintingDisplayCaseActor::RefreshReplicaPaintingTexture()
{
	if (!IsValid(ReplicaVisualComponent) || !HasReplicaPaintingData())
	{
		return;
	}

	if (!IsValid(ReplicaPaintingTexture) || AppliedReplicaPaintingRevision != ReplicaPaintingData.Revision)
	{
		ResetReplicaPaintingResources();
		ReplicaPaintingTexture = BuildReplicaPaintingTexture();
	}
	if (!IsValid(ReplicaPaintingTexture))
	{
		return;
	}
	if (!IsValid(ReplicaPaintingMaterial))
	{
		return;
	}

	ReplicaPaintingDynamicMaterial = UMaterialInstanceDynamic::Create(ReplicaPaintingMaterial, this);
	if (!IsValid(ReplicaPaintingDynamicMaterial))
	{
		return;
	}

	ReplicaPaintingDynamicMaterial->SetTextureParameterValue(ReplicaPaintingTextureParameter, ReplicaPaintingTexture);
	bReplicaPaintingTextureParameterApplied = ReplicaPaintingDynamicMaterial->K2_GetTextureParameterValue(ReplicaPaintingTextureParameter) == ReplicaPaintingTexture;
	ReplicaVisualComponent->SetMaterial(0, ReplicaPaintingDynamicMaterial);
	AppliedReplicaPaintingRevision = ReplicaPaintingData.Revision;
}

UTexture2D* AHeistPaintingDisplayCaseActor::BuildReplicaPaintingTexture() const
{
	FName RejectReason = NAME_None;
	if (!ValidateReplicaPaintingData(ReplicaPaintingData, RejectReason))
	{
		return nullptr;
	}

	const int32 PixelCount = ReplicaPaintingData.Resolution * ReplicaPaintingData.Resolution;
	TArray64<uint8> TextureBytes;
	TextureBytes.SetNumUninitialized(static_cast<int64>(PixelCount) * 4);
	FColor BackgroundColor = ReplicaPaintingBackgroundColor.ToFColorSRGB();
	BackgroundColor.A = 255;

	for (int32 PixelIndex = 0; PixelIndex < PixelCount; ++PixelIndex)
	{
		const uint8 PackedByte = ReplicaPaintingData.PackedPaletteIndices[PixelIndex / 2];
		const uint8 PaletteValue = (PixelIndex & 1) == 0 ? PackedByte & 0x0f : PackedByte >> 4;
		const FColor PixelColor = PaletteValue == 0 ? BackgroundColor : ReplicaPaintingData.Palette[PaletteValue - 1];
		const int64 ByteOffset = static_cast<int64>(PixelIndex) * 4;
		TextureBytes[ByteOffset] = PixelColor.B;
		TextureBytes[ByteOffset + 1] = PixelColor.G;
		TextureBytes[ByteOffset + 2] = PixelColor.R;
		TextureBytes[ByteOffset + 3] = PixelColor.A;
	}

	UTexture2D* NewTexture = UTexture2D::CreateTransient(ReplicaPaintingData.Resolution, ReplicaPaintingData.Resolution, PF_B8G8R8A8, NAME_None, TextureBytes);
	if (!IsValid(NewTexture))
	{
		return nullptr;
	}

	NewTexture->SRGB = true;
	NewTexture->Filter = TF_Bilinear;
	NewTexture->AddressX = TA_Clamp;
	NewTexture->AddressY = TA_Clamp;
	NewTexture->NeverStream = true;
	NewTexture->UpdateResource();
	return NewTexture;
}

void AHeistPaintingDisplayCaseActor::ResetReplicaPaintingResources()
{
	ReplicaPaintingDynamicMaterial = nullptr;
	ReplicaPaintingTexture = nullptr;
	AppliedReplicaPaintingRevision = 0;
	bReplicaPaintingTextureParameterApplied = false;
}

UMaterialInterface* AHeistPaintingDisplayCaseActor::ResolveReplicaTierMaterial(const int32 VisualTier) const
{
	switch (VisualTier)
	{
	case ReplicaTierPoor:
		return ReplicaPoorMaterial.Get();
	case ReplicaTierFair:
		return ReplicaFairMaterial.Get();
	case ReplicaTierGood:
		return ReplicaGoodMaterial.Get();
	case ReplicaTierExcellent:
		return ReplicaExcellentMaterial.Get();
	default:
		return nullptr;
	}
}

int32 AHeistPaintingDisplayCaseActor::ResolveReplicaVisualTier(const float SimilarityScore)
{
	if (SimilarityScore < 25.0f)
	{
		return ReplicaTierPoor;
	}
	if (SimilarityScore < 50.0f)
	{
		return ReplicaTierFair;
	}
	if (SimilarityScore < 75.0f)
	{
		return ReplicaTierGood;
	}
	return ReplicaTierExcellent;
}

FName AHeistPaintingDisplayCaseActor::ResolveReplicaVisualTierName(const int32 VisualTier)
{
	switch (VisualTier)
	{
	case ReplicaTierPoor:
		return FName(TEXT("Poor"));
	case ReplicaTierFair:
		return FName(TEXT("Fair"));
	case ReplicaTierGood:
		return FName(TEXT("Good"));
	case ReplicaTierExcellent:
		return FName(TEXT("Excellent"));
	default:
		return NAME_None;
	}
}

bool AHeistPaintingDisplayCaseActor::TryStageReplicaPreview(AHeistPlayerState* RequestingPlayerState, const FHeistForgeryResult& ForgeryResult,
													 const FHeistReplicaPaintingData& PaintingData)
{
	if (!HasAuthority())
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Replica preview rejected: Case=%s Requester=%s Reason=NotAuthority"), *GetNameSafe(this), *GetNameSafe(RequestingPlayerState));
		return false;
	}

	FName RejectReason = NAME_None;
	if (!ValidateReplicaPlacementRequest(RequestingPlayerState, ForgeryResult, RejectReason) || !ValidateReplicaPaintingData(PaintingData, RejectReason))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Replica preview rejected: Case=%s CaseId=%s Artifact=%s Requester=%s State=%s Locked=%s ExistingResult=%s Reason=%s"), *GetNameSafe(this),
			   *DisplayCaseId.ToString(), *TargetArtifactId.ToString(), *GetNameSafe(RequestingPlayerState), *UEnum::GetValueAsString(DisplayCaseState), bSessionLocked ? TEXT("true") : TEXT("false"),
			   bHasCommittedForgeryResult ? TEXT("true") : TEXT("false"), *RejectReason.ToString());
		return false;
	}
	if (!TryTransitionToDisplayCaseState(EHeistDisplayCaseState::ReplicaReady))
	{
		UE_LOG(LogHeistNetwork, Error, TEXT("Replica preview rejected: Case=%s State=%s Reason=StateTransitionFailed"), *GetNameSafe(this), *UEnum::GetValueAsString(DisplayCaseState));
		return false;
	}

	CommittedForgeryResult = ForgeryResult;
	CommittedForgeryResult.bReplicaPlaced = false;
	bHasCommittedForgeryResult = true;
	++CommittedForgeryRevision;
	ReplicaPaintingData = PaintingData;
	ReplicaPaintingData.Revision = CommittedForgeryRevision;
	RefreshReplicaWorldVisual();
	RefreshReplicaReviewWorldFeedback();
	ForceNetUpdate();

	UE_LOG(LogHeistNetwork, Log,
		TEXT(
			"Replica preview staged: Case=%s CaseId=%s Artifact=%s Template=%s Requester=%s Score=%.2f ReplicaPlaced=false State=%s Locked=%s Revision=%d PaintingResolution=%d PaintingPalette=%d PaintingBytes=%d PaintingRevision=%d Authority=true Result=PASS"),
		*GetNameSafe(this), *DisplayCaseId.ToString(), *CommittedForgeryResult.ArtifactId.ToString(), *CommittedForgeryResult.TemplateId.ToString(), *GetNameSafe(RequestingPlayerState),
		CommittedForgeryResult.SimilarityScore, *UEnum::GetValueAsString(DisplayCaseState), bSessionLocked ? TEXT("true") : TEXT("false"), CommittedForgeryRevision,
		ReplicaPaintingData.Resolution, ReplicaPaintingData.Palette.Num(), ReplicaPaintingData.PackedPaletteIndices.Num(), ReplicaPaintingData.Revision);
	return true;
}

bool AHeistPaintingDisplayCaseActor::TryRestartForgeryFromPreview(AHeistPlayerState* RequestingPlayerState, FName& OutRejectReason)
{
	if (!HasAuthority() || !ValidateReplicaReviewOwner(RequestingPlayerState, OutRejectReason))
	{
		if (!HasAuthority())
		{
			OutRejectReason = FName(TEXT("NotAuthority"));
		}
		return false;
	}

	const EHeistDisplayCaseState PreviousState = DisplayCaseState;
	ResetReplicaPreviewData();
	DisplayCaseState = EHeistDisplayCaseState::ForgeryInProgress;
	HandleDisplayCaseStateChanged(PreviousState);
	ForceNetUpdate();
	UE_LOG(LogHeistNetwork, Log, TEXT("Replica preview discarded: Case=%s Requester=%s State=%s Result=PASS"), *GetNameSafe(this), *GetNameSafe(RequestingPlayerState),
		   *UEnum::GetValueAsString(DisplayCaseState));
	return true;
}

bool AHeistPaintingDisplayCaseActor::TryCommitReplicaSwapAndTakeOriginal(AHeistPlayerState* RequestingPlayerState, int32& OutAddedInstanceId, FName& OutRejectReason)
{
	OutAddedInstanceId = INDEX_NONE;
	if (!HasAuthority() || !ValidateReplicaReviewOwner(RequestingPlayerState, OutRejectReason))
	{
		if (!HasAuthority())
		{
			OutRejectReason = FName(TEXT("NotAuthority"));
		}
		return false;
	}

	int32 ArtifactValue = 0;
	float ArtifactWeight = 0.0f;
	bool bRequiredTarget = false;
	if (!ValidateOriginalTakeRequest(RequestingPlayerState, ArtifactValue, ArtifactWeight, bRequiredTarget, OutRejectReason, true))
	{
		return false;
	}

	const float PreviousResolvedInspectionDelay = ResolvedInspectionDelay;
	const float PreviousInspectionReadyServerTime = InspectionReadyServerTime;
	const int32 PreviousInspectionScheduleRevision = InspectionScheduleRevision;
	if (!ResolveInspectionSchedule(OutRejectReason))
	{
		return false;
	}
	ResolvedInspectionDelay = FMath::Max(ResolvedInspectionDelay, PaintingReplicaSwapMinimumInspectionDelaySeconds);
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const float ServerTime = IsValid(HeistGameState) ? HeistGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	InspectionReadyServerTime = ServerTime + ResolvedInspectionDelay;

	AHeistPlayerCharacter* PlayerCharacter = Cast<AHeistPlayerCharacter>(RequestingPlayerState->GetPawn());
	UHeistInventoryComponent* InventoryComponent = IsValid(PlayerCharacter) ? PlayerCharacter->GetInventoryComponent() : nullptr;
	const TCHAR* InventoryRejectReason = nullptr;
	if (!IsValid(InventoryComponent) ||
		!InventoryComponent->TryAddOriginalArtifact(RequestingPlayerState, TargetArtifactId, bRequiredTarget, this, OutAddedInstanceId, InventoryRejectReason))
	{
		ResolvedInspectionDelay = PreviousResolvedInspectionDelay;
		InspectionReadyServerTime = PreviousInspectionReadyServerTime;
		InspectionScheduleRevision = PreviousInspectionScheduleRevision;
		OutRejectReason = FName(InventoryRejectReason != nullptr ? InventoryRejectReason : TEXT("InventoryCommitFailed"));
		return false;
	}

	OriginalCarrier = RequestingPlayerState;
	OriginalCarrierArrestChangedHandle = RequestingPlayerState->GetArrestStateChangedDelegate().AddUObject(this, &AHeistPaintingDisplayCaseActor::HandleOriginalCarrierArrestStateChanged);
	const EHeistDisplayCaseState PreviousState = DisplayCaseState;
	DisplayCaseState = EHeistDisplayCaseState::OriginalRemoved;
	CommittedForgeryResult.bReplicaPlaced = true;
	++CommittedForgeryRevision;
	ReplicaPaintingData.Revision = CommittedForgeryRevision;
	HandleDisplayCaseStateChanged(PreviousState);
	++OriginalCarryRevision;
	SyncObjectiveCarrierCandidate(RequestingPlayerState);
	StartInspectionDelayTimer();
	RefreshInspectionRegistration();
	RefreshReplicaWorldVisual();
	ClearSession(FName(TEXT("ReplicaSwapCommitted")));
	EmitReplicaSwapFeedback();
	ForceNetUpdate();
	BroadcastOriginalCarrySnapshot(TEXT("ServerReplicaSwap"), FName(TEXT("TakeAccepted")));

	UE_LOG(LogHeistNetwork, Log,
		TEXT("Replica swap committed: Case=%s CaseId=%s Artifact=%s Requester=%s InventoryInstance=%d GridCommitted=true Score=%.2f NoiseRadius=%.1f State=%s Result=PASS"),
		*GetNameSafe(this), *DisplayCaseId.ToString(), *TargetArtifactId.ToString(), *GetNameSafe(RequestingPlayerState), OutAddedInstanceId, CommittedForgeryResult.SimilarityScore,
		ReplicaSwapNoiseRadius, *UEnum::GetValueAsString(DisplayCaseState));
	return true;
}

bool AHeistPaintingDisplayCaseActor::ValidateReplicaPlacementRequest(AHeistPlayerState* RequestingPlayerState, const FHeistForgeryResult& ForgeryResult, FName& OutRejectReason) const
{
	OutRejectReason = NAME_None;
	if (!IsValid(RequestingPlayerState))
	{
		OutRejectReason = FName(TEXT("MissingPlayerState"));
		return false;
	}
	if (bHasCommittedForgeryResult)
	{
		OutRejectReason = FName(TEXT("DuplicateReplicaPlacement"));
		return false;
	}
	if (DisplayCaseState != EHeistDisplayCaseState::ForgeryInProgress)
	{
		OutRejectReason = FName(TEXT("CaseStateMismatch"));
		return false;
	}
	if (!bSessionLocked || SessionOwner.Get() != RequestingPlayerState)
	{
		OutRejectReason = FName(TEXT("CaseOwnershipMismatch"));
		return false;
	}
	if (ForgeryResult.ArtifactId.IsNone() || ForgeryResult.ArtifactId != TargetArtifactId || ForgeryResult.TemplateId.IsNone() || ForgeryResult.ForgeryType != EHeistForgeryType::Drawing)
	{
		OutRejectReason = FName(TEXT("ForgeryIdentityMismatch"));
		return false;
	}
	if (!FMath::IsFinite(ForgeryResult.SimilarityScore) || !FMath::IsFinite(ForgeryResult.CoverageScore) || !FMath::IsFinite(ForgeryResult.MajorShapeScore) ||
		!FMath::IsFinite(ForgeryResult.MissingShapePenalty) || !FMath::IsFinite(ForgeryResult.ExtraStrokePenalty) ||
		!FMath::IsFinite(ForgeryResult.CompletionTime) || !FMath::IsWithinInclusive(ForgeryResult.SimilarityScore, 0.0f, 100.0f) || ForgeryResult.CoverageScore < 0.0f ||
		ForgeryResult.MajorShapeScore < 0.0f || ForgeryResult.MissingShapePenalty < 0.0f || ForgeryResult.ExtraStrokePenalty < 0.0f ||
		ForgeryResult.CompletionTime < 0.0f || ForgeryResult.bReplicaPlaced)
	{
		OutRejectReason = FName(TEXT("ForgeryResultInvalid"));
		return false;
	}

	const AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	FHeistArtifactDataRow ArtifactDefinition;
	if (!IsValid(HeistGameMode) || !HeistGameMode->TryGetArtifactDefinition(TargetArtifactId, ArtifactDefinition))
	{
		OutRejectReason = FName(TEXT("InvalidArtifactDefinition"));
		return false;
	}
	if (!HeistReplicaAcceptance::MeetsMinimumQuality(ForgeryResult.SimilarityScore, ArtifactDefinition.MinimumForgeryScore))
	{
		OutRejectReason = FName(TEXT("QualityBelowMinimum"));
		return false;
	}
	return true;
}

bool AHeistPaintingDisplayCaseActor::ValidateReplicaReviewOwner(AHeistPlayerState* RequestingPlayerState, FName& OutRejectReason) const
{
	OutRejectReason = NAME_None;
	if (!IsValid(RequestingPlayerState))
	{
		OutRejectReason = FName(TEXT("MissingPlayerState"));
		return false;
	}
	if (DisplayCaseState != EHeistDisplayCaseState::ReplicaReady || !bHasCommittedForgeryResult || CommittedForgeryResult.bReplicaPlaced)
	{
		OutRejectReason = FName(TEXT("ReplicaPreviewNotReady"));
		return false;
	}
	if (!bSessionLocked || SessionOwner.Get() != RequestingPlayerState)
	{
		OutRejectReason = FName(TEXT("CaseOwnershipMismatch"));
		return false;
	}
	if (!ValidateReplicaPaintingData(ReplicaPaintingData, OutRejectReason) || ReplicaPaintingData.Revision != CommittedForgeryRevision)
	{
		return false;
	}
	return ValidateSessionRequest(RequestingPlayerState, OutRejectReason);
}

void AHeistPaintingDisplayCaseActor::ResetReplicaPreviewData()
{
	if (!bHasCommittedForgeryResult || CommittedForgeryResult.bReplicaPlaced)
	{
		return;
	}

	bHasCommittedForgeryResult = false;
	CommittedForgeryResult = FHeistForgeryResult();
	++CommittedForgeryRevision;
	ReplicaPaintingData = FHeistReplicaPaintingData();
	ReplicaPaintingData.Revision = CommittedForgeryRevision;
	RefreshReplicaWorldVisual();
}

void AHeistPaintingDisplayCaseActor::EmitReplicaSwapFeedback()
{
	if (!HasAuthority())
	{
		return;
	}

	if (AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr)
	{
		FHeistSoundPingEvent SoundPingEvent;
		SoundPingEvent.SoundPingTag = FHeistGameplayTags::Get().Event_SoundPing_ReplicaSwap;
		SoundPingEvent.PingType = EHeistSoundPingType::ReplicaSwap;
		SoundPingEvent.WorldLocation = GetActorLocation();
		SoundPingEvent.Radius = FMath::Max(0.0f, ReplicaSwapNoiseRadius);
		SoundPingEvent.Duration = FMath::Max(0.0f, ReplicaSwapNoiseDuration);
		SoundPingEvent.bAffectsGuards = true;
		HeistGameState->ReportSoundPing(SoundPingEvent);
	}

	Multicast_PlayReplicaSwapFeedback();
}

void AHeistPaintingDisplayCaseActor::Multicast_PlayReplicaSwapFeedback_Implementation()
{
	if (IsValid(ReplicaSwapSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, ReplicaSwapSound, GetActorLocation());
	}
	BP_OnReplicaSwapCommitted();
}

bool AHeistPaintingDisplayCaseActor::ValidateReplicaPaintingData(const FHeistReplicaPaintingData& PaintingData, FName& OutRejectReason) const
{
	OutRejectReason = NAME_None;
	if (PaintingData.Resolution != ReplicaPaintingResolution)
	{
		OutRejectReason = FName(TEXT("PaintingResolutionMismatch"));
		return false;
	}
	if (!FMath::IsWithinInclusive(PaintingData.Palette.Num(), 2, ReplicaPaintingMaximumPaletteColors))
	{
		OutRejectReason = FName(TEXT("PaintingPaletteInvalid"));
		return false;
	}

	const int32 PixelCount = PaintingData.Resolution * PaintingData.Resolution;
	const int32 ExpectedPackedByteCount = FMath::DivideAndRoundUp(PixelCount, 2);
	if (PaintingData.PackedPaletteIndices.Num() != ExpectedPackedByteCount)
	{
		OutRejectReason = FName(TEXT("PaintingPackedSizeMismatch"));
		return false;
	}

	for (int32 PixelIndex = 0; PixelIndex < PixelCount; ++PixelIndex)
	{
		const uint8 PackedByte = PaintingData.PackedPaletteIndices[PixelIndex / 2];
		const uint8 PaletteValue = (PixelIndex & 1) == 0 ? PackedByte & 0x0f : PackedByte >> 4;
		if (PaletteValue > PaintingData.Palette.Num())
		{
			OutRejectReason = FName(TEXT("PaintingPaletteIndexOutOfBounds"));
			return false;
		}
	}
	return true;
}

void AHeistPaintingDisplayCaseActor::OnRep_CommittedForgeryRevision()
{
	RefreshReplicaWorldVisual();
	RefreshReplicaReviewWorldFeedback();
	const TCHAR* ReplicaPhase = !bHasCommittedForgeryResult ? TEXT("CLEARED") : CommittedForgeryResult.bReplicaPlaced ? TEXT("COMMITTED") : TEXT("PREVIEW");

	UE_LOG(LogHeistNetwork, Log, TEXT("Replica state replicated: Case=%s CaseId=%s Artifact=%s Template=%s Score=%.2f ReplicaPlaced=%s State=%s Locked=%s Revision=%d Authority=false Phase=%s"),
		   *GetNameSafe(this), *DisplayCaseId.ToString(), *CommittedForgeryResult.ArtifactId.ToString(), *CommittedForgeryResult.TemplateId.ToString(), CommittedForgeryResult.SimilarityScore,
		   CommittedForgeryResult.bReplicaPlaced ? TEXT("true") : TEXT("false"), *UEnum::GetValueAsString(DisplayCaseState), bSessionLocked ? TEXT("true") : TEXT("false"), CommittedForgeryRevision,
		   ReplicaPhase);
}

void AHeistPaintingDisplayCaseActor::OnRep_ReplicaPaintingData()
{
	RefreshReplicaWorldVisual();

	bool bTextureBuilt = false;
	bool bDynamicMaterialBuilt = false;
	bool bTextureParameterApplied = false;
	bool bContractPassed = false;
	int32 Resolution = 0;
	int32 PaletteColorCount = 0;
	int32 PackedByteCount = 0;
	int32 PaintingRevision = 0;
	GetReplicaPaintingDebugState(Resolution, PaletteColorCount, PackedByteCount, PaintingRevision, bTextureBuilt, bDynamicMaterialBuilt, bTextureParameterApplied, bContractPassed);

	UE_LOG(LogHeistNetwork, Log,
		   TEXT("Replica painting data replicated: Case=%s Resolution=%d Palette=%d PackedBytes=%d PaintingRevision=%d CommittedRevision=%d Texture=%s MID=%s Parameter=%s Authority=false Result=%s"),
		   *GetNameSafe(this), Resolution, PaletteColorCount, PackedByteCount, PaintingRevision, CommittedForgeryRevision, bTextureBuilt ? TEXT("true") : TEXT("false"),
		   bDynamicMaterialBuilt ? TEXT("true") : TEXT("false"), bTextureParameterApplied ? TEXT("true") : TEXT("false"), bContractPassed ? TEXT("PASS") : TEXT("PENDING_OR_FAIL"));
}

#pragma endregion

#pragma region InspectionTarget

bool AHeistPaintingDisplayCaseActor::CalculateInspectionSchedule(const float BaseInspectionDelay, float& OutDelay)
{
	OutDelay = 0.0f;
	if (!FMath::IsFinite(BaseInspectionDelay) || BaseInspectionDelay < 0.0f)
	{
		return false;
	}

	OutDelay = BaseInspectionDelay;
	return true;
}

bool AHeistPaintingDisplayCaseActor::ResolveInspectionSchedule(FName& OutRejectReason)
{
	OutRejectReason = NAME_None;
	const AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	FHeistArtifactDataRow ArtifactDefinition;
	if (!HasAuthority() || !IsValid(HeistGameMode) || !HeistGameMode->TryGetArtifactDefinition(TargetArtifactId, ArtifactDefinition) || !FMath::IsFinite(ArtifactDefinition.BaseInspectionDelay) ||
		ArtifactDefinition.BaseInspectionDelay < 0.0f)
	{
		OutRejectReason = FName(TEXT("InvalidInspectionDelayData"));
		return false;
	}

	if (!CalculateInspectionSchedule(ArtifactDefinition.BaseInspectionDelay, ResolvedInspectionDelay))
	{
		OutRejectReason = FName(TEXT("InspectionScheduleMappingFailed"));
		return false;
	}
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const float ServerTime = IsValid(HeistGameState) ? HeistGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	InspectionReadyServerTime = ServerTime + ResolvedInspectionDelay;
	++InspectionScheduleRevision;

	UE_LOG(LogHeistNetwork, Log,
		TEXT("Inspection schedule resolved: Case=%s CaseId=%s Artifact=%s BaseDelay=%.2f Delay=%.2f ReadyServerTime=%.2f ScheduleRevision=%d Authority=true Result=PASS"),
		*GetNameSafe(this), *DisplayCaseId.ToString(), *TargetArtifactId.ToString(), ArtifactDefinition.BaseInspectionDelay, ResolvedInspectionDelay,
		InspectionReadyServerTime, InspectionScheduleRevision);
	return true;
}

void AHeistPaintingDisplayCaseActor::StartInspectionDelayTimer()
{
	ClearInspectionDelayTimer();
	if (!HasAuthority() || !IsValid(GetWorld()) || ResolvedInspectionDelay <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FTimerDelegate TimerDelegate;
	TimerDelegate.BindUObject(this, &AHeistPaintingDisplayCaseActor::HandleInspectionDelayExpired, InspectionScheduleRevision, InspectionDelayTimerRevision);
	GetWorld()->GetTimerManager().SetTimer(InspectionDelayTimerHandle, TimerDelegate, ResolvedInspectionDelay, false);
}

void AHeistPaintingDisplayCaseActor::ClearInspectionDelayTimer()
{
	++InspectionDelayTimerRevision;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InspectionDelayTimerHandle);
	}
	InspectionDelayTimerHandle.Invalidate();
}

void AHeistPaintingDisplayCaseActor::HandleInspectionDelayExpired(const int32 ExpectedScheduleRevision, const int32 ExpectedTimerRevision)
{
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!HasAuthority() || ExpectedScheduleRevision != InspectionScheduleRevision || ExpectedTimerRevision != InspectionDelayTimerRevision || !IsValid(HeistGameState) ||
		HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame)
	{
		UE_LOG(LogHeistNetwork, Warning,
			   TEXT("Inspection delay callback blocked: Case=%s CaseId=%s ExpectedScheduleRevision=%d CurrentScheduleRevision=%d ExpectedTimerRevision=%d CurrentTimerRevision=%d MatchPhase=%s "
					"Authority=%s Result=PASS Reason=StaleOrMatchEnded"),
			   *GetNameSafe(this), *DisplayCaseId.ToString(), ExpectedScheduleRevision, InspectionScheduleRevision, ExpectedTimerRevision, InspectionDelayTimerRevision,
			   IsValid(HeistGameState) ? *UEnum::GetValueAsString(HeistGameState->GetMatchPhase()) : TEXT("MissingGameState"), HasAuthority() ? TEXT("true") : TEXT("false"));
		return;
	}

	InspectionDelayTimerHandle.Invalidate();
	RefreshInspectionRegistration();
	UE_LOG(LogHeistNetwork, Log, TEXT("Inspection delay expired: Case=%s CaseId=%s Delay=%.2f Registered=%s ScheduleRevision=%d Authority=true Result=%s"), *GetNameSafe(this),
		   *DisplayCaseId.ToString(), ResolvedInspectionDelay, bRegisteredForInspection ? TEXT("true") : TEXT("false"),
		   InspectionScheduleRevision, bRegisteredForInspection ? TEXT("PASS") : TEXT("INELIGIBLE"));
}

bool AHeistPaintingDisplayCaseActor::HasInspectionDelayElapsed() const
{
	return GetInspectionDelayRemaining() <= KINDA_SMALL_NUMBER;
}

bool AHeistPaintingDisplayCaseActor::IsRegisteredForInspection() const
{
	return bRegisteredForInspection;
}

bool AHeistPaintingDisplayCaseActor::IsValidInspectionCandidate() const
{
	const bool bEligibleState =
		DisplayCaseState == EHeistDisplayCaseState::ReplicaPlaced || DisplayCaseState == EHeistDisplayCaseState::OriginalAvailable || DisplayCaseState == EHeistDisplayCaseState::OriginalRemoved;
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const bool bMatchInGame = IsValid(HeistGameState) && HeistGameState->GetMatchPhase() == EHeistMatchPhase::InGame;
	return bContractExhibitActive && bMatchInGame && bRegisteredForInspection && bHasCommittedForgeryResult && CommittedForgeryResult.bReplicaPlaced && bEligibleState && HasInspectionDelayElapsed() &&
		   !InspectingGuardActor.IsValid() && LastAppliedInspectionScheduleRevision != InspectionScheduleRevision;
}

int32 AHeistPaintingDisplayCaseActor::GetInspectionRegistrationRevision() const
{
	return InspectionRegistrationRevision;
}

float AHeistPaintingDisplayCaseActor::GetResolvedInspectionDelay() const
{
	return ResolvedInspectionDelay;
}

float AHeistPaintingDisplayCaseActor::GetInspectionReadyServerTime() const
{
	return InspectionReadyServerTime;
}

float AHeistPaintingDisplayCaseActor::GetInspectionDelayRemaining() const
{
	if (InspectionReadyServerTime <= 0.0f)
	{
		return 0.0f;
	}
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const float ServerTime = IsValid(HeistGameState) ? HeistGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	return FMath::Max(0.0f, InspectionReadyServerTime - ServerTime);
}

int32 AHeistPaintingDisplayCaseActor::GetInspectionScheduleRevision() const
{
	return InspectionScheduleRevision;
}

bool AHeistPaintingDisplayCaseActor::TryBeginInspection(AActor* InspectingGuard)
{
	if (!HasAuthority() || !IsValid(InspectingGuard) || InspectingGuardActor.IsValid() || !IsValidInspectionCandidate())
	{
		UE_LOG(LogHeistNetwork, Warning,
			   TEXT("Exhibit inspection begin rejected: Case=%s Guard=%s ExistingGuard=%s State=%s ScheduleRevision=%d LastAppliedRevision=%d Authority=%s ValidCandidate=%s "
					"Result=PASS Reason=%s"),
			   *GetNameSafe(this), *GetNameSafe(InspectingGuard), *GetNameSafe(InspectingGuardActor.Get()), *UEnum::GetValueAsString(DisplayCaseState), InspectionScheduleRevision,
			   LastAppliedInspectionScheduleRevision, HasAuthority() ? TEXT("true") : TEXT("false"), IsValidInspectionCandidate() ? TEXT("true") : TEXT("false"),
			   InspectingGuardActor.IsValid() || DisplayCaseState == EHeistDisplayCaseState::Inspecting ? TEXT("DuplicateClaim") : TEXT("InvalidCandidate"));
		return false;
	}

	PreInspectionState = DisplayCaseState;
	InspectingGuardActor = InspectingGuard;
	ActiveInspectionScheduleRevision = InspectionScheduleRevision;
	const EHeistDisplayCaseState PreviousState = DisplayCaseState;
	DisplayCaseState = EHeistDisplayCaseState::Inspecting;
	HandleDisplayCaseStateChanged(PreviousState);
	ForceNetUpdate();

	UE_LOG(LogHeistNetwork, Log, TEXT("Exhibit inspection begun: Case=%s CaseId=%s Guard=%s PreviousState=%s NewState=%s ScheduleRevision=%d Authority=true Result=PASS"),
		   *GetNameSafe(this), *DisplayCaseId.ToString(), *GetNameSafe(InspectingGuard), *UEnum::GetValueAsString(PreviousState), *UEnum::GetValueAsString(DisplayCaseState),
		   ActiveInspectionScheduleRevision);
	return true;
}

bool AHeistPaintingDisplayCaseActor::InterruptInspection(AActor* InspectingGuard, const FName Reason)
{
	if (!HasAuthority() || DisplayCaseState != EHeistDisplayCaseState::Inspecting || !IsInspectionOwnedBy(InspectingGuard))
	{
		return false;
	}

	const EHeistDisplayCaseState PreviousState = DisplayCaseState;
	DisplayCaseState = PreInspectionState;
	InspectingGuardActor.Reset();
	ActiveInspectionScheduleRevision = INDEX_NONE;
	HandleDisplayCaseStateChanged(PreviousState);
	ForceNetUpdate();

	UE_LOG(LogHeistNetwork, Log, TEXT("Exhibit inspection interrupted: Case=%s CaseId=%s Guard=%s RestoredState=%s Reason=%s Authority=true Result=PASS"), *GetNameSafe(this),
		   *DisplayCaseId.ToString(), *GetNameSafe(InspectingGuard), *UEnum::GetValueAsString(DisplayCaseState), *Reason.ToString());
	return true;
}

bool AHeistPaintingDisplayCaseActor::ApplyInspectionResult(AActor* InspectingGuard)
{
	if (HasAuthority() && IsValid(InspectingGuard) && LastAppliedInspectionScheduleRevision == InspectionScheduleRevision)
	{
		++InspectionDuplicateBlockCount;
		UE_LOG(LogHeistNetwork, Warning,
			   TEXT("Exhibit inspection result blocked: Case=%s CaseId=%s Guard=%s ScheduleRevision=%d LastAppliedRevision=%d DuplicateBlocks=%d Authority=true Result=PASS "
					"Reason=DuplicateResult"),
			   *GetNameSafe(this), *DisplayCaseId.ToString(), *GetNameSafe(InspectingGuard), InspectionScheduleRevision, LastAppliedInspectionScheduleRevision,
			   InspectionDuplicateBlockCount);
		return false;
	}

	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!HasAuthority() || !IsValid(HeistGameState) || HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame || DisplayCaseState != EHeistDisplayCaseState::Inspecting ||
		!IsInspectionOwnedBy(InspectingGuard) || ActiveInspectionScheduleRevision != InspectionScheduleRevision)
	{
		return false;
	}

	const EHeistDisplayCaseState PreviousState = DisplayCaseState;
	LastAppliedInspectionScheduleRevision = ActiveInspectionScheduleRevision;
	ActiveInspectionScheduleRevision = INDEX_NONE;
	++InspectionResultApplicationCount;
	DisplayCaseState = EHeistDisplayCaseState::Completed;
	InspectingGuardActor.Reset();
	HandleDisplayCaseStateChanged(PreviousState);
	ForceNetUpdate();

	UE_LOG(LogHeistNetwork, Log,
		   TEXT("Exhibit inspection result applied: Case=%s CaseId=%s Guard=%s NewState=%s ScheduleRevision=%d Applications=%d QualityIndependent=true AlertUnchanged=true "
					"Authority=true Result=PASS"),
		   *GetNameSafe(this), *DisplayCaseId.ToString(), *GetNameSafe(InspectingGuard), *UEnum::GetValueAsString(DisplayCaseState),
		   LastAppliedInspectionScheduleRevision, InspectionResultApplicationCount);
	return true;
}

bool AHeistPaintingDisplayCaseActor::IsInspectionOwnedBy(const AActor* InspectingGuard) const
{
	return IsValid(InspectingGuard) && InspectingGuardActor.Get() == InspectingGuard;
}

bool AHeistPaintingDisplayCaseActor::IsInspectionClaimActive() const
{
	return DisplayCaseState == EHeistDisplayCaseState::Inspecting && InspectingGuardActor.IsValid() && ActiveInspectionScheduleRevision == InspectionScheduleRevision;
}

AActor* AHeistPaintingDisplayCaseActor::GetInspectingGuard() const
{
	return InspectingGuardActor.Get();
}

bool AHeistPaintingDisplayCaseActor::IsInspectionDelayTimerActive() const
{
	UWorld* World = GetWorld();
	return IsValid(World) && World->GetTimerManager().TimerExists(InspectionDelayTimerHandle);
}

int32 AHeistPaintingDisplayCaseActor::GetInspectionResultApplicationCount() const
{
	return InspectionResultApplicationCount;
}

int32 AHeistPaintingDisplayCaseActor::GetInspectionDuplicateBlockCount() const
{
	return InspectionDuplicateBlockCount;
}

void AHeistPaintingDisplayCaseActor::RefreshInspectionRegistration()
{
	if (!HasAuthority())
	{
		return;
	}

	const bool bEligibleState =
		DisplayCaseState == EHeistDisplayCaseState::ReplicaPlaced || DisplayCaseState == EHeistDisplayCaseState::OriginalAvailable || DisplayCaseState == EHeistDisplayCaseState::OriginalRemoved;
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const bool bMatchInGame = IsValid(HeistGameState) && HeistGameState->GetMatchPhase() == EHeistMatchPhase::InGame;
	const bool bShouldRegister = bMatchInGame && bHasCommittedForgeryResult && CommittedForgeryResult.bReplicaPlaced && bEligibleState && HasInspectionDelayElapsed();
	if (bRegisteredForInspection == bShouldRegister)
	{
		return;
	}

	bRegisteredForInspection = bShouldRegister;
	++InspectionRegistrationRevision;
	UE_LOG(LogHeistNetwork, Log, TEXT("Inspection target registration changed: Case=%s CaseId=%s Registered=%s State=%s Score=%.2f Revision=%d Authority=true Result=PASS"), *GetNameSafe(this),
		   *DisplayCaseId.ToString(), bRegisteredForInspection ? TEXT("true") : TEXT("false"), *UEnum::GetValueAsString(DisplayCaseState), CommittedForgeryResult.SimilarityScore,
		   InspectionRegistrationRevision);
}

void AHeistPaintingDisplayCaseActor::OnRep_InspectionScheduleRevision()
{
	UE_LOG(LogHeistNetwork, Log,
		TEXT("Inspection schedule replicated: Case=%s CaseId=%s Delay=%.2f ReadyServerTime=%.2f Remaining=%.2f ScheduleRevision=%d Authority=false Result=PASS"),
		*GetNameSafe(this), *DisplayCaseId.ToString(), ResolvedInspectionDelay, InspectionReadyServerTime, GetInspectionDelayRemaining(), InspectionScheduleRevision);
}

#pragma endregion

#pragma region OriginalCarry

FName AHeistPaintingDisplayCaseActor::GetTargetArtifactId() const
{
	return TargetArtifactId;
}

FName AHeistPaintingDisplayCaseActor::GetDisplayCaseId() const
{
	return DisplayCaseId;
}

AHeistPlayerState* AHeistPaintingDisplayCaseActor::GetOriginalCarrier() const
{
	return OriginalCarrier.Get();
}

int32 AHeistPaintingDisplayCaseActor::GetOriginalCarryRevision() const
{
	return OriginalCarryRevision;
}

bool AHeistPaintingDisplayCaseActor::IsOriginalSecuredAtExit() const
{
	return bOriginalSecuredAtExit;
}

bool AHeistPaintingDisplayCaseActor::TryTakeOriginal(AHeistPlayerState* RequestingPlayerState)
{
	if (!HasAuthority())
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Original carry rejected: Case=%s Reason=NotAuthority"), *GetNameSafe(this));
		return false;
	}

	int32 ArtifactValue = 0;
	float ArtifactWeight = 0.0f;
	bool bRequiredTarget = false;
	FName RejectReason = NAME_None;
	if (!ValidateOriginalTakeRequest(RequestingPlayerState, ArtifactValue, ArtifactWeight, bRequiredTarget, RejectReason))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Original carry rejected: Case=%s Artifact=%s Requester=%s Reason=%s"), *GetNameSafe(this), *TargetArtifactId.ToString(),
			   *GetNameSafe(RequestingPlayerState), *RejectReason.ToString());
		return false;
	}

	AHeistPlayerCharacter* PlayerCharacter = Cast<AHeistPlayerCharacter>(RequestingPlayerState->GetPawn());
	UHeistInventoryComponent* InventoryComponent = IsValid(PlayerCharacter) ? PlayerCharacter->GetInventoryComponent() : nullptr;
	check(IsValid(InventoryComponent));

	int32 AddedInstanceId = INDEX_NONE;
	const TCHAR* InventoryRejectReason = nullptr;
	if (!InventoryComponent->TryAddOriginalArtifact(RequestingPlayerState, TargetArtifactId, bRequiredTarget, this, AddedInstanceId, InventoryRejectReason))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Original grid add rejected: Case=%s Artifact=%s Requester=%s Reason=%s"), *GetNameSafe(this), *TargetArtifactId.ToString(),
			   *GetNameSafe(RequestingPlayerState), InventoryRejectReason != nullptr ? InventoryRejectReason : TEXT("InventoryCommitFailed"));
		return false;
	}

	OriginalCarrier = RequestingPlayerState;
	OriginalCarrierArrestChangedHandle = RequestingPlayerState->GetArrestStateChangedDelegate().AddUObject(this, &AHeistPaintingDisplayCaseActor::HandleOriginalCarrierArrestStateChanged);
	if (!TryTransitionToDisplayCaseState(EHeistDisplayCaseState::OriginalRemoved))
	{
		UnbindOriginalCarrierDelegate();
		OriginalCarrier = nullptr;
		FHeistInventoryItem RolledBackItem;
		const bool bRolledBack = InventoryComponent->TryRemoveOriginalArtifactForSourceCase(RequestingPlayerState, this, RolledBackItem);
		checkf(bRolledBack, TEXT("Failed OriginalRemoved transition must roll back carry weight."));
		return false;
	}

	++OriginalCarryRevision;
	SyncObjectiveCarrierCandidate(RequestingPlayerState);
	ForceNetUpdate();
	BroadcastOriginalCarrySnapshot(TEXT("ServerTake"), FName(TEXT("TakeAccepted")));
	return true;
}

bool AHeistPaintingDisplayCaseActor::ReleaseOriginalForCarrier(AHeistPlayerState* ExpectedCarrier, const FName Reason)
{
	const bool bOriginalCanReturn = DisplayCaseState == EHeistDisplayCaseState::OriginalRemoved || DisplayCaseState == EHeistDisplayCaseState::Inspecting ||
		DisplayCaseState == EHeistDisplayCaseState::Completed ||
		DisplayCaseState == EHeistDisplayCaseState::Failed;
	if (!HasAuthority() || !bOriginalCanReturn || !IsValid(OriginalCarrier.Get()) || OriginalCarrier.Get() != ExpectedCarrier)
	{
		return false;
	}

	AHeistPlayerCharacter* PlayerCharacter = Cast<AHeistPlayerCharacter>(ExpectedCarrier->GetPawn());
	UHeistInventoryComponent* InventoryComponent = IsValid(PlayerCharacter) ? PlayerCharacter->GetInventoryComponent() : nullptr;
	FHeistInventoryItem ReleasedItem;
	const bool bCarryEntryReleased = IsValid(InventoryComponent) && InventoryComponent->TryRemoveOriginalArtifactForSourceCase(ExpectedCarrier, this, ReleasedItem);
	const bool bAllowMissingInventoryCleanup = Reason == FName(TEXT("OwnerDisconnected")) || Reason == FName(TEXT("OwnerArrested"));
	if (!bCarryEntryReleased && (IsValid(InventoryComponent) || !bAllowMissingInventoryCleanup))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Original drop rejected: Case=%s Carrier=%s Reason=CarryEntryReleaseFailed"), *GetNameSafe(this), *GetNameSafe(ExpectedCarrier));
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

bool AHeistPaintingDisplayCaseActor::DropOriginalForCarrier(AHeistPlayerState* ExpectedCarrier, const FName Reason)
{
	const bool bOriginalOutsideCase = DisplayCaseState == EHeistDisplayCaseState::OriginalRemoved || DisplayCaseState == EHeistDisplayCaseState::Inspecting ||
		DisplayCaseState == EHeistDisplayCaseState::Completed ||
		DisplayCaseState == EHeistDisplayCaseState::Failed;
	if (!HasAuthority() || !bOriginalOutsideCase || !IsValid(ExpectedCarrier) || OriginalCarrier.Get() != ExpectedCarrier)
	{
		return false;
	}

	AHeistPlayerCharacter* PlayerCharacter = Cast<AHeistPlayerCharacter>(ExpectedCarrier->GetPawn());
	UHeistInventoryComponent* InventoryComponent = IsValid(PlayerCharacter) ? PlayerCharacter->GetInventoryComponent() : nullptr;
	FHeistInventoryItem DropItem;
	const bool bHasCommittedGridItem = IsValid(InventoryComponent) && InventoryComponent->TryGetOriginalArtifactForSourceCase(this, DropItem);
	if (!bHasCommittedGridItem)
	{
		const AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
		FHeistArtifactDataRow ArtifactDefinition;
		const bool bAllowRecoveryFallback = Reason == FName(TEXT("OwnerDisconnected")) || Reason == FName(TEXT("OwnerArrested"));
		if (!bAllowRecoveryFallback || !IsValid(HeistGameMode) ||
			!HeistGameMode->TryGetArtifactDefinition(TargetArtifactId, ArtifactDefinition))
		{
			UE_LOG(LogHeistNetwork, Warning, TEXT("Original world drop rejected: Case=%s Carrier=%s Reason=MissingCarryEntry"), *GetNameSafe(this), *GetNameSafe(ExpectedCarrier));
			return false;
		}

		const AHeistGameState* HeistGameState = GetWorld()->GetGameState<AHeistGameState>();
		const FHeistContractSnapshot ContractSnapshot = IsValid(HeistGameState) ? HeistGameState->GetContractSnapshot() : FHeistContractSnapshot();
		DropItem.ItemId = TargetArtifactId;
		DropItem.ContractValue = ArtifactDefinition.ArtifactValue;
		DropItem.Weight = ArtifactDefinition.Weight;
		DropItem.BaseGridSize = FIntPoint(ArtifactDefinition.GridWidth, ArtifactDefinition.GridHeight);
		DropItem.bOriginalArtifact = true;
		DropItem.bRequiredTarget = ContractSnapshot.IsInitialized() && ContractSnapshot.RequiredTargetArtifactId == TargetArtifactId &&
			ContractSnapshot.RequiredTargetCaseId == DisplayCaseId;
		DropItem.SourceDisplayCase = this;
	}

	if (!DropItem.HasValidOriginalData() || DropItem.SourceDisplayCase != this || DropItem.ItemId != TargetArtifactId)
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Original world drop rejected: Case=%s Carrier=%s Artifact=%s Reason=CarrySourceMismatch"), *GetNameSafe(this), *GetNameSafe(ExpectedCarrier),
			   *DropItem.ItemId.ToString());
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
		UE_LOG(LogHeistNetwork, Error, TEXT("Original world drop rejected: Case=%s Carrier=%s Reason=SpawnFailed"), *GetNameSafe(this), *GetNameSafe(ExpectedCarrier));
		return false;
	}

	DroppedOriginal->InitializeDroppedOriginal(DropItem.ItemId, DropItem.ContractValue, DropItem.Weight, DropItem.bRequiredTarget, this);
	DroppedOriginal->FinishSpawning(DropTransform);

	if (bHasCommittedGridItem)
	{
		check(IsValid(InventoryComponent));
		FHeistInventoryItem ReleasedItem;
		if (!InventoryComponent->TryRemoveOriginalArtifactForSourceCase(ExpectedCarrier, this, ReleasedItem))
		{
			DroppedOriginal->Destroy();
			UE_LOG(LogHeistNetwork, Warning, TEXT("Original world drop rejected: Case=%s Carrier=%s Reason=CarryEntryReleaseFailed"), *GetNameSafe(this), *GetNameSafe(ExpectedCarrier));
			return false;
		}
	}

	UnbindOriginalCarrierDelegate();
	OriginalCarrier = nullptr;
	++OriginalCarryRevision;
	SyncObjectiveCarrierCandidate(nullptr);
	ForceNetUpdate();
	BroadcastOriginalCarrySnapshot(TEXT("ServerWorldDrop"), Reason);
	UE_LOG(LogHeistNetwork, Log,
		   TEXT("Original world drop committed: Case=%s CaseId=%s Actor=%s Artifact=%s Value=%d Weight=%.1f Required=%s Reason=%s Revision=%d Authority=true Result=PASS"),
		   *GetNameSafe(this), *DisplayCaseId.ToString(), *GetNameSafe(DroppedOriginal), *DropItem.ItemId.ToString(), DropItem.ContractValue, DropItem.Weight,
		   DropItem.bRequiredTarget ? TEXT("true") : TEXT("false"), *Reason.ToString(), OriginalCarryRevision);
	return true;
}

bool AHeistPaintingDisplayCaseActor::TryClaimDroppedOriginal(AHeistPlayerState* RequestingPlayerState, AHeistDroppedOriginalActor* DroppedOriginal)
{
	const bool bOriginalOutsideCase = DisplayCaseState == EHeistDisplayCaseState::OriginalRemoved || DisplayCaseState == EHeistDisplayCaseState::Inspecting ||
		DisplayCaseState == EHeistDisplayCaseState::Completed ||
		DisplayCaseState == EHeistDisplayCaseState::Failed;
	if (!HasAuthority() || !bOriginalOutsideCase || bOriginalSecuredAtExit || IsValid(OriginalCarrier.Get()) || !IsValid(RequestingPlayerState) || RequestingPlayerState->IsArrested() ||
		RequestingPlayerState->IsEscaped() || !IsValid(DroppedOriginal) || DroppedOriginal->GetSourceDisplayCase() != this || DroppedOriginal->GetArtifactId() != TargetArtifactId)
	{
		return false;
	}

	AHeistPlayerCharacter* PlayerCharacter = Cast<AHeistPlayerCharacter>(RequestingPlayerState->GetPawn());
	UHeistInventoryComponent* InventoryComponent = IsValid(PlayerCharacter) ? PlayerCharacter->GetInventoryComponent() : nullptr;
	int32 AddedInstanceId = INDEX_NONE;
	const TCHAR* InventoryRejectReason = nullptr;
	if (!IsValid(InventoryComponent) || !InventoryComponent->TryAddOriginalArtifact(RequestingPlayerState, DroppedOriginal->GetArtifactId(), DroppedOriginal->IsRequiredTarget(), this,
																	 AddedInstanceId, InventoryRejectReason))
	{
		return false;
	}

	OriginalCarrier = RequestingPlayerState;
	OriginalCarrierArrestChangedHandle = RequestingPlayerState->GetArrestStateChangedDelegate().AddUObject(this, &AHeistPaintingDisplayCaseActor::HandleOriginalCarrierArrestStateChanged);
	++OriginalCarryRevision;
	SyncObjectiveCarrierCandidate(RequestingPlayerState);
	ForceNetUpdate();
	BroadcastOriginalCarrySnapshot(TEXT("ServerWorldPickup"), FName(TEXT("PickupAccepted")));
	return true;
}

bool AHeistPaintingDisplayCaseActor::CanCommitOriginalDepositForCarrier(const AHeistPlayerState* ExpectedCarrier, const FHeistInventoryItem& OriginalItem) const
{
	const bool bOriginalOutsideCase = DisplayCaseState == EHeistDisplayCaseState::OriginalRemoved || DisplayCaseState == EHeistDisplayCaseState::Inspecting ||
		DisplayCaseState == EHeistDisplayCaseState::Completed ||
		DisplayCaseState == EHeistDisplayCaseState::Failed;
	return HasAuthority() && bOriginalOutsideCase && !bOriginalSecuredAtExit && IsValid(ExpectedCarrier) && OriginalCarrier.Get() == ExpectedCarrier && OriginalItem.HasValidOriginalData() &&
		   OriginalItem.SourceDisplayCase == this && OriginalItem.ItemId == TargetArtifactId;
}

bool AHeistPaintingDisplayCaseActor::CommitOriginalDepositForCarrier(AHeistPlayerState* ExpectedCarrier, const FHeistInventoryItem& OriginalItem)
{
	if (!CanCommitOriginalDepositForCarrier(ExpectedCarrier, OriginalItem))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Original exit secure rejected: Case=%s Carrier=%s Artifact=%s Reason=InvalidDepositState"), *GetNameSafe(this),
			   *GetNameSafe(ExpectedCarrier), *OriginalItem.ItemId.ToString());
		return false;
	}

	UnbindOriginalCarrierDelegate();
	OriginalCarrier = nullptr;
	bOriginalSecuredAtExit = true;
	++OriginalCarryRevision;
	SyncObjectiveCarrierCandidate(nullptr);
	ForceNetUpdate();
	BroadcastOriginalCarrySnapshot(TEXT("ServerDeposit"), FName(TEXT("DepositedAtSharedExit")));
	return true;
}

bool AHeistPaintingDisplayCaseActor::ValidateOriginalTakeRequest(AHeistPlayerState* RequestingPlayerState, int32& OutArtifactValue, float& OutArtifactWeight, bool& bOutRequiredTarget,
																 FName& OutRejectReason, const bool bAllowLockedReplicaReady) const
{
	OutArtifactValue = 0;
	OutArtifactWeight = 0.0f;
	bOutRequiredTarget = false;
	OutRejectReason = NAME_None;
	if (!IsValid(RequestingPlayerState))
	{
		OutRejectReason = FName(TEXT("MissingPlayerState"));
		return false;
	}
	const bool bStandardPickupState = DisplayCaseState == EHeistDisplayCaseState::OriginalAvailable && !bSessionLocked;
	const bool bReplicaReviewState = bAllowLockedReplicaReady && DisplayCaseState == EHeistDisplayCaseState::ReplicaReady && bSessionLocked && SessionOwner.Get() == RequestingPlayerState;
	if (!bStandardPickupState && !bReplicaReviewState)
	{
		OutRejectReason = FName(TEXT("OriginalNotAvailable"));
		return false;
	}
	if (bSessionLocked && !bReplicaReviewState)
	{
		OutRejectReason = FName(TEXT("CaseSessionLocked"));
		return false;
	}
	if (IsValid(OriginalCarrier.Get()))
	{
		OutRejectReason = FName(TEXT("OriginalAlreadyCarried"));
		return false;
	}

	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistGameState) || HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame)
	{
		OutRejectReason = FName(TEXT("MatchPhaseNotInGame"));
		return false;
	}
	const bool bPlayerBelongsToMatch =
		HeistGameState->PlayerArray.ContainsByPredicate([RequestingPlayerState](const TObjectPtr<APlayerState>& CandidatePlayerState) { return CandidatePlayerState.Get() == RequestingPlayerState; });
	if (!bPlayerBelongsToMatch)
	{
		OutRejectReason = FName(TEXT("PlayerStateNotInMatch"));
		return false;
	}
	if (RequestingPlayerState->IsArrested() || RequestingPlayerState->IsEscaped())
	{
		OutRejectReason = FName(TEXT("PlayerStateBlocked"));
		return false;
	}

	AHeistPlayerCharacter* PlayerCharacter = Cast<AHeistPlayerCharacter>(RequestingPlayerState->GetPawn());
	UHeistInventoryComponent* InventoryComponent = IsValid(PlayerCharacter) ? PlayerCharacter->GetInventoryComponent() : nullptr;
	if (!IsValid(PlayerCharacter) || !IsValid(InventoryComponent))
	{
		OutRejectReason = FName(TEXT("MissingCharacterOrInventory"));
		return false;
	}
	if (FVector::DistSquared(PlayerCharacter->GetActorLocation(), GetActorLocation()) > FMath::Square(MaximumSessionDistance))
	{
		OutRejectReason = FName(TEXT("OutOfRange"));
		return false;
	}
	const AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	FHeistArtifactDataRow ArtifactDefinition;
	if (!IsValid(HeistGameMode) || !HeistGameMode->TryGetArtifactDefinition(TargetArtifactId, ArtifactDefinition))
	{
		OutRejectReason = FName(TEXT("InvalidArtifactDefinition"));
		return false;
	}

	OutArtifactValue = ArtifactDefinition.ArtifactValue;
	OutArtifactWeight = ArtifactDefinition.Weight;
	const FHeistContractSnapshot& ContractSnapshot = HeistGameState->GetContractSnapshot();
	bOutRequiredTarget = ContractSnapshot.IsInitialized()
		? ContractSnapshot.RequiredTargetArtifactId == TargetArtifactId && ContractSnapshot.RequiredTargetCaseId == DisplayCaseId
		: HeistGameState->GetActiveTargetArtifactId() == TargetArtifactId && HeistGameState->GetActiveTargetCaseId() == DisplayCaseId;
	if (!RequestingPlayerState->CanAddLootScoreAndWeight(0, OutArtifactWeight))
	{
		OutRejectReason = FName(TEXT("InvalidCarryWeight"));
		return false;
	}
	return true;
}

void AHeistPaintingDisplayCaseActor::SyncObjectiveCarrierCandidate(AHeistPlayerState* Carrier)
{
	AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistGameState))
	{
		return;
	}
	if ((!HeistGameState->GetActiveTargetArtifactId().IsNone() && HeistGameState->GetActiveTargetArtifactId() != TargetArtifactId) ||
		(!HeistGameState->GetActiveTargetCaseId().IsNone() && HeistGameState->GetActiveTargetCaseId() != DisplayCaseId))
	{
		return;
	}

	const FName ObjectiveArtifactId = HeistGameState->GetActiveTargetArtifactId().IsNone() ? TargetArtifactId : HeistGameState->GetActiveTargetArtifactId();
	const FName ObjectiveCaseId = HeistGameState->GetActiveTargetCaseId().IsNone() ? DisplayCaseId : HeistGameState->GetActiveTargetCaseId();
	const EHeistObjectiveState ObjectiveState = HeistGameState->GetObjectiveState() == EHeistObjectiveState::Inactive ? EHeistObjectiveState::InProgress : HeistGameState->GetObjectiveState();
	HeistGameState->SetObjectiveSnapshot(ObjectiveArtifactId, ObjectiveCaseId, ObjectiveState, Carrier);
}

void AHeistPaintingDisplayCaseActor::UnbindOriginalCarrierDelegate()
{
	if (IsValid(OriginalCarrier.Get()) && OriginalCarrierArrestChangedHandle.IsValid())
	{
		OriginalCarrier->GetArrestStateChangedDelegate().Remove(OriginalCarrierArrestChangedHandle);
	}
	OriginalCarrierArrestChangedHandle.Reset();
}

void AHeistPaintingDisplayCaseActor::BroadcastOriginalCarrySnapshot(const TCHAR* ChangeSource, const FName Reason)
{
	OnOriginalCarryChanged.Broadcast(OriginalCarrier.Get(), TargetArtifactId, OriginalCarryRevision);
	UE_LOG(LogHeistNetwork, Log, TEXT("Original carry %s: Case=%s CaseId=%s Artifact=%s Carrier=%s CarrierPlayerId=%d State=%s Revision=%d Reason=%s Authority=%s"), ChangeSource, *GetNameSafe(this),
		   *DisplayCaseId.ToString(), *TargetArtifactId.ToString(), *GetNameSafe(OriginalCarrier.Get()), IsValid(OriginalCarrier.Get()) ? OriginalCarrier->HeistPlayerId : INDEX_NONE,
		   *UEnum::GetValueAsString(DisplayCaseState), OriginalCarryRevision, Reason.IsNone() ? TEXT("None") : *Reason.ToString(), HasAuthority() ? TEXT("true") : TEXT("false"));
}

void AHeistPaintingDisplayCaseActor::HandleOriginalCarrierArrestStateChanged(const bool bArrested)
{
	if (HasAuthority() && bArrested && IsValid(OriginalCarrier.Get()))
	{
		DropOriginalForCarrier(OriginalCarrier.Get(), FName(TEXT("OwnerArrested")));
	}
}

void AHeistPaintingDisplayCaseActor::OnRep_OriginalCarryRevision()
{
	BroadcastOriginalCarrySnapshot(TEXT("Replicated"), NAME_None);
}

#pragma endregion

#pragma region Session

AHeistPlayerState* AHeistPaintingDisplayCaseActor::GetSessionOwner() const
{
	return SessionOwner.Get();
}

bool AHeistPaintingDisplayCaseActor::IsSessionLocked() const
{
	return bSessionLocked;
}

int32 AHeistPaintingDisplayCaseActor::GetSessionRevision() const
{
	return SessionRevision;
}

float AHeistPaintingDisplayCaseActor::GetMaximumSessionDistance() const
{
	return MaximumSessionDistance;
}

bool AHeistPaintingDisplayCaseActor::TryBeginSession(AHeistPlayerState* RequestingPlayerState)
{
	if (!HasAuthority())
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Display case session begin rejected: Case=%s Reason=NotAuthority"), *GetNameSafe(this));
		return false;
	}

	if (bSessionLocked)
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Display case session begin rejected: Case=%s Requester=%s Owner=%s Reason=AlreadyLocked"), *GetNameSafe(this), *GetNameSafe(RequestingPlayerState),
			   *GetNameSafe(SessionOwner.Get()));
		return false;
	}

	FName RejectReason = NAME_None;
	if (!ValidateSessionRequest(RequestingPlayerState, RejectReason))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Display case session begin rejected: Case=%s Requester=%s Reason=%s"), *GetNameSafe(this), *GetNameSafe(RequestingPlayerState),
			   *RejectReason.ToString());
		return false;
	}

	SessionOwner = RequestingPlayerState;
	bSessionLocked = true;
	++SessionRevision;
	SessionOwnerArrestChangedHandle = RequestingPlayerState->GetArrestStateChangedDelegate().AddUObject(this, &AHeistPaintingDisplayCaseActor::HandleSessionOwnerArrestStateChanged);
	ForceNetUpdate();
	BroadcastSessionSnapshot(TEXT("ServerBegin"), FName(TEXT("BeginAccepted")));
	return true;
}

bool AHeistPaintingDisplayCaseActor::TryCancelSession(AHeistPlayerState* RequestingPlayerState)
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
		UE_LOG(LogHeistNetwork, Warning, TEXT("Display case session cancel rejected: Case=%s Requester=%s Owner=%s Reason=NotSessionOwner"), *GetNameSafe(this), *GetNameSafe(RequestingPlayerState),
			   *GetNameSafe(SessionOwner.Get()));
		return false;
	}

	ClearSession(FName(TEXT("OwnerCancelled")));
	return true;
}

bool AHeistPaintingDisplayCaseActor::CancelSessionForOwner(AHeistPlayerState* ExpectedOwner, const FName Reason)
{
	if (!HasAuthority() || !bSessionLocked || SessionOwner.Get() != ExpectedOwner)
	{
		return false;
	}

	ClearSession(Reason);
	return true;
}

bool AHeistPaintingDisplayCaseActor::ValidateSessionRequest(AHeistPlayerState* RequestingPlayerState, FName& OutRejectReason) const
{
	OutRejectReason = NAME_None;
	if (!bContractExhibitActive)
	{
		OutRejectReason = FName(TEXT("InactiveContractExhibit"));
		return false;
	}
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

	const bool bPlayerBelongsToMatch =
		HeistGameState->PlayerArray.ContainsByPredicate([RequestingPlayerState](const TObjectPtr<APlayerState>& CandidatePlayerState) { return CandidatePlayerState.Get() == RequestingPlayerState; });
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

	if (FVector::DistSquared(RequestingPawn->GetActorLocation(), GetActorLocation()) > FMath::Square(MaximumSessionDistance))
	{
		OutRejectReason = FName(TEXT("OutOfRange"));
		return false;
	}

	return true;
}

void AHeistPaintingDisplayCaseActor::ClearSession(const FName Reason)
{
	AHeistPlayerState* PreviousOwner = SessionOwner.Get();
	ResetForgerySessionState(Reason);
	UnbindSessionOwnerDelegate();
	SessionOwner = nullptr;
	bSessionLocked = false;
	++SessionRevision;
	ForceNetUpdate();

	UE_LOG(LogHeistNetwork, Log, TEXT("Display case session cleared: Case=%s PreviousOwner=%s Reason=%s Revision=%d"), *GetNameSafe(this), *GetNameSafe(PreviousOwner), *Reason.ToString(),
		   SessionRevision);
	BroadcastSessionSnapshot(TEXT("ServerClear"), Reason);
}

void AHeistPaintingDisplayCaseActor::UnbindSessionOwnerDelegate()
{
	if (IsValid(SessionOwner.Get()) && SessionOwnerArrestChangedHandle.IsValid())
	{
		SessionOwner->GetArrestStateChangedDelegate().Remove(SessionOwnerArrestChangedHandle);
	}
	SessionOwnerArrestChangedHandle.Reset();
}

void AHeistPaintingDisplayCaseActor::OnRep_SessionRevision()
{
	BroadcastSessionSnapshot(TEXT("Replicated"), NAME_None);
}

void AHeistPaintingDisplayCaseActor::BroadcastSessionSnapshot(const TCHAR* ChangeSource, const FName Reason)
{
	RefreshReplicaReviewWorldFeedback();
	OnDisplayCaseSessionChanged.Broadcast(SessionOwner.Get(), bSessionLocked, SessionRevision);
	UE_LOG(LogHeistNetwork, Log, TEXT("Display case session %s: Case=%s Owner=%s OwnerPlayerId=%d Locked=%s Revision=%d Reason=%s Authority=%s"), ChangeSource, *GetNameSafe(this),
		   *GetNameSafe(SessionOwner.Get()), IsValid(SessionOwner.Get()) ? SessionOwner->HeistPlayerId : INDEX_NONE, bSessionLocked ? TEXT("true") : TEXT("false"), SessionRevision,
		   Reason.IsNone() ? TEXT("None") : *Reason.ToString(), HasAuthority() ? TEXT("true") : TEXT("false"));
}

void AHeistPaintingDisplayCaseActor::RefreshReplicaReviewWorldFeedback()
{
	const APawn* LocalPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const bool bReadyForLocalPlayer = IsReplicaReviewReadyFor(LocalPawn);
	const bool bUntouched = IsUntouchedForWorldFeedback();
	const FLinearColor SuggestedColor = bReadyForLocalPlayer ? FLinearColor::Green : bUntouched ? FLinearColor::Red : FLinearColor::Transparent;
	const FText SuggestedHint = bReadyForLocalPlayer
		? NSLOCTEXT("HeistDisplayCase", "ReplicaReviewHint", "E 회수  |  R 다시 그리기")
		: bUntouched ? NSLOCTEXT("HeistDisplayCase", "UntouchedHint", "E 위조 시작") : FText::GetEmpty();
	BP_ApplyReplicaReviewWorldFeedback(bReadyForLocalPlayer, bUntouched, SuggestedColor, SuggestedHint);
}

void AHeistPaintingDisplayCaseActor::HandleSessionOwnerArrestStateChanged(const bool bArrested)
{
	if (HasAuthority() && bSessionLocked && bArrested)
	{
		ClearSession(FName(TEXT("OwnerArrested")));
	}
}

void AHeistPaintingDisplayCaseActor::HandleMatchPhaseChanged(const EHeistMatchPhase PreviousMatchPhase, const EHeistMatchPhase NewMatchPhase)
{
	if (HasAuthority() && bSessionLocked && PreviousMatchPhase != NewMatchPhase)
	{
		ClearSession(FName(TEXT("MatchPhaseChanged")));
	}
	if (HasAuthority() && PreviousMatchPhase != NewMatchPhase)
	{
		if (NewMatchPhase != EHeistMatchPhase::InGame)
		{
			ClearInspectionDelayTimer();
			InspectionReadyServerTime = 0.0f;
			if (DisplayCaseState == EHeistDisplayCaseState::Inspecting)
			{
				if (AActor* InspectingGuard = InspectingGuardActor.Get(); IsValid(InspectingGuard))
				{
					InterruptInspection(InspectingGuard, FName(TEXT("MatchEnded")));
				}
				else
				{
					const EHeistDisplayCaseState PreviousState = DisplayCaseState;
					DisplayCaseState = PreInspectionState;
					InspectingGuardActor.Reset();
					ActiveInspectionScheduleRevision = INDEX_NONE;
					HandleDisplayCaseStateChanged(PreviousState);
				}
			}
			else
			{
				InspectingGuardActor.Reset();
				ActiveInspectionScheduleRevision = INDEX_NONE;
			}
			ForceNetUpdate();
		}
		RefreshInspectionRegistration();
	}
}

#pragma endregion

#pragma region Replication

void AHeistPaintingDisplayCaseActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeistPaintingDisplayCaseActor, DisplayCaseState);
	DOREPLIFETIME(AHeistPaintingDisplayCaseActor, bContractExhibitActive);
	DOREPLIFETIME(AHeistPaintingDisplayCaseActor, OriginalVisualTemplateId);
	DOREPLIFETIME(AHeistPaintingDisplayCaseActor, OriginalReferenceImage);
	DOREPLIFETIME(AHeistPaintingDisplayCaseActor, OriginalVisualRevision);
	DOREPLIFETIME(AHeistPaintingDisplayCaseActor, bHasCommittedForgeryResult);
	DOREPLIFETIME(AHeistPaintingDisplayCaseActor, CommittedForgeryResult);
	DOREPLIFETIME(AHeistPaintingDisplayCaseActor, CommittedForgeryRevision);
	DOREPLIFETIME(AHeistPaintingDisplayCaseActor, ReplicaPaintingData);
	DOREPLIFETIME(AHeistPaintingDisplayCaseActor, OriginalCarrier);
	DOREPLIFETIME(AHeistPaintingDisplayCaseActor, OriginalCarryRevision);
	DOREPLIFETIME(AHeistPaintingDisplayCaseActor, bOriginalSecuredAtExit);
	DOREPLIFETIME(AHeistPaintingDisplayCaseActor, SessionOwner);
	DOREPLIFETIME(AHeistPaintingDisplayCaseActor, bSessionLocked);
	DOREPLIFETIME(AHeistPaintingDisplayCaseActor, SessionRevision);
	DOREPLIFETIME(AHeistPaintingDisplayCaseActor, ResolvedInspectionDelay);
	DOREPLIFETIME(AHeistPaintingDisplayCaseActor, InspectionReadyServerTime);
	DOREPLIFETIME(AHeistPaintingDisplayCaseActor, InspectionScheduleRevision);
}

#pragma endregion
