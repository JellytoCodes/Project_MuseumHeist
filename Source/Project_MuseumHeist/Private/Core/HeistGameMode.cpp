#include "Core/HeistGameMode.h"

#include "Character/HeistPlayerCharacter.h"
#include "Core/HeistGameState.h"
#include "Core/HeistHUD.h"
#include "Core/HeistLogChannels.h"
#include "Core/HeistPlayerController.h"
#include "Core/HeistPlayerState.h"

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

AHeistGameMode::AHeistGameMode()
{
	PlayerControllerClass = AHeistPlayerController::StaticClass();
	PlayerStateClass = AHeistPlayerState::StaticClass();
	GameStateClass = AHeistGameState::StaticClass();
	HUDClass = AHeistHUD::StaticClass();
	DefaultPawnClass = AHeistPlayerCharacter::StaticClass();
}

void AHeistGameMode::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogHeist, Log, TEXT("HeistGameMode BeginPlay"));
}

void AHeistGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	AHeistPlayerState* HeistPlayerState = NewPlayer ? NewPlayer->GetPlayerState<AHeistPlayerState>() : nullptr;
	const AHeistGameState* HeistGameState = GetGameState<AHeistGameState>();
	const int32 ConnectedPlayers = HeistGameState ? HeistGameState->GetConnectedPlayerCount() : 0;

	UE_LOG(
		LogHeistNetwork,
		Log,
		TEXT("Player joined: Controller=%s ConnectedPlayers=%d HasPlayerController=%s HasPlayerState=%s HasPawn=%s HeistPlayerId=%d"),
		*GetNameSafe(NewPlayer),
		ConnectedPlayers,
		NewPlayer ? TEXT("true") : TEXT("false"),
		HeistPlayerState ? TEXT("true") : TEXT("false"),
		NewPlayer && NewPlayer->GetPawn() ? TEXT("true") : TEXT("false"),
		HeistPlayerState ? HeistPlayerState->HeistPlayerId : INDEX_NONE);
}

void AHeistGameMode::Logout(AController* Exiting)
{
	const FString LeavingControllerName = GetNameSafe(Exiting);
	const bool bHasLeavingPlayerState = Exiting && Exiting->GetPlayerState<AHeistPlayerState>();

	Super::Logout(Exiting);

	const AHeistGameState* HeistGameState = GetGameState<AHeistGameState>();
	const int32 TrackedPlayers = HeistGameState ? HeistGameState->GetConnectedPlayerCount() : 0;
	const int32 RemainingPlayers = FMath::Max(0, TrackedPlayers - (bHasLeavingPlayerState ? 1 : 0));

	UE_LOG(
		LogHeistNetwork,
		Log,
		TEXT("Player left: Controller=%s RemainingPlayers=%d"),
		*LeavingControllerName,
		RemainingPlayers);
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

	const APawn* SpawnedPawn = NewPlayer ? NewPlayer->GetPawn() : nullptr;
	const FString PawnLocation = SpawnedPawn ? SpawnedPawn->GetActorLocation().ToCompactString() : TEXT("Unavailable");

	UE_LOG(
		LogHeistNetwork,
		Log,
		TEXT("Player spawn verified: Controller=%s HeistPlayerId=%d Pawn=%s Location=%s"),
		*GetNameSafe(NewPlayer),
		HeistPlayerState ? HeistPlayerState->HeistPlayerId : INDEX_NONE,
		*GetNameSafe(SpawnedPawn),
		*PawnLocation);
}
