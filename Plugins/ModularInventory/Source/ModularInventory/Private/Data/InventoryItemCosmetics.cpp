#include "Data/InventoryItemCosmetics.h"

#include "Animation/AnimInstance.h"

#include "Equipment/InventoryEquipmentPiece.h"

void UInventoryItemCosmetics::GatherEquipAssets(TArray<FSoftObjectPath>& OutAssets) const
{
	for (const UInventoryEquipmentPiece* Piece : Pieces)
	{
		if (Piece)
		{
			Piece->GatherAssets(OutAssets);
		}
	}

	for (const FInventoryAnimLayer& AnimLayer : AnimLayers)
	{
		if (!AnimLayer.LayerClass.IsNull())
		{
			OutAssets.Add(AnimLayer.LayerClass.ToSoftObjectPath());
		}
	}
}
