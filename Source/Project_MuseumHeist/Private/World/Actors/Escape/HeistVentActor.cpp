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
		BindToGameState();
		RefreshVentActiveState();
	}
}

void AHeistVentActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BoundGameState.IsValid() && EscapePhaseStateChangedHandle.IsValid())
	{
		BoundGameState->GetEscapePhaseStateChangedDelegate().Remove(EscapePhaseStateChangedHandle);
	}
	if (BoundGameState.IsValid() && AlertStateChangedHandle.IsValid())
	{
		BoundGameState->GetAlertStateChangedDelegate().Remove(AlertStateChangedHandle);
	}

	BoundGameState.Reset();
	EscapePhaseStateChangedHandle.Reset();
	AlertStateChangedHandle.Reset();

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
	const bool bEscapeRequirementMet = !bRequiresEscapePhase || (IsValid(HeistGameState) && HeistGameState->IsEscapePhaseOpen());
	const bool bWorldInteractionAllowed = !IsValid(HeistGameState) || !HeistGameState->AreWorldInteractionsRestricted();
	const bool bShouldBeActive = bVentManuallyEnabled && bEscapeRequirementMet && bWorldInteractionAllowed;

	if (bVentActive == bShouldBeActive)
	{
		return;
	}

	bVentActive = bShouldBeActive;
	ForceNetUpdate();

	UE_LOG(LogHeist, Log, TEXT("Vent active state changed: Vent=%s IsActive=%s RequiresEscapePhase=%s ManuallyEnabled=%s WorldRestricted=%s"), *GetNameSafe(this),
		   bVentActive ? TEXT("true") : TEXT("false"), bRequiresEscapePhase ? TEXT("true") : TEXT("false"), bVentManuallyEnabled ? TEXT("true") : TEXT("false"),
		   bWorldInteractionAllowed ? TEXT("false") : TEXT("true"));
}

void AHeistVentActor::OnRep_VentActive()
{
	UE_LOG(LogHeistNetwork, Log, TEXT("Vent active state replicated: Vent=%s IsActive=%s"), *GetNameSafe(this), bVentActive ? TEXT("true") : TEXT("false"));
}

#pragma endregion

#pragma region Interaction

bool AHeistVentActor::CanInteract(const AActor* Interactor) const
{
	return CanUseVent(Cast<AHeistPlayerCharacter>(Interactor));
}

bool AHeistVentActor::CanUseVent(const AHeistPlayerCharacter* RequestingCharacter) const
{
	return IsValid(this) && !IsActorBeingDestroyed() && bVentActive && IsValid(RequestingCharacter) && Super::CanInteract(RequestingCharacter);
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

void AHeistVentActor::BindToGameState()
{
	AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistGameState))
	{
		UE_LOG(LogHeist, Warning, TEXT("Vent GameState binding skipped: Vent=%s Reason=MissingGameState"), *GetNameSafe(this));
		return;
	}

	BoundGameState = HeistGameState;
	EscapePhaseStateChangedHandle = HeistGameState->GetEscapePhaseStateChangedDelegate().AddUObject(this, &AHeistVentActor::HandleEscapePhaseStateChanged);
	AlertStateChangedHandle = HeistGameState->GetAlertStateChangedDelegate().AddUObject(this, &AHeistVentActor::HandleAlertStateChanged);
}

void AHeistVentActor::HandleEscapePhaseStateChanged(bool)
{
	RefreshVentActiveState();
}

void AHeistVentActor::HandleAlertStateChanged(EHeistAlertLevel, EHeistAlertLevel, int32, FName)
{
	RefreshVentActiveState();
}

#pragma endregion
