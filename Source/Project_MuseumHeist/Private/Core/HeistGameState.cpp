#include "Core/HeistGameState.h"

AHeistGameState::AHeistGameState()
{
}

int32 AHeistGameState::GetConnectedPlayerCount() const
{
	return PlayerArray.Num();
}
