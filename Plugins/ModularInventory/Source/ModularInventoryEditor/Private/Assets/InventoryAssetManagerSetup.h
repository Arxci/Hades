#pragma once

#include "CoreMinimal.h"

/**
 * Item definitions need an entry in the Asset Manager settings to have IDs. The editor checks once the assets are found, and again when
 * an item is created, and offers to add it, so there's nothing to run by hand.
 */
namespace InventoryAssetManagerSetup
{
	/** Adds the InventoryItem type to the Asset Manager settings if it's missing, and resaves items that still carry an old ID. */
	void RegisterItems();

	/** Offers to register items, or to resave items with an old ID, unless neither is needed. */
	void PromptIfNeeded();

	/** Calls Prompt If Needed once the Asset Registry has found every asset, if the project has items. */
	void CheckOnceLoaded();
	void StopChecking();
}
