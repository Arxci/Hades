#pragma once

#include "CoreMinimal.h"

#include "Widgets/InventoryItemMenuWidget.h"

#include "HadesItemMenu.generated.h"


/**
 * The Hades inventory screen's item menu. Its Actions start as every Hades item action, each on the button placed for it under its Button
 * Name, such as UseButton, or else on one made in Action Panel. Edit, reorder or remove them in Class Defaults.
 * It comes and goes as the RE2 remake's item menu does: see Make Transition. An action picked runs as the menu starts to close, and a
 * prompt starts on its first button, as the menu starts on its first entry.
 */
UCLASS(Abstract, Blueprintable)
class HADES_API UHadesItemMenu : public UInventoryItemMenuWidget
{
	GENERATED_BODY()

public:
	UHadesItemMenu(const FObjectInitializer& ObjectInitializer);

	/**
	 * How Hades' item menus come in and go, measured frame by frame from the RE2 remake's: over five frames at 60 fps, the menu fades in
	 * as it slides 20 pixels out from the item at an even pace. It closes the same way backwards.
	 */
	static FInventoryMenuTransition MakeTransition();

#if WITH_EDITOR
	virtual void ValidateCompiledDefaults(class IWidgetCompilerLog& CompileLog) const override;
#endif
};
