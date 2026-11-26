#pragma once

#include "CoreMinimal.h"

#include "Components/Widget.h"
#include "Containers/Ticker.h"

class ULocalPlayer;
class UModularActivatableWidget;
class UObject;


/** Helpers for moving focus around widgets that come and go, such as pages of a tab switcher, and for finding the widgets around a widget. */
namespace ModularUI
{
	/** How long to wait for a page a switcher is bringing in to be drawn, however long the switcher's own transition is. */
	inline constexpr float PageDrawnTimeout = 5.0f;

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

	/**
	 * The widget Widget is drawn in: the panel holding it, or at the top of a user widget's designer, that user widget. Null at the top of
	 * everything.
	 */
	MODULARUI_API UWidget* GetEnclosingWidget(const UWidget& Widget);

	/** The nearest widget of type T that Widget is drawn in, such as the screen a button is on. Widget itself doesn't count. */
	template <typename T>
	T* FindEnclosing(const UWidget& Widget)
	{
		for (UWidget* Current = GetEnclosingWidget(Widget); Current; Current = GetEnclosingWidget(*Current))
		{
			if (T* Found = Cast<T>(Current))
			{
				return Found;
			}
		}
		return nullptr;
	}

	/**
	 * The Modular Activatable Widget whose Description Text describes Widget: the nearest one Widget is drawn in with a description line,
	 * or else the nearest one.
	 */
	MODULARUI_API UModularActivatableWidget* FindDescriptionOwner(const UWidget& Widget);
}
