#pragma once

#include "CoreMinimal.h"

#include "EnhancedInputComponent.h"
#include "InputActionValue.h"

#include "Hades/Input/HadesInputConfig.h"

#include "HadesInputComponent.generated.h"


DECLARE_DELEGATE_OneParam(FHadesInputTagSignature, FGameplayTag);


/**
 * The player pawn's Enhanced Input component. Binds through a UHadesInputConfig by input tag
 * instead of per-action properties, and turns each ability action into exactly one press and
 * one release, whatever triggers the action uses.
 */
UCLASS()
class HADES_API UHadesInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()

public:
	/** Bind Func to the native action mapped to InputTag. Logs and skips if the config doesn't map it. */
	template<class UserClass, typename FuncType>
	void BindNativeAction(const UHadesInputConfig* InputConfig, const FGameplayTag& InputTag, ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func);

	/**
	 * Bind every ability action in InputConfig. PressedFunc runs once when an action first triggers
	 * (so a Hold or Tap trigger on the action is respected); ReleasedFunc runs once when a pressed
	 * action completes or is canceled. Both receive the action's input tag.
	 */
	template<class UserClass>
	void BindAbilityActions(const UHadesInputConfig* InputConfig, UserClass* Object, void (UserClass::*PressedFunc)(FGameplayTag), void (UserClass::*ReleasedFunc)(FGameplayTag));

	/** Run ReleasedFunc for every ability input still held. Call before input is torn down mid-hold. */
	void ReleaseHeldAbilityInputs();

	/** Track the live value of every action in InputConfig, native and ability alike, so GetInputValue can read it by tag. */
	void BindInputValues(const UHadesInputConfig* InputConfig);

	/** The live value of the action mapped to InputTag (zero when released). Zero if BindInputValues never saw the tag. */
	FInputActionValue GetInputValue(const FGameplayTag& InputTag) const;

private:
	void HandleAbilityInputTriggered(FGameplayTag InputTag);
	void HandleAbilityInputEnded(FGameplayTag InputTag);

	FHadesInputTagSignature AbilityInputPressed;
	FHadesInputTagSignature AbilityInputReleased;

	/** Ability inputs whose press has been sent and whose release hasn't. */
	TSet<FGameplayTag> HeldAbilityInputTags;

	/** Input tag -> the action whose value BindInputValues is tracking for it. */
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<const UInputAction>> ValueBoundActions;
};


template<class UserClass, typename FuncType>
void UHadesInputComponent::BindNativeAction(const UHadesInputConfig* InputConfig, const FGameplayTag& InputTag, ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func)
{
	check(InputConfig);
	if (const UInputAction* InputAction = InputConfig->FindNativeInputActionForTag(InputTag))
	{
		BindAction(InputAction, TriggerEvent, Object, Func);
	}
}

template<class UserClass>
void UHadesInputComponent::BindAbilityActions(const UHadesInputConfig* InputConfig, UserClass* Object, void (UserClass::*PressedFunc)(FGameplayTag), void (UserClass::*ReleasedFunc)(FGameplayTag))
{
	check(InputConfig);

	AbilityInputPressed.BindUObject(Object, PressedFunc);
	AbilityInputReleased.BindUObject(Object, ReleasedFunc);

	for (const FHadesAbilityInputAction& Action : InputConfig->AbilityInputActions)
	{
		if (Action.InputAction && Action.InputTag.IsValid())
		{
			BindAction(Action.InputAction, ETriggerEvent::Triggered, this, &ThisClass::HandleAbilityInputTriggered, Action.InputTag);
			BindAction(Action.InputAction, ETriggerEvent::Completed, this, &ThisClass::HandleAbilityInputEnded, Action.InputTag);
			BindAction(Action.InputAction, ETriggerEvent::Canceled, this, &ThisClass::HandleAbilityInputEnded, Action.InputTag);
		}
	}
}