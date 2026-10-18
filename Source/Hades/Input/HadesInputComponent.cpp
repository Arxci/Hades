#include "HadesInputComponent.h"

#include "InputAction.h"

void UHadesInputComponent::HandleInputTriggered(FGameplayTag InputTag)
{
	bool bAlreadyHeld = false;
	HeldInputTags.Add(InputTag, &bAlreadyHeld);
	if (!bAlreadyHeld)
	{
		OnInputPressed.ExecuteIfBound(InputTag);
	}
}

void UHadesInputComponent::HandleInputEnded(FGameplayTag InputTag)
{
	if (HeldInputTags.Remove(InputTag) > 0)
	{
		OnInputReleased.ExecuteIfBound(InputTag);
	}
}

void UHadesInputComponent::ReleaseHeldInputs()
{
	const TArray<FGameplayTag> HeldTags = HeldInputTags.Array();
	HeldInputTags.Reset();

	for (const FGameplayTag& InputTag : HeldTags)
	{
		OnInputReleased.ExecuteIfBound(InputTag);
	}
}

void UHadesInputComponent::BindInputValues(const UHadesInputConfig* InputConfig)
{
	check(InputConfig);

	for (const FHadesInputAction& Action : InputConfig->InputActions)
	{
		if (Action.InputAction && Action.InputTag.IsValid() && !ValueBoundActions.Contains(Action.InputTag))
		{
			BindActionValue(Action.InputAction);
			ValueBoundActions.Add(Action.InputTag, Action.InputAction);
		}
	}
}

FInputActionValue UHadesInputComponent::GetInputValue(const FGameplayTag& InputTag) const
{
	const UInputAction* InputAction = ValueBoundActions.FindRef(InputTag);
	return InputAction ? GetBoundActionValue(InputAction) : FInputActionValue();
}
