#pragma once

#include "CoreMinimal.h"

class UInventoryItemCosmetics;
class UInventoryItemDefinition;
struct FAssetData;

namespace InventoryItemCreation
{
	/** Static meshes and textures can become items. */
	bool CanCreateFrom(const FAssetData& Asset);

	/**
	 * Makes an item definition and its cosmetics next to each mesh and texture: meshes become the world mesh, textures the icon.
	 * One mesh and one texture together make a single item with both. The new items are selected in the Content Browser, unsaved.
	 */
	void CreateItemsFrom(const TArray<FAssetData>& Sources);

	/** Makes empty cosmetics next to the item, named after it. Doesn't assign them. */
	UInventoryItemCosmetics* CreateCosmetics(const UInventoryItemDefinition& Item);
}
