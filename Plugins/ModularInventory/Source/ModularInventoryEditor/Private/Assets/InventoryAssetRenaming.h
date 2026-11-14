#pragma once

#include "CoreMinimal.h"

/**
 * Keeps an item's assets together: renaming or moving an item definition renames or moves its cosmetics and icon with it, when they're
 * named after it and sit where the tools put them. Rename Item Assets in the settings turns it off.
 */
namespace InventoryAssetRenaming
{
	void Register();
	void Unregister();
}
