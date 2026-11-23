#include "Assets/InventoryAssetManagerSetup.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Containers/Ticker.h"
#include "Engine/AssetManager.h"
#include "Engine/AssetManagerSettings.h"
#include "FileHelpers.h"
#include "Misc/App.h"

#include "Data/InventoryItemDefinition.h"
#include "ModularInventoryEditor.h"

#define LOCTEXT_NAMESPACE "InventoryAssetManagerSetup"

namespace
{
	FDelegateHandle FilesLoadedHandle;
	FDelegateHandle AssetAddedHandle;
	FTSTicker::FDelegateHandle SetUpTicker;

	bool IsItemTypeRegistered()
	{
		return GetDefault<UAssetManagerSettings>()->PrimaryAssetTypesToScan.ContainsByPredicate([](const FPrimaryAssetTypeInfo& TypeInfo)
		{
			return TypeInfo.PrimaryAssetType == UInventoryItemDefinition::AssetType.GetName();
		});
	}

	bool IsItemClass(const FAssetData& Asset)
	{
		const UClass* AssetClass = Asset.GetClass();
		return AssetClass && AssetClass->IsChildOf(UInventoryItemDefinition::StaticClass());
	}

	/** Whether the project has an item definition, saved or only just made. */
	bool HasItems()
	{
		TArray<FAssetData> Items;
		IAssetRegistry::GetChecked().GetAssetsByClass(UInventoryItemDefinition::StaticClass()->GetClassPathName(), Items, true);
		return !Items.IsEmpty();
	}

	/**
	 * Items saved by a version of the plugin from before item definitions had a fixed Asset Manager type. Their ID is read from the file, so
	 * it stays old until they're saved again.
	 */
	TArray<FAssetData> FindItemsWithOldIds()
	{
		FARFilter Filter;
		Filter.ClassPaths.Add(UInventoryItemDefinition::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;
		Filter.bIncludeOnlyOnDiskAssets = true;

		TArray<FAssetData> Items;
		IAssetRegistry::GetChecked().GetAssets(Filter, Items);
		return Items.FilterByPredicate([](const FAssetData& Item)
		{
			return Item.GetPrimaryAssetId().PrimaryAssetType != UInventoryItemDefinition::AssetType;
		});
	}

	/** Adds the InventoryItem entry to the Asset Manager settings, and saves it in Config/DefaultGame.ini. False when the file couldn't be written. */
	bool AddItemType()
	{
		FDirectoryPath GameContent;
		GameContent.Path = TEXT("/Game");

		FPrimaryAssetTypeInfo ItemTypeInfo(UInventoryItemDefinition::AssetType.GetName(), UInventoryItemDefinition::StaticClass(), false, false, { GameContent }, {});
		// Save games may be the only thing referencing an item, so every item is cooked.
		ItemTypeInfo.Rules.CookRule = EPrimaryAssetCookRule::AlwaysCook;

		UAssetManagerSettings* Settings = GetMutableDefault<UAssetManagerSettings>();
		Settings->PrimaryAssetTypesToScan.Add(ItemTypeInfo);
		Settings->PostEditChange();
		return Settings->TryUpdateDefaultConfigFile();
	}

	int32 ResaveItemsWithOldIds()
	{
		TArray<UPackage*> Packages;
		for (const FAssetData& Item : FindItemsWithOldIds())
		{
			if (const UObject* Asset = Item.GetAsset())
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

	/** Not while cooking, or on a build machine, where nobody would see or commit what changed. */
	bool CanChangeProject()
	{
		return GIsEditor && !IsRunningCommandlet() && !FApp::IsUnattended();
	}

	void SetUpItems()
	{
		if (!CanChangeProject() || !HasItems())
		{
			return;
		}

		const bool bAddType = !IsItemTypeRegistered();
		const bool bSavedType = bAddType && AddItemType();
		const int32 ResavedCount = ResaveItemsWithOldIds();
		if (!bAddType && ResavedCount == 0)
		{
			return;
		}

		UAssetManager::Get().ReinitializeFromConfig();

		TArray<FText> Lines;
		if (bAddType)
		{
			Lines.Add(bSavedType
				? LOCTEXT("TypeAdded", "Inventory items now have Asset Manager IDs: the InventoryItem entry was added to Config/DefaultGame.ini. Commit that file.")
				: LOCTEXT("TypeNotSaved", "Inventory items have Asset Manager IDs for this session, but Config/DefaultGame.ini couldn't be written. Make it writable and restart the editor."));
		}
		if (ResavedCount > 0)
		{
			Lines.Add(FText::Format(LOCTEXT("Resaved", "Resaved {0} item definitions that still had an ID from an older version of the plugin."), FText::AsNumber(ResavedCount)));
		}
		InventoryEditor::Notify(FText::Join(FText::FromString(TEXT("\n")), Lines));
	}

	void HandleAssetAdded(const FAssetData& Asset)
	{
		if (!IsItemTypeRegistered() && IsItemClass(Asset))
		{
			InventoryAssetManagerSetup::SetUpSoon();
		}
	}

	void HandleFilesLoaded()
	{
		// Only now: while the Asset Registry is still looking, every asset it finds is announced as added.
		IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
		AssetRegistry.OnAssetAdded().Remove(AssetAddedHandle);
		AssetAddedHandle = AssetRegistry.OnAssetAdded().AddStatic(&HandleAssetAdded);
		InventoryAssetManagerSetup::SetUpSoon();
	}
}

void InventoryAssetManagerSetup::Start()
{
	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
	if (AssetRegistry.IsLoadingAssets())
	{
		FilesLoadedHandle = AssetRegistry.OnFilesLoaded().AddStatic(&HandleFilesLoaded);
	}
	else
	{
		HandleFilesLoaded();
	}
}

void InventoryAssetManagerSetup::Stop()
{
	if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
	{
		AssetRegistry->OnFilesLoaded().Remove(FilesLoadedHandle);
		AssetRegistry->OnAssetAdded().Remove(AssetAddedHandle);
	}
	FilesLoadedHandle.Reset();
	AssetAddedHandle.Reset();

	FTSTicker::RemoveTicker(SetUpTicker);
	SetUpTicker.Reset();
}

void InventoryAssetManagerSetup::SetUpSoon()
{
	// On the next frame, once the editor has finished starting, or the asset being made exists.
	if (!SetUpTicker.IsValid())
	{
		SetUpTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
		{
			SetUpTicker.Reset();
			SetUpItems();
			return false;
		}));
	}
}

#undef LOCTEXT_NAMESPACE
