#include "Core/HeistPlayerState.h"

#include "Core/HeistLogChannels.h"
#include "Net/UnrealNetwork.h"

AHeistPlayerState::AHeistPlayerState()
{
}

void AHeistPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeistPlayerState, HeistPlayerId);
	DOREPLIFETIME(AHeistPlayerState, PlayerColor);
}

void AHeistPlayerState::InitializeVerificationIdentity(int32 InHeistPlayerId, const FLinearColor& InPlayerColor)
{
	check(HasAuthority());

	HeistPlayerId = InHeistPlayerId;
	PlayerColor = InPlayerColor;
	ForceNetUpdate();
}

void AHeistPlayerState::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogHeist, Log, TEXT("HeistPlayerState BeginPlay"));
}

void AHeistPlayerState::OnRep_HeistPlayerId()
{
	UE_LOG(
		LogHeistNetwork,
		Log,
		TEXT("PlayerState ID replicated: PlayerState=%s HeistPlayerId=%d"),
		*GetNameSafe(this),
		HeistPlayerId);
}

void AHeistPlayerState::OnRep_PlayerColor()
{
	UE_LOG(
		LogHeistNetwork,
		Log,
		TEXT("PlayerState color replicated: PlayerState=%s PlayerColor=%s"),
		*GetNameSafe(this),
		*PlayerColor.ToString());
}
