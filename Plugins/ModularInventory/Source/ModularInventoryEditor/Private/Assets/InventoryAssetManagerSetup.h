#pragma once

#include "CoreMinimal.h"

/**
 * Item definitions need an InventoryItem entry in the Asset Manager settings to have IDs: Inventory.Give finds items by them, saves can
 * store them, and the entry's Always Cook rule packages items that nothing references. The editor adds the entry by itself, in
 * Config/DefaultGame.ini, as soon as the project has an item definition, and resaves any item still carrying the ID of an older
 * version of the plugin. There's nothing to run by hand; an entry the project already has is left as it is.
 */
namespace InventoryAssetManagerSetup
{
	/** Sets up items once the Asset Registry has found every asset, and again whenever an item definition is added while they aren't. */
	void Start();
	void Stop();

	/** Sets up items on the next frame, such as for an item definition being made now. Does nothing for a project that's set up. */
	void SetUpSoon();
}
