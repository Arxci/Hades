#include "InventoryEditorMenus.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserMenuContexts.h"
#include "Editor.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Logging/MessageLog.h"
#include "ToolMenus.h"

#include "Assets/InventoryItemCreation.h"
#include "Data/InventoryItemDefinition.h"
#include "Documentation/SInventoryDocumentation.h"
#include "Icons/InventoryIconGenerator.h"
#include "Icons/SInventoryIconStudio.h"
#include "InventoryEditorSettings.h"
#include "InventoryEditorStyle.h"
#include "ModularInventoryEditor.h"

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

	void OpenMessageLog()
	{
		FMessageLog(InventoryEditor::MessageLogName).Open(EMessageSeverity::Info, true);
	}

	/** The tools work on assets, which shouldn't change while the game is playing in the editor. */
	bool IsNotPlaying()
	{
		return !GEditor || GEditor->PlayWorld == nullptr;
	}

	FSlateIcon MakeIcon(const FName& StyleName)
	{
		return FSlateIcon(FInventoryEditorStyle::GetStyleSetName(), StyleName);
	}

	/** What the toolbar button opens: the documentation first, then the tools, then where to set them up and see what they did. */
	TSharedRef<SWidget> MakeToolbarMenu()
	{
		FMenuBuilder Menu(true, nullptr);

		Menu.BeginSection("ModularInventoryHelp", LOCTEXT("HelpSection", "Help"));
		Menu.AddMenuEntry(
			LOCTEXT("Documentation", "Documentation"),
			LOCTEXT("DocumentationTooltip", "The Modular Inventory guide: setting it up, every asset and component, the UI, the editor tools and troubleshooting."),
			MakeIcon("ModularInventory.Documentation"),
			FUIAction(FExecuteAction::CreateLambda([]() { SInventoryDocumentation::Open(); })));
		Menu.EndSection();

		Menu.BeginSection("ModularInventoryItems", LOCTEXT("ItemsSection", "Items"));
		Menu.AddMenuEntry(
			LOCTEXT("GenerateAllIcons", "Generate All Item Icons"),
			LOCTEXT("GenerateAllIconsTooltip", "Generate Item Icons for every item in the project: generated icons are rendered again with the current settings, missing ones are created, hand-made ones are left alone."),
			MakeIcon("ModularInventory.GenerateIcons"),
			FUIAction(FExecuteAction::CreateStatic(&GenerateAllIcons), FCanExecuteAction::CreateStatic(&IsNotPlaying)));
		Menu.EndSection();

		Menu.BeginSection("ModularInventorySetup", LOCTEXT("SetupSection", "Setup"));
		Menu.AddMenuEntry(
			LOCTEXT("MessageLog", "Message Log"),
			LOCTEXT("MessageLogTooltip", "What the tools skipped or couldn't do, and why. Click an asset's name there to find it."),
			MakeIcon("ModularInventory.MessageLog"),
			FUIAction(FExecuteAction::CreateStatic(&OpenMessageLog)));
		Menu.AddMenuEntry(
			LOCTEXT("Settings", "Settings"),
			LOCTEXT("SettingsTooltip", "How the tools name assets and render icons, in Project Settings > Plugins > Modular Inventory."),
			MakeIcon("ModularInventory.Settings"),
			FUIAction(FExecuteAction::CreateStatic(&UInventoryEditorSettings::Open)));
		Menu.EndSection();

		return Menu.MakeWidget();
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
				MakeIcon("ModularInventory.Icon"),
				FUIAction(FExecuteAction::CreateLambda([Sources]() { InventoryItemCreation::CreateItemsFrom(Sources); })));
		}

		if (!Items.IsEmpty())
		{
			Section.AddMenuEntry("GenerateItemIcons",
				LOCTEXT("GenerateIcons", "Generate Item Icons"),
				LOCTEXT("GenerateIconsTooltip", "Renders an icon from each item's World Mesh. Icons made this way are rendered again with their own framing; hand-made icons are left alone."),
				MakeIcon("ModularInventory.GenerateIcons"),
				FUIAction(FExecuteAction::CreateLambda([Items]() { InventoryIcons::GenerateIcons(LoadItems(Items)); })));
		}

		if (Items.Num() == 1)
		{
			Section.AddMenuEntry("EditItemIcon",
				LOCTEXT("EditIcon", "Edit Item Icon..."),
				LOCTEXT("EditIconTooltip", "Opens the Icon Studio to frame and save this item's icon."),
				MakeIcon("ModularInventory.GenerateIcons"),
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
	// Beside the play buttons, where plugins put their own menus, as Lyra's editor does. The label shows next to the icon.
	UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
	FToolMenuSection& ToolbarSection = Toolbar->FindOrAddSection("PluginTools");
	FToolMenuEntry MenuEntry = FToolMenuEntry::InitComboButton(
		"ModularInventory",
		FUIAction(),
		FOnGetContent::CreateStatic(&MakeToolbarMenu),
		LOCTEXT("ToolbarLabel", "Modular Inventory"),
		LOCTEXT("ToolbarTooltip", "Modular Inventory: its documentation and editor tools."),
		MakeIcon("ModularInventory.Toolbar"));
	MenuEntry.StyleNameOverride = "CalloutToolbar";
	ToolbarSection.AddEntry(MenuEntry);

	UToolMenu* AssetMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu");
	AssetMenu->FindOrAddSection("GetAssetActions").AddDynamicEntry("ModularInventory", FNewToolMenuSectionDelegate::CreateStatic(&AddAssetActions));
}

#undef LOCTEXT_NAMESPACE
