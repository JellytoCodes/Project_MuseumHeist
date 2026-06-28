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
}

void UHeistGuardNoiseReactionComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (AHeistGameState* HeistGameState =
		GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr)
	{
		HeistGameState->GetSoundPingEventReportedDelegate().RemoveAll(this);
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

	UHeistDebugFunctionLibrary::DebugGuardNoiseReactionAccepted(
		this,
		GuardCharacter,
		SoundPingEvent,
		Distance,
		InvestigateDuration);
	return true;
}

void UHeistGuardNoiseReactionComponent::HandleSoundPingReported(
	const FHeistSoundPingEvent& SoundPingEvent)
{
	ReactToSoundPing(SoundPingEvent);
}

#pragma endregion
