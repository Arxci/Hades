#include "HadesGameplayTags.h"

namespace HadesGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG(Ability_Run,            "Ability.Run");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Crouch,         "Ability.Crouch");

	UE_DEFINE_GAMEPLAY_TAG(Character_State_Running, "Character.State.Running");
	UE_DEFINE_GAMEPLAY_TAG(Character_State_Crouching, "Character.State.Crouching");

	UE_DEFINE_GAMEPLAY_TAG(Interactable_State_InRange, "Interactable.State.InRange");
	UE_DEFINE_GAMEPLAY_TAG(Interactable_State_Focused, "Interactable.State.Focused");
	UE_DEFINE_GAMEPLAY_TAG(Interactable_State_Occluded, "Interactable.State.Occluded");
	UE_DEFINE_GAMEPLAY_TAG(Interactable_State_Disabled, "Interactable.State.Disabled");
	UE_DEFINE_GAMEPLAY_TAG(Interactable_State_Interacting, "Interactable.State.Interacting");

	UE_DEFINE_GAMEPLAY_TAG(Input_Move,   "Input.Move");
	UE_DEFINE_GAMEPLAY_TAG(Input_Look,   "Input.Look");
	UE_DEFINE_GAMEPLAY_TAG(Input_Jump,   "Input.Jump");
	UE_DEFINE_GAMEPLAY_TAG(Input_Run,    "Input.Run");
	UE_DEFINE_GAMEPLAY_TAG(Input_Crouch, "Input.Crouch");
}