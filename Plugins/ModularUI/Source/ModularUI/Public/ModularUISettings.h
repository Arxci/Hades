#pragma once

#include "CoreMinimal.h"

#include "Engine/DeveloperSettings.h"
#include "Engine/Scene.h"

#include "ModularUISettings.generated.h"

class UModularRootLayout;


/** Project-wide Modular UI settings, under Project Settings > Plugins > Modular UI. Saved to DefaultGame.ini. */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Modular UI"))
class MODULARUI_API UModularUISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UModularUISettings();

	/** Whether Paused Look changes anything: at least one of its settings is ticked, or it has a blendable such as a post process material. */
	bool HasPausedLook() const;

	/** Whether any navigation setting differs from Unreal's own, so Modular UI sets the navigation up while the game runs. */
	bool ChangesNavigation() const { return !bTabNavigation || !bArrowKeyNavigation || !bSpaceBarAccepts; }

	// Layers
	/**
	 * The widget each local player's UI is laid out in, made and added to their screen once they have a player controller: a Widget
	 * Blueprint whose parent is Modular Root Layout, holding a Common Activatable Widget Stack for each layer, such as Game, Game Menu, Menu
	 * and Modal, each named in its Layers. None makes no root layout, such as when another plugin, like Common Game, makes one; register its
	 * layers with the Modular UI Subsystem's Register Layer, or bind its On Find Layer, so Modular UI can push onto them.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Layers")
	TSoftClassPtr<UModularRootLayout> RootLayoutClass;

	/** The Z Order the root layout is added to the player's screen with. Above whatever else the game adds, so menus cover it. */
	UPROPERTY(config, EditAnywhere, Category = "Layers")
	int32 RootLayoutZOrder = 1000;

	/** The root layout is collapsed while the player's HUD is hidden, such as with the ShowHUD console command or for a cinematic. */
	UPROPERTY(config, EditAnywhere, Category = "Layers")
	bool bHideRootLayoutWithHUD = true;

	// Navigation. Unreal's defaults are all on; turning any off replaces the navigation setup while the game runs, and puts it back after.
	/** Tab and Shift+Tab move focus between widgets. Off, the Tab key is free for the game, such as to open an inventory. */
	UPROPERTY(config, EditAnywhere, Category = "Navigation")
	bool bTabNavigation = true;

	/** The arrow keys move focus between widgets. Off, only the gamepad, and the keys the game binds, move it. */
	UPROPERTY(config, EditAnywhere, Category = "Navigation")
	bool bArrowKeyNavigation = true;

	/** The space bar presses the focused button, as Enter does. Off, the space bar is free for UI actions bound to it. */
	UPROPERTY(config, EditAnywhere, Category = "Navigation")
	bool bSpaceBarAccepts = true;

	// Pause
	/**
	 * The look the world fades to while a menu pauses it, such as Color Grading > Global > Saturation at 0 to grey it out. Tick only the
	 * settings it should change. The Pause Subsystem shows it with a post process volume of its own, so nothing needs adding to an actor.
	 * It wins over the levels' post process volumes with a lower Priority, but a camera's own post process settings still win for any
	 * setting they tick.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Pause", meta = (ShowPostProcessCategories))
	FPostProcessSettings PausedLook;

	/** How strongly Paused Look applies once it has faded in. 1 is fully. */
	UPROPERTY(config, EditAnywhere, Category = "Pause", meta = (ClampMin = "0", ClampMax = "1"))
	float PausedLookWeight = 1.0f;

	/** The Priority of the volume showing Paused Look. Keep it above your levels' post process volumes', so the paused look wins over theirs. */
	UPROPERTY(config, EditAnywhere, Category = "Pause")
	float PausedLookPriority = 1000.0f;

	/** How long Paused Look takes to fade in as the world pauses. It runs in real time, as the world is paused meanwhile. */
	UPROPERTY(config, EditAnywhere, Category = "Pause", meta = (ClampMin = "0", Units = "s"))
	float PausedLookFadeInTime = 0.15f;

	/** How long Paused Look takes to fade out as the world starts again. */
	UPROPERTY(config, EditAnywhere, Category = "Pause", meta = (ClampMin = "0", Units = "s"))
	float PausedLookFadeOutTime = 0.2f;
};
