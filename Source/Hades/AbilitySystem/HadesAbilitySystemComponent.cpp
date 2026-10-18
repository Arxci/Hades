#include "HadesAbilitySystemComponent.h"

#include "Hades/AbilitySystem/HadesAbility.h"

namespace
{
	bool HasInputTag(const FGameplayAbilitySpec& Spec, const FGameplayTag& InputTag)
	{
		const UHadesAbility* Ability = Cast<UHadesAbility>(Spec.Ability);
		return Ability && Ability->GetInputTag() == InputTag;
	}

	// Wait Input Press/Release listen for the input event under the key the ability was activated with.
	FPredictionKey GetActivationPredictionKey(const FGameplayAbilitySpec& Spec)
	{
		const UGameplayAbility* Instance = Spec.GetPrimaryInstance();
		return Instance ? Instance->GetCurrentActivationInfo().GetActivationPredictionKey() : FPredictionKey();
	}
}

void UHadesAbilitySystemComponent::AbilityInputTagPressed(FGameplayTag InputTag)
{
	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!HasInputTag(Spec, InputTag))
		{
			continue;
		}

		Spec.InputPressed = true;
		if (Spec.IsActive())
		{
			AbilitySpecInputPressed(Spec);
			InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle, GetActivationPredictionKey(Spec));
		}
		else
		{
			TryActivateAbility(Spec.Handle);
		}
	}
}

void UHadesAbilitySystemComponent::AbilityInputTagReleased(FGameplayTag InputTag)
{
	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!HasInputTag(Spec, InputTag))
		{
			continue;
		}

		Spec.InputPressed = false;
		if (Spec.IsActive())
		{
			AbilitySpecInputReleased(Spec);
			InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle, GetActivationPredictionKey(Spec));
		}
	}
}
