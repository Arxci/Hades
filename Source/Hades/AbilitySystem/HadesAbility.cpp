#include "HadesAbility.h"

#include "GameFramework/PlayerController.h"

#include "Hades/Character/HadesCharacterBase.h"
#include "Hades/Player/HadesPlayerSettings.h"

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
	const APlayerController* PC = ActorInfo ? ActorInfo->PlayerController.Get() : nullptr;
	const UHadesPlayerSettings* Settings = PC ? UHadesPlayerSettings::Get(PC->GetLocalPlayer()) : nullptr;
	return Settings && Settings->IsInputToggleEnabled(InputTag);
}
