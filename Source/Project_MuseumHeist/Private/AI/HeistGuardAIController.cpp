#include "AI/HeistGuardAIController.h"

#include "AI/HeistGuardCharacter.h"
#include "AI/HeistGuardStateComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Components/StateTreeAIComponent.h"
#include "Core/HeistGameplayTags.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Engine/World.h"
#include "Inventory/HeistItemDataTypes.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "TimerManager.h"
#include "World/Actors/Area/HeistSmokeCloudActor.h"

#pragma region Construction

AHeistGuardAIController::AHeistGuardAIController()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	GuardPerceptionComponent =
		CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("GuardPerceptionComponent"));
	GuardSightConfig =
		CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("GuardSightConfig"));
	SetPerceptionComponent(*GuardPerceptionComponent);

	GuardStateTreeComponent =
		CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("GuardStateTreeComponent"));
	GuardStateTreeComponent->SetStartLogicAutomatically(false);
	BrainComponent = GuardStateTreeComponent;
}

#pragma endregion

#pragma region Lifecycle

void AHeistGuardAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	AHeistGuardCharacter* GuardCharacter = CastChecked<AHeistGuardCharacter>(InPawn);
	UHeistGuardStateComponent* GuardStateComponent =
		GuardCharacter->GetGuardStateComponent();
	checkf(IsValid(GuardStateComponent), TEXT("HeistGuardAIController requires GuardStateComponent."));
	checkf(IsValid(GuardPerceptionComponent), TEXT("HeistGuardAIController requires GuardPerceptionComponent."));
	checkf(IsValid(GuardSightConfig), TEXT("HeistGuardAIController requires GuardSightConfig."));

	GuardStateComponent->GetGuardStateChangedDelegate().AddUObject(
		this,
		&AHeistGuardAIController::HandleGuardStateChanged);
	GuardPerceptionComponent->OnTargetPerceptionUpdated.AddUniqueDynamic(
		this,
		&AHeistGuardAIController::HandleTargetPerceptionUpdated);

	if (GuardCharacter->HasResolvedGuardProfile())
	{
		ConfigurePerceptionFromGuardProfile(GuardCharacter->GetGuardProfile());
	}

	if (HasAuthority() && bStartStateTreeAutomatically)
	{
		GuardStateTreeComponent->StartLogic();
		SendGuardStateTreeEvent(GuardStateComponent->GetGuardState());
	}
}

void AHeistGuardAIController::OnUnPossess()
{
	ClearSightValidationTimer();
	GuardPerceptionComponent->OnTargetPerceptionUpdated.RemoveDynamic(
		this,
		&AHeistGuardAIController::HandleTargetPerceptionUpdated);

	if (AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetPawn()))
	{
		if (UHeistGuardStateComponent* GuardStateComponent =
			GuardCharacter->GetGuardStateComponent())
		{
			GuardStateComponent->GetGuardStateChangedDelegate().RemoveAll(this);
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

void AHeistGuardAIController::ConfigurePerceptionFromGuardProfile(
	const FHeistGuardDataRow& GuardData)
{
	if (!HasAuthority()
		|| !IsValid(GuardPerceptionComponent)
		|| !IsValid(GuardSightConfig))
	{
		return;
	}

	SightRadius = FMath::Max(0.0f, GuardData.SightRadius);
	AggroResetDistance = FMath::Max(SightRadius, GuardData.AggroResetDistance);
	DefaultSightHalfAngle = FMath::Clamp(GuardData.SightAngle * 0.5f, 0.0f, 180.0f);
	InvestigateSightHalfAngle =
		FMath::Clamp(GuardData.InvestigateSightAngle * 0.5f, 0.0f, 180.0f);
	SightUpdateInterval = FMath::Max(0.01f, GuardData.SightUpdateInterval);

	GuardSightConfig->SightRadius = SightRadius;
	GuardSightConfig->LoseSightRadius = AggroResetDistance;
	GuardSightConfig->PeripheralVisionAngleDegrees = DefaultSightHalfAngle;
	GuardSightConfig->DetectionByAffiliation.bDetectEnemies = true;
	GuardSightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	GuardSightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	GuardPerceptionComponent->ConfigureSense(*GuardSightConfig);
	GuardPerceptionComponent->SetDominantSense(
		GuardSightConfig->GetSenseImplementation());
	bPerceptionConfigured = SightRadius > 0.0f;

	const AHeistGuardCharacter* GuardCharacter =
		Cast<AHeistGuardCharacter>(GetPawn());
	const UHeistGuardStateComponent* GuardStateComponent =
		IsValid(GuardCharacter)
			? GuardCharacter->GetGuardStateComponent()
			: nullptr;
	UpdateSightForGuardState(
		IsValid(GuardStateComponent)
			? GuardStateComponent->GetGuardState()
			: EHeistGuardState::Patrol);
	StartSightValidationTimer();

	UHeistDebugFunctionLibrary::DebugGuardPerceptionConfigured(
		this,
		GuardCharacter,
		SightRadius,
		AggroResetDistance,
		DefaultSightHalfAngle * 2.0f,
		InvestigateSightHalfAngle * 2.0f,
		SightUpdateInterval);
}

bool AHeistGuardAIController::DebugEvaluateSightTarget(AActor* TargetActor)
{
	const TCHAR* RejectReason = nullptr;
	AHeistSmokeCloudActor* BlockingSmokeCloud = nullptr;
	const bool bCanSeeTarget = CanInitiallySeeTarget(
		TargetActor,
		RejectReason,
		BlockingSmokeCloud);

	AHeistGuardCharacter* GuardCharacter =
		Cast<AHeistGuardCharacter>(GetPawn());
	UHeistGuardStateComponent* GuardStateComponent =
		IsValid(GuardCharacter)
			? GuardCharacter->GetGuardStateComponent()
			: nullptr;
	if (bCanSeeTarget && IsValid(GuardStateComponent))
	{
		GuardStateComponent->EnterChasePlayer(TargetActor);
	}

	UHeistDebugFunctionLibrary::DebugGuardSightEvaluated(
		this,
		GuardCharacter,
		TargetActor,
		bCanSeeTarget,
		RejectReason,
		BlockingSmokeCloud);
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
		ClearSightValidationTimer();
		if (IsValid(GuardPerceptionComponent))
		{
			GuardPerceptionComponent->SetSenseEnabled(
				UAISense_Sight::StaticClass(),
				false);
			GuardPerceptionComponent->ForgetAll();
		}
		return;
	}

	const AHeistGuardCharacter* GuardCharacter =
		Cast<AHeistGuardCharacter>(GetPawn());
	const UHeistGuardStateComponent* GuardStateComponent =
		IsValid(GuardCharacter)
			? GuardCharacter->GetGuardStateComponent()
			: nullptr;
	UpdateSightForGuardState(
		IsValid(GuardStateComponent)
			? GuardStateComponent->GetGuardState()
			: EHeistGuardState::Patrol);
	StartSightValidationTimer();
}

bool AHeistGuardAIController::IsAutomaticSightEnabled() const
{
	return bAutomaticSightEnabled;
}

void AHeistGuardAIController::HandleTargetPerceptionUpdated(
	AActor* TargetActor,
	FAIStimulus Stimulus)
{
	if (!bAutomaticSightEnabled
		|| !HasAuthority()
		|| !IsValid(Cast<AHeistPlayerCharacter>(TargetActor)))
	{
		return;
	}

	AHeistGuardCharacter* GuardCharacter =
		Cast<AHeistGuardCharacter>(GetPawn());
	UHeistGuardStateComponent* GuardStateComponent =
		IsValid(GuardCharacter)
			? GuardCharacter->GetGuardStateComponent()
			: nullptr;
	if (!IsValid(GuardStateComponent))
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		const TCHAR* RejectReason = nullptr;
		AHeistSmokeCloudActor* BlockingSmokeCloud = nullptr;
		if (CanInitiallySeeTarget(
			TargetActor,
			RejectReason,
			BlockingSmokeCloud))
		{
			if (GuardStateComponent->EnterChasePlayer(TargetActor))
			{
				UHeistDebugFunctionLibrary::DebugGuardSightTargetAcquired(
					this,
					GuardCharacter,
					TargetActor);
			}
			return;
		}

		UHeistDebugFunctionLibrary::DebugGuardSightEvaluated(
			this,
			GuardCharacter,
			TargetActor,
			false,
			RejectReason,
			BlockingSmokeCloud);
	}

	if (GuardStateComponent->GetGuardState() == EHeistGuardState::ChasePlayer
		&& GuardStateComponent->GetChaseTarget() == TargetActor)
	{
		const FVector LastKnownLocation =
			GuardStateComponent->GetStateFocusLocation();
		if (GuardStateComponent->EnterSearchLastKnownLocation(LastKnownLocation))
		{
			UHeistDebugFunctionLibrary::DebugGuardSightTargetLost(
				this,
				GuardCharacter,
				TargetActor,
				LastKnownLocation,
				TEXT("PerceptionLost"));
		}
	}
}

bool AHeistGuardAIController::CanInitiallySeeTarget(
	const AActor* TargetActor,
	const TCHAR*& OutRejectReason,
	AHeistSmokeCloudActor*& OutBlockingSmokeCloud) const
{
	OutRejectReason = nullptr;
	OutBlockingSmokeCloud = nullptr;

	const AHeistGuardCharacter* GuardCharacter =
		Cast<AHeistGuardCharacter>(GetPawn());
	const UHeistGuardStateComponent* GuardStateComponent =
		IsValid(GuardCharacter)
			? GuardCharacter->GetGuardStateComponent()
			: nullptr;
	if (!HasAuthority()
		|| !bPerceptionConfigured
		|| !IsValid(GuardCharacter)
		|| !IsValid(GuardStateComponent)
		|| !IsValid(Cast<AHeistPlayerCharacter>(TargetActor)))
	{
		OutRejectReason = TEXT("InvalidSightContext");
		return false;
	}

	if (GuardStateComponent->GetGuardState() == EHeistGuardState::Disabled
		|| GuardStateComponent->GetGuardState() == EHeistGuardState::Stunned)
	{
		OutRejectReason = TEXT("GuardCannotSee");
		return false;
	}

	FVector EyeLocation;
	FRotator EyeRotation;
	GuardCharacter->GetActorEyesViewPoint(EyeLocation, EyeRotation);
	const FVector ToTarget = TargetActor->GetActorLocation() - EyeLocation;
	const float Distance = ToTarget.Size();
	if (Distance > SightRadius)
	{
		OutRejectReason = TEXT("OutsideSightRadius");
		return false;
	}

	const float ActiveHalfAngle =
		GuardStateComponent->GetGuardState() == EHeistGuardState::InvestigateNoise
			? InvestigateSightHalfAngle
			: DefaultSightHalfAngle;
	const FVector DirectionToTarget = ToTarget.GetSafeNormal();
	const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(ActiveHalfAngle));
	if (FVector::DotProduct(EyeRotation.Vector(), DirectionToTarget) < MinimumDot)
	{
		OutRejectReason = TEXT("OutsideSightAngle");
		return false;
	}

	return !IsChaseTargetOccluded(
		TargetActor,
		OutRejectReason,
		OutBlockingSmokeCloud);
}

bool AHeistGuardAIController::IsChaseTargetOccluded(
	const AActor* TargetActor,
	const TCHAR*& OutRejectReason,
	AHeistSmokeCloudActor*& OutBlockingSmokeCloud) const
{
	OutRejectReason = nullptr;
	OutBlockingSmokeCloud = nullptr;

	const AHeistGuardCharacter* GuardCharacter =
		Cast<AHeistGuardCharacter>(GetPawn());
	if (!IsValid(GuardCharacter)
		|| !IsValid(TargetActor)
		|| !IsValid(GetWorld()))
	{
		OutRejectReason = TEXT("InvalidSightContext");
		return true;
	}

	FVector EyeLocation;
	FRotator EyeRotation;
	GuardCharacter->GetActorEyesViewPoint(EyeLocation, EyeRotation);
	if (AHeistSmokeCloudActor::IsAISightBlockedBySmoke(
		this,
		EyeLocation,
		TargetActor->GetActorLocation(),
		OutBlockingSmokeCloud))
	{
		OutRejectReason = TEXT("BlockedBySmoke");
		return true;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HeistGuardSight), false);
	QueryParams.AddIgnoredActor(GuardCharacter);
	QueryParams.AddIgnoredActor(TargetActor);
	if (GetWorld()->LineTraceTestByChannel(
		EyeLocation,
		TargetActor->GetActorLocation(),
		ECC_Visibility,
		QueryParams))
	{
		OutRejectReason = TEXT("BlockedByWorld");
		return true;
	}

	return false;
}

void AHeistGuardAIController::TryAcquirePerceivedTarget()
{
	AHeistGuardCharacter* GuardCharacter =
		Cast<AHeistGuardCharacter>(GetPawn());
	UHeistGuardStateComponent* GuardStateComponent =
		IsValid(GuardCharacter)
			? GuardCharacter->GetGuardStateComponent()
			: nullptr;
	if (!IsValid(GuardStateComponent)
		|| GuardStateComponent->GetGuardState() == EHeistGuardState::Disabled
		|| GuardStateComponent->GetGuardState() == EHeistGuardState::Stunned)
	{
		return;
	}

	TArray<AActor*> PerceivedActors;
	GuardPerceptionComponent->GetCurrentlyPerceivedActors(
		UAISense_Sight::StaticClass(),
		PerceivedActors);
	for (AActor* PerceivedActor : PerceivedActors)
	{
		const TCHAR* RejectReason = nullptr;
		AHeistSmokeCloudActor* BlockingSmokeCloud = nullptr;
		if (IsValid(Cast<AHeistPlayerCharacter>(PerceivedActor))
			&& CanInitiallySeeTarget(
				PerceivedActor,
				RejectReason,
				BlockingSmokeCloud)
			&& GuardStateComponent->EnterChasePlayer(PerceivedActor))
		{
			UHeistDebugFunctionLibrary::DebugGuardSightTargetAcquired(
				this,
				GuardCharacter,
				PerceivedActor);
			return;
		}
	}
}

void AHeistGuardAIController::ValidateCurrentChaseTarget()
{
	if (!bAutomaticSightEnabled || !HasAuthority())
	{
		return;
	}

	AHeistGuardCharacter* GuardCharacter =
		Cast<AHeistGuardCharacter>(GetPawn());
	UHeistGuardStateComponent* GuardStateComponent =
		IsValid(GuardCharacter)
			? GuardCharacter->GetGuardStateComponent()
			: nullptr;
	if (!IsValid(GuardStateComponent))
	{
		return;
	}

	if (GuardStateComponent->GetGuardState() != EHeistGuardState::ChasePlayer)
	{
		TryAcquirePerceivedTarget();
		return;
	}

	AActor* ChaseTarget = GuardStateComponent->GetChaseTarget();
	if (!IsValid(ChaseTarget))
	{
		GuardStateComponent->EnterSearchLastKnownLocation(
			GuardStateComponent->GetStateFocusLocation());
		return;
	}

	const float Distance = FVector::Dist(
		GuardCharacter->GetActorLocation(),
		ChaseTarget->GetActorLocation());
	if (Distance > AggroResetDistance)
	{
		GuardStateComponent->EnterPatrol();
		UHeistDebugFunctionLibrary::DebugGuardSightTargetLost(
			this,
			GuardCharacter,
			ChaseTarget,
			ChaseTarget->GetActorLocation(),
			TEXT("AggroResetDistance"));
		return;
	}

	const FVector LastKnownLocation =
		GuardStateComponent->GetStateFocusLocation();
	const TCHAR* RejectReason = nullptr;
	AHeistSmokeCloudActor* BlockingSmokeCloud = nullptr;
	if (IsChaseTargetOccluded(
		ChaseTarget,
		RejectReason,
		BlockingSmokeCloud))
	{
		GuardStateComponent->EnterSearchLastKnownLocation(LastKnownLocation);
		UHeistDebugFunctionLibrary::DebugGuardSightTargetLost(
			this,
			GuardCharacter,
			ChaseTarget,
			LastKnownLocation,
			RejectReason);
		return;
	}

	GuardStateComponent->RefreshChaseTargetLocation();
}

void AHeistGuardAIController::UpdateSightForGuardState(
	const EHeistGuardState NewState)
{
	if (!bPerceptionConfigured || !IsValid(GuardPerceptionComponent))
	{
		return;
	}

	const bool bSightEnabled =
		bAutomaticSightEnabled
		&& NewState != EHeistGuardState::Disabled
		&& NewState != EHeistGuardState::Stunned;
	GuardPerceptionComponent->SetSenseEnabled(
		UAISense_Sight::StaticClass(),
		bSightEnabled);
	if (!bSightEnabled)
	{
		GuardPerceptionComponent->ForgetAll();
		return;
	}

	GuardSightConfig->PeripheralVisionAngleDegrees =
		NewState == EHeistGuardState::InvestigateNoise
			|| NewState == EHeistGuardState::SearchLastKnownLocation
			? InvestigateSightHalfAngle
			: DefaultSightHalfAngle;
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
			World->GetTimerManager().SetTimer(
				SightValidationTimerHandle,
				this,
				&AHeistGuardAIController::ValidateCurrentChaseTarget,
				SightUpdateInterval,
				true);
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

#pragma region StateTree

UStateTreeAIComponent* AHeistGuardAIController::GetGuardStateTreeComponent() const
{
	return GuardStateTreeComponent.Get();
}

void AHeistGuardAIController::HandleGuardStateChanged(
	const EHeistGuardState,
	const EHeistGuardState NewState)
{
	if (!HasAuthority())
	{
		return;
	}

	UpdateSightForGuardState(NewState);
	SendGuardStateTreeEvent(NewState);
}

void AHeistGuardAIController::SendGuardStateTreeEvent(
	const EHeistGuardState NewState)
{
	if (!IsValid(GuardStateTreeComponent)
		|| !GuardStateTreeComponent->IsRunning())
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
		UHeistDebugFunctionLibrary::DebugGuardStateTreeEvent(
			this,
			GetPawn(),
			StateEventTag);
	}
}

#pragma endregion
