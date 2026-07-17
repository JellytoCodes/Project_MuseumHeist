#include "Character/Components/HeistForgeryComponent.h"

#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameState.h"
#include "Core/HeistLogChannels.h"
#include "Core/HeistPlayerState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "World/Actors/Loot/HeistDisplayCaseActor.h"

UHeistForgeryComponent::UHeistForgeryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UHeistForgeryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetOwner() && GetOwner()->HasAuthority() && bSessionActive)
	{
		ClearSession(FName(TEXT("OwnerEndPlay")), true);
	}
	else
	{
		UnbindActiveDisplayCase();
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(SessionTimeoutTimerHandle);
		}
	}

	Super::EndPlay(EndPlayReason);
}

bool UHeistForgeryComponent::TryBeginForgerySession(
	AHeistDisplayCaseActor* TargetDisplayCase,
	const float DurationSeconds)
{
	AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter)
		? HeistCharacter->GetPlayerState<AHeistPlayerState>()
		: nullptr;
	if (!IsValid(HeistCharacter)
		|| !HeistCharacter->HasAuthority()
		|| !IsValid(HeistPlayerState)
		|| !IsValid(TargetDisplayCase))
	{
		UE_LOG(
			LogHeistNetwork,
			Warning,
			TEXT("Forgery session begin rejected: Character=%s Case=%s Reason=InvalidAuthorityContext"),
			*GetNameSafe(HeistCharacter),
			*GetNameSafe(TargetDisplayCase));
		return false;
	}

	if (bSessionActive || bSubmitPending || IsValid(ActiveDisplayCase.Get()))
	{
		UE_LOG(
			LogHeistNetwork,
			Warning,
			TEXT("Forgery session begin rejected: Character=%s Case=%s ActiveCase=%s Reason=SessionAlreadyActive"),
			*GetNameSafe(HeistCharacter),
			*GetNameSafe(TargetDisplayCase),
			*GetNameSafe(ActiveDisplayCase.Get()));
		return false;
	}

	if (!TargetDisplayCase->IsSessionLocked()
		|| TargetDisplayCase->GetSessionOwner() != HeistPlayerState)
	{
		UE_LOG(
			LogHeistNetwork,
			Warning,
			TEXT("Forgery session begin rejected: Character=%s Case=%s CaseOwner=%s Reason=CaseOwnershipMismatch"),
			*GetNameSafe(HeistCharacter),
			*GetNameSafe(TargetDisplayCase),
			*GetNameSafe(TargetDisplayCase->GetSessionOwner()));
		return false;
	}

	if (TargetDisplayCase->GetDisplayCaseState() != EHeistDisplayCaseState::Observed)
	{
		UE_LOG(
			LogHeistNetwork,
			Warning,
			TEXT("Forgery session begin rejected: Character=%s Case=%s CaseState=%s Reason=CaseNotObserved"),
			*GetNameSafe(HeistCharacter),
			*GetNameSafe(TargetDisplayCase),
			*UEnum::GetValueAsString(TargetDisplayCase->GetDisplayCaseState()));
		return false;
	}

	if (HeistPlayerState->IsArrested() || HeistPlayerState->IsEscaped())
	{
		UE_LOG(
			LogHeistNetwork,
			Warning,
			TEXT("Forgery session begin rejected: Character=%s Case=%s Reason=PlayerStateBlocked"),
			*GetNameSafe(HeistCharacter),
			*GetNameSafe(TargetDisplayCase));
		return false;
	}

	if (FVector::DistSquared(
			HeistCharacter->GetActorLocation(),
			TargetDisplayCase->GetActorLocation())
		> FMath::Square(TargetDisplayCase->GetMaximumSessionDistance()))
	{
		UE_LOG(
			LogHeistNetwork,
			Warning,
			TEXT("Forgery session begin rejected: Character=%s Case=%s Reason=OutOfRange"),
			*GetNameSafe(HeistCharacter),
			*GetNameSafe(TargetDisplayCase));
		return false;
	}

	if (!TargetDisplayCase->TryTransitionToDisplayCaseState(
		EHeistDisplayCaseState::ForgeryInProgress))
	{
		UE_LOG(
			LogHeistNetwork,
			Warning,
			TEXT("Forgery session begin rejected: Character=%s Case=%s Reason=CaseTransitionRejected"),
			*GetNameSafe(HeistCharacter),
			*GetNameSafe(TargetDisplayCase));
		return false;
	}

	ActiveDisplayCase = TargetDisplayCase;
	ActiveDisplayCase->OnDisplayCaseSessionChanged.AddDynamic(
		this,
		&UHeistForgeryComponent::HandleDisplayCaseSessionChanged);
	bSessionActive = true;
	bSubmitPending = false;
	LastCleanupReason = NAME_None;

	const float SafeDurationSeconds = DurationSeconds > 0.0f
		? DurationSeconds
		: FMath::Max(1.0f, DefaultSessionDurationSeconds);
	const AHeistGameState* HeistGameState = GetWorld()
		? GetWorld()->GetGameState<AHeistGameState>()
		: nullptr;
	const float ServerWorldTime = IsValid(HeistGameState)
		? HeistGameState->GetServerWorldTimeSeconds()
		: (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	SessionEndServerTime = ServerWorldTime + SafeDurationSeconds;
	++SessionRevision;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			SessionTimeoutTimerHandle,
			this,
			&UHeistForgeryComponent::HandleSessionTimeout,
			SafeDurationSeconds,
			false);
	}

	HeistCharacter->ForceNetUpdate();
	BroadcastSessionSnapshot(TEXT("ServerBegin"), FName(TEXT("BeginAccepted")));
	return true;
}

bool UHeistForgeryComponent::TryBeginSubmit()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	FName RejectReason = NAME_None;
	if (!ValidateActiveSession(RejectReason) || bSubmitPending)
	{
		UE_LOG(
			LogHeistNetwork,
			Warning,
			TEXT("Forgery submit rejected: Character=%s Case=%s SubmitPending=%s Reason=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(ActiveDisplayCase.Get()),
			bSubmitPending ? TEXT("true") : TEXT("false"),
			bSubmitPending ? TEXT("DuplicateSubmit") : *RejectReason.ToString());
		return false;
	}

	bSubmitPending = true;
	++SessionRevision;
	GetOwner()->ForceNetUpdate();
	BroadcastSessionSnapshot(TEXT("ServerSubmit"), FName(TEXT("SubmitPending")));
	return true;
}

bool UHeistForgeryComponent::CancelForgerySession(const FName Reason)
{
	if (!GetOwner()
		|| !GetOwner()->HasAuthority()
		|| (!bSessionActive && !bSubmitPending && !IsValid(ActiveDisplayCase.Get())))
	{
		return false;
	}

	ClearSession(
		Reason.IsNone() ? FName(TEXT("OwnerCancelled")) : Reason,
		true);
	return true;
}

bool UHeistForgeryComponent::ForceTimeoutForDebug()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !bSessionActive)
	{
		return false;
	}

	HandleSessionTimeout();
	return true;
}

bool UHeistForgeryComponent::IsSessionActive() const
{
	return bSessionActive;
}

bool UHeistForgeryComponent::IsSubmitPending() const
{
	return bSubmitPending;
}

float UHeistForgeryComponent::GetSessionEndServerTime() const
{
	return SessionEndServerTime;
}

int32 UHeistForgeryComponent::GetSessionRevision() const
{
	return SessionRevision;
}

AHeistDisplayCaseActor* UHeistForgeryComponent::GetActiveDisplayCase() const
{
	return ActiveDisplayCase.Get();
}

FName UHeistForgeryComponent::GetLastCleanupReason() const
{
	return LastCleanupReason;
}

FHeistForgerySessionStateChanged& UHeistForgeryComponent::GetSessionStateChangedDelegate()
{
	return SessionStateChangedDelegate;
}

bool UHeistForgeryComponent::ValidateActiveSession(FName& OutRejectReason) const
{
	OutRejectReason = NAME_None;
	const AHeistPlayerCharacter* HeistCharacter =
		Cast<AHeistPlayerCharacter>(GetOwner());
	const AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter)
		? HeistCharacter->GetPlayerState<AHeistPlayerState>()
		: nullptr;
	const AHeistDisplayCaseActor* TargetDisplayCase = ActiveDisplayCase.Get();

	if (!bSessionActive)
	{
		OutRejectReason = FName(TEXT("SessionInactive"));
		return false;
	}
	if (!IsValid(HeistCharacter)
		|| !HeistCharacter->HasAuthority()
		|| !IsValid(HeistPlayerState))
	{
		OutRejectReason = FName(TEXT("InvalidAuthorityContext"));
		return false;
	}
	if (!IsValid(TargetDisplayCase))
	{
		OutRejectReason = FName(TEXT("MissingDisplayCase"));
		return false;
	}
	if (!TargetDisplayCase->IsSessionLocked()
		|| TargetDisplayCase->GetSessionOwner() != HeistPlayerState)
	{
		OutRejectReason = FName(TEXT("CaseOwnershipMismatch"));
		return false;
	}
	if (TargetDisplayCase->GetDisplayCaseState()
		!= EHeistDisplayCaseState::ForgeryInProgress)
	{
		OutRejectReason = FName(TEXT("CaseStateMismatch"));
		return false;
	}
	if (HeistPlayerState->IsArrested() || HeistPlayerState->IsEscaped())
	{
		OutRejectReason = FName(TEXT("PlayerStateBlocked"));
		return false;
	}
	if (FVector::DistSquared(
			HeistCharacter->GetActorLocation(),
			TargetDisplayCase->GetActorLocation())
		> FMath::Square(TargetDisplayCase->GetMaximumSessionDistance()))
	{
		OutRejectReason = FName(TEXT("OutOfRange"));
		return false;
	}

	return true;
}

void UHeistForgeryComponent::HandleSessionTimeout()
{
	if (GetOwner() && GetOwner()->HasAuthority() && bSessionActive)
	{
		ClearSession(FName(TEXT("Timeout")), true);
	}
}

void UHeistForgeryComponent::ClearSession(
	const FName Reason,
	const bool bReleaseCaseLock)
{
	AHeistDisplayCaseActor* PreviousDisplayCase = ActiveDisplayCase.Get();
	AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter)
		? HeistCharacter->GetPlayerState<AHeistPlayerState>()
		: nullptr;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SessionTimeoutTimerHandle);
	}
	UnbindActiveDisplayCase();

	ActiveDisplayCase = nullptr;
	bSessionActive = false;
	bSubmitPending = false;
	SessionEndServerTime = 0.0f;
	LastCleanupReason = Reason;
	++SessionRevision;

	if (bReleaseCaseLock
		&& IsValid(PreviousDisplayCase)
		&& IsValid(HeistPlayerState)
		&& PreviousDisplayCase->GetSessionOwner() == HeistPlayerState)
	{
		bHandlingCaseSessionCallback = true;
		PreviousDisplayCase->CancelSessionForOwner(HeistPlayerState, Reason);
		bHandlingCaseSessionCallback = false;
	}

	if (IsValid(HeistCharacter))
	{
		HeistCharacter->ForceNetUpdate();
	}

	UE_LOG(
		LogHeistNetwork,
		Log,
		TEXT("Forgery session cleared: Character=%s PreviousCase=%s Reason=%s Revision=%d"),
		*GetNameSafe(HeistCharacter),
		*GetNameSafe(PreviousDisplayCase),
		*Reason.ToString(),
		SessionRevision);
	BroadcastSessionSnapshot(TEXT("ServerClear"), Reason);
}

void UHeistForgeryComponent::BroadcastSessionSnapshot(
	const TCHAR* ChangeSource,
	const FName Reason)
{
	SessionStateChangedDelegate.Broadcast();
	UE_LOG(
		LogHeistNetwork,
		Log,
		TEXT("Forgery session %s: Character=%s Case=%s Active=%s SubmitPending=%s EndServerTime=%.2f Revision=%d LastCleanup=%s Reason=%s Authority=%s"),
		ChangeSource,
		*GetNameSafe(GetOwner()),
		*GetNameSafe(ActiveDisplayCase.Get()),
		bSessionActive ? TEXT("true") : TEXT("false"),
		bSubmitPending ? TEXT("true") : TEXT("false"),
		SessionEndServerTime,
		SessionRevision,
		LastCleanupReason.IsNone() ? TEXT("None") : *LastCleanupReason.ToString(),
		Reason.IsNone() ? TEXT("None") : *Reason.ToString(),
		GetOwner() && GetOwner()->HasAuthority() ? TEXT("true") : TEXT("false"));
}

void UHeistForgeryComponent::UnbindActiveDisplayCase()
{
	if (IsValid(ActiveDisplayCase.Get()))
	{
		ActiveDisplayCase->OnDisplayCaseSessionChanged.RemoveDynamic(
			this,
			&UHeistForgeryComponent::HandleDisplayCaseSessionChanged);
	}
}

void UHeistForgeryComponent::HandleDisplayCaseSessionChanged(
	AHeistPlayerState* SessionOwner,
	const bool bLocked,
	const int32)
{
	if (bHandlingCaseSessionCallback
		|| !GetOwner()
		|| !GetOwner()->HasAuthority()
		|| !bSessionActive)
	{
		return;
	}

	const AHeistPlayerCharacter* HeistCharacter =
		Cast<AHeistPlayerCharacter>(GetOwner());
	const AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter)
		? HeistCharacter->GetPlayerState<AHeistPlayerState>()
		: nullptr;
	if (!bLocked || !IsValid(SessionOwner) || SessionOwner != HeistPlayerState)
	{
		ClearSession(FName(TEXT("CaseSessionInvalidated")), false);
	}
}

void UHeistForgeryComponent::OnRep_SessionRevision()
{
	BroadcastSessionSnapshot(TEXT("Replicated"), NAME_None);
}

void UHeistForgeryComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(
		UHeistForgeryComponent,
		ActiveDisplayCase,
		COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(
		UHeistForgeryComponent,
		bSessionActive,
		COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(
		UHeistForgeryComponent,
		bSubmitPending,
		COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(
		UHeistForgeryComponent,
		SessionEndServerTime,
		COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(
		UHeistForgeryComponent,
		SessionRevision,
		COND_OwnerOnly);
}
