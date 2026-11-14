#include "Assets/InventoryAssetManagerSetup.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"
#include "Engine/AssetManagerSettings.h"
#include "FileHelpers.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/App.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "Data/InventoryItemDefinition.h"
#include "ModularInventoryEditor.h"

#define LOCTEXT_NAMESPACE "InventoryAssetManagerSetup"

namespace
{
	TWeakPtr<SNotificationItem> RegisterPrompt;
	FDelegateHandle FilesLoadedHandle;

	bool IsItemTypeRegistered()
	{
		return GetDefault<UAssetManagerSettings>()->PrimaryAssetTypesToScan.ContainsByPredicate([](const FPrimaryAssetTypeInfo& TypeInfo)
		{
			return TypeInfo.PrimaryAssetType == UInventoryItemDefinition::AssetType.GetName();
		});
	}

	TArray<FAssetData> FindItems()
	{
		TArray<FAssetData> Items;
		IAssetRegistry::GetChecked().GetAssetsByClass(UInventoryItemDefinition::StaticClass()->GetClassPathName(), Items, true);
		return Items;
	}

	/** Items saved before item definitions had a fixed Asset Manager type keep their old ID until they're saved again. */
	TArray<FAssetData> FindItemsWithOldIds()
	{
		return FindItems().FilterByPredicate([](const FAssetData& Item)
		{
			return Item.GetPrimaryAssetId().PrimaryAssetType != UInventoryItemDefinition::AssetType;
		});
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

	void CloseRegisterPrompt()
	{
		if (const TSharedPtr<SNotificationItem> Prompt = RegisterPrompt.Pin())
		{
			Prompt->ExpireAndFadeout();
		}
		RegisterPrompt.Reset();
	}
}

void InventoryAssetManagerSetup::RegisterItems()
{
	CloseRegisterPrompt();

	UAssetManagerSettings* Settings = GetMutableDefault<UAssetManagerSettings>();
	if (!IsItemTypeRegistered())
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

	InventoryEditor::Notify(FText::Format(LOCTEXT("Registered", "Items are registered with the Asset Manager (Config/DefaultGame.ini). Resaved {0} item definitions that had an old ID."),
		FText::AsNumber(ResavedCount)));
}

void InventoryAssetManagerSetup::PromptIfNeeded()
{
	if (RegisterPrompt.IsValid() || IsRunningCommandlet() || FApp::IsUnattended())
	{
		return;
	}

	const bool bRegistered = IsItemTypeRegistered();
	const int32 OldIdCount = bRegistered ? FindItemsWithOldIds().Num() : 0;
	if (bRegistered && OldIdCount == 0)
	{
		return;
	}

	FNotificationInfo Info(bRegistered
		? FText::Format(LOCTEXT("ResavePrompt", "{0} inventory items still carry an old ID, so they can't be found by it. Resave them to give them their ID."), FText::AsNumber(OldIdCount))
		: LOCTEXT("RegisterPrompt", "Inventory items aren't registered with the Asset Manager yet, so they can't be found by ID, given with Inventory.Give or saved by ID."));
	Info.bFireAndForget = false;
	Info.ButtonDetails.Emplace(bRegistered ? LOCTEXT("Resave", "Resave Items") : LOCTEXT("Register", "Register Items"), FText(),
		FSimpleDelegate::CreateStatic(&InventoryAssetManagerSetup::RegisterItems));
	Info.ButtonDetails.Emplace(LOCTEXT("NotNow", "Not Now"), FText(), FSimpleDelegate::CreateStatic(&CloseRegisterPrompt));
	RegisterPrompt = FSlateNotificationManager::Get().AddNotification(Info);
}

void InventoryAssetManagerSetup::CheckOnceLoaded()
{
	auto Check = []()
	{
		if (!FindItems().IsEmpty())
		{
			PromptIfNeeded();
		}
	};

	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
	if (AssetRegistry.IsLoadingAssets())
	{
		FilesLoadedHandle = AssetRegistry.OnFilesLoaded().AddLambda(Check);
	}
	else
	{
		Check();
	}
}

void InventoryAssetManagerSetup::StopChecking()
{
	if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
	{
		AssetRegistry->OnFilesLoaded().Remove(FilesLoadedHandle);
	}
	FilesLoadedHandle.Reset();
	CloseRegisterPrompt();
}

#undef LOCTEXT_NAMESPACE
