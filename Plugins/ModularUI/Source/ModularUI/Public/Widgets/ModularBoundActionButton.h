#pragma once

#include "CoreMinimal.h"

#include "Input/CommonBoundActionButton.h"
#include "Input/UIActionBindingHandle.h"

#include "ModularBoundActionButton.generated.h"


/**
 * A Common Bound Action Button that knows whether its action is a hold: an input action with a Hold trigger, run once its key has been held
 * for the trigger's Hold Time Threshold. Use it as an action bar's Action Button Class, such as a Modular Bound Action Bar's.
 * On Represented Action Changed says whether the action shown is a hold, such as to show an empty ring around its key before it's held, so
 * the player knows to hold it, which a Common Bound Action Button can't tell. On Action Progress fills the ring as the key is held, 0 to 1:
 * it reaches 1 as the hold completes, and drops to 0 at once, as it does when the key is let go early.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class MODULARUI_API UModularBoundActionButton : public UCommonBoundActionButton
{
	GENERATED_BODY()

public:
	//~ Begin ICommonBoundActionButtonInterface Interface
	virtual void SetRepresentedAction(FUIActionBindingHandle InBindingHandle) override;
	//~ End ICommonBoundActionButtonInterface Interface

	/** Whether the action shown is a hold: its key has to be held for Get Action Hold Time before it runs. */
	UFUNCTION(BlueprintPure, Category = "Bound Action")
	bool IsHoldAction() const;

	/**
	 * How long the action's key has to be held before it runs, in seconds: its Hold trigger's Hold Time Threshold, or 0 for an action that
	 * runs as its key goes down.
	 */
	UFUNCTION(BlueprintPure, Category = "Bound Action")
	float GetActionHoldTime() const;

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	/**
	 * The button shows another action, such as when an action bar gives it one. Show here whether it's a hold, such as an empty ring around
	 * its key, and hide that for an action that isn't.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Bound Action")
	void OnRepresentedActionChanged(bool bHoldAction, float ActionHoldTime);

private:
	/** Stops listening to the hold progress of the binding shown. */
	void StopListening();

	/** The binding shown, whose hold progress it listens to. */
	FUIActionBindingHandle ShownBinding;
};
