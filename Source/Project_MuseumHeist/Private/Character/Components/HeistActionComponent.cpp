#include "Character/Components/HeistActionComponent.h"

#include "Character/Components/HeistInventoryComponent.h"
#include "Character/Components/HeistForgeryComponent.h"
#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerState.h"
#include "Data/HeistGameBalanceDataAsset.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "World/Actors/Loot/HeistPaintingDisplayCaseActor.h"
#include "World/Actors/Escape/HeistVentActor.h"

#pragma region Construction

UHeistActionComponent::UHeistActionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

#pragma endregion

#pragma region Lifecycle

void UHeistActionComponent::BeginPlay()
{
	Super::BeginPlay();

	SetComponentTickEnabled(false);

	AActor* OwnerActor = GetOwner();
	if (IsValid(OwnerActor) && OwnerActor->HasAuthority())
	{
		OwnerActor->OnTakeAnyDamage.AddDynamic(this, &UHeistActionComponent::HandleOwnerTakeAnyDamage);
	}
}

void UHeistActionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AActor* OwnerActor = GetOwner();
	if (IsValid(OwnerActor))
	{
		OwnerActor->OnTakeAnyDamage.RemoveDynamic(this, &UHeistActionComponent::HandleOwnerTakeAnyDamage);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EscapeCastTimerHandle);
		World->GetTimerManager().ClearTimer(ObservationCastTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UHeistActionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEscapeCastActive && !bObservationCastActive)
	{
		SetComponentTickEnabled(false);
		return;
	}

	const AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	if (!IsValid(HeistCharacter))
	{
		CancelEscapeCast(TEXT("InvalidCastState"));
		CancelObservationCast(TEXT("InvalidCastState"));
		return;
	}

	if (bEscapeCastActive && !PendingEscapeVent.IsValid())
	{
		CancelEscapeCast(TEXT("InvalidCastState"));
		return;
	}

	if (bEscapeCastActive && HasMovedBeyondEscapeCastTolerance())
	{
		CancelEscapeCast(TEXT("Movement"));
		return;
	}

	if (bObservationCastActive)
	{
		AHeistPaintingDisplayCaseActor* TargetDisplayCase = PendingObservationDisplayCase.Get();
		const AHeistPlayerState* HeistPlayerState = HeistCharacter->GetPlayerState<AHeistPlayerState>();
		if (!IsValid(TargetDisplayCase) || !IsValid(HeistPlayerState) || !TargetDisplayCase->IsSessionLocked() || TargetDisplayCase->GetSessionOwner() != HeistPlayerState)
		{
			CancelObservationCast(TEXT("SessionInvalid"));
			return;
		}

		if (HasMovedBeyondObservationCastTolerance())
		{
			CancelObservationCast(TEXT("Movement"));
			return;
		}
	}

	if (bEscapeCastActive && !PendingEscapeVent->CanUseVent(HeistCharacter))
	{
		CancelEscapeCast(TEXT("VentUnavailable"));
	}
}

#pragma endregion

#pragma region EscapeCast

bool UHeistActionComponent::TryBeginEscapeRequest(AHeistVentActor* TargetVentActor)
{
	AActor* OwnerActor = GetOwner();
	const AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(OwnerActor);
	const AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter) ? HeistCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || !IsValid(HeistPlayerState) || HeistPlayerState->IsEscaped() || !IsValid(TargetVentActor) || bEscapeCastActive ||
		IsGameplayCastActive() || PendingEscapeVent.IsValid())
	{
		return false;
	}

	PendingEscapeVent = TargetVentActor;
	bEscapeCastActive = true;
	EscapeCastStartLocation = OwnerActor->GetActorLocation();

	const float EscapeCastDurationSeconds = ResolveEscapeCastDurationSeconds();
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const float ServerWorldTime = IsValid(HeistGameState) ? HeistGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	EscapeCastEndServerTime = ServerWorldTime + EscapeCastDurationSeconds;

	SetComponentTickEnabled(true);
	OwnerActor->ForceNetUpdate();
	ActionStateChangedDelegate.Broadcast();

	UHeistDebugFunctionLibrary::DebugEscapeCastStarted(this, OwnerActor, TargetVentActor, EscapeCastDurationSeconds, EscapeCastEndServerTime);

	if (EscapeCastDurationSeconds <= 0.0f)
	{
		HandleEscapeCastTimerElapsed();
	}
	else if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(EscapeCastTimerHandle, this, &UHeistActionComponent::HandleEscapeCastTimerElapsed, EscapeCastDurationSeconds, false);
	}
	else
	{
		CancelEscapeCast(TEXT("MissingWorld"));
		return false;
	}

	return true;
}

bool UHeistActionComponent::IsGameplayCastActive() const
{
	return bEscapeCastActive || bObservationCastActive;
}

void UHeistActionComponent::CancelGameplayActions(const TCHAR* Reason)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	CancelEscapeCast(Reason ? Reason : TEXT("Cancelled"));
	CancelObservationCast(Reason ? Reason : TEXT("Cancelled"));
}

bool UHeistActionComponent::HasPendingEscapeRequest() const
{
	return bEscapeCastActive || PendingEscapeVent.IsValid();
}

AHeistVentActor* UHeistActionComponent::GetPendingEscapeVent() const
{
	return PendingEscapeVent.Get();
}

void UHeistActionComponent::ClearPendingEscapeRequest()
{
	const AActor* OwnerActor = GetOwner();
	if (IsValid(OwnerActor) && OwnerActor->HasAuthority())
	{
		ClearEscapeCastState();
	}
}

bool UHeistActionComponent::IsEscapeCastActive() const
{
	return bEscapeCastActive;
}

float UHeistActionComponent::GetEscapeCastEndServerTime() const
{
	return EscapeCastEndServerTime;
}

FHeistEscapeCastCompleted& UHeistActionComponent::GetEscapeCastCompletedDelegate()
{
	return EscapeCastCompletedDelegate;
}

FHeistActionStateChanged& UHeistActionComponent::GetActionStateChangedDelegate()
{
	return ActionStateChangedDelegate;
}

void UHeistActionComponent::OnRep_EscapeCastActive()
{
	ActionStateChangedDelegate.Broadcast();
	UHeistDebugFunctionLibrary::DebugEscapeCastStateReplicated(this, GetOwner(), bEscapeCastActive, EscapeCastEndServerTime);
}

void UHeistActionComponent::HandleOwnerTakeAnyDamage(AActor*, float Damage, const UDamageType*, AController*, AActor*)
{
	if (bEscapeCastActive && Damage > 0.0f)
	{
		CancelEscapeCast(TEXT("Damage"));
	}

	if (bObservationCastActive && Damage > 0.0f)
	{
		CancelObservationCast(TEXT("Damage"));
	}
}

#pragma endregion

#pragma region ObservationCast

bool UHeistActionComponent::TryBeginObservationRequest(AHeistPaintingDisplayCaseActor* TargetDisplayCase)
{
	AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter) ? HeistCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	UHeistForgeryComponent* ForgeryComponent = IsValid(HeistCharacter) ? HeistCharacter->GetForgeryComponent() : nullptr;
	if (!IsValid(HeistCharacter) || !HeistCharacter->HasAuthority() || !IsValid(HeistPlayerState) || !IsValid(TargetDisplayCase) || !IsValid(ForgeryComponent) || IsGameplayCastActive() ||
		TargetDisplayCase->GetDisplayCaseState() != EHeistDisplayCaseState::Secured)
	{
		return false;
	}

	float TemplateObservationDuration = 0.0f;
	if (!ForgeryComponent->TryPrepareForgeryTemplate(TargetDisplayCase, TemplateObservationDuration))
	{
		return false;
	}

	if (!TargetDisplayCase->TryBeginSession(HeistPlayerState))
	{
		ForgeryComponent->ClearPreparedForgeryTemplate(FName(TEXT("ObservationSessionRejected")));
		return false;
	}

	PendingObservationDisplayCase = TargetDisplayCase;
	bObservationCastActive = true;
	ObservationCastStartLocation = HeistCharacter->GetActorLocation();

	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const float ServerWorldTime = IsValid(HeistGameState) ? HeistGameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	const float SafeDurationSeconds = FMath::Max(0.0f, TemplateObservationDuration);
	ObservationCastEndServerTime = ServerWorldTime + SafeDurationSeconds;

	SetComponentTickEnabled(true);
	HeistCharacter->ForceNetUpdate();
	ActionStateChangedDelegate.Broadcast();
	UHeistDebugFunctionLibrary::DebugObservationCastStarted(this, HeistCharacter, TargetDisplayCase, SafeDurationSeconds, ObservationCastEndServerTime);

	if (SafeDurationSeconds <= 0.0f)
	{
		HandleObservationCastTimerElapsed();
	}
	else if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ObservationCastTimerHandle, this, &UHeistActionComponent::HandleObservationCastTimerElapsed, SafeDurationSeconds, false);
	}
	else
	{
		CancelObservationCast(TEXT("MissingWorld"));
		return false;
	}

	return true;
}

void UHeistActionComponent::CancelObservationRequest(const TCHAR* Reason)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		CancelObservationCast(Reason ? Reason : TEXT("OwnerCancelled"));
	}
}

bool UHeistActionComponent::IsObservationCastActive() const
{
	return bObservationCastActive;
}

float UHeistActionComponent::GetObservationCastEndServerTime() const
{
	return ObservationCastEndServerTime;
}

AHeistPaintingDisplayCaseActor* UHeistActionComponent::GetPendingObservationDisplayCase() const
{
	return PendingObservationDisplayCase.Get();
}

FHeistObservationCastCompleted& UHeistActionComponent::GetObservationCastCompletedDelegate()
{
	return ObservationCastCompletedDelegate;
}

void UHeistActionComponent::OnRep_ObservationCastActive()
{
	ActionStateChangedDelegate.Broadcast();
	UHeistDebugFunctionLibrary::DebugObservationCastStateReplicated(this, GetOwner(), bObservationCastActive, ObservationCastEndServerTime);
}

#pragma endregion

#pragma region Replication

void UHeistActionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UHeistActionComponent, bEscapeCastActive);
	DOREPLIFETIME(UHeistActionComponent, EscapeCastEndServerTime);
	DOREPLIFETIME(UHeistActionComponent, bObservationCastActive);
	DOREPLIFETIME(UHeistActionComponent, ObservationCastEndServerTime);
}

#pragma endregion

#pragma region InternalHelpers

float UHeistActionComponent::ResolveEscapeCastDurationSeconds() const
{
	const UWorld* World = GetWorld();
	const AHeistGameMode* HeistGameMode = World ? World->GetAuthGameMode<AHeistGameMode>() : nullptr;
	if (IsValid(HeistGameMode))
	{
		return HeistGameMode->GetEscapeCastTimeSeconds();
	}

	return FMath::Max(0.0f, GetDefault<UHeistGameBalanceDataAsset>()->EscapeCastTime);
}

bool UHeistActionComponent::HasMovedBeyondEscapeCastTolerance() const
{
	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return true;
	}

	return FVector::DistSquared2D(OwnerActor->GetActorLocation(), EscapeCastStartLocation) > FMath::Square(EscapeCastMovementCancelDistance);
}

bool UHeistActionComponent::HasMovedBeyondObservationCastTolerance() const
{
	const AActor* OwnerActor = GetOwner();
	return !IsValid(OwnerActor) || FVector::DistSquared2D(OwnerActor->GetActorLocation(), ObservationCastStartLocation) > FMath::Square(ObservationCastMovementCancelDistance);
}

void UHeistActionComponent::HandleEscapeCastTimerElapsed()
{
	AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	AHeistVentActor* TargetVentActor = PendingEscapeVent.Get();
	AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter) ? HeistCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	if (!bEscapeCastActive || !IsValid(HeistCharacter) || !IsValid(HeistPlayerState) || !IsValid(TargetVentActor) || HasMovedBeyondEscapeCastTolerance() ||
		!TargetVentActor->CanUseVent(HeistCharacter))
	{
		CancelEscapeCast(TEXT("CompletionValidationFailed"));
		return;
	}

	if (!HeistPlayerState->MarkEscaped())
	{
		CancelEscapeCast(TEXT("EscapeCommitRejected"));
		return;
	}

	ClearEscapeCastState();

	UHeistDebugFunctionLibrary::DebugEscapeCastCompleted(this, HeistCharacter, TargetVentActor);

	EscapeCastCompletedDelegate.Broadcast(HeistCharacter, TargetVentActor);
}

void UHeistActionComponent::HandleObservationCastTimerElapsed()
{
	AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter) ? HeistCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	AHeistPaintingDisplayCaseActor* TargetDisplayCase = PendingObservationDisplayCase.Get();
	if (!bObservationCastActive || !IsValid(HeistCharacter) || !IsValid(HeistPlayerState) || !IsValid(TargetDisplayCase) || HasMovedBeyondObservationCastTolerance() ||
		!TargetDisplayCase->IsSessionLocked() || TargetDisplayCase->GetSessionOwner() != HeistPlayerState || TargetDisplayCase->GetDisplayCaseState() != EHeistDisplayCaseState::Secured)
	{
		CancelObservationCast(TEXT("CompletionValidationFailed"));
		return;
	}

	if (!TargetDisplayCase->TryTransitionToDisplayCaseState(EHeistDisplayCaseState::Observed))
	{
		CancelObservationCast(TEXT("ObservationCommitRejected"));
		return;
	}

	UHeistForgeryComponent* ForgeryComponent = HeistCharacter->GetForgeryComponent();
	if (!IsValid(ForgeryComponent) || !ForgeryComponent->TryBeginForgerySession(TargetDisplayCase))
	{
		CancelObservationCast(TEXT("ForgeryHandoffRejected"));
		return;
	}

	ClearObservationCastState();
	UHeistDebugFunctionLibrary::DebugObservationCastCompleted(this, HeistCharacter, TargetDisplayCase);
	ObservationCastCompletedDelegate.Broadcast(HeistCharacter, TargetDisplayCase);
}

void UHeistActionComponent::CancelEscapeCast(const TCHAR* Reason)
{
	if (!bEscapeCastActive)
	{
		return;
	}

	const FString CharacterName = GetNameSafe(GetOwner());
	const FString VentName = GetNameSafe(PendingEscapeVent.Get());
	ClearEscapeCastState();

	UHeistDebugFunctionLibrary::DebugEscapeCastCancelled(this, CharacterName, VentName, Reason);
}

void UHeistActionComponent::CancelObservationCast(const TCHAR* Reason)
{
	if (!bObservationCastActive)
	{
		return;
	}

	AHeistPaintingDisplayCaseActor* TargetDisplayCase = PendingObservationDisplayCase.Get();
	AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter) ? HeistCharacter->GetPlayerState<AHeistPlayerState>() : nullptr;
	const FString CharacterName = GetNameSafe(HeistCharacter);
	const FString DisplayCaseName = GetNameSafe(TargetDisplayCase);
	ClearObservationCastState();

	if (IsValid(HeistCharacter) && IsValid(HeistCharacter->GetForgeryComponent()))
	{
		HeistCharacter->GetForgeryComponent()->ClearPreparedForgeryTemplate(FName(Reason ? Reason : TEXT("ObservationCancelled")));
	}

	if (IsValid(TargetDisplayCase) && IsValid(HeistPlayerState) && TargetDisplayCase->GetSessionOwner() == HeistPlayerState)
	{
		TargetDisplayCase->TryCancelSession(HeistPlayerState);
	}

	UHeistDebugFunctionLibrary::DebugObservationCastCancelled(this, CharacterName, DisplayCaseName, Reason);
}

void UHeistActionComponent::ClearEscapeCastState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EscapeCastTimerHandle);
	}

	PendingEscapeVent.Reset();
	bEscapeCastActive = false;
	EscapeCastEndServerTime = 0.0f;

	if (!bObservationCastActive)
	{
		SetComponentTickEnabled(false);	
	}

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}

	ActionStateChangedDelegate.Broadcast();
}

void UHeistActionComponent::ClearObservationCastState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ObservationCastTimerHandle);
	}

	PendingObservationDisplayCase.Reset();
	bObservationCastActive = false;
	ObservationCastEndServerTime = 0.0f;

	if (!bEscapeCastActive)
	{
		SetComponentTickEnabled(false);
	}

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}

	ActionStateChangedDelegate.Broadcast();
}

#pragma endregion
