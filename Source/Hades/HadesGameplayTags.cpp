#include "HadesGameplayTags.h"

namespace HadesGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG(Ability_Run,            "Ability.Run");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Crouch,         "Ability.Crouch");

	UE_DEFINE_GAMEPLAY_TAG(Character_State_Running, "Character.State.Running");
	UE_DEFINE_GAMEPLAY_TAG(Character_State_Crouching, "Character.State.Crouching");

	UE_DEFINE_GAMEPLAY_TAG(Interactable_State_Nearby, "Interactable.State.Nearby");
	UE_DEFINE_GAMEPLAY_TAG(Interactable_State_Focused, "Interactable.State.Focused");
	UE_DEFINE_GAMEPLAY_TAG(Interactable_State_Occluded, "Interactable.State.Occluded");
	UE_DEFINE_GAMEPLAY_TAG(Interactable_State_Disabled, "Interactable.State.Disabled");
	UE_DEFINE_GAMEPLAY_TAG(Interactable_State_Interacting, "Interactable.State.Interacting");

	UE_DEFINE_GAMEPLAY_TAG(Interactable_Event_Completed, "Interactable.Event.Completed");
	UE_DEFINE_GAMEPLAY_TAG(Interactable_Event_Cancelled, "Interactable.Event.Cancelled");

	UE_DEFINE_GAMEPLAY_TAG(UI_Layer_Game,     "UI.Layer.Game");
	UE_DEFINE_GAMEPLAY_TAG(UI_Layer_GameMenu, "UI.Layer.GameMenu");
	UE_DEFINE_GAMEPLAY_TAG(UI_Layer_Menu,     "UI.Layer.Menu");
	UE_DEFINE_GAMEPLAY_TAG(UI_Layer_Modal,    "UI.Layer.Modal");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Preview_QuickSlot, "UI.Preview.QuickSlot", "Tag value on a designer preview item: the quick slot it's drawn on, 1 for the first.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Preview_InHand, "UI.Preview.InHand", "Tag value on a designer preview item: above 0 draws it in hand.");

	UE_DEFINE_GAMEPLAY_TAG(Input_Move,   "Input.Move");
	UE_DEFINE_GAMEPLAY_TAG(Input_Look,   "Input.Look");
	UE_DEFINE_GAMEPLAY_TAG(Input_Jump,   "Input.Jump");
	UE_DEFINE_GAMEPLAY_TAG(Input_Run,    "Input.Run");
	UE_DEFINE_GAMEPLAY_TAG(Input_Crouch, "Input.Crouch");
	UE_DEFINE_GAMEPLAY_TAG(Input_Interact, "Input.Interact");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_LightSource, "Input.LightSource", "Turns the light source the player carries on or off.");
	UE_DEFINE_GAMEPLAY_TAG(Input_QuickSlot_1, "Input.QuickSlot.1");
	UE_DEFINE_GAMEPLAY_TAG(Input_QuickSlot_2, "Input.QuickSlot.2");
	UE_DEFINE_GAMEPLAY_TAG(Input_QuickSlot_3, "Input.QuickSlot.3");
	UE_DEFINE_GAMEPLAY_TAG(Input_QuickSlot_4, "Input.QuickSlot.4");

	UE_DEFINE_GAMEPLAY_TAG(Container_Inventory,  "Container.Inventory");
	UE_DEFINE_GAMEPLAY_TAG(Container_KeyItems, "Container.KeyItems");
	UE_DEFINE_GAMEPLAY_TAG(Container_ItemBox,  "Container.ItemBox");
	UE_DEFINE_GAMEPLAY_TAG(Container_Pickup,   "Container.Pickup");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Container_LightSource, "Container.LightSource", "The one slot holding the light source the player carries.");

	UE_DEFINE_GAMEPLAY_TAG(Equipment_Slot_Hand_Right, "Equipment.Slot.Hand.Right");
	UE_DEFINE_GAMEPLAY_TAG(Equipment_Slot_Hand_Left,  "Equipment.Slot.Hand.Left");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot_LightSource, "Equipment.Slot.LightSource", "The light source slot: a flashlight, a lighter, a flare, one at a time.");

	UE_DEFINE_GAMEPLAY_TAG(Item_Type_KeyItem, "Item.Type.KeyItem");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Type_LightSource, "Item.Type.LightSource", "A light source: it goes in the light source slot, never the inventory grid.");
	UE_DEFINE_GAMEPLAY_TAG(Item_Stat_Uses,    "Item.Stat.Uses");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Stat_Rounds,   "Item.Stat.Rounds",   "Rounds loaded in a gun. Its definition's value is what it's found with.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Stat_Capacity, "Item.Stat.Capacity", "Rounds a gun holds when full. Attachments can add to it, such as an extended magazine.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Stat_Charge,   "Item.Stat.Charge",   "What a light source has left to burn, such as a battery or fuel. Its definition's value is full.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_State_LightOn, "Item.State.LightOn", "Tag value on a light source: 1 while its light is on. Give its definition 1 for one found on.");
}