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
		HeldInputSpecHandles.AddUnique(Spec.Handle);

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

	// Release them all before ending any, so ending one can't restart another on the same input.
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (HasInputTag(Spec, InputTag))
		{
			Spec.InputPressed = false;
			HeldInputSpecHandles.Remove(Spec.Handle);
		}
	}

	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (HasInputTag(Spec, InputTag) && Spec.IsActive())
		{
			AbilitySpecInputReleased(Spec);
			InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle, GetActivationPredictionKey(Spec));
		}
	}
}

void UHadesAbilitySystemComponent::ClearAbilityInput()
{
	HeldInputSpecHandles.Reset();

	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		Spec.InputPressed = false;

		const UHadesAbility* Ability = Cast<UHadesAbility>(Spec.Ability);
		if (Ability && Ability->EndsOnInputRelease() && Spec.IsActive())
		{
			CancelAbilityHandle(Spec.Handle);
		}
	}
}

void UHadesAbilitySystemComponent::NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, bool bWasCancelled)
{
	Super::NotifyAbilityEnded(Handle, Ability, bWasCancelled);

	// Only when an ability finishes: restarting after a cancel would fight whatever cancelled it.
	// One that finished while its input is still held stays finished until the next press.
	if (!bWasCancelled)
	{
		HeldInputSpecHandles.Remove(Handle);
		TryActivateHeldAbilities();
	}
}

void UHadesAbilitySystemComponent::TryActivateHeldAbilities()
{
	if (bActivatingHeldAbilities || HeldInputSpecHandles.IsEmpty())
	{
		return;
	}
	TGuardValue<bool> ActivatingGuard(bActivatingHeldAbilities, true);

	// Oldest press first: each restart cancels what it cancelled the first time, so the latest press still wins.
	const TArray<FGameplayAbilitySpecHandle> HeldHandles = HeldInputSpecHandles;
	for (const FGameplayAbilitySpecHandle& Handle : HeldHandles)
	{
		const FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
		const UHadesAbility* Ability = Spec ? Cast<UHadesAbility>(Spec->Ability) : nullptr;
		if (Ability && !Spec->IsActive() && Ability->EndsOnInputRelease() && !Ability->IsInputToggled(AbilityActorInfo.Get()))
		{
			TryActivateAbility(Handle);
		}
	}
}
