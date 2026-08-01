#include "AI/HeistGuardAIController.h"

#include "AI/HeistGuardCharacter.h"
#include "AI/HeistGuardNoiseReactionComponent.h"
#include "AI/HeistGuardStateComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Components/StateTreeAIComponent.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistGameplayTags.h"
#include "Core/HeistPlayerState.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Components/PrimitiveComponent.h"
#include "Core/HeistGameState.h"
#include "Core/HeistLogChannels.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Inventory/HeistItemDataTypes.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "TimerManager.h"
#include "World/Actors/Escape/HeistVentActor.h"
#include "World/Actors/Loot/HeistObjectDisplayCaseActor.h"
#include "World/Actors/Loot/HeistPaintingDisplayCaseActor.h"

namespace
{
float ResolveAlertMultiplier(const FVector4& Multipliers, const EHeistAlertLevel AlertLevel)
{
	float Multiplier = 1.0f;
	switch (AlertLevel)
	{
	case EHeistAlertLevel::Suspicious:
		Multiplier = Multipliers.X;
		break;
	case EHeistAlertLevel::Searching:
		Multiplier = Multipliers.Y;
		break;
	case EHeistAlertLevel::Alarmed:
		Multiplier = Multipliers.Z;
		break;
	case EHeistAlertLevel::Lockdown:
		Multiplier = Multipliers.W;
		break;
	case EHeistAlertLevel::Quiet:
	default:
		break;
	}

	return FMath::Max(0.0f, FMath::IsFinite(Multiplier) ? Multiplier : 1.0f);
}

bool IsValidInspectionCandidate(const AActor* Candidate)
{
	if (const AHeistPaintingDisplayCaseActor* PaintingCase = Cast<AHeistPaintingDisplayCaseActor>(Candidate))
	{
		return PaintingCase->IsValidInspectionCandidate();
	}
	if (const AHeistObjectDisplayCaseActor* ObjectCase = Cast<AHeistObjectDisplayCaseActor>(Candidate))
	{
		return ObjectCase->IsValidInspectionCandidate();
	}
	return false;
}

FName GetInspectionCaseId(const AActor* Candidate)
{
	if (const AHeistPaintingDisplayCaseActor* PaintingCase = Cast<AHeistPaintingDisplayCaseActor>(Candidate))
	{
		return PaintingCase->GetDisplayCaseId();
	}
	if (const AHeistObjectDisplayCaseActor* ObjectCase = Cast<AHeistObjectDisplayCaseActor>(Candidate))
	{
		return ObjectCase->GetObjectCaseId();
	}
	return NAME_None;
}

bool TryBeginCaseInspection(AActor* Candidate, AActor* InspectingGuard)
{
	if (AHeistPaintingDisplayCaseActor* PaintingCase = Cast<AHeistPaintingDisplayCaseActor>(Candidate))
	{
		return PaintingCase->TryBeginInspection(InspectingGuard);
	}
	if (AHeistObjectDisplayCaseActor* ObjectCase = Cast<AHeistObjectDisplayCaseActor>(Candidate))
	{
		return ObjectCase->TryBeginInspection(InspectingGuard);
	}
	return false;
}

bool IsCaseInspectionOwnedBy(const AActor* Candidate, const AActor* InspectingGuard)
{
	if (const AHeistPaintingDisplayCaseActor* PaintingCase = Cast<AHeistPaintingDisplayCaseActor>(Candidate))
	{
		return PaintingCase->IsInspectionOwnedBy(InspectingGuard);
	}
	if (const AHeistObjectDisplayCaseActor* ObjectCase = Cast<AHeistObjectDisplayCaseActor>(Candidate))
	{
		return ObjectCase->IsInspectionOwnedBy(InspectingGuard);
	}
	return false;
}

bool InterruptCaseInspection(AActor* Candidate, AActor* InspectingGuard, const FName Reason)
{
	if (AHeistPaintingDisplayCaseActor* PaintingCase = Cast<AHeistPaintingDisplayCaseActor>(Candidate))
	{
		return PaintingCase->InterruptInspection(InspectingGuard, Reason);
	}
	if (AHeistObjectDisplayCaseActor* ObjectCase = Cast<AHeistObjectDisplayCaseActor>(Candidate))
	{
		return ObjectCase->InterruptInspection(InspectingGuard, Reason);
	}
	return false;
}

bool ApplyCaseInspectionResult(AActor* Candidate, AActor* InspectingGuard)
{
	if (AHeistPaintingDisplayCaseActor* PaintingCase = Cast<AHeistPaintingDisplayCaseActor>(Candidate))
	{
		return PaintingCase->ApplyInspectionResult(InspectingGuard);
	}
	if (AHeistObjectDisplayCaseActor* ObjectCase = Cast<AHeistObjectDisplayCaseActor>(Candidate))
	{
		return ObjectCase->ApplyInspectionResult(InspectingGuard);
	}
	return false;
}
}

#pragma region Construction

AHeistGuardAIController::AHeistGuardAIController()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	GuardPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("GuardPerceptionComponent"));
	GuardSightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("GuardSightConfig"));
	SetPerceptionComponent(*GuardPerceptionComponent);

	GuardStateTreeComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("GuardStateTreeComponent"));
	GuardStateTreeComponent->SetStartLogicAutomatically(false);
	BrainComponent = GuardStateTreeComponent;
}

#pragma endregion

#pragma region Lifecycle

void AHeistGuardAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	AHeistGuardCharacter* GuardCharacter = CastChecked<AHeistGuardCharacter>(InPawn);
	UHeistGuardStateComponent* GuardStateComponent = GuardCharacter->GetGuardStateComponent();
	checkf(IsValid(GuardStateComponent), TEXT("HeistGuardAIController requires GuardStateComponent."));
	checkf(IsValid(GuardPerceptionComponent), TEXT("HeistGuardAIController requires GuardPerceptionComponent."));
	checkf(IsValid(GuardSightConfig), TEXT("HeistGuardAIController requires GuardSightConfig."));

	GuardStateComponent->GetGuardStateChangedDelegate().AddUObject(this, &AHeistGuardAIController::HandleGuardStateChanged);
	GuardStateComponent->GetInspectExhibitCastExpiredDelegate().AddUObject(this, &AHeistGuardAIController::HandleInspectionCastExpired);
	GuardPerceptionComponent->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &AHeistGuardAIController::HandleTargetPerceptionUpdated);
	if (HasAuthority())
	{
		if (AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr)
		{
			HeistGameState->GetAlertStateChangedDelegate().AddUObject(this, &AHeistGuardAIController::HandleAlertStateChanged);
			HeistGameState->GetMatchPhaseChangedDelegate().AddUObject(this, &AHeistGuardAIController::HandleMatchPhaseChanged);
		}
	}

	if (GuardCharacter->HasResolvedGuardProfile())
	{
		ConfigurePerceptionFromGuardProfile(GuardCharacter->GetGuardProfile());
	}

	if (HasAuthority() && bStartStateTreeAutomatically)
	{
		GuardStateTreeComponent->StartLogic();
		SendGuardStateTreeEvent(GuardStateComponent->GetGuardState());
	}

	HandleGuardStateChanged(GuardStateComponent->GetGuardState(), GuardStateComponent->GetGuardState());
}

void AHeistGuardAIController::OnUnPossess()
{
	if (AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr)
	{
		HeistGameState->GetAlertStateChangedDelegate().RemoveAll(this);
		HeistGameState->GetMatchPhaseChangedDelegate().RemoveAll(this);
	}

	StopMovement();
	AbortInspection(FName(TEXT("GuardUnpossessed")));
	if (InspectionTarget.IsValid())
	{
		InspectionTarget.Reset();
		++InspectionTargetSelectionRevision;
	}
	ClearDetectionGrace(TEXT("GuardUnpossessed"));
	ClearSightValidationTimer();
	GuardPerceptionComponent->OnTargetPerceptionUpdated.RemoveDynamic(this, &AHeistGuardAIController::HandleTargetPerceptionUpdated);

	if (AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn()))
	{
		if (UHeistGuardStateComponent* GuardStateComponent = GuardCharacter->GetGuardStateComponent())
		{
			if (HasAuthority() && GuardStateComponent->GetGuardState() == EHeistGuardState::InspectExhibit)
			{
				GuardStateComponent->EnterPatrol();
			}
			GuardStateComponent->GetGuardStateChangedDelegate().RemoveAll(this);
			GuardStateComponent->GetInspectExhibitCastExpiredDelegate().RemoveAll(this);
		}
	}

	if (IsValid(GuardStateTreeComponent) && GuardStateTreeComponent->IsRunning())
	{
		GuardStateTreeComponent->StopLogic(TEXT("Guard unpossessed"));
	}

	Super::OnUnPossess();
}

#pragma endregion

#pragma region Perception

void AHeistGuardAIController::ConfigurePerceptionFromGuardProfile(const FHeistGuardDataRow& GuardData)
{
	if (!HasAuthority() || !IsValid(GuardPerceptionComponent) || !IsValid(GuardSightConfig))
	{
		return;
	}

	SightRadius = FMath::Max(0.0f, GuardData.SightRadius);
	AggroResetDistance = FMath::Max(SightRadius, GuardData.AggroResetDistance);
	ActiveSightRadius = SightRadius;
	ActiveAggroResetDistance = AggroResetDistance;
	DefaultSightHalfAngle = FMath::Clamp(GuardData.SightAngle * 0.5f, 0.0f, 180.0f);
	InvestigateSightHalfAngle = FMath::Clamp(GuardData.InvestigateSightAngle * 0.5f, 0.0f, 180.0f);
	EyeHeight = FMath::Max(0.0f, GuardData.EyeHeight);
	DetectionGrace = FMath::Max(0.0f, GuardData.DetectionGrace);
	SightUpdateInterval = FMath::Max(0.01f, GuardData.SightUpdateInterval);
	bDoorsBlockSight = GuardData.bDoorsBlockSight;
	bDisplayCasesBlockSight = GuardData.bDisplayCasesBlockSight;
	DoorOccluderTag = GuardData.DoorOccluderTag;

	GuardSightConfig->SightRadius = ActiveSightRadius;
	GuardSightConfig->LoseSightRadius = ActiveAggroResetDistance;
	GuardSightConfig->PeripheralVisionAngleDegrees = DefaultSightHalfAngle;
	GuardSightConfig->DetectionByAffiliation.bDetectEnemies = true;
	GuardSightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	GuardSightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	GuardPerceptionComponent->ConfigureSense(*GuardSightConfig);
	GuardPerceptionComponent->SetDominantSense(GuardSightConfig->GetSenseImplementation());
	bPerceptionConfigured = SightRadius > 0.0f;
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	ApplyAlertModifiers(IsValid(HeistGameState) ? HeistGameState->GetAlertLevel() : EHeistAlertLevel::Quiet);

	const AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	const UHeistGuardStateComponent* GuardStateComponent = IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
	UpdateSightForGuardState(IsValid(GuardStateComponent) ? GuardStateComponent->GetGuardState() : EHeistGuardState::Patrol);
	StartSightValidationTimer();

	UHeistDebugFunctionLibrary::DebugGuardPerceptionConfigured(this, GuardCharacter, ActiveSightRadius, ActiveAggroResetDistance, DefaultSightHalfAngle * 2.0f, InvestigateSightHalfAngle * 2.0f, EyeHeight,
															   DetectionGrace, bDoorsBlockSight, bDisplayCasesBlockSight, DoorOccluderTag, SightUpdateInterval);
}

bool AHeistGuardAIController::DebugEvaluateSightTarget(AActor* TargetActor)
{
	const TCHAR* RejectReason = nullptr;
	AActor* BlockingActor = nullptr;
	const bool bCanSeeTarget = CanInitiallySeeTarget(TargetActor, RejectReason, BlockingActor);

	AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());

	UHeistDebugFunctionLibrary::DebugGuardSightEvaluated(this, GuardCharacter, TargetActor, bCanSeeTarget, RejectReason, BlockingActor);
	return bCanSeeTarget;
}

void AHeistGuardAIController::SetAutomaticSightEnabled(const bool bEnabled)
{
	if (!HasAuthority() || bAutomaticSightEnabled == bEnabled)
	{
		return;
	}

	bAutomaticSightEnabled = bEnabled;
	if (!bAutomaticSightEnabled)
	{
		ClearDetectionGrace(TEXT("AutomaticSightDisabled"));
		ClearSightValidationTimer();
		if (IsValid(GuardPerceptionComponent))
		{
			GuardPerceptionComponent->SetSenseEnabled(UAISense_Sight::StaticClass(), false);
			GuardPerceptionComponent->ForgetAll();
		}
		return;
	}

	const AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	const UHeistGuardStateComponent* GuardStateComponent = IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
	UpdateSightForGuardState(IsValid(GuardStateComponent) ? GuardStateComponent->GetGuardState() : EHeistGuardState::Patrol);
	StartSightValidationTimer();
}

bool AHeistGuardAIController::IsAutomaticSightEnabled() const
{
	return bAutomaticSightEnabled;
}

void AHeistGuardAIController::HandleTargetPerceptionUpdated(AActor* TargetActor, FAIStimulus Stimulus)
{
	if (!bAutomaticSightEnabled || !HasAuthority() || !IsValid(Cast<AHeistPlayerCharacter>(TargetActor)))
	{
		return;
	}

	AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	UHeistGuardStateComponent* GuardStateComponent = IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
	if (!IsValid(GuardStateComponent))
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		if (GuardStateComponent->GetGuardState() == EHeistGuardState::ChasePlayer && GuardStateComponent->GetChaseTarget() == TargetActor)
		{
			GuardStateComponent->RefreshChaseTargetLocation();
			return;
		}

		const TCHAR* RejectReason = nullptr;
		AActor* BlockingActor = nullptr;
		if (CanInitiallySeeTarget(TargetActor, RejectReason, BlockingActor))
		{
			BeginDetectionGrace(TargetActor);
			return;
		}

		UHeistDebugFunctionLibrary::DebugGuardSightEvaluated(this, GuardCharacter, TargetActor, false, RejectReason, BlockingActor);
	}
	else if (PendingSightTarget.Get() == TargetActor)
	{
		ClearDetectionGrace(TEXT("PerceptionLost"));
	}

	if (GuardStateComponent->GetGuardState() == EHeistGuardState::ChasePlayer && GuardStateComponent->GetChaseTarget() == TargetActor)
	{
		const TCHAR* RejectReason = nullptr;
		AActor* BlockingActor = nullptr;
		if (!IsChaseTargetOccluded(TargetActor, RejectReason, BlockingActor))
		{
			return;
		}

		const FVector LastKnownLocation = GuardStateComponent->GetStateFocusLocation();
		if (GuardStateComponent->EnterSearchLastKnownLocation(LastKnownLocation))
		{
			UHeistDebugFunctionLibrary::DebugGuardSightTargetLost(this, GuardCharacter, TargetActor, LastKnownLocation, RejectReason);
		}
	}
}

bool AHeistGuardAIController::CanInitiallySeeTarget(const AActor* TargetActor, const TCHAR*& OutRejectReason, AActor*& OutBlockingActor) const
{
	OutRejectReason = nullptr;
	OutBlockingActor = nullptr;

	const AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	const UHeistGuardStateComponent* GuardStateComponent = IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
	if (!HasAuthority() || !bPerceptionConfigured || !IsValid(GuardCharacter) || !IsValid(GuardStateComponent) || !IsValid(Cast<AHeistPlayerCharacter>(TargetActor)))
	{
		OutRejectReason = TEXT("InvalidSightContext");
		return false;
	}

	if (GuardStateComponent->GetGuardState() == EHeistGuardState::Disabled || GuardStateComponent->GetGuardState() == EHeistGuardState::Stunned)
	{
		OutRejectReason = TEXT("GuardCannotSee");
		return false;
	}

	const AHeistPlayerCharacter* PlayerCharacter = Cast<AHeistPlayerCharacter>(TargetActor);
	const AHeistPlayerState* HeistPlayerState = IsValid(PlayerCharacter) ? PlayerCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	if (!IsValid(HeistPlayerState) || HeistPlayerState->IsEscaped() || HeistPlayerState->IsArrested())
	{
		OutRejectReason = TEXT("PlayerUnavailable");
		return false;
	}

	FVector EyeLocation;
	FRotator EyeRotation;
	GuardCharacter->GetActorEyesViewPoint(EyeLocation, EyeRotation);
	const FVector ToTarget = GetTargetSightLocation(TargetActor) - EyeLocation;
	const float Distance = ToTarget.Size();
	if (Distance > ActiveSightRadius)
	{
		OutRejectReason = TEXT("OutsideSightRadius");
		return false;
	}

	const float ActiveHalfAngle = GuardStateComponent->GetGuardState() == EHeistGuardState::InvestigateNoise || GuardStateComponent->GetGuardState() == EHeistGuardState::SearchLastKnownLocation
									  ? InvestigateSightHalfAngle
									  : DefaultSightHalfAngle;
	const FVector DirectionToTarget = ToTarget.GetSafeNormal();
	const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(ActiveHalfAngle));
	if (FVector::DotProduct(EyeRotation.Vector(), DirectionToTarget) < MinimumDot)
	{
		OutRejectReason = TEXT("OutsideSightAngle");
		return false;
	}

	return !IsChaseTargetOccluded(TargetActor, OutRejectReason, OutBlockingActor);
}

bool AHeistGuardAIController::IsChaseTargetOccluded(const AActor* TargetActor, const TCHAR*& OutRejectReason, AActor*& OutBlockingActor) const
{
	OutRejectReason = nullptr;
	OutBlockingActor = nullptr;

	const AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	if (!IsValid(GuardCharacter) || !IsValid(TargetActor) || !IsValid(GetWorld()))
	{
		OutRejectReason = TEXT("InvalidSightContext");
		return true;
	}

	FVector EyeLocation;
	FRotator EyeRotation;
	GuardCharacter->GetActorEyesViewPoint(EyeLocation, EyeRotation);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HeistGuardSight), false);
	QueryParams.AddIgnoredActor(GuardCharacter);
	QueryParams.AddIgnoredActor(TargetActor);
	TArray<FHitResult> HitResults;
	GetWorld()->LineTraceMultiByChannel(HitResults, EyeLocation, GetTargetSightLocation(TargetActor), ECC_Visibility, QueryParams);

	for (const FHitResult& HitResult : HitResults)
	{
		AActor* HitActor = HitResult.GetActor();
		if (IsValid(HitActor) && HitActor->IsA<AHeistPaintingDisplayCaseActor>())
		{
			if (!bDisplayCasesBlockSight)
			{
				continue;
			}

			OutBlockingActor = HitActor;
			OutRejectReason = TEXT("BlockedByDisplayCase");
			return true;
		}

		if (IsDoorOccluder(HitResult))
		{
			if (!bDoorsBlockSight)
			{
				continue;
			}

			OutBlockingActor = HitActor;
			OutRejectReason = TEXT("BlockedByDoor");
			return true;
		}

		OutBlockingActor = HitActor;
		OutRejectReason = TEXT("BlockedByWorld");
		return true;
	}

	return false;
}

FVector AHeistGuardAIController::GetTargetSightLocation(const AActor* TargetActor) const
{
	if (!IsValid(TargetActor))
	{
		return FVector::ZeroVector;
	}

	FVector TargetEyeLocation;
	FRotator TargetEyeRotation;
	TargetActor->GetActorEyesViewPoint(TargetEyeLocation, TargetEyeRotation);
	return TargetEyeLocation;
}

bool AHeistGuardAIController::IsDoorOccluder(const FHitResult& HitResult) const
{
	if (DoorOccluderTag.IsNone())
	{
		return false;
	}

	const AActor* HitActor = HitResult.GetActor();
	const UPrimitiveComponent* HitComponent = HitResult.GetComponent();
	return (IsValid(HitActor) && HitActor->ActorHasTag(DoorOccluderTag)) || (IsValid(HitComponent) && HitComponent->ComponentHasTag(DoorOccluderTag));
}

void AHeistGuardAIController::TryAcquireSightTarget()
{
	AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	UHeistGuardStateComponent* GuardStateComponent = IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
	if (!IsValid(GuardStateComponent) || GuardStateComponent->GetGuardState() == EHeistGuardState::Disabled || GuardStateComponent->GetGuardState() == EHeistGuardState::Stunned)
	{
		ClearDetectionGrace(TEXT("GuardCannotSee"));
		return;
	}

	AHeistPlayerCharacter* BestTarget = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<AHeistPlayerCharacter> It(GetWorld()); It; ++It)
	{
		AHeistPlayerCharacter* CandidateTarget = *It;
		const TCHAR* RejectReason = nullptr;
		AActor* BlockingActor = nullptr;
		if (!IsValid(CandidateTarget) || !CanInitiallySeeTarget(CandidateTarget, RejectReason, BlockingActor))
		{
			continue;
		}

		const float CandidateDistanceSquared = FVector::DistSquared(GuardCharacter->GetActorLocation(), CandidateTarget->GetActorLocation());
		if (CandidateDistanceSquared < BestDistanceSquared)
		{
			BestTarget = CandidateTarget;
			BestDistanceSquared = CandidateDistanceSquared;
		}
	}

	if (!IsValid(BestTarget))
	{
		ClearDetectionGrace(TEXT("NoVisibleTarget"));
		return;
	}

	BeginDetectionGrace(BestTarget);
}

void AHeistGuardAIController::BeginDetectionGrace(AActor* TargetActor)
{
	if (!HasAuthority() || !IsValid(TargetActor))
	{
		return;
	}

	if (PendingSightTarget.Get() == TargetActor && GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(DetectionGraceTimerHandle))
	{
		return;
	}

	ClearDetectionGrace(TEXT("TargetReplaced"));
	PendingSightTarget = TargetActor;
	if (DetectionGrace <= 0.0f)
	{
		CompleteDetectionGrace();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(DetectionGraceTimerHandle, this, &AHeistGuardAIController::CompleteDetectionGrace, DetectionGrace, false);
		UHeistDebugFunctionLibrary::DebugGuardDetectionGraceStarted(this, GetPawn(), TargetActor, DetectionGrace);
	}
}

void AHeistGuardAIController::CompleteDetectionGrace()
{
	AActor* TargetActor = PendingSightTarget.Get();
	PendingSightTarget.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DetectionGraceTimerHandle);
	}

	const TCHAR* RejectReason = nullptr;
	AActor* BlockingActor = nullptr;
	if (!IsValid(TargetActor) || !CanInitiallySeeTarget(TargetActor, RejectReason, BlockingActor))
	{
		UHeistDebugFunctionLibrary::DebugGuardDetectionGraceCancelled(this, GetPawn(), TargetActor, RejectReason ? RejectReason : TEXT("InvalidTarget"));
		return;
	}

	AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	UHeistGuardStateComponent* GuardStateComponent = IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
	if (IsValid(GuardStateComponent) && GuardStateComponent->EnterChasePlayer(TargetActor))
	{
		if (AHeistGameMode* HeistGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeistGameMode>() : nullptr)
		{
			const FName AlertTriggerId(*FString::Printf(TEXT("GuardSight_%s_%s"), *GuardCharacter->GetFName().ToString(), *TargetActor->GetFName().ToString()));
			if (HeistGameMode->RequestAlertEscalation(EHeistAlertLevel::Suspicious, AlertTriggerId))
			{
				if (AHeistPlayerCharacter* DetectedPlayer = Cast<AHeistPlayerCharacter>(TargetActor))
				{
					if (AHeistPlayerState* DetectedPlayerState = DetectedPlayer->GetPlayerState<AHeistPlayerState>())
					{
						DetectedPlayerState->RecordAlarmContribution();
					}
				}
			}
		}
		UHeistDebugFunctionLibrary::DebugGuardSightTargetAcquired(this, GuardCharacter, TargetActor);
	}
}

void AHeistGuardAIController::ClearDetectionGrace(const TCHAR* Reason)
{
	AActor* PreviousTarget = PendingSightTarget.Get();
	PendingSightTarget.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DetectionGraceTimerHandle);
	}

	if (IsValid(PreviousTarget))
	{
		UHeistDebugFunctionLibrary::DebugGuardDetectionGraceCancelled(this, GetPawn(), PreviousTarget, Reason ? Reason : TEXT("Cancelled"));
	}
}

void AHeistGuardAIController::ValidateCurrentChaseTarget()
{
	if (!bAutomaticSightEnabled || !HasAuthority())
	{
		return;
	}

	AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	UHeistGuardStateComponent* GuardStateComponent = IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
	if (!IsValid(GuardStateComponent))
	{
		return;
	}

	if (GuardStateComponent->GetGuardState() != EHeistGuardState::ChasePlayer)
	{
		TryAcquireSightTarget();
		return;
	}

	AActor* ChaseTarget = GuardStateComponent->GetChaseTarget();
	if (!IsValid(ChaseTarget))
	{
		GuardStateComponent->EnterSearchLastKnownLocation(GuardStateComponent->GetStateFocusLocation());
		return;
	}

	const float Distance = FVector::Dist(GuardCharacter->GetActorLocation(), ChaseTarget->GetActorLocation());
	if (Distance <= ArrestDistance && TryArrestChaseTarget())
	{
		return;
	}
	if (Distance > ActiveAggroResetDistance)
	{
		const FVector LastKnownLocation = GuardStateComponent->GetStateFocusLocation();
		GuardStateComponent->EnterSearchLastKnownLocation(LastKnownLocation);
		UHeistDebugFunctionLibrary::DebugGuardSightTargetLost(this, GuardCharacter, ChaseTarget, LastKnownLocation, TEXT("AggroResetDistance"));
		return;
	}

	const FVector LastKnownLocation = GuardStateComponent->GetStateFocusLocation();
	const TCHAR* RejectReason = nullptr;
	AActor* BlockingActor = nullptr;
	if (IsChaseTargetOccluded(ChaseTarget, RejectReason, BlockingActor))
	{
		GuardStateComponent->EnterSearchLastKnownLocation(LastKnownLocation);
		UHeistDebugFunctionLibrary::DebugGuardSightTargetLost(this, GuardCharacter, ChaseTarget, LastKnownLocation, RejectReason);
		return;
	}

	GuardStateComponent->RefreshChaseTargetLocation();
}

bool AHeistGuardAIController::TryArrestChaseTarget()
{
	AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	UHeistGuardStateComponent* GuardStateComponent = IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
	AHeistPlayerCharacter* PlayerCharacter = IsValid(GuardStateComponent) ? Cast<AHeistPlayerCharacter>(GuardStateComponent->GetChaseTarget()) : nullptr;
	AHeistPlayerState* HeistPlayerState = IsValid(PlayerCharacter) ? PlayerCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	if (!HasAuthority() || !IsValid(GuardCharacter) || !IsValid(GuardStateComponent) || GuardStateComponent->GetGuardState() != EHeistGuardState::ChasePlayer || !IsValid(HeistPlayerState) ||
		HeistPlayerState->IsEscaped() || HeistPlayerState->IsArrested())
	{
		return false;
	}

	const float Distance = FVector::Dist(GuardCharacter->GetActorLocation(), PlayerCharacter->GetActorLocation());
	if (Distance > ArrestDistance)
	{
		return false;
	}

	if (!HeistPlayerState->MarkArrested(GuardCharacter))
	{
		return false;
	}

	StopMovement();
	UHeistDebugFunctionLibrary::Message(this, FString::Printf(TEXT("Guard arrest committed: Guard=%s Player=%s PlayerId=%d Distance=%.1f ArrestDistance=%.1f Authority=true"),
															  *GetNameSafe(GuardCharacter), *GetNameSafe(PlayerCharacter), HeistPlayerState->HeistPlayerId, Distance, ArrestDistance));
	GuardStateComponent->EnterReturnToPatrol();
	return true;
}

bool AHeistGuardAIController::TryGetAlertExitSurveillanceTarget(AActor*& OutTargetActor, float& OutAcceptanceRadius) const
{
	OutTargetActor = nullptr;
	OutAcceptanceRadius = FMath::Max(0.0f, AlertExitSurveillanceAcceptanceRadius);
	if (!HasAuthority() || !bAlertExitSurveillanceActive || !IsValid(GetPawn()) || !IsValid(GetWorld()))
	{
		return false;
	}

	float BestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<AHeistVentActor> It(GetWorld()); It; ++It)
	{
		AHeistVentActor* Candidate = *It;
		if (!IsValid(Candidate) || Candidate->IsActorBeingDestroyed())
		{
			continue;
		}

		const float CandidateDistanceSquared = FVector::DistSquared(GetPawn()->GetActorLocation(), Candidate->GetActorLocation());
		const bool bCloser = CandidateDistanceSquared < BestDistanceSquared && !FMath::IsNearlyEqual(CandidateDistanceSquared, BestDistanceSquared);
		const bool bStableTieBreak = FMath::IsNearlyEqual(CandidateDistanceSquared, BestDistanceSquared) &&
			(!IsValid(OutTargetActor) || Candidate->GetFName().LexicalLess(OutTargetActor->GetFName()));
		if (bCloser || bStableTieBreak)
		{
			OutTargetActor = Candidate;
			BestDistanceSquared = CandidateDistanceSquared;
		}
	}

	return IsValid(OutTargetActor);
}

bool AHeistGuardAIController::IsAlertExitSurveillanceActive() const
{
	return bAlertExitSurveillanceActive;
}

EHeistAlertLevel AHeistGuardAIController::GetAppliedAlertLevel() const
{
	return AppliedAlertLevel;
}

float AHeistGuardAIController::GetActiveSightRadius() const
{
	return ActiveSightRadius;
}

float AHeistGuardAIController::GetAlertSightRadiusMultiplier() const
{
	return AlertSightRadiusMultiplier;
}

void AHeistGuardAIController::HandleAlertStateChanged(const EHeistAlertLevel, const EHeistAlertLevel NewLevel, const int32, const FName)
{
	ApplyAlertModifiers(NewLevel);

	const AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	const UHeistGuardStateComponent* GuardStateComponent = IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
	if (IsValid(GuardStateComponent) && GuardStateComponent->GetGuardState() == EHeistGuardState::Patrol)
	{
		StopMovement();
		SendGuardStateTreeEvent(EHeistGuardState::Patrol);
	}
}

void AHeistGuardAIController::HandleMatchPhaseChanged(const EHeistMatchPhase PreviousMatchPhase, const EHeistMatchPhase NewMatchPhase)
{
	if (!HasAuthority() || PreviousMatchPhase == NewMatchPhase || NewMatchPhase == EHeistMatchPhase::InGame)
	{
		return;
	}

	AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	UHeistGuardStateComponent* GuardStateComponent = IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);
	AbortInspection(FName(TEXT("MatchEnded")));
	if (InspectionTarget.IsValid())
	{
		InspectionTarget.Reset();
		++InspectionTargetSelectionRevision;
	}
	ClearDetectionGrace(TEXT("MatchEnded"));
	ClearSightValidationTimer();
	if (IsValid(GuardStateComponent))
	{
		GuardStateComponent->SetDisabled(true);
	}
	if (IsValid(GuardStateTreeComponent) && GuardStateTreeComponent->IsRunning())
	{
		GuardStateTreeComponent->StopLogic(TEXT("Match ended"));
	}

	const FTimerManager& TimerManager = GetWorldTimerManager();
	const bool bTimersCleared = !TimerManager.TimerExists(DetectionGraceTimerHandle) && !TimerManager.TimerExists(SightValidationTimerHandle) &&
								(!IsValid(GuardStateComponent) || !GuardStateComponent->IsStateTimerActive());
	UE_LOG(LogHeistNetwork, Log, TEXT("Guard match cleanup: Guard=%s PreviousPhase=%s NewPhase=%s State=%s InspectionTarget=%s TimersCleared=%s Authority=true Result=%s"),
		   *GetNameSafe(GuardCharacter), *UEnum::GetValueAsString(PreviousMatchPhase), *UEnum::GetValueAsString(NewMatchPhase),
		   IsValid(GuardStateComponent) ? *UEnum::GetValueAsString(GuardStateComponent->GetGuardState()) : TEXT("MissingStateComponent"), *GetNameSafe(InspectionTarget.Get()),
		   bTimersCleared ? TEXT("true") : TEXT("false"), bTimersCleared ? TEXT("PASS") : TEXT("FAIL"));
}

void AHeistGuardAIController::ApplyAlertModifiers(const EHeistAlertLevel NewLevel)
{
	AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	if (!HasAuthority() || !IsValid(GuardCharacter) || !GuardCharacter->HasResolvedGuardProfile())
	{
		return;
	}

	const FHeistGuardDataRow& GuardProfile = GuardCharacter->GetGuardProfile();
	AppliedAlertLevel = NewLevel;
	const float PatrolSpeedMultiplier = ResolveAlertMultiplier(GuardProfile.AlertPatrolSpeedMultipliers, NewLevel);
	const float NoiseRadiusMultiplier = ResolveAlertMultiplier(GuardProfile.AlertNoiseRadiusMultipliers, NewLevel);
	AlertSightRadiusMultiplier = ResolveAlertMultiplier(GuardProfile.AlertSightRadiusMultipliers, NewLevel);
	const float SearchDurationMultiplier = ResolveAlertMultiplier(GuardProfile.AlertSearchDurationMultipliers, NewLevel);
	ActiveSightRadius = SightRadius * AlertSightRadiusMultiplier;
	ActiveAggroResetDistance = FMath::Max(ActiveSightRadius, AggroResetDistance * AlertSightRadiusMultiplier);
	AlertExitSurveillanceAcceptanceRadius = FMath::Max(0.0f, GuardProfile.ExitSurveillanceAcceptanceRadius);
	bAlertExitSurveillanceActive = GuardProfile.bEnableExitSurveillance && static_cast<uint8>(NewLevel) >= static_cast<uint8>(GuardProfile.ExitSurveillanceAlertLevel);

	GuardCharacter->SetAlertPatrolSpeedMultiplier(PatrolSpeedMultiplier);
	if (UHeistGuardNoiseReactionComponent* NoiseReactionComponent = GuardCharacter->GetNoiseReactionComponent())
	{
		NoiseReactionComponent->SetAlertNoiseRadiusMultiplier(NoiseRadiusMultiplier);
	}
	if (UHeistGuardStateComponent* GuardStateComponent = GuardCharacter->GetGuardStateComponent())
	{
		GuardStateComponent->SetAlertSearchDurationMultiplier(SearchDurationMultiplier);
	}

	if (IsValid(GuardSightConfig) && IsValid(GuardPerceptionComponent))
	{
		GuardSightConfig->SightRadius = ActiveSightRadius;
		GuardSightConfig->LoseSightRadius = ActiveAggroResetDistance;
		UpdateSightForGuardState(GuardCharacter->GetGuardStateComponent()->GetGuardState());
	}

}

void AHeistGuardAIController::UpdateSightForGuardState(const EHeistGuardState NewState)
{
	if (!bPerceptionConfigured || !IsValid(GuardPerceptionComponent))
	{
		return;
	}

	const bool bSightEnabled = bAutomaticSightEnabled && NewState != EHeistGuardState::Disabled && NewState != EHeistGuardState::Stunned;
	GuardPerceptionComponent->SetSenseEnabled(UAISense_Sight::StaticClass(), bSightEnabled);
	if (!bSightEnabled)
	{
		ClearDetectionGrace(TEXT("GuardStateBlocksSight"));
		GuardPerceptionComponent->ForgetAll();
		return;
	}

	GuardSightConfig->PeripheralVisionAngleDegrees =
		NewState == EHeistGuardState::InvestigateNoise || NewState == EHeistGuardState::SearchLastKnownLocation ? InvestigateSightHalfAngle : DefaultSightHalfAngle;
	GuardSightConfig->SightRadius = ActiveSightRadius;
	GuardSightConfig->LoseSightRadius = ActiveAggroResetDistance;
	GuardPerceptionComponent->ConfigureSense(*GuardSightConfig);
	GuardPerceptionComponent->RequestStimuliListenerUpdate();
}

void AHeistGuardAIController::StartSightValidationTimer()
{
	ClearSightValidationTimer();
	if (bAutomaticSightEnabled)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(SightValidationTimerHandle, this, &AHeistGuardAIController::ValidateCurrentChaseTarget, SightUpdateInterval, true);
		}
	}
}

void AHeistGuardAIController::ClearSightValidationTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SightValidationTimerHandle);
	}
}

#pragma endregion

#pragma region InspectionTarget

bool AHeistGuardAIController::TrySelectInspectionTarget()
{
	AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	UHeistGuardStateComponent* GuardStateComponent = IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
	if (!HasAuthority() || !IsValid(GuardCharacter) || !IsValid(GuardStateComponent) || !IsValid(GetWorld()))
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Inspection target selection rejected: Controller=%s Guard=%s Reason=InvalidAuthorityContext"), *GetNameSafe(this), *GetNameSafe(GetPawn()));
		return false;
	}

	const AHeistGameState* HeistGameState = GetWorld()->GetGameState<AHeistGameState>();
	if (!IsValid(HeistGameState) || HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame)
	{
		UE_LOG(LogHeistNetwork, Warning, TEXT("Inspection target selection rejected: Controller=%s Guard=%s MatchPhase=%s Reason=MatchNotInGame"), *GetNameSafe(this), *GetNameSafe(GetPawn()),
			   IsValid(HeistGameState) ? *UEnum::GetValueAsString(HeistGameState->GetMatchPhase()) : TEXT("MissingGameState"));
		return false;
	}

	AActor* CurrentTarget = InspectionTarget.Get();
	if (GuardStateComponent->GetGuardState() == EHeistGuardState::InspectExhibit && IsValid(CurrentTarget) && IsCaseInspectionOwnedBy(CurrentTarget, GuardCharacter))
	{
		return true;
	}
	if (GuardStateComponent->GetGuardState() != EHeistGuardState::Patrol)
	{
		return false;
	}

	AActor* BestTarget = FindBestInspectionTarget();
	if (InspectionTarget.Get() != BestTarget)
	{
		InspectionTarget = BestTarget;
		++InspectionTargetSelectionRevision;
	}

	const bool bSelectedValidTarget = IsValidInspectionCandidate(BestTarget);
	if (bSelectedValidTarget)
	{
		UE_LOG(LogHeistNetwork, Log, TEXT("Inspection target selected: Controller=%s Guard=%s Case=%s CaseId=%s Distance=%.1f SelectionRevision=%d Authority=true Result=PASS"), *GetNameSafe(this),
			   *GetNameSafe(GetPawn()), *GetNameSafe(BestTarget), *GetInspectionCaseId(BestTarget).ToString(), FVector::Dist(GetPawn()->GetActorLocation(), BestTarget->GetActorLocation()),
			   InspectionTargetSelectionRevision);
	}
	return bSelectedValidTarget;
}

bool AHeistGuardAIController::TryBeginInspection()
{
	AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	UHeistGuardStateComponent* GuardStateComponent = IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
	if (!HasAuthority() || !IsValid(GuardCharacter) || !IsValid(GuardStateComponent) || GuardStateComponent->GetGuardState() != EHeistGuardState::Patrol)
	{
		return false;
	}

	AActor* Target = InspectionTarget.Get();
	if (!IsValidInspectionCandidate(Target))
	{
		if (!TrySelectInspectionTarget())
		{
			return false;
		}
		Target = InspectionTarget.Get();
	}

	if (!IsValid(Target) || !TryBeginCaseInspection(Target, GuardCharacter))
	{
		return false;
	}

	if (!GuardStateComponent->EnterInspectExhibit(Target->GetActorLocation()))
	{
		InterruptCaseInspection(Target, GuardCharacter, FName(TEXT("GuardStateRejected")));
		return false;
	}

	UE_LOG(LogHeistNetwork, Log, TEXT("Guard inspection state entered: Controller=%s Guard=%s Case=%s CaseId=%s State=InspectExhibit Authority=true Result=PASS"), *GetNameSafe(this),
		   *GetNameSafe(GuardCharacter), *GetNameSafe(Target), *GetInspectionCaseId(Target).ToString());
	return true;
}

bool AHeistGuardAIController::StartInspectionCast()
{
	AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	UHeistGuardStateComponent* GuardStateComponent = IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
	AActor* Target = InspectionTarget.Get();
	if (!HasAuthority() || !IsValid(GuardCharacter) || !IsValid(GuardStateComponent) || GuardStateComponent->GetGuardState() != EHeistGuardState::InspectExhibit || !IsValid(Target) ||
		!IsCaseInspectionOwnedBy(Target, GuardCharacter))
	{
		return false;
	}

	const FVector Direction = Target->GetActorLocation() - GuardCharacter->GetActorLocation();
	if (!Direction.IsNearlyZero())
	{
		const FRotator FacingRotation(0.0f, Direction.Rotation().Yaw, 0.0f);
		GuardCharacter->SetActorRotation(FacingRotation);
		SetControlRotation(FacingRotation);
	}
	SetFocus(Target, EAIFocusPriority::Gameplay);

	const float SafeCastDuration = FMath::Max(0.1f, InspectionCastDuration);
	if (!GuardStateComponent->StartInspectExhibitCast(SafeCastDuration))
	{
		ClearFocus(EAIFocusPriority::Gameplay);
		return false;
	}

	UE_LOG(LogHeistNetwork, Log, TEXT("Guard inspection cast started: Controller=%s Guard=%s Case=%s CaseId=%s Duration=%.2f FacingYaw=%.2f Authority=true Result=PASS"), *GetNameSafe(this),
		   *GetNameSafe(GuardCharacter), *GetNameSafe(Target), *GetInspectionCaseId(Target).ToString(), SafeCastDuration, GuardCharacter->GetActorRotation().Yaw);
	return true;
}

void AHeistGuardAIController::AbortInspection(const FName Reason)
{
	ClearFocus(EAIFocusPriority::Gameplay);
	AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	AActor* Target = InspectionTarget.Get();
	bool bInterruptedClaim = false;
	if (HasAuthority() && IsValid(GuardCharacter) && IsValid(Target))
	{
		bInterruptedClaim = InterruptCaseInspection(Target, GuardCharacter, Reason);
	}
	if (HasAuthority() && IsValid(GuardCharacter) && !bInterruptedClaim && IsValid(GetWorld()))
	{
		for (TActorIterator<AHeistPaintingDisplayCaseActor> It(GetWorld()); It; ++It)
		{
			AHeistPaintingDisplayCaseActor* PaintingCase = *It;
			if (IsValid(PaintingCase) && PaintingCase->IsInspectionOwnedBy(GuardCharacter))
			{
				bInterruptedClaim = PaintingCase->InterruptInspection(GuardCharacter, Reason);
				break;
			}
		}
		for (TActorIterator<AHeistObjectDisplayCaseActor> It(GetWorld()); It && !bInterruptedClaim; ++It)
		{
			AHeistObjectDisplayCaseActor* ObjectCase = *It;
			if (IsValid(ObjectCase) && ObjectCase->IsInspectionOwnedBy(GuardCharacter))
			{
				bInterruptedClaim = ObjectCase->InterruptInspection(GuardCharacter, Reason);
			}
		}
	}
	if (IsValid(Target))
	{
		InspectionTarget.Reset();
		++InspectionTargetSelectionRevision;
	}
}

void AHeistGuardAIController::HandleInspectionCastExpired()
{
	AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	UHeistGuardStateComponent* GuardStateComponent = IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
	AActor* Target = InspectionTarget.Get();
	ClearFocus(EAIFocusPriority::Gameplay);

	const bool bResultApplied = HasAuthority() && IsValid(GuardCharacter) && IsValid(GuardStateComponent) && GuardStateComponent->GetGuardState() == EHeistGuardState::InspectExhibit &&
								IsValid(Target) && ApplyCaseInspectionResult(Target, GuardCharacter);
	if (bResultApplied)
	{
		InspectionTarget.Reset();
		++InspectionTargetSelectionRevision;
		GuardStateComponent->EnterPatrol();
		return;
	}

	AbortInspection(FName(TEXT("InspectionCompletionInvalid")));
	if (IsValid(GuardStateComponent) && GuardStateComponent->GetGuardState() == EHeistGuardState::InspectExhibit)
	{
		GuardStateComponent->EnterPatrol();
	}
}

AActor* AHeistGuardAIController::GetInspectionTarget() const
{
	return InspectionTarget.Get();
}

bool AHeistGuardAIController::IsInspectionTargetValid() const
{
	const AHeistGameState* HeistGameState = IsValid(GetWorld()) ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn());
	const UHeistGuardStateComponent* GuardStateComponent = IsValid(GuardCharacter) ? GuardCharacter->GetGuardStateComponent() : nullptr;
	AActor* Target = InspectionTarget.Get();
	if (!HasAuthority() || !IsValid(GuardCharacter) || !IsValid(GuardStateComponent) || !IsValid(HeistGameState) || HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame ||
		!IsValid(Target))
	{
		return false;
	}

	return IsValidInspectionCandidate(Target) ||
		   (GuardStateComponent->GetGuardState() == EHeistGuardState::InspectExhibit && IsCaseInspectionOwnedBy(Target, GuardCharacter));
}

int32 AHeistGuardAIController::GetInspectionTargetSelectionRevision() const
{
	return InspectionTargetSelectionRevision;
}

float AHeistGuardAIController::GetInspectionAcceptanceRadius() const
{
	return FMath::Max(0.0f, InspectionAcceptanceRadius);
}

AActor* AHeistGuardAIController::FindBestInspectionTarget() const
{
	if (!HasAuthority() || !IsValid(GetPawn()) || !IsValid(GetWorld()))
	{
		return nullptr;
	}

	AActor* BestTarget = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	auto ConsiderCandidate = [this, &BestTarget, &BestDistanceSquared](AActor* Candidate)
	{
		if (!IsValidInspectionCandidate(Candidate))
		{
			return;
		}

		const float CandidateDistanceSquared = FVector::DistSquared(GetPawn()->GetActorLocation(), Candidate->GetActorLocation());
		const bool bCloser = CandidateDistanceSquared < BestDistanceSquared && !FMath::IsNearlyEqual(CandidateDistanceSquared, BestDistanceSquared);
		const bool bEqualDistance = FMath::IsNearlyEqual(CandidateDistanceSquared, BestDistanceSquared);
		const FName CandidateCaseId = GetInspectionCaseId(Candidate);
		const FName BestCaseId = GetInspectionCaseId(BestTarget);
		const bool bStableTieBreak = bEqualDistance && (!IsValid(BestTarget) || CandidateCaseId.ToString() < BestCaseId.ToString() ||
														(CandidateCaseId == BestCaseId && Candidate->GetFName().LexicalLess(BestTarget->GetFName())));
		if (bCloser || bStableTieBreak)
		{
			BestTarget = Candidate;
			BestDistanceSquared = CandidateDistanceSquared;
		}
	};
	for (TActorIterator<AHeistPaintingDisplayCaseActor> It(GetWorld()); It; ++It)
	{
		ConsiderCandidate(*It);
	}
	for (TActorIterator<AHeistObjectDisplayCaseActor> It(GetWorld()); It; ++It)
	{
		ConsiderCandidate(*It);
	}

	return BestTarget;
}

#pragma endregion

#pragma region StateTree

UStateTreeAIComponent* AHeistGuardAIController::GetGuardStateTreeComponent() const
{
	return GuardStateTreeComponent.Get();
}

void AHeistGuardAIController::HandleGuardStateChanged(const EHeistGuardState PreviousState, const EHeistGuardState NewState)
{
	if (!HasAuthority())
	{
		return;
	}

	StopMovement();
	if (PreviousState == EHeistGuardState::InspectExhibit && NewState != EHeistGuardState::InspectExhibit)
	{
		AbortInspection(NewState == EHeistGuardState::ChasePlayer ? FName(TEXT("ChasePreempted")) : FName(TEXT("GuardStateChanged")));
	}

	UpdateSightForGuardState(NewState);
	SendGuardStateTreeEvent(NewState);
}

void AHeistGuardAIController::SendGuardStateTreeEvent(const EHeistGuardState NewState)
{
	if (!IsValid(GuardStateTreeComponent) || !GuardStateTreeComponent->IsRunning())
	{
		return;
	}

	const FHeistGameplayTags& GameplayTags = FHeistGameplayTags::Get();
	FGameplayTag StateEventTag;
	switch (NewState)
	{
	case EHeistGuardState::Disabled:
		StateEventTag = GameplayTags.AI_State_Disabled;
		break;
	case EHeistGuardState::Stunned:
		StateEventTag = GameplayTags.AI_State_Stunned;
		break;
	case EHeistGuardState::Patrol:
		StateEventTag = GameplayTags.AI_State_Patrol;
		break;
	case EHeistGuardState::InvestigateNoise:
		StateEventTag = GameplayTags.AI_State_InvestigateNoise;
		break;
	case EHeistGuardState::InspectExhibit:
		StateEventTag = GameplayTags.AI_State_InspectExhibit;
		break;
	case EHeistGuardState::ChasePlayer:
		StateEventTag = GameplayTags.AI_State_ChasePlayer;
		break;
	case EHeistGuardState::SearchLastKnownLocation:
		StateEventTag = GameplayTags.AI_State_SearchLastKnownLocation;
		break;
	case EHeistGuardState::ReturnToPatrol:
		StateEventTag = GameplayTags.AI_State_ReturnToPatrol;
		break;
	default:
		break;
	}

	if (StateEventTag.IsValid())
	{
		GuardStateTreeComponent->SendStateTreeEvent(StateEventTag);
		UHeistDebugFunctionLibrary::DebugGuardStateTreeEvent(this, GetPawn(), StateEventTag);
	}
}

#pragma endregion
