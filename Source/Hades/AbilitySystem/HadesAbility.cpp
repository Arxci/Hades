#include "HadesAbility.h"

#include "Hades/Character/HadesCharacterBase.h"

UHadesAbility::UHadesAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

AHadesCharacterBase* UHadesAbility::GetHadesCharacterFromActorInfo() const
{
	return Cast<AHadesCharacterBase>(GetAvatarActorFromActorInfo());
}
