#include "Character/Components/HeistActionComponent.h"

#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameMode.h"
#include "Core/HeistGameState.h"
#include "Core/HeistPlayerState.h"
#include "Data/HeistGameBalanceDataAsset.h"
#include "Debug/HeistDebugFunctionLibrary.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "World/HeistVentActor.h"

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
	}

	Super::EndPlay(EndPlayReason);
}

void UHeistActionComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEscapeCastActive)
	{
		SetComponentTickEnabled(false);
		return;
	}

	const AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	if (!IsValid(HeistCharacter) || !PendingEscapeVent.IsValid())
	{
		CancelEscapeCast(TEXT("InvalidCastState"));
		return;
	}

	if (HasMovedBeyondEscapeCastTolerance())
	{
		CancelEscapeCast(TEXT("Movement"));
		return;
	}

	if (!PendingEscapeVent->CanUseVent(HeistCharacter))
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
	const AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter)
		? HeistCharacter->GetPlayerState<AHeistPlayerState>()
		: nullptr;
	if (!IsValid(OwnerActor)
		|| !OwnerActor->HasAuthority()
		|| !IsValid(HeistPlayerState)
		|| HeistPlayerState->IsEscaped()
		|| !IsValid(TargetVentActor)
		|| bEscapeCastActive
		|| PendingEscapeVent.IsValid())
	{
		return false;
	}

	PendingEscapeVent = TargetVentActor;
	bEscapeCastActive = true;
	EscapeCastStartLocation = OwnerActor->GetActorLocation();

	const float EscapeCastDurationSeconds = ResolveEscapeCastDurationSeconds();
	const AHeistGameState* HeistGameState = GetWorld() ? GetWorld()->GetGameState<AHeistGameState>() : nullptr;
	const float ServerWorldTime = IsValid(HeistGameState)
		? HeistGameState->GetServerWorldTimeSeconds()
		: (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	EscapeCastEndServerTime = ServerWorldTime + EscapeCastDurationSeconds;

	SetComponentTickEnabled(true);
	OwnerActor->ForceNetUpdate();

	UHeistDebugFunctionLibrary::DebugEscapeCastStarted(
		this,
		OwnerActor,
		TargetVentActor,
		EscapeCastDurationSeconds,
		EscapeCastEndServerTime);

	if (EscapeCastDurationSeconds <= 0.0f)
	{
		HandleEscapeCastTimerElapsed();
	}
	else if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			EscapeCastTimerHandle,
			this,
			&UHeistActionComponent::HandleEscapeCastTimerElapsed,
			EscapeCastDurationSeconds,
			false);
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
	return bEscapeCastActive;
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

void UHeistActionComponent::OnRep_EscapeCastActive()
{
	UHeistDebugFunctionLibrary::DebugEscapeCastStateReplicated(
		this,
		GetOwner(),
		bEscapeCastActive,
		EscapeCastEndServerTime);
}

void UHeistActionComponent::HandleOwnerTakeAnyDamage(
	AActor*,
	float Damage,
	const UDamageType*,
	AController*,
	AActor*)
{
	if (bEscapeCastActive && Damage > 0.0f)
	{
		CancelEscapeCast(TEXT("Damage"));
	}
}

#pragma endregion

#pragma region Replication

void UHeistActionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UHeistActionComponent, bEscapeCastActive);
	DOREPLIFETIME(UHeistActionComponent, EscapeCastEndServerTime);
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

	return FVector::DistSquared2D(OwnerActor->GetActorLocation(), EscapeCastStartLocation)
		> FMath::Square(EscapeCastMovementCancelDistance);
}

void UHeistActionComponent::HandleEscapeCastTimerElapsed()
{
	AHeistPlayerCharacter* HeistCharacter = Cast<AHeistPlayerCharacter>(GetOwner());
	AHeistVentActor* TargetVentActor = PendingEscapeVent.Get();
	AHeistPlayerState* HeistPlayerState = IsValid(HeistCharacter)
		? HeistCharacter->GetPlayerState<AHeistPlayerState>()
		: nullptr;
	if (!bEscapeCastActive
		|| !IsValid(HeistCharacter)
		|| !IsValid(HeistPlayerState)
		|| !IsValid(TargetVentActor)
		|| HasMovedBeyondEscapeCastTolerance()
		|| !TargetVentActor->CanUseVent(HeistCharacter))
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

void UHeistActionComponent::ClearEscapeCastState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EscapeCastTimerHandle);
	}

	PendingEscapeVent.Reset();
	bEscapeCastActive = false;
	EscapeCastEndServerTime = 0.0f;
	SetComponentTickEnabled(false);

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
}

#pragma endregion
