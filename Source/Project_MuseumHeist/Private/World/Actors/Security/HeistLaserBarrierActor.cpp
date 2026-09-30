#include "World/Actors/Security/HeistLaserBarrierActor.h"

#include "Character/HeistPlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HeistCollisionChannels.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerState.h"
#include "Data/HeistArtifactDataTypes.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "TimerManager.h"
#include "World/Actors/Loot/HeistPaintingDisplayCaseActor.h"

AHeistLaserBarrierActor::AHeistLaserBarrierActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(10.0f);

	SceneRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRootComponent"));
	SetRootComponent(SceneRootComponent);

	BeamTriggerComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BeamTriggerComponent"));
	BeamTriggerComponent->SetupAttachment(SceneRootComponent);
	BeamTriggerComponent->InitBoxExtent(FVector(10.0f, 150.0f, 120.0f));
	BeamTriggerComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BeamTriggerComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	BeamTriggerComponent->SetGenerateOverlapEvents(true);

	BeamVisualComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeamVisualComponent"));
	BeamVisualComponent->SetupAttachment(SceneRootComponent);
	BeamVisualComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeamVisualComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	BeamVisualComponent->SetGenerateOverlapEvents(false);

	for (int32 Index = 0; Index < 3; ++Index)
	{
		UNiagaraComponent* Beam = CreateDefaultSubobject<UNiagaraComponent>(*FString::Printf(TEXT("LaserBeam%d"), Index + 1));
		Beam->SetupAttachment(BeamTriggerComponent);
		Beam->SetAutoActivate(false);
		Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Beam->SetGenerateOverlapEvents(false);
		BeamEffectComponents.Add(Beam);
	}
}

void AHeistLaserBarrierActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ConfigureBeamEffects();
	// Editor preview only. BeginPlay and replicated state own gameplay visibility.
	SetBeamEffectsActive(GetWorld() && !GetWorld()->IsGameWorld());
}

void AHeistLaserBarrierActor::ConfigureBeamEffects()
{
	const FVector Extent = BeamTriggerComponent->GetUnscaledBoxExtent();
	for (int32 Index = 0; Index < BeamEffectComponents.Num(); ++Index)
	{
		UNiagaraComponent* Beam = BeamEffectComponents[Index];
		if (!IsValid(Beam)) continue;
		if (Beam->GetAsset() != BeamEffect) Beam->SetAsset(BeamEffect);
		Beam->SetRelativeLocation(FVector(0, 0, (Index - 1) * Extent.Z * (2.0 / 3.0)));
		Beam->SetRelativeRotation(FRotator::ZeroRotator);
		Beam->SetRelativeScale3D(FVector::OneVector);
		// The authored system uses local-space Vec3 endpoints. Parent scale is applied once.
		Beam->SetVariableVec3(TEXT("User.BeamStart"), FVector(0, -Extent.Y, 0));
		Beam->SetVariableVec3(TEXT("User.BeamEnd"), FVector(0, Extent.Y, 0));
		Beam->SetVariableFloat(TEXT("User.BeamWidth"), FMath::Max(0.1f, BeamWidth));
		Beam->SetSystemFixedBounds(FBox(FVector(-16, -Extent.Y - 16, -16), FVector(16, Extent.Y + 16, 16)));
	}
}

void AHeistLaserBarrierActor::SetBeamEffectsActive(const bool bActive)
{
	const bool bHasEffect = IsValid(BeamEffect);
	for (UNiagaraComponent* Beam : BeamEffectComponents)
	{
		if (!IsValid(Beam)) continue;
		const bool bShow = bActive && bHasEffect && GetNetMode() != NM_DedicatedServer;
		Beam->SetVisibility(bShow);
		if (bShow && !Beam->IsActive()) Beam->Activate(true);
		else if (!bShow && Beam->IsActive()) Beam->DeactivateImmediate();
	}
	if (IsValid(BeamVisualComponent)) BeamVisualComponent->SetVisibility(bActive && !bHasEffect, true);
}

void AHeistLaserBarrierActor::BeginPlay()
{
	Super::BeginPlay();
	ConfigureBeamEffects();

	BeamTriggerComponent->SetCollisionObjectType(ECC_WorldDynamic);
	BeamTriggerComponent->SetCollisionResponseToChannel(HeistCollisionChannels::Player, ECR_Overlap);
	BeamTriggerComponent->OnComponentBeginOverlap.AddDynamic(this, &AHeistLaserBarrierActor::HandleBeamBeginOverlap);
	BeamTriggerComponent->OnComponentEndOverlap.AddDynamic(this, &AHeistLaserBarrierActor::HandleBeamEndOverlap);

	if (HasAuthority())
	{
		AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
		BoundGameState = HeistGameState;
		if (IsValid(HeistGameState))
		{
			HeistGameState->GetMatchPhaseChangedDelegate().AddUObject(this, &AHeistLaserBarrierActor::HandleMatchPhaseChanged);
			HeistGameState->GetContractSnapshotChangedDelegate().AddUObject(this, &AHeistLaserBarrierActor::HandleContractSnapshotChanged);
		}

		RefreshRuntimeConfiguration();
		ScheduleConfigurationRefresh();
	}

	ApplyPresentation();
}

void AHeistLaserBarrierActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(GetWorld()))
	{
		GetWorldTimerManager().ClearTimer(RearmTimerHandle);
		GetWorldTimerManager().ClearTimer(ConfigurationRefreshTimerHandle);
	}
	RearmTimerHandle.Invalidate();
	ConfigurationRefreshTimerHandle.Invalidate();
	TrippedPlayersInsideBeam.Reset();
	if (BoundGameState.IsValid())
	{
		BoundGameState->GetMatchPhaseChangedDelegate().RemoveAll(this);
		BoundGameState->GetContractSnapshotChangedDelegate().RemoveAll(this);
		BoundGameState.Reset();
	}
	if (IsValid(BeamTriggerComponent))
	{
		BeamTriggerComponent->OnComponentBeginOverlap.RemoveAll(this);
		BeamTriggerComponent->OnComponentEndOverlap.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

bool AHeistLaserBarrierActor::IsBarrierEnabled() const
{
	return bBarrierEnabled;
}

bool AHeistLaserBarrierActor::IsBeamActive() const
{
	return bBarrierEnabled && bBeamActive;
}

bool AHeistLaserBarrierActor::IsRearming() const
{
	return bBarrierEnabled && bRearmGraceActive;
}

int32 AHeistLaserBarrierActor::GetSecurityRevision() const
{
	return SecurityRevision;
}

AHeistPlayerState* AHeistLaserBarrierActor::GetBypassHolderPlayerState() const
{
	return BypassHolderPlayerState.Get();
}

AHeistPaintingDisplayCaseActor* AHeistLaserBarrierActor::GetProtectedPaintingCase() const
{
	return ProtectedPaintingCase.Get();
}

bool AHeistLaserBarrierActor::TryActivateBypass(AHeistPlayerState* RequestingHolder)
{
	if (!HasAuthority() || !bBarrierEnabled || !bBeamActive || bRearmGraceActive || !IsValid(RequestingHolder) || RequestingHolder->IsArrested() || RequestingHolder->IsEscaped() ||
		!IsRuntimeConfigurationValid())
	{
		return false;
	}

	GetWorldTimerManager().ClearTimer(RearmTimerHandle);
	bBeamActive = false;
	bRearmGraceActive = false;
	BypassHolderPlayerState = RequestingHolder;
	++SecurityRevision;
	ForceNetUpdate();
	ApplyPresentation();
	return true;
}

bool AHeistLaserBarrierActor::BeginRearm(AHeistPlayerState* ReleasingHolder)
{
	if (!HasAuthority() || !bBarrierEnabled || bBeamActive || !IsValid(ReleasingHolder) || BypassHolderPlayerState.Get() != ReleasingHolder)
	{
		return false;
	}

	bRearmGraceActive = true;
	++SecurityRevision;
	const AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	const float SafeGrace = IsValid(HeistGameMode) ? HeistGameMode->GetSecurityLaserRearmGraceSeconds() : 0.75f;
	if (SafeGrace <= KINDA_SMALL_NUMBER)
	{
		CompleteRearm();
	}
	else
	{
		GetWorldTimerManager().SetTimer(RearmTimerHandle, this, &AHeistLaserBarrierActor::CompleteRearm, SafeGrace, false);
		ForceNetUpdate();
		ApplyPresentation();
	}
	return true;
}

void AHeistLaserBarrierActor::ForceRestoreDefaultState()
{
	if (!HasAuthority())
	{
		return;
	}

	const bool bWasBeamActive = IsBeamActive();
	GetWorldTimerManager().ClearTimer(RearmTimerHandle);
	bRearmGraceActive = false;
	BypassHolderPlayerState = nullptr;
	bBarrierEnabled = IsRuntimeConfigurationValid();
	bBeamActive = bBarrierEnabled;
	++SecurityRevision;
	ForceNetUpdate();
	ApplyPresentation();
	if (!bWasBeamActive && IsBeamActive())
	{
		TripOverlappingPlayers();
	}
}

bool AHeistLaserBarrierActor::IsRuntimeConfigurationValid() const
{
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!HasAuthority() || !IsValid(HeistGameState) || HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame || !IsValid(ProtectedPaintingCase) ||
		!ProtectedPaintingCase->IsContractExhibitActive())
	{
		return false;
	}

	const AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	FHeistArtifactDataRow ArtifactDefinition;
	if (!IsValid(HeistGameMode) || !HeistGameMode->TryGetArtifactDefinition(ProtectedPaintingCase->GetTargetArtifactId(), ArtifactDefinition) ||
		ArtifactDefinition.ItemGrade != EHeistLootGrade::FourStar || ArtifactDefinition.ForgeryType != EHeistForgeryType::Drawing)
	{
		return false;
	}

	const FHeistContractSnapshot& ContractSnapshot = HeistGameState->GetContractSnapshot();
	return ContractSnapshot.IsInitialized() && ContractSnapshot.RequiredTargetCaseId != ProtectedPaintingCase->GetDisplayCaseId();
}

bool AHeistLaserBarrierActor::IsEligibleEntrant(const AHeistPlayerCharacter* PlayerCharacter) const
{
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const AHeistPlayerState* PlayerState = IsValid(PlayerCharacter) ? PlayerCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	const bool bConnectedPlayerState = IsValid(HeistGameState) && HeistGameState->PlayerArray.ContainsByPredicate(
		[PlayerState](const TObjectPtr<APlayerState>& Candidate) { return Candidate.Get() == PlayerState; });
	return HasAuthority() && bBarrierEnabled && IsValid(HeistGameState) && HeistGameState->GetMatchPhase() == EHeistMatchPhase::InGame && IsValid(PlayerCharacter) &&
		IsValid(PlayerState) && bConnectedPlayerState && PlayerState->GetCrewStatus() != EHeistCrewStatus::Stunned && !PlayerState->IsArrested() && !PlayerState->IsEscaped();
}

void AHeistLaserBarrierActor::CompleteRearm()
{
	ForceRestoreDefaultState();
}

void AHeistLaserBarrierActor::TripOverlappingPlayers()
{
	if (!HasAuthority() || !IsBeamActive() || !IsValid(BeamTriggerComponent))
	{
		return;
	}

	TArray<AActor*> OverlappingPlayers;
	BeamTriggerComponent->GetOverlappingActors(OverlappingPlayers, AHeistPlayerCharacter::StaticClass());
	for (AActor* Actor : OverlappingPlayers)
	{
		AHeistPlayerCharacter* PlayerCharacter = Cast<AHeistPlayerCharacter>(Actor);
		if (IsEligibleEntrant(PlayerCharacter))
		{
			CommitTrip(PlayerCharacter);
		}
	}
}

void AHeistLaserBarrierActor::CommitTrip(AHeistPlayerCharacter* PlayerCharacter)
{
	AHeistPlayerState* TrippedPlayerState = IsValid(PlayerCharacter) ? PlayerCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	if (!HasAuthority() || !IsValid(TrippedPlayerState) || TrippedPlayersInsideBeam.Contains(PlayerCharacter))
	{
		return;
	}

	// Record trips, not harmless bypass entries, until the whole actor leaves the beam.
	TrippedPlayersInsideBeam.Add(PlayerCharacter);
	++TripSequence;
	++SecurityRevision;
	LastTrippedPlayerState = TrippedPlayerState;
	const FName IncidentId(*FString::Printf(TEXT("Laser_%s_%d"), *GetFName().ToString(), TripSequence));
	if (AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr)
	{
		HeistGameMode->RequestSecurityIncident(PlayerCharacter->GetActorLocation(), IncidentId);
	}
	ForceNetUpdate();
	ApplyPresentation();
	BP_PlayLaserTripPresentation(TrippedPlayerState, TripSequence);
	AppliedTripPresentationSequence = TripSequence;
}

void AHeistLaserBarrierActor::ApplyPresentation()
{
	if (bAppliedBarrierEnabled == bBarrierEnabled && bAppliedBeamActive == bBeamActive &&
		bAppliedRearmGraceActive == bRearmGraceActive && AppliedSecurityRevision == SecurityRevision &&
		AppliedBypassHolderPlayerState.Get() == BypassHolderPlayerState.Get())
	{
		return;
	}

	bAppliedBarrierEnabled = bBarrierEnabled;
	bAppliedBeamActive = bBeamActive;
	bAppliedRearmGraceActive = bRearmGraceActive;
	AppliedSecurityRevision = SecurityRevision;
	AppliedBypassHolderPlayerState = BypassHolderPlayerState;
	BP_ApplyLaserBarrierPresentation(bBarrierEnabled, IsBeamActive(), bRearmGraceActive, SecurityRevision, BypassHolderPlayerState.Get());
	SetBeamEffectsActive(IsBeamActive());
}

void AHeistLaserBarrierActor::ScheduleConfigurationRefresh()
{
	if (!HasAuthority() || !IsValid(GetWorld()))
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(ConfigurationRefreshTimerHandle);
	ConfigurationRefreshTimerHandle = GetWorldTimerManager().SetTimerForNextTick(this, &AHeistLaserBarrierActor::RefreshRuntimeConfiguration);
}

void AHeistLaserBarrierActor::RefreshRuntimeConfiguration()
{
	if (!HasAuthority())
	{
		return;
	}
	if (IsValid(GetWorld()))
	{
		GetWorldTimerManager().ClearTimer(ConfigurationRefreshTimerHandle);
	}
	ConfigurationRefreshTimerHandle.Invalidate();

	const bool bShouldEnable = IsRuntimeConfigurationValid();
	if (bShouldEnable == bBarrierEnabled)
	{
		return;
	}

	const bool bWasBeamActive = IsBeamActive();
	GetWorldTimerManager().ClearTimer(RearmTimerHandle);
	bBarrierEnabled = bShouldEnable;
	bBeamActive = bShouldEnable;
	bRearmGraceActive = false;
	BypassHolderPlayerState = nullptr;
	++SecurityRevision;
	ForceNetUpdate();
	ApplyPresentation();
	if (!bWasBeamActive && IsBeamActive())
	{
		TripOverlappingPlayers();
	}
}

void AHeistLaserBarrierActor::HandleMatchPhaseChanged(const EHeistMatchPhase, const EHeistMatchPhase NewMatchPhase)
{
	if (!HasAuthority())
	{
		return;
	}

	if (NewMatchPhase == EHeistMatchPhase::InGame)
	{
		RefreshRuntimeConfiguration();
		ScheduleConfigurationRefresh();
		return;
	}

	GetWorldTimerManager().ClearTimer(ConfigurationRefreshTimerHandle);
	GetWorldTimerManager().ClearTimer(RearmTimerHandle);
	TrippedPlayersInsideBeam.Reset();
	bBarrierEnabled = false;
	bBeamActive = false;
	bRearmGraceActive = false;
	BypassHolderPlayerState = nullptr;
	++SecurityRevision;
	ForceNetUpdate();
	ApplyPresentation();
}

void AHeistLaserBarrierActor::HandleContractSnapshotChanged(const FHeistContractSnapshot&)
{
	if (!HasAuthority())
	{
		return;
	}

	ScheduleConfigurationRefresh();
}

void AHeistLaserBarrierActor::HandleBeamBeginOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	AHeistPlayerCharacter* PlayerCharacter = Cast<AHeistPlayerCharacter>(OtherActor);
	if (!IsEligibleEntrant(PlayerCharacter))
	{
		return;
	}

	AHeistPlayerState* PlayerState = PlayerCharacter->GetPlayerState<AHeistPlayerState>();
	const bool bHolderTriedToUseOwnBypass = !bBeamActive && IsValid(BypassHolderPlayerState) && BypassHolderPlayerState.Get() == PlayerState;
	if (bBeamActive || bHolderTriedToUseOwnBypass)
	{
		CommitTrip(PlayerCharacter);
	}
}

void AHeistLaserBarrierActor::HandleBeamEndOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32)
{
	if (HasAuthority())
	{
		if (AHeistPlayerCharacter* PlayerCharacter = Cast<AHeistPlayerCharacter>(OtherActor))
		{
			if (!BeamTriggerComponent->IsOverlappingActor(PlayerCharacter))
			{
				TrippedPlayersInsideBeam.Remove(PlayerCharacter);
			}
		}
	}
}

void AHeistLaserBarrierActor::OnRep_LaserState()
{
	ApplyPresentation();
	if (IsValid(LastTrippedPlayerState) && TripSequence > AppliedTripPresentationSequence)
	{
		BP_PlayLaserTripPresentation(LastTrippedPlayerState.Get(), TripSequence);
		AppliedTripPresentationSequence = TripSequence;
	}
}

void AHeistLaserBarrierActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeistLaserBarrierActor, bBarrierEnabled);
	DOREPLIFETIME(AHeistLaserBarrierActor, bBeamActive);
	DOREPLIFETIME(AHeistLaserBarrierActor, bRearmGraceActive);
	DOREPLIFETIME(AHeistLaserBarrierActor, SecurityRevision);
	DOREPLIFETIME(AHeistLaserBarrierActor, BypassHolderPlayerState);
	DOREPLIFETIME(AHeistLaserBarrierActor, LastTrippedPlayerState);
	DOREPLIFETIME(AHeistLaserBarrierActor, TripSequence);
}
