#pragma once

#include "Engine/EngineTypes.h"

namespace HeistCollisionChannels
{
	inline constexpr ECollisionChannel InteractionTrace = ECC_GameTraceChannel1;
	inline constexpr ECollisionChannel Player = ECC_GameTraceChannel2;
	inline constexpr ECollisionChannel Guard = ECC_GameTraceChannel3;
	inline constexpr ECollisionChannel Interactable = ECC_GameTraceChannel4;
}
