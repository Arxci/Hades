#include "HadesAbility_Run.h"

#include "Hades/Character/HadesCharacterBase.h"
#include "Hades/HadesGameplayTags.h"

UHadesAbility_Run::UHadesAbility_Run()
{
	SetAssetTags(FGameplayTagContainer(HadesGameplayTags::Ability_Run));
	CancelAbilitiesWithTag.AddTag(HadesGameplayTags::Ability_Crouch);

	InputTag = HadesGameplayTags::Input_Run;
	bEndOnInputRelease = true;
}

void UHadesAbility_Run::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		constexpr bool bReplicateEndAbility = true;
		constexpr bool bWasCancelled = true;
		EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
		return;
	}

	if (AHadesCharacterBase* Character = GetHadesCharacterFromActorInfo())
	{
		Character->Run();
	}
	
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UHadesAbility_Run::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (AHadesCharacterBase* Character = GetHadesCharacterFromActorInfo())
	{
		Character->StopRun();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
