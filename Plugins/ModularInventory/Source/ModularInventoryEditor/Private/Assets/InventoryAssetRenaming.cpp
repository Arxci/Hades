#include "Assets/InventoryAssetRenaming.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Containers/Ticker.h"
#include "Engine/Texture2D.h"
#include "IAssetTools.h"
#include "Logging/MessageLog.h"
#include "Misc/PackageName.h"
#include "Misc/UObjectToken.h"

#include "Data/InventoryItemCosmetics.h"
#include "Data/InventoryItemDefinition.h"
#include "InventoryEditorSettings.h"
#include "ModularInventoryEditor.h"

#define LOCTEXT_NAMESPACE "InventoryAssetRenaming"

namespace
{
	struct FRenamedItem
	{
		TWeakObjectPtr<UInventoryItemDefinition> Item;
		FSoftObjectPath OldPath;
	};

	FDelegateHandle PostRenameHandle;
	FTSTicker::FDelegateHandle FollowHandle;
	TArray<FRenamedItem> RenamedItems;

	FString GetFolder(const UObject& Asset)
	{
		return FPackageName::GetLongPackagePath(Asset.GetPackage()->GetName());
	}

	/**
	 * Renames Asset from OldName to NewName when it has OldName, and moves it from OldFolder to NewFolder when it's in OldFolder. Assets with
	 * other names, or in other folders, aren't the item's own, or were put there on purpose, so they're left alone.
	 */
	void Follow(UObject& Asset, const FString& OldName, const FString& NewName, const FString& OldFolder, const FString& NewFolder, TArray<FAssetRenameData>& OutRenames)
	{
		const FString AssetFolder = GetFolder(Asset);
		const FString TargetFolder = AssetFolder == OldFolder ? NewFolder : AssetFolder;
		const FString TargetName = Asset.GetName() == OldName ? NewName : Asset.GetName();
		if (TargetFolder == AssetFolder && TargetName == Asset.GetName())
		{
			return;
		}

		const FSoftObjectPath TargetPath(TargetFolder / TargetName + TEXT(".") + TargetName);
		if (IAssetRegistry::GetChecked().GetAssetByObjectPath(TargetPath).IsValid())
		{
			FMessageLog(InventoryEditor::MessageLogName).Warning()
				->AddToken(FUObjectToken::Create(&Asset))
				->AddToken(FTextToken::Create(FText::Format(LOCTEXT("TargetTaken", "wasn't renamed with its item: {0} already exists."), FText::FromString(TargetPath.ToString()))));
			return;
		}
		OutRenames.Emplace(&Asset, TargetFolder, TargetName);
	}

	void FollowRenamedItems()
	{
		FollowHandle.Reset();
		const TArray<FRenamedItem> Items = MoveTemp(RenamedItems);
		RenamedItems.Reset();

		const UInventoryEditorSettings* Settings = GetDefault<UInventoryEditorSettings>();
		TArray<FAssetRenameData> Renames;
		for (const FRenamedItem& Renamed : Items)
		{
			UInventoryItemDefinition* Item = Renamed.Item.Get();
			UInventoryItemCosmetics* Cosmetics = Item ? Item->Cosmetics.Get() : nullptr;
			if (!Cosmetics)
			{
				continue;
			}

			const FString OldBaseName = Settings->GetBaseName(Renamed.OldPath.GetAssetName());
			const FString NewBaseName = Settings->GetBaseName(Item->GetName());
			const FString OldItemFolder = FPackageName::GetLongPackagePath(Renamed.OldPath.GetLongPackageName());
			const FString NewItemFolder = GetFolder(*Item);

			// The cosmetics are made beside the item, and the icon beside the cosmetics, or in Icon Folder below them when it's a relative one.
			const FString OldCosmeticsFolder = GetFolder(*Cosmetics);
			const FString NewCosmeticsFolder = OldCosmeticsFolder == OldItemFolder ? NewItemFolder : OldCosmeticsFolder;
			Follow(*Cosmetics, Settings->MakeCosmeticsName(OldBaseName), Settings->MakeCosmeticsName(NewBaseName), OldItemFolder, NewItemFolder, Renames);

			if (UTexture2D* Icon = Cosmetics->Icon.LoadSynchronous())
			{
				const bool bRelativeIconFolder = !Settings->IconFolder.IsEmpty() && !Settings->IconFolder.StartsWith(TEXT("/"));
				const bool bInIconFolder = bRelativeIconFolder && GetFolder(*Icon) == OldCosmeticsFolder / Settings->IconFolder;
				const FString OldIconFolder = bInIconFolder ? OldCosmeticsFolder / Settings->IconFolder : OldCosmeticsFolder;
				const FString NewIconFolder = bInIconFolder ? NewCosmeticsFolder / Settings->IconFolder : NewCosmeticsFolder;
				Follow(*Icon, Settings->MakeIconName(OldBaseName), Settings->MakeIconName(NewBaseName), OldIconFolder, NewIconFolder, Renames);
			}
		}

		if (!Renames.IsEmpty())
		{
			FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().RenameAssets(Renames);
		}
	}

	void HandleAssetsRenamed(const TArray<FAssetRenameData>& Renamed)
	{
		if (!GetDefault<UInventoryEditorSettings>()->bRenameItemAssets)
		{
			return;
		}

		for (const FAssetRenameData& RenameData : Renamed)
		{
			if (UInventoryItemDefinition* Item = Cast<UInventoryItemDefinition>(RenameData.Asset.Get()))
			{
				RenamedItems.Add({ Item, RenameData.OldObjectPath });
			}
		}

		// The item's own rename is still being finished, so its assets follow on the next tick. Renaming them comes back here, and is ignored.
		if (!RenamedItems.IsEmpty() && !FollowHandle.IsValid())
		{
			FollowHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
			{
				FollowRenamedItems();
				return false;
			}));
		}
	}
}

void InventoryAssetRenaming::Register()
{
	PostRenameHandle = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().OnAssetPostRename().AddStatic(&HandleAssetsRenamed);
}

void InventoryAssetRenaming::Unregister()
{
	if (FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools"))
	{
		AssetToolsModule->Get().OnAssetPostRename().Remove(PostRenameHandle);
	}
	PostRenameHandle.Reset();

	if (FollowHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(FollowHandle);
		FollowHandle.Reset();
	}
	RenamedItems.Reset();
}

#undef LOCTEXT_NAMESPACE
