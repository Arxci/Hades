#include "HadesAbility_Crouch.h"

#include "Hades/Character/HadesCharacterBase.h"
#include "Hades/HadesGameplayTags.h"


UHadesAbility_Crouch::UHadesAbility_Crouch()
{
	SetAssetTags(FGameplayTagContainer(HadesGameplayTags::Ability_Crouch));
	CancelAbilitiesWithTag.AddTag(HadesGameplayTags::Ability_Run);

	InputTag = HadesGameplayTags::Input_Crouch;
	bEndOnInputRelease = true;
}

void UHadesAbility_Crouch::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
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
		Character->Crouch();
	}
	
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UHadesAbility_Crouch::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (AHadesCharacterBase* Character = GetHadesCharacterFromActorInfo())
	{
		Character->UnCrouch();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
