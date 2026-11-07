#pragma once

#include "CoreMinimal.h"

namespace InventoryAssetManagerSetup
{
	/** Adds the InventoryItem type to the Asset Manager settings if it's missing, and resaves items that still carry an old ID. */
	void RegisterItems();

	/** Offers to register items, unless they already are. Called when an item is created, which is when it starts to matter. */
	void PromptIfUnregistered();
}
