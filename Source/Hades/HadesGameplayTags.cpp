#include "HadesGameplayTags.h"

namespace HadesGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG(Ability_Run,            "Ability.Run");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Crouch,         "Ability.Crouch");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Intent_Crouch,   "Ability.Intent.Crouch");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Intent_UnCrouch, "Ability.Intent.UnCrouch");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Intent_Run,      "Ability.Intent.Run");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Intent_Walk,     "Ability.Intent.Walk");

	UE_DEFINE_GAMEPLAY_TAG(Character_State_Running, "Character.State.Running");
	UE_DEFINE_GAMEPLAY_TAG(Character_State_Crouching, "Character.State.Crouching");

	UE_DEFINE_GAMEPLAY_TAG(Input_Move,   "Input.Move");
	UE_DEFINE_GAMEPLAY_TAG(Input_Look,   "Input.Look");
	UE_DEFINE_GAMEPLAY_TAG(Input_Jump,   "Input.Jump");
	UE_DEFINE_GAMEPLAY_TAG(Input_Run,    "Input.Run");
	UE_DEFINE_GAMEPLAY_TAG(Input_Crouch, "Input.Crouch");
}

