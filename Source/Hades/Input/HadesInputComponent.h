#pragma once

#include "CoreMinimal.h"

#include "EnhancedInputComponent.h"
#include "InputActionValue.h"

#include "Hades/Input/HadesInputConfig.h"

#include "HadesInputComponent.generated.h"


DECLARE_DELEGATE_OneParam(FHadesInputTagSignature, FGameplayTag);


UCLASS()
class HADES_API UHadesInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()

public:
	template<class UserClass, typename FuncType>
	void BindActionByTag(const UHadesInputConfig* InputConfig, const FGameplayTag& InputTag, ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func);

	template<class UserClass>
	void BindPressedAndReleased(const UHadesInputConfig* InputConfig, UserClass* Object, void (UserClass::*PressedFunc)(FGameplayTag), void (UserClass::*ReleasedFunc)(FGameplayTag));

	void ReleaseHeldInputs();

	void BindInputValues(const UHadesInputConfig* InputConfig);

	/** The live value of the action mapped to InputTag (zero when released). Zero if BindInputValues never saw the tag. */
	FInputActionValue GetInputValue(const FGameplayTag& InputTag) const;

private:
	void HandleInputTriggered(FGameplayTag InputTag);
	void HandleInputEnded(FGameplayTag InputTag);

	FHadesInputTagSignature OnInputPressed;
	FHadesInputTagSignature OnInputReleased;

	TSet<FGameplayTag> HeldInputTags;

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<const UInputAction>> ValueBoundActions;
};


template<class UserClass, typename FuncType>
void UHadesInputComponent::BindActionByTag(const UHadesInputConfig* InputConfig, const FGameplayTag& InputTag, ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func)
{
	check(InputConfig);
	if (const UInputAction* InputAction = InputConfig->FindInputActionForTag(InputTag))
	{
		BindAction(InputAction, TriggerEvent, Object, Func);
	}
}

template<class UserClass>
void UHadesInputComponent::BindPressedAndReleased(const UHadesInputConfig* InputConfig, UserClass* Object, void (UserClass::*PressedFunc)(FGameplayTag), void (UserClass::*ReleasedFunc)(FGameplayTag))
{
	check(InputConfig);

	OnInputPressed.BindUObject(Object, PressedFunc);
	OnInputReleased.BindUObject(Object, ReleasedFunc);

	for (const FHadesInputAction& Action : InputConfig->InputActions)
	{
		if (Action.InputAction && Action.InputTag.IsValid())
		{
			// Pressed/released follow the action entering and leaving the Triggered state. It leaves on Completed,
			// or on Ongoing when a trigger like Pulse drops back between pulses.
			BindAction(Action.InputAction, ETriggerEvent::Triggered, this, &ThisClass::HandleInputTriggered, Action.InputTag);
			BindAction(Action.InputAction, ETriggerEvent::Completed, this, &ThisClass::HandleInputEnded, Action.InputTag);
			BindAction(Action.InputAction, ETriggerEvent::Ongoing, this, &ThisClass::HandleInputEnded, Action.InputTag);
		}
	}
}
