#pragma once

#include "CoreMinimal.h"

class FInventoryIconRenderer;
class UInventoryIconRecipe;
class UInventoryItemDefinition;
class UStaticMesh;
class UTexture2D;
struct FInventoryIconFraming;

namespace InventoryIcons
{
	/**
	 * Renders an icon from each item's World Mesh: icons made here are rendered again with their own framing, missing icons are created,
	 * and hand-made icons are left alone. Skipped items are listed in the Message Log.
	 */
	void GenerateIcons(TConstArrayView<UInventoryItemDefinition*> Items);

	/** Renders the item's icon with Framing and saves it. Makes a new texture when the item has none, or a hand-made one. */
	bool SaveIcon(UInventoryItemDefinition& Item, const FInventoryIconFraming& Framing, FInventoryIconRenderer& Renderer, FText& OutError);

	/** The framing kept with the item's generated icon, or the default framing for its mesh. */
	FInventoryIconFraming GetFraming(const UInventoryItemDefinition& Item);
	FInventoryIconFraming MakeDefaultFraming(const UStaticMesh& Mesh, FIntPoint Cells);

	UStaticMesh* LoadWorldMesh(const UInventoryItemDefinition& Item);
	UTexture2D* LoadIcon(const UInventoryItemDefinition& Item);

	/** Null for textures the generator didn't make. */
	UInventoryIconRecipe* FindRecipe(UTexture2D* Icon);

	/** The item's cells times Pixels Per Cell. */
	FIntPoint GetIconSize(const UInventoryItemDefinition& Item);

	/** Whether the texture has the same shape as the cells, so widgets show it unstretched. */
	bool MatchesCells(const UTexture2D& Icon, FIntPoint Cells);
}
