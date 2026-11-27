#pragma once

#include "CoreMinimal.h"

#include "Widgets/InventoryItemMenuWidget.h"

#include "HadesItemMenu.generated.h"


/**
 * The Hades inventory screen's item menu. Its Actions start as every item action the game offers, each on the button placed for it under
 * its Button Name, such as UseButton, or else on one made in Action Panel. Edit, reorder or remove them in Class Defaults.
 * As in the console games it follows, the right mouse button goes back, as Escape does: out of a prompt to the actions, then out of the
 * menu. A click off the menu closes it, and an action's question is asked in place of the actions.
 * It slides out from behind the item, never over it: lay its background and buttons out in Menu Content, inside Menu Panel, which clips it.
 */
UCLASS(Abstract, Blueprintable)
class HADES_API UHadesItemMenu : public UInventoryItemMenuWidget
{
	GENERATED_BODY()

public:
	UHadesItemMenu(const FObjectInitializer& ObjectInitializer);

	/** The open transition every Hades item menu shares, the quick slot menu's included. */
	static FModularUITransition MakeTransition();

#if WITH_EDITOR
	virtual void ValidateCompiledDefaults(class IWidgetCompilerLog& CompileLog) const override;
#endif
};
