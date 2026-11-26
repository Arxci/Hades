#pragma once

#include "CoreMinimal.h"

#include "Widgets/InventoryShortcutMenuWidget.h"

#include "HadesQuickSlotMenu.generated.h"


/**
 * The quick slots, opened by the item menu's Assign Shortcut action to put an item on one: the plugin's shortcut menu, coming and going as
 * the Hades item menu does. Beside the item by default; for a modal picker, set Placement to As Designed and lay Menu Panel out in the
 * middle of a dim backdrop.
 */
UCLASS(Abstract, Blueprintable)
class HADES_API UHadesQuickSlotMenu : public UInventoryShortcutMenuWidget
{
	GENERATED_BODY()

public:
	UHadesQuickSlotMenu(const FObjectInitializer& ObjectInitializer);
};
