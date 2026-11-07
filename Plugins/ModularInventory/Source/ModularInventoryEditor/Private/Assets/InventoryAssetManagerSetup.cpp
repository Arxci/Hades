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

	bool IsItemTypeRegistered()
	{
		return GetDefault<UAssetManagerSettings>()->PrimaryAssetTypesToScan.ContainsByPredicate([](const FPrimaryAssetTypeInfo& TypeInfo)
		{
			return TypeInfo.PrimaryAssetType == UInventoryItemDefinition::AssetType.GetName();
		});
	}

	/** Items saved before item definitions had a fixed Asset Manager type keep their old ID until they're saved again. */
	int32 ResaveItemsWithOldIds()
	{
		TArray<FAssetData> Items;
		IAssetRegistry::GetChecked().GetAssetsByClass(UInventoryItemDefinition::StaticClass()->GetClassPathName(), Items, true);

		TArray<UPackage*> Packages;
		for (const FAssetData& Item : Items)
		{
			const UObject* Asset = Item.GetPrimaryAssetId().PrimaryAssetType != UInventoryItemDefinition::AssetType ? Item.GetAsset() : nullptr;
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

void InventoryAssetManagerSetup::PromptIfUnregistered()
{
	if (IsItemTypeRegistered() || RegisterPrompt.IsValid() || IsRunningCommandlet() || FApp::IsUnattended())
	{
		return;
	}

	FNotificationInfo Info(LOCTEXT("RegisterPrompt", "Inventory items aren't registered with the Asset Manager yet, so they can't be found by ID, given with Inventory.Give or saved by ID."));
	Info.bFireAndForget = false;
	Info.ButtonDetails.Emplace(LOCTEXT("Register", "Register Items"), FText(), FSimpleDelegate::CreateStatic(&InventoryAssetManagerSetup::RegisterItems));
	Info.ButtonDetails.Emplace(LOCTEXT("NotNow", "Not Now"), FText(), FSimpleDelegate::CreateStatic(&CloseRegisterPrompt));
	RegisterPrompt = FSlateNotificationManager::Get().AddNotification(Info);
}

#undef LOCTEXT_NAMESPACE
