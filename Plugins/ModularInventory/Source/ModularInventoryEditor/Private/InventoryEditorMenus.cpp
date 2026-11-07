#include "InventoryEditorMenus.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserMenuContexts.h"
#include "Framework/Commands/UIAction.h"
#include "ToolMenus.h"

#include "Assets/InventoryAssetManagerSetup.h"
#include "Assets/InventoryItemCreation.h"
#include "Data/InventoryItemDefinition.h"
#include "Icons/InventoryIconGenerator.h"
#include "Icons/SInventoryIconStudio.h"
#include "InventoryEditorSettings.h"

#define LOCTEXT_NAMESPACE "InventoryEditorMenus"

namespace
{
	TArray<UInventoryItemDefinition*> LoadItems(const TArray<FAssetData>& Assets)
	{
		TArray<UInventoryItemDefinition*> Items;
		for (const FAssetData& Asset : Assets)
		{
			if (UInventoryItemDefinition* Item = Cast<UInventoryItemDefinition>(Asset.GetAsset()))
			{
				Items.Add(Item);
			}
		}
		return Items;
	}

	void GenerateAllIcons()
	{
		TArray<FAssetData> Assets;
		IAssetRegistry::GetChecked().GetAssetsByClass(UInventoryItemDefinition::StaticClass()->GetClassPathName(), Assets, true);
		InventoryIcons::GenerateIcons(LoadItems(Assets));
	}

	void AddAssetActions(FToolMenuSection& Section)
	{
		const UContentBrowserAssetContextMenuContext* Context = Section.FindContext<UContentBrowserAssetContextMenuContext>();
		if (!Context)
		{
			return;
		}

		TArray<FAssetData> Items;
		TArray<FAssetData> Sources;
		for (const FAssetData& Asset : Context->SelectedAssets)
		{
			const UClass* AssetClass = Asset.GetClass();
			if (AssetClass && AssetClass->IsChildOf(UInventoryItemDefinition::StaticClass()))
			{
				Items.Add(Asset);
			}
			else if (InventoryItemCreation::CanCreateFrom(Asset))
			{
				Sources.Add(Asset);
			}
		}

		if (!Sources.IsEmpty())
		{
			Section.AddMenuEntry("CreateInventoryItems",
				LOCTEXT("CreateItems", "Create Inventory Item"),
				LOCTEXT("CreateItemsTooltip", "Makes an item definition and cosmetics next to each selected static mesh or texture: meshes become the world mesh, textures the icon. Select one mesh and one texture to make a single item with both."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([Sources]() { InventoryItemCreation::CreateItemsFrom(Sources); })));
		}

		if (!Items.IsEmpty())
		{
			Section.AddMenuEntry("GenerateItemIcons",
				LOCTEXT("GenerateIcons", "Generate Item Icons"),
				LOCTEXT("GenerateIconsTooltip", "Renders an icon from each item's World Mesh. Icons made this way are rendered again with their own framing; hand-made icons are left alone."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([Items]() { InventoryIcons::GenerateIcons(LoadItems(Items)); })));
		}

		if (Items.Num() == 1)
		{
			Section.AddMenuEntry("EditItemIcon",
				LOCTEXT("EditIcon", "Edit Item Icon..."),
				LOCTEXT("EditIconTooltip", "Opens the Icon Studio to frame and save this item's icon."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([Item = Items[0]]()
				{
					if (UInventoryItemDefinition* Definition = Cast<UInventoryItemDefinition>(Item.GetAsset()))
					{
						SInventoryIconStudio::Open(*Definition);
					}
				})));
		}
	}
}

void InventoryEditorMenus::Register()
{
	UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	FToolMenuSection& ToolsSection = ToolsMenu->AddSection("ModularInventory", LOCTEXT("ToolsSection", "Modular Inventory"));
	ToolsSection.AddMenuEntry("RegisterItemsWithAssetManager",
		LOCTEXT("RegisterItems", "Register Items With Asset Manager"),
		LOCTEXT("RegisterItemsTooltip", "Adds the InventoryItem type to the Asset Manager settings, so items can be found by ID and are always cooked. Also resaves items that still carry an old ID."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&InventoryAssetManagerSetup::RegisterItems)));
	ToolsSection.AddMenuEntry("GenerateAllItemIcons",
		LOCTEXT("GenerateAllIcons", "Generate All Item Icons"),
		LOCTEXT("GenerateAllIconsTooltip", "Generate Item Icons for every item in the project: generated icons are rendered again with the current settings, missing ones are created, hand-made ones are left alone."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&GenerateAllIcons)));
	ToolsSection.AddMenuEntry("InventoryEditorSettings",
		LOCTEXT("Settings", "Settings"),
		LOCTEXT("SettingsTooltip", "Naming conventions and icon lighting, in Project Settings > Plugins > Modular Inventory."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&UInventoryEditorSettings::Open)));

	UToolMenu* AssetMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu");
	AssetMenu->FindOrAddSection("GetAssetActions").AddDynamicEntry("ModularInventory", FNewToolMenuSectionDelegate::CreateStatic(&AddAssetActions));
}

#undef LOCTEXT_NAMESPACE
