#pragma once

#include "CoreMinimal.h"

#include "Input/CommonUIInputTypes.h"
#include "Input/UIActionBindingHandle.h"
#include "InputCoreTypes.h"

class UInputAction;
class UWidget;
enum class ECommonInputType : uint8;


/**
 * Modular UI binds every key a widget takes as an Enhanced Input action, so players can rebind all of them. Each field that takes a key is
 * one Input Action, and the keys themselves are set only in an Input Mapping Context, never on the widgets.
 *
 * Setup, once per project:
 * - Set Game Viewport Client Class to CommonGameViewportClient, or a class based on it, in Project Settings > Engine > General Settings >
 *   Default Classes. Common UI only routes keys to the widgets that bound them through it.
 * - Turn on Enable Enhanced Input Support in Project Settings > Game > Common Input Settings. While it's off, Common UI ignores the actions.
 * - Map the input actions in an Input Mapping Context that's added for the player the whole time, above the gameplay contexts. Common UI only
 *   sees keys mapped in an added context, and only routes them to the widget that bound them.
 * - Untick Consume Lower Priority Enhanced Input Mappings on each, so a key gameplay also uses still reaches gameplay.
 * - Give each that players can rebind Player Mappable Key Settings, and register the context with Enhanced Input's user settings.
 * - Give each that shows in action bars a Description: action bars show it as the action's name.
 * - Leave Is Generic Input Action ticked in any Common Input Metadata given to one: unticked, Common UI sends its key to Enhanced Input
 *   instead, and the widget's action never runs.
 * - A widget's actions only run while the input mode is Menu or Game And Menu: Common UI ignores them while it's Game.
 * - A Hold trigger on one makes it a hold: its key has to be held for the trigger's Hold Time Threshold before it runs. Put it on the input
 *   action itself: a trigger on a mapping in the context isn't read. Hold And Release is a trigger of its own, and isn't a hold here.
 * ModularUI.Input.Status, in the console, lists each local player's actions that take input now, with their keys.
 */
namespace ModularUI
{
	/** Whether Common UI binds Enhanced Input actions: Enable Enhanced Input Support, in Project Settings > Game > Common Input Settings. */
	MODULARUI_API bool IsEnhancedInputSupportEnabled();

	/** How long the input action's key has to be held before it runs: the Hold Time Threshold of a Hold trigger on it, or 0 without one. */
	MODULARUI_API float GetHoldTime(const UInputAction* InputAction);

	/**
	 * Whether a key belongs to the input method: a gamepad's own keys for Gamepad, the keyboard's and the mouse's for Mouse And Keyboard, and
	 * none for Touch, which Common UI asks for no keys.
	 */
	MODULARUI_API bool IsKeyForInputType(const FKey& Key, ECommonInputType InputType);

	/**
	 * Binding arguments for the input action, named in action bars by its Description. They run On Execute Action as soon as the key is
	 * pressed, even for an action with a Hold trigger: bind with Register Input Action for that to hold.
	 */
	MODULARUI_API FBindUIActionArgs MakeBindArgs(const UInputAction* InputAction, bool bShouldDisplayInActionBar, const FSimpleDelegate& OnExecuteAction);

	/**
	 * Binds the input action through Register, a widget's own Register UI Action Binding, such as
	 * [this](const FBindUIActionArgs& Args) { return RegisterUIActionBinding(Args); }. With a Hold trigger on the action, On Execute Action
	 * runs once its key has been held for the trigger's Hold Time Threshold: letting go first cancels, and meanwhile the binding's hold
	 * progress goes to whatever shows it, such as an action bar button's On Action Progress. Owner is the widget binding it. Nothing is bound
	 * for a null action. Logs a warning when Common UI doesn't bind it, such as for an input action bound twice on one widget, when Common UI's
	 * Enhanced Input support is off, and when an action shown in action bars has no Description.
	 */
	MODULARUI_API FUIActionBindingHandle RegisterInputAction(TFunctionRef<FUIActionBindingHandle(const FBindUIActionArgs&)> Register, const UWidget& Owner,
		const UInputAction* InputAction, bool bShouldDisplayInActionBar, const FSimpleDelegate& OnExecuteAction);

	/**
	 * Common UI's Accept as an Enhanced Input action: the Enhanced Input Click Action of the project's Common UI Input Data, while Enhanced
	 * Input support is on. Null when Accept is the Default Click Action row instead.
	 */
	MODULARUI_API const UInputAction* GetAcceptInputAction();

	/** Whether the project has an Accept to bind: an Enhanced Input Click Action, or a Default Click Action row, in its Common UI Input Data. */
	MODULARUI_API bool HasAcceptAction();

	/**
	 * Binding arguments for Common UI's Accept: its Enhanced Input Click Action, or else its Default Click Action row, as the project's Common
	 * UI Input Data gives it. Accept stays Common UI's, so its keys are set there, once.
	 */
	MODULARUI_API FBindUIActionArgs MakeAcceptBindArgs(bool bShouldDisplayInActionBar, const FSimpleDelegate& OnExecuteAction);
}
