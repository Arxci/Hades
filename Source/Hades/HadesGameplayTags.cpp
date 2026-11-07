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

	UE_DEFINE_GAMEPLAY_TAG(Input_Move,   "Input.Move");
	UE_DEFINE_GAMEPLAY_TAG(Input_Look,   "Input.Look");
	UE_DEFINE_GAMEPLAY_TAG(Input_Jump,   "Input.Jump");
	UE_DEFINE_GAMEPLAY_TAG(Input_Run,    "Input.Run");
	UE_DEFINE_GAMEPLAY_TAG(Input_Crouch, "Input.Crouch");
	UE_DEFINE_GAMEPLAY_TAG(Input_Interact, "Input.Interact");
}