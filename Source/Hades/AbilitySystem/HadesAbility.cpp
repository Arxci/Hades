#include "HadesAbility.h"

#include "Hades/Character/HadesCharacterBase.h"
#include "Hades/Player/HadesPlayerController.h"

UHadesAbility::UHadesAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UHadesAbility::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);

	if (bEndOnInputRelease && IsInputToggled(ActorInfo))
	{
		constexpr bool bReplicateEndAbility = true;
		constexpr bool bWasCancelled = false;
		EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	}
}

void UHadesAbility::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);

	if (bEndOnInputRelease && !IsInputToggled(ActorInfo))
	{
		constexpr bool bReplicateEndAbility = true;
		constexpr bool bWasCancelled = false;
		EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	}
}

AHadesCharacterBase* UHadesAbility::GetHadesCharacterFromActorInfo() const
{
	return Cast<AHadesCharacterBase>(GetAvatarActorFromActorInfo());
}

bool UHadesAbility::IsInputToggled(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const AHadesPlayerController* PC = ActorInfo ? Cast<AHadesPlayerController>(ActorInfo->PlayerController.Get()) : nullptr;
	return PC && PC->IsInputToggleEnabled(InputTag);
}
