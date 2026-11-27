#include "HadesAbility_ToggleLightSource.h"

#include "Hades/HadesGameplayTags.h"
#include "Hades/LightSource/HadesLightSource.h"

UHadesAbility_ToggleLightSource::UHadesAbility_ToggleLightSource()
{
	InputTag = HadesGameplayTags::Input_LightSource;
}

void UHadesAbility_ToggleLightSource::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	constexpr bool bReplicateEndAbility = true;
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, /*bWasCancelled*/ true);
		return;
	}

	if (AHadesLightSource* Light = FindSpawnedActor<AHadesLightSource>())
	{
		Light->ToggleLight();
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, /*bWasCancelled*/ false);
}
