#include "World/Actors/Security/HeistSecurityCameraActor.h"

#include "Character/HeistPlayerCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "TimerManager.h"

AHeistSecurityCameraActor::AHeistSecurityCameraActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(10.0f);

	SceneRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRootComponent"));
	SetRootComponent(SceneRootComponent);

	SensorOriginComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SensorOriginComponent"));
	SensorOriginComponent->SetupAttachment(SceneRootComponent);

	VisualMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMeshComponent"));
	VisualMeshComponent->SetupAttachment(SensorOriginComponent);
	VisualMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMeshComponent->SetGenerateOverlapEvents(false);

	SightLightComponent = CreateDefaultSubobject<USpotLightComponent>(TEXT("SightLightComponent"));
	SightLightComponent->SetupAttachment(SensorOriginComponent);
	SightLightComponent->SetMobility(EComponentMobility::Movable);
	SightLightComponent->SetIntensityUnits(ELightUnits::Lumens);
	SightLightComponent->SetIntensity(1800.0f);
	SightLightComponent->SetIndirectLightingIntensity(0.0f);
	SightLightComponent->SetVolumetricScatteringIntensity(0.0f);
	SightLightComponent->SetCastShadows(true);
	ConfigureSightLight();

	CameraPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("CameraPerceptionComponent"));
	CameraSightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("CameraSightConfig"));
	CameraSightConfig->SetStartsEnabled(false);
	CameraSightConfig->SightRadius = DetectionRange;
	CameraSightConfig->LoseSightRadius = DetectionRange;
	CameraSightConfig->PeripheralVisionAngleDegrees = DetectionHalfAngleDegrees;
	CameraSightConfig->AutoSuccessRangeFromLastSeenLocation = FAISystem::InvalidRange;
	CameraSightConfig->DetectionByAffiliation.bDetectEnemies = true;
	CameraSightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	CameraSightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	CameraPerceptionComponent->ConfigureSense(*CameraSightConfig);
	CameraPerceptionComponent->SetDominantSense(UAISense_Sight::StaticClass());
}

void AHeistSecurityCameraActor::BeginPlay()
{
	Super::BeginPlay();

	InitialVisualRelativeRotation = VisualMeshComponent->GetRelativeRotation().Quaternion();
	ConfigureSightLight();
	CameraPerceptionComponent->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &AHeistSecurityCameraActor::HandleTargetPerceptionUpdated);
	if (HasAuthority())
	{
		CameraSightConfig->SightRadius = FMath::Max(100.0f, DetectionRange);
		CameraSightConfig->LoseSightRadius = CameraSightConfig->SightRadius;
		CameraSightConfig->PeripheralVisionAngleDegrees = FMath::Clamp(DetectionHalfAngleDegrees, 1.0f, 89.0f);
		CameraPerceptionComponent->ConfigureSense(*CameraSightConfig);
	}

	if (HasAuthority())
	{
		AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
		BoundGameState = HeistGameState;
		if (IsValid(HeistGameState))
		{
			HeistGameState->GetMatchPhaseChangedDelegate().AddUObject(this, &AHeistSecurityCameraActor::HandleMatchPhaseChanged);
		}

		if (IsValid(HeistGameState) && HeistGameState->GetMatchPhase() == EHeistMatchPhase::InGame)
		{
			StartAuthorityEvaluation();
		}
	}

	ApplyPresentation();
}

void AHeistSecurityCameraActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAuthorityEvaluation(true);
	if (BoundGameState.IsValid())
	{
		BoundGameState->GetMatchPhaseChangedDelegate().RemoveAll(this);
		BoundGameState.Reset();
	}
	CameraPerceptionComponent->OnTargetPerceptionUpdated.RemoveAll(this);
	Super::EndPlay(EndPlayReason);
}

void AHeistSecurityCameraActor::GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	OutLocation = IsValid(SensorOriginComponent) ? SensorOriginComponent->GetComponentLocation() : GetActorLocation();
	OutRotation = ResolveSensorForward().Rotation();
}

void AHeistSecurityCameraActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Preserve the authored mesh orientation while showing the same sweep used by Sight.
	VisualMeshComponent->SetRelativeRotation(FQuat(FVector::UpVector, FMath::DegreesToRadians(GetResolvedSweepYawDegrees())) * InitialVisualRelativeRotation);
	SightLightComponent->SetWorldRotation(ResolveSensorForward().Rotation());
	const bool bConfirmed = GetWorld()->GetTimeSeconds() < ConfirmedLightHoldUntil;
	DisplayedLightRisk = bConfirmed ? 1.0f : FMath::FInterpTo(DisplayedLightRisk, GetDetectionProgress(), DeltaSeconds, 12.0f);
	SightLightComponent->SetLightColor(FMath::Lerp(IdleLightColor, AlertLightColor, DisplayedLightRisk));
}

void AHeistSecurityCameraActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ConfigureSightLight();
}

void AHeistSecurityCameraActor::ConfigureSightLight()
{
	SightLightComponent->SetAttenuationRadius(FMath::Max(100.0f, DetectionRange));
	SightLightComponent->SetOuterConeAngle(FMath::Clamp(DetectionHalfAngleDegrees, 1.0f, 89.0f));
	SightLightComponent->SetInnerConeAngle(SightLightComponent->OuterConeAngle * 0.9f);
	SightLightComponent->SetLightColor(IdleLightColor);
}

bool AHeistSecurityCameraActor::IsCameraEnabled() const
{
	return bCameraEnabled;
}

float AHeistSecurityCameraActor::GetDetectionProgress() const
{
	return static_cast<float>(DetectionProgressByte) / 255.0f;
}

int32 AHeistSecurityCameraActor::GetDetectionRevision() const
{
	return ConfirmedDetectionRevision;
}

AHeistPlayerState* AHeistSecurityCameraActor::GetLastDetectedPlayerState() const
{
	return LastDetectedPlayerState.Get();
}

float AHeistSecurityCameraActor::GetResolvedSweepYawDegrees() const
{
	const float SafePeriod = FMath::Max(0.1f, SweepPeriodSeconds);
	const float SweepTime = IsSweepPaused() ? SweepPausedServerTime : ResolveServerWorldTimeSeconds();
	const float ElapsedSeconds = FMath::Max(0.0f, SweepTime - SweepEpochServerTime);
	return FMath::Sin((ElapsedSeconds / SafePeriod) * UE_TWO_PI) * FMath::Clamp(SweepHalfAngleDegrees, 0.0f, 90.0f);
}

bool AHeistSecurityCameraActor::IsSweepPaused() const
{
	return SweepPausedServerTime >= 0.0f;
}

void AHeistSecurityCameraActor::StartAuthorityEvaluation()
{
	if (!HasAuthority() || !IsValid(GetWorld()))
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(SweepResumeTimerHandle);
	SweepPausedServerTime = -1.0f;
	SweepEpochServerTime = ResolveServerWorldTimeSeconds();
	bCameraEnabled = true;
	CameraPerceptionComponent->SetSenseEnabled(UAISense_Sight::StaticClass(), true);
	DetectionCooldownEndServerTime = 0.0f;
	const float SafeInterval = ResolveEvaluationIntervalSeconds();
	GetWorldTimerManager().SetTimer(DetectionEvaluationTimerHandle, this, &AHeistSecurityCameraActor::EvaluateDetectionCandidates, SafeInterval, true, SafeInterval);
	ForceNetUpdate();
	ApplyPresentation();
}

void AHeistSecurityCameraActor::StopAuthorityEvaluation(const bool bResetReplicatedState)
{
	if (IsValid(GetWorld()))
	{
		GetWorldTimerManager().ClearTimer(DetectionEvaluationTimerHandle);
		GetWorldTimerManager().ClearTimer(SweepResumeTimerHandle);
	}
	DetectionEvaluationTimerHandle.Invalidate();
	SweepResumeTimerHandle.Invalidate();
	if (HasAuthority() && bResetReplicatedState)
	{
		bCameraEnabled = false;
		SweepPausedServerTime = -1.0f;
	}
	CameraPerceptionComponent->SetSenseEnabled(UAISense_Sight::StaticClass(), false);
	CameraPerceptionComponent->ForgetAll();
	DetectionBuildUpByPlayer.Reset();
	DetectionCooldownEndServerTime = 0.0f;

	if (HasAuthority() && bResetReplicatedState)
	{
		bCameraEnabled = false;
		bTrackingAnyTarget = false;
		DetectionProgressByte = 0;
		LastDetectedPlayerState = nullptr;
		ForceNetUpdate();
		ApplyPresentation();
	}
}

void AHeistSecurityCameraActor::EvaluateDetectionCandidates()
{
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!HasAuthority() || !bCameraEnabled || !IsValid(HeistGameState) || HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame)
	{
		StopAuthorityEvaluation(true);
		return;
	}

	const float Now = ResolveServerWorldTimeSeconds();
	const float SafeInterval = ResolveEvaluationIntervalSeconds();
	const float SafeBuildUp = ResolveDetectionBuildUpSeconds();
	const bool bCooldownActive = Now < DetectionCooldownEndServerTime;
	TArray<AActor*> VisibleActors;
	CameraPerceptionComponent->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), VisibleActors);
	UpdateSweepPauseState(VisibleActors);
	for (auto It = DetectionBuildUpByPlayer.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !VisibleActors.Contains(It.Key().Get())) It.RemoveCurrent();
	}
	AHeistPlayerCharacter* ConfirmedTarget = nullptr;

	for (AActor* VisibleActor : VisibleActors)
	{
		AHeistPlayerCharacter* PlayerCharacter = Cast<AHeistPlayerCharacter>(VisibleActor);
		if (!IsValid(PlayerCharacter)) continue;

		float& BuildUp = DetectionBuildUpByPlayer.FindOrAdd(PlayerCharacter);
		if (bCooldownActive || !IsEligibleTarget(PlayerCharacter))
		{
			BuildUp = 0.0f;
			continue;
		}

		BuildUp = FMath::Min(SafeBuildUp, BuildUp + SafeInterval);
		if (BuildUp >= SafeBuildUp - KINDA_SMALL_NUMBER)
		{
			ConfirmedTarget = PlayerCharacter;
			break;
		}
	}

	if (IsValid(ConfirmedTarget))
	{
		CommitDetection(ConfirmedTarget);
	}
	RefreshReplicatedDetectionProgress();
}

void AHeistSecurityCameraActor::UpdateSweepPauseState(const TArray<AActor*>& VisibleActors)
{
	if (!HasAuthority() || !bCameraEnabled) return;
	const bool bHasVisibleTarget = VisibleActors.ContainsByPredicate([this](const AActor* Actor)
	{
		return IsEligibleTarget(Cast<AHeistPlayerCharacter>(Actor));
	});
	if (bHasVisibleTarget)
	{
		GetWorldTimerManager().ClearTimer(SweepResumeTimerHandle);
		if (!IsSweepPaused())
		{
			SweepPausedServerTime = ResolveServerWorldTimeSeconds();
			ForceNetUpdate();
			ApplyPresentation();
		}
	}
	else if (IsSweepPaused() && !GetWorldTimerManager().IsTimerActive(SweepResumeTimerHandle))
	{
		const AHeistGameMode* GameMode = GetWorld()->GetAuthGameMode<AHeistGameMode>();
		const float Delay = IsValid(GameMode) ? GameMode->GetSecurityCameraResumeDelaySeconds() : 1.5f;
		if (Delay <= 0.0f)
		{
			ResumeSweepAfterSightLoss();
		}
		else
		{
			GetWorldTimerManager().SetTimer(SweepResumeTimerHandle, this, &AHeistSecurityCameraActor::ResumeSweepAfterSightLoss, Delay, false);
		}
	}
}

void AHeistSecurityCameraActor::ResumeSweepAfterSightLoss()
{
	if (!HasAuthority() || !bCameraEnabled || !IsSweepPaused()) return;
	TArray<AActor*> VisibleActors;
	CameraPerceptionComponent->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), VisibleActors);
	if (VisibleActors.ContainsByPredicate([this](const AActor* Actor)
	{
		return IsEligibleTarget(Cast<AHeistPlayerCharacter>(Actor));
	})) return;

	// Preserve both the sine phase and its direction, including across long holds.
	SweepEpochServerTime += FMath::Max(0.0f, ResolveServerWorldTimeSeconds() - SweepPausedServerTime);
	SweepPausedServerTime = -1.0f;
	ForceNetUpdate();
	ApplyPresentation();
}

bool AHeistSecurityCameraActor::IsEligibleTarget(const AHeistPlayerCharacter* PlayerCharacter) const
{
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const AHeistPlayerState* HeistPlayerState = IsValid(PlayerCharacter) ? PlayerCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	const bool bConnectedPlayerState = IsValid(HeistGameState) && HeistGameState->PlayerArray.ContainsByPredicate(
		[HeistPlayerState](const TObjectPtr<APlayerState>& Candidate) { return Candidate.Get() == HeistPlayerState; });
	if (!HasAuthority() || !bCameraEnabled || !IsValid(HeistGameState) || HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame ||
		!IsValid(PlayerCharacter) || !IsValid(HeistPlayerState) || !bConnectedPlayerState || HeistPlayerState->GetCrewStatus() == EHeistCrewStatus::Stunned ||
		HeistPlayerState->IsEscaped() || HeistPlayerState->IsArrested() || HeistPlayerState->IsProtectedByDetention())
	{
		return false;
	}

	return true;
}

FVector AHeistSecurityCameraActor::ResolveSensorForward() const
{
	const FVector BaseForward = IsValid(SensorOriginComponent) ? SensorOriginComponent->GetForwardVector() : GetActorForwardVector();
	const FVector UpVector = IsValid(SensorOriginComponent) ? SensorOriginComponent->GetUpVector() : GetActorUpVector();
	return BaseForward.RotateAngleAxis(GetResolvedSweepYawDegrees(), UpVector).GetSafeNormal();
}

float AHeistSecurityCameraActor::ResolveServerWorldTimeSeconds() const
{
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	return IsValid(HeistGameState) ? HeistGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
}

float AHeistSecurityCameraActor::ResolveEvaluationIntervalSeconds() const
{
	const AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	return IsValid(HeistGameMode) ? HeistGameMode->GetSecurityCameraEvaluationIntervalSeconds() : 0.15f;
}

float AHeistSecurityCameraActor::ResolveDetectionBuildUpSeconds() const
{
	const AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	return IsValid(HeistGameMode) ? HeistGameMode->GetSecurityCameraDetectionBuildUpSeconds() : 2.025f;
}

float AHeistSecurityCameraActor::ResolveDetectionCooldownSeconds() const
{
	const AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr;
	return IsValid(HeistGameMode) ? HeistGameMode->GetSecurityCameraDetectionCooldownSeconds() : 4.0f;
}

void AHeistSecurityCameraActor::CommitDetection(AHeistPlayerCharacter* PlayerCharacter)
{
	AHeistPlayerState* DetectedPlayerState = IsValid(PlayerCharacter) ? PlayerCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	if (!HasAuthority() || !IsValid(DetectedPlayerState))
	{
		return;
	}

	++ConfirmedDetectionRevision;
	LastDetectedPlayerState = DetectedPlayerState;
	DetectionCooldownEndServerTime = ResolveServerWorldTimeSeconds() + ResolveDetectionCooldownSeconds();
	for (TPair<TWeakObjectPtr<AHeistPlayerCharacter>, float>& Entry : DetectionBuildUpByPlayer)
	{
		Entry.Value = 0.0f;
	}

	const FName IncidentId(*FString::Printf(TEXT("CCTV_%s_%d"), *GetFName().ToString(), ConfirmedDetectionRevision));
	if (AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr)
	{
		HeistGameMode->RequestSecurityIncident(PlayerCharacter->GetActorLocation(), IncidentId);
	}
	ForceNetUpdate();
	ApplyPresentation();
}

void AHeistSecurityCameraActor::RefreshReplicatedDetectionProgress()
{
	float HighestBuildUp = 0.0f;
	for (const TPair<TWeakObjectPtr<AHeistPlayerCharacter>, float>& Entry : DetectionBuildUpByPlayer)
	{
		HighestBuildUp = FMath::Max(HighestBuildUp, Entry.Value);
	}
	const float SafeBuildUp = ResolveDetectionBuildUpSeconds();
	const uint8 NewProgressByte = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt((HighestBuildUp / SafeBuildUp) * 255.0f), 0, 255));
	const bool bNewTrackingAnyTarget = NewProgressByte > 0;
	if (DetectionProgressByte == NewProgressByte && bTrackingAnyTarget == bNewTrackingAnyTarget)
	{
		return;
	}

	DetectionProgressByte = NewProgressByte;
	bTrackingAnyTarget = bNewTrackingAnyTarget;
	ForceNetUpdate();
	ApplyPresentation();
}

void AHeistSecurityCameraActor::ApplyPresentation()
{
	SetActorTickEnabled(bCameraEnabled && GetNetMode() != NM_DedicatedServer);
	SightLightComponent->SetVisibility(bCameraEnabled && GetNetMode() != NM_DedicatedServer);
	if (!bCameraEnabled)
	{
		ConfirmedLightHoldUntil = 0.0f;
		DisplayedLightRisk = 0.0f;
		SightLightComponent->SetLightColor(IdleLightColor);
	}
	else if (AppliedDetectionRevision != INDEX_NONE && ConfirmedDetectionRevision > AppliedDetectionRevision)
	{
		// Confirmation resets build-up, so preserve a short red signal independently of it.
		ConfirmedLightHoldUntil = GetWorld()->GetTimeSeconds() + 0.75f;
		DisplayedLightRisk = 1.0f;
		SightLightComponent->SetLightColor(AlertLightColor);
	}
	if (bAppliedCameraEnabled == bCameraEnabled && bAppliedTrackingAnyTarget == bTrackingAnyTarget &&
		AppliedDetectionProgressByte == DetectionProgressByte && AppliedDetectionRevision == ConfirmedDetectionRevision &&
		AppliedDetectedPlayerState.Get() == LastDetectedPlayerState.Get() && FMath::IsNearlyEqual(AppliedSweepEpochServerTime, SweepEpochServerTime))
	{
		return;
	}

	bAppliedCameraEnabled = bCameraEnabled;
	bAppliedTrackingAnyTarget = bTrackingAnyTarget;
	AppliedDetectionProgressByte = DetectionProgressByte;
	AppliedDetectionRevision = ConfirmedDetectionRevision;
	AppliedDetectedPlayerState = LastDetectedPlayerState;
	AppliedSweepEpochServerTime = SweepEpochServerTime;
	if (IsValid(VisualMeshComponent))
	{
		VisualMeshComponent->SetVisibility(bCameraEnabled, true);
	}
	BP_ApplySecurityCameraPresentation(bCameraEnabled, bTrackingAnyTarget, GetDetectionProgress(), ConfirmedDetectionRevision, LastDetectedPlayerState.Get());
}

void AHeistSecurityCameraActor::HandleMatchPhaseChanged(const EHeistMatchPhase, const EHeistMatchPhase NewMatchPhase)
{
	if (!HasAuthority())
	{
		return;
	}

	if (NewMatchPhase == EHeistMatchPhase::InGame)
	{
		StartAuthorityEvaluation();
	}
	else
	{
		StopAuthorityEvaluation(true);
	}
}

void AHeistSecurityCameraActor::HandleTargetPerceptionUpdated(AActor* TargetActor, FAIStimulus Stimulus)
{
	if (!HasAuthority() || !bCameraEnabled || Stimulus.Type != UAISense::GetSenseID<UAISense_Sight>()) return;
	if (AHeistPlayerCharacter* Player = Cast<AHeistPlayerCharacter>(TargetActor))
	{
		if (!Stimulus.WasSuccessfullySensed())
		{
			DetectionBuildUpByPlayer.Remove(Player);
			RefreshReplicatedDetectionProgress();
		}
		TArray<AActor*> VisibleActors;
		CameraPerceptionComponent->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), VisibleActors);
		UpdateSweepPauseState(VisibleActors);
	}
}

void AHeistSecurityCameraActor::OnRep_SecurityCameraState()
{
	ApplyPresentation();
}

void AHeistSecurityCameraActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeistSecurityCameraActor, bCameraEnabled);
	DOREPLIFETIME(AHeistSecurityCameraActor, bTrackingAnyTarget);
	DOREPLIFETIME(AHeistSecurityCameraActor, DetectionProgressByte);
	DOREPLIFETIME(AHeistSecurityCameraActor, ConfirmedDetectionRevision);
	DOREPLIFETIME(AHeistSecurityCameraActor, LastDetectedPlayerState);
	DOREPLIFETIME(AHeistSecurityCameraActor, SweepEpochServerTime);
	DOREPLIFETIME(AHeistSecurityCameraActor, SweepPausedServerTime);
}
