#include "Validation/InventoryItemValidator.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "Misc/DataValidation.h"

#include "Data/InventoryItemDefinition.h"
#include "Icons/InventoryIconGenerator.h"

#define LOCTEXT_NAMESPACE "InventoryItemValidator"

bool UInventoryItemValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const
{
	return InObject && InObject->IsA<UInventoryItemDefinition>();
}

EDataValidationResult UInventoryItemValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context)
{
	const UInventoryItemDefinition* Item = CastChecked<UInventoryItemDefinition>(InAsset);
	bool bFailed = false;

	// Item IDs are just the asset name, so two items with one name share an ID.
	TArray<FAssetData> Items;
	IAssetRegistry::GetChecked().GetAssetsByClass(UInventoryItemDefinition::StaticClass()->GetClassPathName(), Items, true);
	for (const FAssetData& Other : Items)
	{
		if (Other.AssetName == Item->GetFName() && Other.PackageName != Item->GetPackage()->GetFName())
		{
			AssetFails(Item, FText::Format(LOCTEXT("DuplicateName", "{0} is also the name of {1}. Items are found by asset name, so rename one of them."),
				FText::FromName(Item->GetFName()), FText::FromName(Other.PackageName)));
			bFailed = true;
		}
	}

	const UTexture2D* Icon = InventoryIcons::LoadIcon(*Item);
	if (!Icon)
	{
		AssetWarning(Item, LOCTEXT("NoIcon", "The item has no icon, so inventory widgets show nothing for it. Set one on its cosmetics, or use Generate Item Icons."));
	}
	else if (!InventoryIcons::MatchesCells(*Icon, Item->Size))
	{
		AssetWarning(Item, FText::Format(LOCTEXT("StretchedIcon", "The icon {0} is {1} x {2} pixels but the item is {3} x {4} cells, so widgets stretch it. Use Generate Item Icons, or reshape the texture."),
			FText::FromString(Icon->GetName()), Icon->Source.GetSizeX(), Icon->Source.GetSizeY(), Item->Size.X, Item->Size.Y));
	}

	if (bFailed)
	{
		return EDataValidationResult::Invalid;
	}

	AssetPasses(Item);
	return EDataValidationResult::Valid;
}

#undef LOCTEXT_NAMESPACE
