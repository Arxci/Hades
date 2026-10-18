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
	void BindNativeAction(const UHadesInputConfig* InputConfig, const FGameplayTag& InputTag, ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func);
	
	template<class UserClass>
	void BindAbilityActions(const UHadesInputConfig* InputConfig, UserClass* Object, void (UserClass::*PressedFunc)(FGameplayTag), void (UserClass::*ReleasedFunc)(FGameplayTag));
	
	void ReleaseHeldAbilityInputs();
	
	void BindInputValues(const UHadesInputConfig* InputConfig);

	/** The live value of the action mapped to InputTag (zero when released). Zero if BindInputValues never saw the tag. */
	FInputActionValue GetInputValue(const FGameplayTag& InputTag) const;

private:
	void HandleAbilityInputTriggered(FGameplayTag InputTag);
	void HandleAbilityInputEnded(FGameplayTag InputTag);

	FHadesInputTagSignature AbilityInputPressed;
	FHadesInputTagSignature AbilityInputReleased;
	
	TSet<FGameplayTag> HeldAbilityInputTags;
	
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