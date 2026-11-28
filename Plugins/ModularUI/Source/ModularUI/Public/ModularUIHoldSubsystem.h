#pragma once

#include "CoreMinimal.h"

#include "Input/UIActionBindingHandle.h"
#include "Subsystems/LocalPlayerSubsystem.h"

#include "ModularUIHoldSubsystem.generated.h"

class FModularUIHoldProcessor;
class UInputAction;
class UWidget;


/**
 * Holds for Modular UI's input actions with a Hold trigger, one local player's each, which Common UI doesn't time for Enhanced Input actions.
 * A hold starts as the action's key goes down and runs the action once the key has been held for the trigger's Hold Time Threshold. Letting
 * go first cancels it, and so does the action no longer taking input meanwhile, such as under a menu that opened over its widget; either way
 * the progress drops to 0 at once. Meanwhile the binding's hold progress, 0 to 1, goes to whatever shows it, such as an action bar button's
 * On Action Progress, as for Common UI's own holds, and a completed hold reports 1 before it drops to 0, so the button can show it finished.
 * Each press holds once. It's timed in real time, so holds work while a menu pauses the game. Only the player's own keys hold: in
 * split-screen, another player's press of the same key doesn't.
 * Input actions bound with ModularUI::RegisterInputAction hold through it.
 */
UCLASS()
class MODULARUI_API UModularUIHoldSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	/** The subsystem of the widget's owning local player, or null. */
	static UModularUIHoldSubsystem* Get(const UWidget& Widget);

	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	/**
	 * Holds the input action, whose binding just saw one of its keys go down: On Complete runs once a key the player has mapped to it has been
	 * held for Hold Seconds, unless every such key is let go first, or the binding stops taking input. A press holds once, and a hold of
	 * another press in progress is cancelled. Does nothing unless one of its keys is down, such as for a click on the action's button in an
	 * action bar.
	 */
	void StartHold(const UInputAction& InputAction, FUIActionBindingHandle ActionBinding, float HoldSeconds, const FSimpleDelegate& OnComplete);

private:
	TSharedPtr<FModularUIHoldProcessor> Processor;
};
