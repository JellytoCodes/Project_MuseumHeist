#include "AI/HeistGuardNoiseReactionComponent.h"

#include "AI/HeistGuardCharacter.h"
#include "AI/HeistGuardStateComponent.h"
#include "Core/HeistGameState.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Engine/World.h"
#include "Inventory/HeistItemDataTypes.h"

#pragma region Construction

UHeistGuardNoiseReactionComponent::UHeistGuardNoiseReactionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

#pragma endregion

#pragma region Lifecycle

void UHeistGuardNoiseReactionComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return;
	}

	AHeistGameState* HeistGameState =
		GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistGameState))
	{
		UHeistDebugFunctionLibrary::DebugGuardNoiseReactionRejected(
			this,
			OwnerActor,
			FHeistSoundPingEvent(),
			TEXT("MissingGameState"));
		return;
	}

	HeistGameState->GetSoundPingEventReportedDelegate().AddUObject(
		this,
		&UHeistGuardNoiseReactionComponent::HandleSoundPingReported);

	AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(OwnerActor);
	if (IsValid(GuardCharacter))
	{
		GuardCharacter->GetGuardStateComponent()->GetGuardStateChangedDelegate().AddUObject(
			this,
			&UHeistGuardNoiseReactionComponent::HandleGuardStateChanged);
	}
}

void UHeistGuardNoiseReactionComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (AHeistGameState* HeistGameState =
		GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr)
	{
		HeistGameState->GetSoundPingEventReportedDelegate().RemoveAll(this);
	}

	if (AHeistGuardCharacter* GuardCharacter = Cast<AHeistGuardCharacter>(GetOwner()))
	{
		GuardCharacter->GetGuardStateComponent()->GetGuardStateChangedDelegate().RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

#pragma endregion

#pragma region NoiseReaction

void UHeistGuardNoiseReactionComponent::ConfigureGuardProfile(
	const FHeistGuardDataRow& GuardData)
{
	InvestigateDuration = FMath::Max(0.0f, GuardData.InvestigateDuration);
}

bool UHeistGuardNoiseReactionComponent::ReactToSoundPing(
	const FHeistSoundPingEvent& SoundPingEvent)
{
	AHeistGuardCharacter* GuardCharacter =
		Cast<AHeistGuardCharacter>(GetOwner());
	if (!IsValid(GuardCharacter)
		|| !GuardCharacter->HasAuthority()
		|| !SoundPingEvent.bAffectsGuards
		|| SoundPingEvent.Radius <= 0.0f)
	{
		UHeistDebugFunctionLibrary::DebugGuardNoiseReactionRejected(
			this,
			GuardCharacter,
			SoundPingEvent,
			TEXT("InvalidSoundPing"));
		return false;
	}

	const float Distance = FVector::Dist(
		GuardCharacter->GetActorLocation(),
		SoundPingEvent.WorldLocation);
	if (Distance > SoundPingEvent.Radius)
	{
		UHeistDebugFunctionLibrary::DebugGuardNoiseReactionRejected(
			this,
			GuardCharacter,
			SoundPingEvent,
			TEXT("OutsideRadius"),
			Distance);
		return false;
	}

	if (InvestigateDuration <= 0.0f)
	{
		UHeistDebugFunctionLibrary::DebugGuardNoiseReactionRejected(
			this,
			GuardCharacter,
			SoundPingEvent,
			TEXT("MissingGuardProfile"),
			Distance);
		return false;
	}

	UHeistGuardStateComponent* GuardStateComponent =
		GuardCharacter->GetGuardStateComponent();
	checkf(IsValid(GuardStateComponent), TEXT("HeistGuardCharacter requires GuardStateComponent."));
	const EHeistGuardState GuardStateBeforeReaction = GuardStateComponent->GetGuardState();
	const int32 NewPriority = ResolveCandidatePriority(SoundPingEvent.PingType);
	const EHeistSoundPingType PreviousCandidateType = bHasCurrentCandidate
		? CurrentCandidate.PingType
		: EHeistSoundPingType::None;
	const int32 PreviousCandidatePriority = bHasCurrentCandidate
		? ResolveCandidatePriority(CurrentCandidate.PingType)
		: MAX_int32;

	if (GuardStateBeforeReaction == EHeistGuardState::ChasePlayer)
	{
		UHeistDebugFunctionLibrary::DebugGuardNoiseReactionRejected(
			this,
			GuardCharacter,
			SoundPingEvent,
			TEXT("ChaseHasPriority"),
			Distance);
		if (SoundPingEvent.PingType == EHeistSoundPingType::CoinImpact)
		{
			UHeistDebugFunctionLibrary::DebugCoinDistractionDecision(
				this,
				GuardCharacter,
				SoundPingEvent,
				GuardStateBeforeReaction,
				TEXT("REJECT"),
				TEXT("ChaseHasPriority"),
				NewPriority,
				PreviousCandidateType,
				PreviousCandidatePriority);
		}
		return false;
	}

	if (GuardStateBeforeReaction == EHeistGuardState::InvestigateNoise
		&& bHasCurrentCandidate)
	{
		if (NewPriority > PreviousCandidatePriority
			|| (NewPriority == PreviousCandidatePriority && Distance >= CurrentCandidateDistance))
		{
			UHeistDebugFunctionLibrary::DebugGuardNoiseReactionRejected(
				this,
				GuardCharacter,
				SoundPingEvent,
				TEXT("LowerPriorityCandidate"),
				Distance);
			if (SoundPingEvent.PingType == EHeistSoundPingType::CoinImpact)
			{
				UHeistDebugFunctionLibrary::DebugCoinDistractionDecision(
					this,
					GuardCharacter,
					SoundPingEvent,
					GuardStateBeforeReaction,
					TEXT("REJECT"),
					TEXT("LowerPriorityCandidate"),
					NewPriority,
					PreviousCandidateType,
					PreviousCandidatePriority);
			}
			return false;
		}
	}

	if (!GuardStateComponent->EnterInvestigateNoise(
		SoundPingEvent.WorldLocation,
		InvestigateDuration))
	{
		UHeistDebugFunctionLibrary::DebugGuardNoiseReactionRejected(
			this,
			GuardCharacter,
			SoundPingEvent,
			TEXT("StateRejected"),
			Distance);
		return false;
	}

	CurrentCandidate = SoundPingEvent;
	CurrentCandidateDistance = Distance;
	bHasCurrentCandidate = true;

	UHeistDebugFunctionLibrary::DebugGuardNoiseReactionAccepted(
		this,
		GuardCharacter,
		SoundPingEvent,
		Distance,
		InvestigateDuration);
	if (SoundPingEvent.PingType == EHeistSoundPingType::CoinImpact)
	{
		const TCHAR* SelectionRule = TEXT("InitialCandidate");
		if (PreviousCandidateType != EHeistSoundPingType::None)
		{
			SelectionRule = NewPriority < PreviousCandidatePriority
				? TEXT("HigherPriorityReplacement")
				: TEXT("CloserSamePriorityReplacement");
		}
		UHeistDebugFunctionLibrary::DebugCoinDistractionDecision(
			this,
			GuardCharacter,
			SoundPingEvent,
			GuardStateBeforeReaction,
			TEXT("ACCEPT"),
			SelectionRule,
			NewPriority,
			PreviousCandidateType,
			PreviousCandidatePriority);
	}
	return true;
}

void UHeistGuardNoiseReactionComponent::HandleSoundPingReported(
	const FHeistSoundPingEvent& SoundPingEvent)
{
	ReactToSoundPing(SoundPingEvent);
}

void UHeistGuardNoiseReactionComponent::HandleGuardStateChanged(
	const EHeistGuardState,
	const EHeistGuardState NewState)
{
	if (NewState == EHeistGuardState::InvestigateNoise)
	{
		return;
	}

	CurrentCandidate = FHeistSoundPingEvent();
	CurrentCandidateDistance = TNumericLimits<float>::Max();
	bHasCurrentCandidate = false;
}

int32 UHeistGuardNoiseReactionComponent::ResolveCandidatePriority(
	const EHeistSoundPingType PingType)
{
	switch (PingType)
	{
	case EHeistSoundPingType::StunHit:
		return 0;
	case EHeistSoundPingType::GlassBreak:
	case EHeistSoundPingType::NoiseTrap:
		return 1;
	case EHeistSoundPingType::CoinImpact:
		return 2;
	case EHeistSoundPingType::Footstep:
		return 3;
	default:
		return MAX_int32;
	}
}

#pragma endregion
