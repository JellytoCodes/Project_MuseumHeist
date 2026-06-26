#include "World/Actors/Escape/HeistVentActor.h"

#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameState.h"
#include "Core/HeistLogChannels.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

#pragma region Construction

AHeistVentActor::AHeistVentActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);
}

#pragma endregion

#pragma region Lifecycle

void AHeistVentActor::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		BindToEscapePhaseState();
		RefreshVentActiveState();
	}
}

void AHeistVentActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BoundGameState.IsValid() && EscapePhaseStateChangedHandle.IsValid())
	{
		BoundGameState->GetEscapePhaseStateChangedDelegate().Remove(EscapePhaseStateChangedHandle);
	}

	BoundGameState.Reset();
	EscapePhaseStateChangedHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

#pragma endregion

#pragma region VentState

bool AHeistVentActor::IsVentActive() const
{
	return bVentActive;
}

void AHeistVentActor::RefreshVentActiveState()
{
	if (!HasAuthority())
	{
		return;
	}

	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const bool bEscapeRequirementMet =
		!bRequiresEscapePhase
		|| (IsValid(HeistGameState) && HeistGameState->IsEscapePhaseOpen());
	const bool bShouldBeActive = bVentManuallyEnabled && bEscapeRequirementMet;

	if (bVentActive == bShouldBeActive)
	{
		return;
	}

	bVentActive = bShouldBeActive;
	ForceNetUpdate();

	UE_LOG(
		LogHeist,
		Log,
		TEXT("Vent active state changed: Vent=%s IsActive=%s RequiresEscapePhase=%s ManuallyEnabled=%s"),
		*GetNameSafe(this),
		bVentActive ? TEXT("true") : TEXT("false"),
		bRequiresEscapePhase ? TEXT("true") : TEXT("false"),
		bVentManuallyEnabled ? TEXT("true") : TEXT("false"));
}

void AHeistVentActor::OnRep_VentActive()
{
	UE_LOG(
		LogHeistNetwork,
		Log,
		TEXT("Vent active state replicated: Vent=%s IsActive=%s"),
		*GetNameSafe(this),
		bVentActive ? TEXT("true") : TEXT("false"));
}

#pragma endregion

#pragma region Interaction

bool AHeistVentActor::CanInteract(const AActor* Interactor) const
{
	return CanUseVent(Cast<AHeistPlayerCharacter>(Interactor));
}

bool AHeistVentActor::CanUseVent(const AHeistPlayerCharacter* RequestingCharacter) const
{
	return IsValid(this)
		&& !IsActorBeingDestroyed()
		&& bVentActive
		&& IsValid(RequestingCharacter)
		&& Super::CanInteract(RequestingCharacter);
}

#pragma endregion

#pragma region Replication

void AHeistVentActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeistVentActor, bVentActive);
}

#pragma endregion

#pragma region InternalHelpers

void AHeistVentActor::BindToEscapePhaseState()
{
	AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistGameState))
	{
		UE_LOG(
			LogHeist,
			Warning,
			TEXT("Vent Escape Phase binding skipped: Vent=%s Reason=MissingGameState"),
			*GetNameSafe(this));
		return;
	}

	BoundGameState = HeistGameState;
	EscapePhaseStateChangedHandle = HeistGameState->GetEscapePhaseStateChangedDelegate().AddUObject(
		this,
		&AHeistVentActor::HandleEscapePhaseStateChanged);
}

void AHeistVentActor::HandleEscapePhaseStateChanged(bool)
{
	RefreshVentActiveState();
}

#pragma endregion
