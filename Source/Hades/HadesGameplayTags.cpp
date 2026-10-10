#include "HadesGameplayTags.h"

namespace HadesGameplayTags
{
	// Ability identity tags
	UE_DEFINE_GAMEPLAY_TAG(Ability_Run,            "Ability.Run");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Crouch,         "Ability.Crouch");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Intent_Crouch,   "Ability.Intent.Crouch");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Intent_UnCrouch, "Ability.Intent.UnCrouch");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Intent_Run,      "Ability.Intent.Run");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Intent_Walk,     "Ability.Intent.Walk");

	UE_DEFINE_GAMEPLAY_TAG(Character_State_Running, "Character.State.Running");
	UE_DEFINE_GAMEPLAY_TAG(Character_State_Crouching, "Character.State.Crouching");
}

