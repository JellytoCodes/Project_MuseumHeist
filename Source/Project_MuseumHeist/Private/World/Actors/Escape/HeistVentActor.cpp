#include "World/Actors/Escape/HeistVentActor.h"

#include "Character/HeistPlayerCharacter.h"
#include "Character/Components/HeistInteractionComponent.h"
#include "Core/HeistGameState.h"
#include "Core/HeistLogChannels.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
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
	RefreshLocalInteractionTarget();

	UE_LOG(LogHeist, Log, TEXT("Vent active state changed: Vent=%s IsActive=%s RequiresEscapePhase=%s ManuallyEnabled=%s WorldRestricted=%s"), *GetNameSafe(this),
		   bVentActive ? TEXT("true") : TEXT("false"), bRequiresEscapePhase ? TEXT("true") : TEXT("false"), bVentManuallyEnabled ? TEXT("true") : TEXT("false"),
		   bWorldInteractionAllowed ? TEXT("false") : TEXT("true"));
}

void AHeistVentActor::OnRep_VentActive()
{
	RefreshLocalInteractionTarget();
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

bool AHeistVentActor::CanShowLockedPrompt(const AHeistPlayerCharacter* RequestingCharacter) const
{
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	return IsValid(this) && !IsActorBeingDestroyed() && bVentManuallyEnabled && bRequiresEscapePhase && !bVentActive && IsValid(RequestingCharacter) && Super::CanInteract(RequestingCharacter) &&
		   IsValid(HeistGameState) && HeistGameState->GetMatchPhase() == EHeistMatchPhase::InGame && !HeistGameState->IsEscapePhaseOpen() && GetUnlockTimeRemaining() >= 0.0f;
}

float AHeistVentActor::GetUnlockTimeRemaining() const
{
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	if (!IsValid(HeistGameState) || HeistGameState->GetMatchPhase() != EHeistMatchPhase::InGame)
	{
		return -1.0f;
	}

	const float UnlockServerTime = HeistGameState->GetEscapePhaseUnlockServerTime();
	return FMath::IsFinite(UnlockServerTime) && UnlockServerTime >= 0.0f ? FMath::Max(UnlockServerTime - HeistGameState->GetServerWorldTimeSeconds(), 0.0f) : -1.0f;
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

void AHeistVentActor::RefreshLocalInteractionTarget()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		if (IsValid(PlayerController) && PlayerController->IsLocalController())
		{
			if (AHeistPlayerCharacter* Character = PlayerController->GetPawn<AHeistPlayerCharacter>())
			{
				if (UHeistInteractionComponent* Interaction = Character->GetInteractionComponent())
				{
					Interaction->RefreshInteractionTarget();
				}
			}
		}
	}
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
