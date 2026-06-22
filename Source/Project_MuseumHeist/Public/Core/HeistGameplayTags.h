#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

struct PROJECT_MUSEUMHEIST_API FHeistGameplayTags
{
	static const FHeistGameplayTags& Get();
	static void InitializeNativeGameplayTags();

	FGameplayTag State_Alive;
	FGameplayTag State_Stunned;
	FGameplayTag State_StunImmune;
	FGameplayTag State_Escaped;
	FGameplayTag State_InventoryOpen;
	FGameplayTag State_Interacting;
	FGameplayTag State_MovementLocked;
	FGameplayTag State_CarryingLoot;
	FGameplayTag State_Revealed;
	FGameplayTag State_InSmoke;

	FGameplayTag Action_Looting;
	FGameplayTag Action_Escaping;
	FGameplayTag Action_PlacingTrap;
	FGameplayTag Action_Throwing;
	FGameplayTag Action_UsingItem;
	FGameplayTag Action_OpeningInventory;

	FGameplayTag Item_Loot;
	FGameplayTag Item_Loot_RoyalCrown;
	FGameplayTag Item_Loot_RareArtifact;
	FGameplayTag Item_Loot_Painting;
	FGameplayTag Item_Loot_AncientSword;
	FGameplayTag Item_Loot_GoldenVase;
	FGameplayTag Item_Loot_JewelNecklace;
	FGameplayTag Item_Trap;
	FGameplayTag Item_Trap_Glue;
	FGameplayTag Item_Trap_Noise;
	FGameplayTag Item_Throwable;
	FGameplayTag Item_Throwable_Coin;
	FGameplayTag Item_Throwable_Smoke;

	FGameplayTag Event_Loot_PickedUp;
	FGameplayTag Event_Loot_Dropped;
	FGameplayTag Event_Loot_RareSpawned;
	FGameplayTag Event_Player_Stunned;
	FGameplayTag Event_Player_Escaped;
	FGameplayTag Event_Player_Interrupted;
	FGameplayTag Event_Trap_Placed;
	FGameplayTag Event_Trap_Triggered;
	FGameplayTag Event_SoundPing_Footstep;
	FGameplayTag Event_SoundPing_GlassBreak;
	FGameplayTag Event_SoundPing_CoinImpact;
	FGameplayTag Event_SoundPing_NoiseTrap;
	FGameplayTag Event_SoundPing_StunHit;
	FGameplayTag Event_Vent_Opened;
	FGameplayTag Event_Vent_Used;
	FGameplayTag Event_Match_Started;
	FGameplayTag Event_Match_Ended;

	FGameplayTag AI_State_Patrol;
	FGameplayTag AI_State_Chase;
	FGameplayTag AI_State_Stunned;
	FGameplayTag AI_State_Investigate;
	FGameplayTag AI_Stimulus_Sight;
	FGameplayTag AI_Stimulus_Noise;
	FGameplayTag AI_Stimulus_Coin;
	FGameplayTag AI_Stimulus_GlassBreak;
	FGameplayTag AI_Stimulus_Trap;

	FGameplayTag Vent_Locked;
	FGameplayTag Vent_Active;
	FGameplayTag Vent_Casting;
	FGameplayTag Vent_Used;

	FGameplayTag Match_Lobby;
	FGameplayTag Match_InGame;
	FGameplayTag Match_ExtractionPhase;
	FGameplayTag Match_End;

	FGameplayTag UI_Warning_InventoryFull;
	FGameplayTag UI_Warning_Revealed;
	FGameplayTag UI_Warning_RareLootIncoming;
	FGameplayTag UI_Indicator_GapTracker;
	FGameplayTag UI_Indicator_SoundPing;

private:
	static FHeistGameplayTags GameplayTags;
};
