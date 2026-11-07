#include "InventoryEditorActions.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "ContentBrowserMenuContexts.h"
#include "ContentBrowserModule.h"
#include "Engine/AssetManager.h"
#include "Engine/AssetManagerSettings.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "FileHelpers.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"
#include "Misc/PackageName.h"
#include "ToolMenus.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "Data/InventoryItemCosmetics.h"
#include "Data/InventoryItemDefinition.h"
#include "InventoryAssetFactories.h"

#define LOCTEXT_NAMESPACE "InventoryEditorActions"

namespace
{
	void Notify(const FText& Message)
	{
		FNotificationInfo Info(Message);
		Info.ExpireDuration = 6.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}

	// Asset Manager
	bool IsItemTypeInSettings(const UAssetManagerSettings& Settings)
	{
		return Settings.PrimaryAssetTypesToScan.ContainsByPredicate([](const FPrimaryAssetTypeInfo& TypeInfo)
		{
			return TypeInfo.PrimaryAssetType == UInventoryItemDefinition::AssetType.GetName();
		});
	}

	/** Items saved before item definitions had a fixed Asset Manager type keep their old ID until they're saved again. */
	int32 ResaveItemsWithOldIds()
	{
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TArray<FAssetData> Items;
		AssetRegistry.GetAssetsByClass(UInventoryItemDefinition::StaticClass()->GetClassPathName(), Items, true);

		TArray<UPackage*> Packages;
		for (const FAssetData& Item : Items)
		{
			UObject* Asset = Item.GetPrimaryAssetId().PrimaryAssetType != UInventoryItemDefinition::AssetType ? Item.GetAsset() : nullptr;
			if (Asset)
			{
				Packages.Add(Asset->GetPackage());
			}
		}
		if (!Packages.IsEmpty())
		{
			UEditorLoadingAndSavingUtils::SavePackages(Packages, false);
		}
		return Packages.Num();
	}

	void RegisterItemsWithAssetManager()
	{
		UAssetManagerSettings* Settings = GetMutableDefault<UAssetManagerSettings>();
		if (!IsItemTypeInSettings(*Settings))
		{
			FDirectoryPath GameContent;
			GameContent.Path = TEXT("/Game");

			FPrimaryAssetTypeInfo ItemTypeInfo(UInventoryItemDefinition::AssetType.GetName(), UInventoryItemDefinition::StaticClass(), false, false, { GameContent }, {});
			// Save games may be the only thing referencing an item, so every item is cooked.
			ItemTypeInfo.Rules.CookRule = EPrimaryAssetCookRule::AlwaysCook;

			Settings->PrimaryAssetTypesToScan.Add(ItemTypeInfo);
			Settings->PostEditChange();
			Settings->TryUpdateDefaultConfigFile();
		}

		const int32 ResavedCount = ResaveItemsWithOldIds();
		UAssetManager::Get().ReinitializeFromConfig();

		Notify(FText::Format(LOCTEXT("Registered", "Items are registered with the Asset Manager (Config/DefaultGame.ini). Resaved {0} item definitions that had an old ID."),
			FText::AsNumber(ResavedCount)));
	}

	// Content Browser
	bool IsAssetOf(const FAssetData& Asset, const UClass* Class)
	{
		const UClass* AssetClass = Asset.GetClass();
		return AssetClass && AssetClass->IsChildOf(Class);
	}

	bool IsItemSource(const FAssetData& Asset)
	{
		return IsAssetOf(Asset, UStaticMesh::StaticClass()) || IsAssetOf(Asset, UTexture2D::StaticClass());
	}

	/** SM_Lantern and T_Lantern_Icon both give Lantern. */
	FString GetItemBaseName(const FAssetData& Source)
	{
		FString BaseName = Source.AssetName.ToString();
		for (const TCHAR* Prefix : { TEXT("SM_"), TEXT("T_"), TEXT("TX_"), TEXT("UI_") })
		{
			if (BaseName.RemoveFromStart(Prefix))
			{
				break;
			}
		}
		BaseName.RemoveFromEnd(TEXT("_Icon"));
		return BaseName;
	}

	template <typename T>
	T* CreateInventoryAsset(IAssetTools& AssetTools, const FString& Folder, const FString& AssetName, UFactory* Factory)
	{
		FString UniquePackageName;
		FString UniqueAssetName;
		AssetTools.CreateUniqueAssetName(Folder / AssetName, FString(), UniquePackageName, UniqueAssetName);
		return Cast<T>(AssetTools.CreateAsset(UniqueAssetName, FPackageName::GetLongPackagePath(UniquePackageName), T::StaticClass(), Factory));
	}

	/** Makes an item definition and its cosmetics next to the source asset. Mesh or Icon may be null, not both. */
	UInventoryItemDefinition* CreateItem(const FAssetData* Mesh, const FAssetData* Icon)
	{
		const FAssetData& Source = Mesh ? *Mesh : *Icon;
		const FString BaseName = GetItemBaseName(Source);
		const FString Folder = Source.PackagePath.ToString();
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

		UInventoryItemCosmetics* Cosmetics = CreateInventoryAsset<UInventoryItemCosmetics>(AssetTools, Folder, TEXT("DA_") + BaseName + TEXT("_Cosmetics"), NewObject<UInventoryItemCosmeticsFactory>());
		UInventoryItemDefinition* Definition = CreateInventoryAsset<UInventoryItemDefinition>(AssetTools, Folder, TEXT("DA_") + BaseName, NewObject<UInventoryItemDefinitionFactory>());
		if (!Cosmetics || !Definition)
		{
			return nullptr;
		}

		Cosmetics->WorldMesh = Mesh ? Cast<UStaticMesh>(Mesh->GetAsset()) : nullptr;
		Cosmetics->Icon = Icon ? Cast<UTexture2D>(Icon->GetAsset()) : nullptr;
		Cosmetics->PostEditChange();

		Definition->DisplayName = FText::FromString(FName::NameToDisplayString(BaseName, false));
		Definition->Cosmetics = Cosmetics;
		Definition->PostEditChange();
		return Definition;
	}

	void CreateItemsFrom(const TArray<FAssetData>& SelectedAssets)
	{
		TArray<const FAssetData*> Meshes;
		TArray<const FAssetData*> Icons;
		for (const FAssetData& Asset : SelectedAssets)
		{
			if (IsAssetOf(Asset, UStaticMesh::StaticClass()))
			{
				Meshes.Add(&Asset);
			}
			else if (IsAssetOf(Asset, UTexture2D::StaticClass()))
			{
				Icons.Add(&Asset);
			}
		}

		TArray<UObject*> Created;
		if (Meshes.Num() == 1 && Icons.Num() == 1)
		{
			Created.Add(CreateItem(Meshes[0], Icons[0]));
		}
		else
		{
			for (const FAssetData* Mesh : Meshes)
			{
				Created.Add(CreateItem(Mesh, nullptr));
			}
			for (const FAssetData* Icon : Icons)
			{
				Created.Add(CreateItem(nullptr, Icon));
			}
		}
		Created.Remove(nullptr);

		FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get().SyncBrowserToAssets(Created);
	}
}

void InventoryEditorActions::RegisterMenus()
{
	UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	FToolMenuSection& ToolsSection = ToolsMenu->AddSection("ModularInventory", LOCTEXT("ToolsSection", "Modular Inventory"));
	ToolsSection.AddMenuEntry("RegisterItemsWithAssetManager",
		LOCTEXT("RegisterItems", "Register Items With Asset Manager"),
		LOCTEXT("RegisterItemsTooltip", "Adds the InventoryItem type to the Asset Manager settings, so items can be found by ID and are always cooked. Also resaves items that still carry an old ID."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&RegisterItemsWithAssetManager)));

	UToolMenu* AssetMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu");
	FToolMenuSection& AssetSection = AssetMenu->FindOrAddSection("GetAssetActions");
	AssetSection.AddDynamicEntry("CreateInventoryItems", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
	{
		const UContentBrowserAssetContextMenuContext* Context = InSection.FindContext<UContentBrowserAssetContextMenuContext>();
		if (!Context || !Context->SelectedAssets.ContainsByPredicate(&IsItemSource))
		{
			return;
		}

		const TArray<FAssetData> SelectedAssets = Context->SelectedAssets;
		InSection.AddMenuEntry("CreateInventoryItems",
			LOCTEXT("CreateItems", "Create Inventory Item"),
			LOCTEXT("CreateItemsTooltip", "Makes an item definition and cosmetics next to each selected static mesh or texture: meshes become the world mesh, textures the icon. Select one mesh and one texture to make a single item with both."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([SelectedAssets]() { CreateItemsFrom(SelectedAssets); })));
	}));
}

#undef LOCTEXT_NAMESPACE
