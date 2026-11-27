#pragma once

#include "CoreMinimal.h"

#include "Input/UIActionBindingHandle.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "ModularUIHoldSubsystem.generated.h"

class FModularUIHoldProcessor;
class UInputAction;
class UWidget;


/**
 * Holds for Modular UI's input actions with a Hold trigger, which Common UI doesn't time for Enhanced Input actions. A hold starts as the
 * action's key goes down and runs the action once the key has been held for the trigger's Hold Time Threshold. Letting go first cancels it,
 * and the progress drops to 0 at once. Meanwhile the binding's hold progress, 0 to 1, goes to whatever shows it, such as an action bar
 * button's On Action Progress, as for Common UI's own holds. It's timed in real time, so holds work while a menu pauses the game.
 * Input actions bound with ModularUI::RegisterInputAction hold through it.
 */
UCLASS()
class MODULARUI_API UModularUIHoldSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** The subsystem of the game instance the widget's player is in, or null. */
	static UModularUIHoldSubsystem* Get(const UWidget& Widget);

	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	/**
	 * Holds the input action, whose key Owner's binding just saw go down: On Complete runs once a key mapped to it has been held for Hold Time
	 * seconds, unless every such key is let go first. Replaces any hold in progress. Does nothing unless one of its keys is down, such as for a
	 * click on the action's button in an action bar.
	 */
	void StartHold(const UWidget& Owner, const UInputAction& InputAction, FUIActionBindingHandle Binding, float HoldTime, const FSimpleDelegate& OnComplete);

private:
	TSharedPtr<FModularUIHoldProcessor> Processor;
};
