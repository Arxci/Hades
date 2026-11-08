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

	UE_DEFINE_GAMEPLAY_TAG(UI_Action_Escape, "UI.Action.Escape");
	UE_DEFINE_GAMEPLAY_TAG(UI_Action_Inventory, "UI.Action.Inventory");
	UE_DEFINE_GAMEPLAY_TAG(UI_Action_RotateItem, "UI.Action.RotateItem");
	UE_DEFINE_GAMEPLAY_TAG(UI_Action_SplitStack, "UI.Action.SplitStack");
	UE_DEFINE_GAMEPLAY_TAG(UI_Action_ItemMenu, "UI.Action.ItemMenu");
	UE_DEFINE_GAMEPLAY_TAG(UI_Action_Organize, "UI.Action.Organize");

	UE_DEFINE_GAMEPLAY_TAG(Input_Move,   "Input.Move");
	UE_DEFINE_GAMEPLAY_TAG(Input_Look,   "Input.Look");
	UE_DEFINE_GAMEPLAY_TAG(Input_Jump,   "Input.Jump");
	UE_DEFINE_GAMEPLAY_TAG(Input_Run,    "Input.Run");
	UE_DEFINE_GAMEPLAY_TAG(Input_Crouch, "Input.Crouch");
	UE_DEFINE_GAMEPLAY_TAG(Input_Interact, "Input.Interact");
	UE_DEFINE_GAMEPLAY_TAG(Input_QuickSlot_1, "Input.QuickSlot.1");
	UE_DEFINE_GAMEPLAY_TAG(Input_QuickSlot_2, "Input.QuickSlot.2");
	UE_DEFINE_GAMEPLAY_TAG(Input_QuickSlot_3, "Input.QuickSlot.3");
	UE_DEFINE_GAMEPLAY_TAG(Input_QuickSlot_4, "Input.QuickSlot.4");

	UE_DEFINE_GAMEPLAY_TAG(Container_Pockets,  "Container.Pockets");
	UE_DEFINE_GAMEPLAY_TAG(Container_KeyItems, "Container.KeyItems");
	UE_DEFINE_GAMEPLAY_TAG(Container_Hands,    "Container.Hands");
	UE_DEFINE_GAMEPLAY_TAG(Container_ItemBox,  "Container.ItemBox");
	UE_DEFINE_GAMEPLAY_TAG(Container_Pickup,   "Container.Pickup");

	UE_DEFINE_GAMEPLAY_TAG(Equipment_Slot_Hand_Right, "Equipment.Slot.Hand.Right");
	UE_DEFINE_GAMEPLAY_TAG(Equipment_Slot_Hand_Left,  "Equipment.Slot.Hand.Left");

	UE_DEFINE_GAMEPLAY_TAG(Item_Type_KeyItem, "Item.Type.KeyItem");
	UE_DEFINE_GAMEPLAY_TAG(Item_Type_Pouch,   "Item.Type.Pouch");
	UE_DEFINE_GAMEPLAY_TAG(Item_Stat_Uses,    "Item.Stat.Uses");
}