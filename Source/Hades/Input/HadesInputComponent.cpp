#include "HadesInputComponent.h"

#include "InputAction.h"

void UHadesInputComponent::HandleAbilityInputTriggered(FGameplayTag InputTag)
{
	bool bAlreadyHeld = false;
	HeldAbilityInputTags.Add(InputTag, &bAlreadyHeld);
	if (!bAlreadyHeld)
	{
		AbilityInputPressed.ExecuteIfBound(InputTag);
	}
}

void UHadesInputComponent::HandleAbilityInputEnded(FGameplayTag InputTag)
{
	if (HeldAbilityInputTags.Remove(InputTag) > 0)
	{
		AbilityInputReleased.ExecuteIfBound(InputTag);
	}
}

void UHadesInputComponent::ReleaseHeldAbilityInputs()
{
	const TArray<FGameplayTag> HeldTags = HeldAbilityInputTags.Array();
	HeldAbilityInputTags.Reset();

	for (const FGameplayTag& InputTag : HeldTags)
	{
		AbilityInputReleased.ExecuteIfBound(InputTag);
	}
}

void UHadesInputComponent::BindInputValues(const UHadesInputConfig* InputConfig)
{
	check(InputConfig);

	auto BindValue = [this](const FGameplayTag& InputTag, const UInputAction* InputAction)
	{
		if (InputAction && InputTag.IsValid() && !ValueBoundActions.Contains(InputTag))
		{
			BindActionValue(InputAction);
			ValueBoundActions.Add(InputTag, InputAction);
		}
	};

	for (const FHadesInputAction& Action : InputConfig->NativeInputActions)
	{
		BindValue(Action.InputTag, Action.InputAction);
	}
	for (const FHadesAbilityInputAction& Action : InputConfig->AbilityInputActions)
	{
		BindValue(Action.InputTag, Action.InputAction);
	}
}

FInputActionValue UHadesInputComponent::GetInputValue(const FGameplayTag& InputTag) const
{
	const UInputAction* InputAction = ValueBoundActions.FindRef(InputTag);
	return InputAction ? GetBoundActionValue(InputAction) : FInputActionValue();
}