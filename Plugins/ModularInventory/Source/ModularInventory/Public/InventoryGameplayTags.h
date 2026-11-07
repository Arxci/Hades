#pragma once

#include "NativeGameplayTags.h"

namespace InventoryGameplayTags
{
	/** Tag value read as an item's level. Equipment grants scale with it; items without it are level 1. */
	MODULARINVENTORY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Level);
}
