#pragma once

#include "CoreMinimal.h"

#include "Engine/DeveloperSettings.h"
#include "Engine/Scene.h"

#include "ModularUISettings.generated.h"


/** Project-wide Modular UI settings, under Project Settings > Plugins > Modular UI. Saved to DefaultGame.ini. */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Modular UI"))
class MODULARUI_API UModularUISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UModularUISettings();

	/** Whether Paused Look changes anything: at least one of its settings is ticked, or it has a blendable such as a post process material. */
	bool HasPausedLook() const;

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
