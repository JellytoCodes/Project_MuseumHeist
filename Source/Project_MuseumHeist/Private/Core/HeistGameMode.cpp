#include "Core/HeistGameMode.h"

#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameState.h"
#include "Core/HeistHUD.h"
#include "Core/HeistLogChannels.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistPlayerState.h"
#include "Data/HeistGameBalanceDataAsset.h"
#include "Engine/World.h"
#include "TimerManager.h"

#pragma region InternalHelpers

namespace
{
	const FLinearColor VerificationPlayerColors[] =
	{
		FLinearColor::Red,
		FLinearColor::Green,
		FLinearColor::Blue,
		FLinearColor::Yellow
	};
}

#pragma endregion

#pragma region Construction

AHeistGameMode::AHeistGameMode()
{
	PlayerControllerClass = AHeistPlayerController::StaticClass();
	PlayerStateClass = AHeistPlayerState::StaticClass();
	GameStateClass = AHeistGameState::StaticClass();
	HUDClass = AHeistHUD::StaticClass();
	DefaultPawnClass = AHeistPlayerCharacter::StaticClass();
}

#pragma endregion

#pragma region Lifecycle

void AHeistGameMode::StartPlay()
{
	Super::StartPlay();
	StartEscapePhaseTimer();
}

void AHeistGameMode::RestartPlayer(AController* NewPlayer)
{
	AHeistPlayerState* HeistPlayerState = NewPlayer ? NewPlayer->GetPlayerState<AHeistPlayerState>() : nullptr;
	if (HeistPlayerState && HeistPlayerState->HeistPlayerId == INDEX_NONE)
	{
		const int32 AssignedPlayerId = NextHeistPlayerId++;
		const int32 ColorIndex = (AssignedPlayerId - 1) % UE_ARRAY_COUNT(VerificationPlayerColors);
		HeistPlayerState->InitializeVerificationIdentity(AssignedPlayerId, VerificationPlayerColors[ColorIndex]);
	}

	Super::RestartPlayer(NewPlayer);
}

#pragma endregion

#pragma region EscapePhase

float AHeistGameMode::GetEscapeCastTimeSeconds() const
{
	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset)
		? GameBalanceDataAsset.Get()
		: GetDefault<UHeistGameBalanceDataAsset>();

	return FMath::Max(0.0f, ResolvedBalanceData->EscapeCastTime);
}

void AHeistGameMode::StartEscapePhaseTimer()
{
	if (!HasAuthority())
	{
		return;
	}

	AHeistGameState* HeistGameState = GetGameState<AHeistGameState>();
	if (!IsValid(HeistGameState))
	{
		UE_LOG(LogHeist, Warning, TEXT("Escape phase timer skipped: Reason=MissingGameState"));
		return;
	}

	FTimerManager& TimerManager = GetWorldTimerManager();
	if (TimerManager.IsTimerActive(EscapePhaseTimerHandle) || HeistGameState->IsEscapePhaseOpen())
	{
		return;
	}

	const float EscapePhaseDelaySeconds = ResolveEscapePhaseDelaySeconds();
	HeistGameState->InitializeEscapePhase(EscapePhaseDelaySeconds);

	if (EscapePhaseDelaySeconds <= 0.0f)
	{
		HeistGameState->OpenEscapePhase();
		return;
	}

	TimerManager.SetTimer(
		EscapePhaseTimerHandle,
		this,
		&AHeistGameMode::HandleEscapePhaseTimerElapsed,
		EscapePhaseDelaySeconds,
		false);

	UE_LOG(
		LogHeist,
		Log,
		TEXT("Escape phase timer started: Delay=%.2f BalanceData=%s"),
		EscapePhaseDelaySeconds,
		*GetNameSafe(GameBalanceDataAsset));
}

void AHeistGameMode::HandleEscapePhaseTimerElapsed()
{
	if (AHeistGameState* HeistGameState = GetGameState<AHeistGameState>())
	{
		HeistGameState->OpenEscapePhase();
	}
	else
	{
		UE_LOG(LogHeist, Warning, TEXT("Escape phase open skipped: Reason=MissingGameState"));
	}
}

float AHeistGameMode::ResolveEscapePhaseDelaySeconds() const
{
	const UHeistGameBalanceDataAsset* ResolvedBalanceData = IsValid(GameBalanceDataAsset)
		? GameBalanceDataAsset.Get()
		: GetDefault<UHeistGameBalanceDataAsset>();

	return FMath::Max(0.0f, ResolvedBalanceData->VentUnlockTime);
}

#pragma endregion
