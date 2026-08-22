#include "World/Actors/Security/HeistSecurityCameraActor.h"

#include "Character/HeistPlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HeistCollisionChannels.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AHeistSecurityCameraActor::AHeistSecurityCameraActor()
{
	PrimaryActorTick.bCanEverTick = false;
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

	DetectionVolumeComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("DetectionVolumeComponent"));
	DetectionVolumeComponent->SetupAttachment(SceneRootComponent);
	DetectionVolumeComponent->InitBoxExtent(FVector(900.0f, 900.0f, 300.0f));
	DetectionVolumeComponent->SetRelativeLocation(FVector(900.0f, 0.0f, 0.0f));
	DetectionVolumeComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DetectionVolumeComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	DetectionVolumeComponent->SetGenerateOverlapEvents(true);
}

void AHeistSecurityCameraActor::BeginPlay()
{
	Super::BeginPlay();

	DetectionVolumeComponent->SetCollisionObjectType(ECC_WorldDynamic);
	DetectionVolumeComponent->SetCollisionResponseToChannel(HeistCollisionChannels::Player, ECR_Overlap);
	DetectionVolumeComponent->OnComponentBeginOverlap.AddDynamic(this, &AHeistSecurityCameraActor::HandleDetectionVolumeBeginOverlap);
	DetectionVolumeComponent->OnComponentEndOverlap.AddDynamic(this, &AHeistSecurityCameraActor::HandleDetectionVolumeEndOverlap);

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
	if (IsValid(DetectionVolumeComponent))
	{
		DetectionVolumeComponent->OnComponentBeginOverlap.RemoveAll(this);
		DetectionVolumeComponent->OnComponentEndOverlap.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
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
	const float ElapsedSeconds = FMath::Max(0.0f, ResolveServerWorldTimeSeconds() - SweepEpochServerTime);
	return FMath::Sin((ElapsedSeconds / SafePeriod) * UE_TWO_PI) * FMath::Clamp(SweepHalfAngleDegrees, 0.0f, 90.0f);
}

void AHeistSecurityCameraActor::StartAuthorityEvaluation()
{
	if (!HasAuthority() || !IsValid(GetWorld()))
	{
		return;
	}

	bCameraEnabled = true;
	SweepEpochServerTime = ResolveServerWorldTimeSeconds();
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
	}
	DetectionEvaluationTimerHandle.Invalidate();
	OverlappingPlayers.Reset();
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
	TArray<TWeakObjectPtr<AHeistPlayerCharacter>> StalePlayers;
	AHeistPlayerCharacter* ConfirmedTarget = nullptr;

	for (const TWeakObjectPtr<AHeistPlayerCharacter>& PlayerPtr : OverlappingPlayers)
	{
		AHeistPlayerCharacter* PlayerCharacter = PlayerPtr.Get();
		if (!IsValid(PlayerCharacter) || !DetectionVolumeComponent->IsOverlappingActor(PlayerCharacter))
		{
			StalePlayers.Add(PlayerPtr);
			continue;
		}

		float& BuildUp = DetectionBuildUpByPlayer.FindOrAdd(PlayerPtr);
		if (bCooldownActive || !IsEligibleTarget(PlayerCharacter) || !HasDetectionLineOfSight(PlayerCharacter))
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

	for (const TWeakObjectPtr<AHeistPlayerCharacter>& StalePlayer : StalePlayers)
	{
		OverlappingPlayers.Remove(StalePlayer);
		DetectionBuildUpByPlayer.Remove(StalePlayer);
	}

	if (IsValid(ConfirmedTarget))
	{
		CommitDetection(ConfirmedTarget);
	}
	RefreshReplicatedDetectionProgress();
}

bool AHeistSecurityCameraActor::IsEligibleTarget(const AHeistPlayerCharacter* PlayerCharacter) const
{
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const AHeistPlayerState* HeistPlayerState = IsValid(PlayerCharacter) ? PlayerCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	const bool bConnectedPlayerState = IsValid(HeistGameState) && HeistGameState->PlayerArray.ContainsByPredicate(
		[HeistPlayerState](const TObjectPtr<APlayerState>& Candidate) { return Candidate.Get() == HeistPlayerState; });
	if (!HasAuthority() || !bCameraEnabled || !IsValid(HeistGameState) || HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame ||
		!IsValid(PlayerCharacter) || !IsValid(HeistPlayerState) || !bConnectedPlayerState || HeistPlayerState->GetCrewStatus() == EHeistCrewStatus::Stunned ||
		HeistPlayerState->IsEscaped() || HeistPlayerState->IsArrested())
	{
		return false;
	}

	const FVector ToTarget = PlayerCharacter->GetActorLocation() - SensorOriginComponent->GetComponentLocation();
	const float Distance = ToTarget.Size();
	if (Distance <= KINDA_SMALL_NUMBER || Distance > FMath::Max(100.0f, DetectionRange))
	{
		return false;
	}

	const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(DetectionHalfAngleDegrees, 1.0f, 89.0f)));
	return FVector::DotProduct(ResolveSensorForward(), ToTarget / Distance) >= MinimumDot;
}

bool AHeistSecurityCameraActor::HasDetectionLineOfSight(const AHeistPlayerCharacter* PlayerCharacter) const
{
	if (!IsValid(GetWorld()) || !IsValid(PlayerCharacter))
	{
		return false;
	}

	FVector TargetLocation;
	FRotator TargetRotation;
	PlayerCharacter->GetActorEyesViewPoint(TargetLocation, TargetRotation);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HeistSecurityCameraLOS), false);
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(PlayerCharacter);
	return !GetWorld()->LineTraceTestByChannel(SensorOriginComponent->GetComponentLocation(), TargetLocation, ECC_Visibility, QueryParams);
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
	return IsValid(HeistGameMode) ? HeistGameMode->GetSecurityCameraDetectionBuildUpSeconds() : 1.35f;
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

void AHeistSecurityCameraActor::HandleDetectionVolumeBeginOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (HasAuthority())
	{
		if (AHeistPlayerCharacter* PlayerCharacter = Cast<AHeistPlayerCharacter>(OtherActor))
		{
			OverlappingPlayers.Add(PlayerCharacter);
			DetectionBuildUpByPlayer.FindOrAdd(PlayerCharacter) = 0.0f;
		}
	}
}

void AHeistSecurityCameraActor::HandleDetectionVolumeEndOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32)
{
	if (HasAuthority())
	{
		if (AHeistPlayerCharacter* PlayerCharacter = Cast<AHeistPlayerCharacter>(OtherActor))
		{
			OverlappingPlayers.Remove(PlayerCharacter);
			DetectionBuildUpByPlayer.Remove(PlayerCharacter);
			RefreshReplicatedDetectionProgress();
		}
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
}
