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

void UInventoryItemCosmetics::GatherAllAssets(TArray<FSoftObjectPath>& OutAssets) const
{
	GatherEquipAssets(OutAssets);

	TArray<FSoftObjectPath> Others = { Icon.ToSoftObjectPath(), WorldMesh.ToSoftObjectPath() };
	for (const TPair<FGameplayTag, TSoftObjectPtr<UObject>>& Asset : Assets)
	{
		Others.Add(Asset.Value.ToSoftObjectPath());
	}
	for (const FSoftObjectPath& Path : Others)
	{
		if (!Path.IsNull())
		{
			OutAssets.Add(Path);
		}
	}
}
