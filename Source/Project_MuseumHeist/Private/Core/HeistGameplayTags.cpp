#include "Core/HeistGameplayTags.h"

#include "GameplayTagsManager.h"

FHeistGameplayTags FHeistGameplayTags::GameplayTags;

const FHeistGameplayTags& FHeistGameplayTags::Get()
{
	return GameplayTags;
}

void FHeistGameplayTags::InitializeNativeGameplayTags()
{
	UGameplayTagsManager& TagsManager = UGameplayTagsManager::Get();

#define HEIST_ADD_NATIVE_TAG(PropertyName, TagName, Description) \
	GameplayTags.PropertyName = TagsManager.AddNativeGameplayTag(FName(TEXT(TagName)), TEXT(Description))

	HEIST_ADD_NATIVE_TAG(State_Alive, "State.Alive", "Player is alive and has not escaped.");
	HEIST_ADD_NATIVE_TAG(State_Escaped, "State.Escaped", "Player has escaped.");
	HEIST_ADD_NATIVE_TAG(State_InventoryOpen, "State.InventoryOpen", "Player inventory is open.");
	HEIST_ADD_NATIVE_TAG(State_Interacting, "State.Interacting", "Player is performing an interaction cast.");
	HEIST_ADD_NATIVE_TAG(State_MovementLocked, "State.MovementLocked", "Player movement is locked.");
	HEIST_ADD_NATIVE_TAG(State_CarryingLoot, "State.CarryingLoot", "Player carries at least one loot item.");

	HEIST_ADD_NATIVE_TAG(Action_Looting, "Action.Looting", "Loot interaction cast is active.");
	HEIST_ADD_NATIVE_TAG(Action_Escaping, "Action.Escaping", "Vent escape cast is active.");
	HEIST_ADD_NATIVE_TAG(Action_Throwing, "Action.Throwing", "Throwable action is active.");
	HEIST_ADD_NATIVE_TAG(Action_UsingItem, "Action.UsingItem", "QuickSlot item use is active.");
	HEIST_ADD_NATIVE_TAG(Action_OpeningInventory, "Action.OpeningInventory", "Inventory opening action is active.");

	HEIST_ADD_NATIVE_TAG(Item_Loot, "Item.Loot", "Loot item category.");
	HEIST_ADD_NATIVE_TAG(Item_Loot_RoyalCrown, "Item.Loot.RoyalCrown", "Royal Crown loot.");
	HEIST_ADD_NATIVE_TAG(Item_Loot_Painting, "Item.Loot.Painting", "Painting loot.");
	HEIST_ADD_NATIVE_TAG(Item_Loot_AncientSword, "Item.Loot.AncientSword", "Ancient Sword loot.");
	HEIST_ADD_NATIVE_TAG(Item_Loot_GoldenVase, "Item.Loot.GoldenVase", "Golden Vase loot.");
	HEIST_ADD_NATIVE_TAG(Item_Loot_JewelNecklace, "Item.Loot.JewelNecklace", "Jewel Necklace loot.");
	HEIST_ADD_NATIVE_TAG(Item_Throwable, "Item.Throwable", "Throwable item category.");
	HEIST_ADD_NATIVE_TAG(Item_Throwable_Coin, "Item.Throwable.Coin", "Coin throwable item.");

	HEIST_ADD_NATIVE_TAG(Event_Loot_PickedUp, "Event.Loot.PickedUp", "Loot pickup completed.");
	HEIST_ADD_NATIVE_TAG(Event_Loot_Dropped, "Event.Loot.Dropped", "Loot was dropped.");
	HEIST_ADD_NATIVE_TAG(Event_Loot_RareSpawned, "Event.Loot.RareSpawned", "Rare loot event spawned.");
	HEIST_ADD_NATIVE_TAG(Event_Player_Stunned, "Event.Player.Stunned", "Player stun was confirmed.");
	HEIST_ADD_NATIVE_TAG(Event_Player_Escaped, "Event.Player.Escaped", "Player escape completed.");
	HEIST_ADD_NATIVE_TAG(Event_Player_Interrupted, "Event.Player.Interrupted", "Player cast was interrupted.");
	HEIST_ADD_NATIVE_TAG(Event_SoundPing_Footstep, "Event.SoundPing.Footstep", "Footstep sound ping.");
	HEIST_ADD_NATIVE_TAG(Event_SoundPing_ReplicaSwap, "Event.SoundPing.ReplicaSwap", "Replica frame swap sound ping.");
	HEIST_ADD_NATIVE_TAG(Event_SoundPing_GlassBreak, "Event.SoundPing.GlassBreak", "Glass break sound ping.");
	HEIST_ADD_NATIVE_TAG(Event_SoundPing_CoinImpact, "Event.SoundPing.CoinImpact", "Coin impact sound ping.");
	HEIST_ADD_NATIVE_TAG(Event_SoundPing_StunHit, "Event.SoundPing.StunHit", "Stun hit sound ping.");
	HEIST_ADD_NATIVE_TAG(Event_Vent_Opened, "Event.Vent.Opened", "Vent extraction phase opened.");
	HEIST_ADD_NATIVE_TAG(Event_Vent_Used, "Event.Vent.Used", "Vent escape completed.");
	HEIST_ADD_NATIVE_TAG(Event_Match_Started, "Event.Match.Started", "Match started.");
	HEIST_ADD_NATIVE_TAG(Event_Match_Ended, "Event.Match.Ended", "Match ended.");

	HEIST_ADD_NATIVE_TAG(AI_State_Disabled, "AI.State.Disabled", "Guard disabled state.");
	HEIST_ADD_NATIVE_TAG(AI_State_Stunned, "AI.State.Stunned", "Guard stunned state.");
	HEIST_ADD_NATIVE_TAG(AI_State_Patrol, "AI.State.Patrol", "Guard patrol state.");
	HEIST_ADD_NATIVE_TAG(AI_State_InvestigateNoise, "AI.State.InvestigateNoise", "Guard noise investigation state.");
	HEIST_ADD_NATIVE_TAG(AI_State_InspectExhibit, "AI.State.InspectExhibit", "Guard exhibit inspection state.");
	HEIST_ADD_NATIVE_TAG(AI_State_ChasePlayer, "AI.State.ChasePlayer", "Guard player chase state.");
	HEIST_ADD_NATIVE_TAG(AI_State_SearchLastKnownLocation, "AI.State.SearchLastKnownLocation", "Guard last-known-location search state.");
	HEIST_ADD_NATIVE_TAG(AI_State_ReturnToPatrol, "AI.State.ReturnToPatrol", "Guard return-to-patrol state.");
	HEIST_ADD_NATIVE_TAG(AI_Stimulus_Sight, "AI.Stimulus.Sight", "Guard sight stimulus.");
	HEIST_ADD_NATIVE_TAG(AI_Stimulus_Noise, "AI.Stimulus.Noise", "Guard noise stimulus.");
	HEIST_ADD_NATIVE_TAG(AI_Stimulus_Coin, "AI.Stimulus.Coin", "Guard coin stimulus.");
	HEIST_ADD_NATIVE_TAG(AI_Stimulus_GlassBreak, "AI.Stimulus.GlassBreak", "Guard glass break stimulus.");

	HEIST_ADD_NATIVE_TAG(Vent_Locked, "Vent.Locked", "Vent is locked.");
	HEIST_ADD_NATIVE_TAG(Vent_Active, "Vent.Active", "Vent is active.");
	HEIST_ADD_NATIVE_TAG(Vent_Casting, "Vent.Casting", "Vent escape cast is active.");
	HEIST_ADD_NATIVE_TAG(Vent_Used, "Vent.Used", "Vent was used.");
	HEIST_ADD_NATIVE_TAG(Match_Lobby, "Match.Lobby", "Match is in lobby phase.");
	HEIST_ADD_NATIVE_TAG(Match_InGame, "Match.InGame", "Match is in gameplay phase.");
	HEIST_ADD_NATIVE_TAG(Match_ExtractionPhase, "Match.ExtractionPhase", "Match is in extraction phase.");
	HEIST_ADD_NATIVE_TAG(Match_End, "Match.End", "Match has ended.");
	HEIST_ADD_NATIVE_TAG(UI_Warning_InventoryFull, "UI.Warning.InventoryFull", "Inventory full warning.");

#undef HEIST_ADD_NATIVE_TAG
}
