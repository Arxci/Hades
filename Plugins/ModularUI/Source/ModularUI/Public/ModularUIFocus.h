#pragma once

#include "CoreMinimal.h"

#include "Containers/Ticker.h"

class ULocalPlayer;
class UObject;
class UWidget;


/** Helpers for moving focus around widgets that come and go, such as pages of a tab switcher. */
namespace ModularUI
{
	/** Whether the widget is drawn now, where it can take focus or be hovered: shown, and in the window's widget tree. */
	MODULARUI_API bool IsWidgetDrawn(const UWidget& Widget);

	/**
	 * Calls Callback with Target once it's drawn, checked each frame for up to Timeout seconds, such as to focus a cell on a page a tab
	 * switcher is still bringing in. It runs from the core ticker, so it works while the game is paused, and stops if Owner goes away.
	 * Handle keeps the wait: a new wait in it replaces the last.
	 */
	MODULARUI_API void CallWhenDrawn(FTSTicker::FDelegateHandle& Handle, UObject& Owner, UWidget* Target, TFunction<void(UWidget&)> Callback, float Timeout = 1.0f);

	/** Stops the wait in Handle, if any. */
	MODULARUI_API void CancelCallWhenDrawn(FTSTicker::FDelegateHandle& Handle);

	/**
	 * The user widget inside Root that has the local player's focus, the deepest one on the way to it, such as a button or a grid cell.
	 * Null when the focus isn't inside Root.
	 */
	MODULARUI_API UWidget* FindFocusedWidgetIn(const UWidget& Root, const ULocalPlayer* LocalPlayer);

	/** The widget named Name: Root itself, or one inside it, in its own designer when it's a user widget. Null when there's none. */
	MODULARUI_API UWidget* FindWidgetByName(UWidget& Root, FName Name);
}
