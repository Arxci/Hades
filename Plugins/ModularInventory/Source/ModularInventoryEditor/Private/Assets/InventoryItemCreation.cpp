#include "Assets/InventoryItemCreation.h"

#include "AssetRegistry/AssetData.h"
#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"
#include "Misc/PackageName.h"

#include "Assets/InventoryAssetFactories.h"
#include "Data/InventoryItemCosmetics.h"
#include "Data/InventoryItemDefinition.h"
#include "Icons/InventoryIconGenerator.h"
#include "InventoryEditorSettings.h"

namespace
{
	bool IsAssetOf(const FAssetData& Asset, const UClass* Class)
	{
		const UClass* AssetClass = Asset.GetClass();
		return AssetClass && AssetClass->IsChildOf(Class);
	}

	template <typename T>
	T* CreateInventoryAsset(const FString& Folder, const FString& Name, UFactory* Factory)
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		FString UniquePackageName;
		FString UniqueAssetName;
		AssetTools.CreateUniqueAssetName(Folder / Name, FString(), UniquePackageName, UniqueAssetName);
		return Cast<T>(AssetTools.CreateAsset(UniqueAssetName, FPackageName::GetLongPackagePath(UniquePackageName), T::StaticClass(), Factory));
	}

	UInventoryItemCosmetics* CreateCosmeticsAsset(const FString& Folder, const FString& BaseName)
	{
		const FString Name = GetDefault<UInventoryEditorSettings>()->MakeCosmeticsName(BaseName);
		return CreateInventoryAsset<UInventoryItemCosmetics>(Folder, Name, NewObject<UInventoryItemCosmeticsFactory>());
	}

	/** Named after Source and made in its folder. Mesh or Icon may be null. */
	UInventoryItemDefinition* CreateItem(const FAssetData& Source, UStaticMesh* Mesh, UTexture2D* Icon)
	{
		const UInventoryEditorSettings* Settings = GetDefault<UInventoryEditorSettings>();
		const FString BaseName = Settings->GetBaseName(Source.AssetName.ToString());
		const FString Folder = Source.PackagePath.ToString();

		UInventoryItemCosmetics* Cosmetics = CreateCosmeticsAsset(Folder, BaseName);
		UInventoryItemDefinition* Item = CreateInventoryAsset<UInventoryItemDefinition>(Folder, Settings->MakeItemName(BaseName), NewObject<UInventoryItemDefinitionFactory>());
		if (!Cosmetics || !Item)
		{
			return nullptr;
		}

		Cosmetics->WorldMesh = Mesh;
		Cosmetics->Icon = Icon;
		Cosmetics->PostEditChange();

		Item->DisplayName = FText::FromString(FName::NameToDisplayString(BaseName, false));
		Item->Cosmetics = Cosmetics;
		Item->PostEditChange();
		return Item;
	}

	template <typename T>
	T* LoadAsset(const FAssetData* Asset)
	{
		return Asset ? Cast<T>(Asset->GetAsset()) : nullptr;
	}
}

bool InventoryItemCreation::CanCreateFrom(const FAssetData& Asset)
{
	return IsAssetOf(Asset, UStaticMesh::StaticClass()) || IsAssetOf(Asset, UTexture2D::StaticClass());
}

void InventoryItemCreation::CreateItemsFrom(const TArray<FAssetData>& Sources)
{
	TArray<const FAssetData*> Meshes;
	TArray<const FAssetData*> Icons;
	for (const FAssetData& Source : Sources)
	{
		if (IsAssetOf(Source, UStaticMesh::StaticClass()))
		{
			Meshes.Add(&Source);
		}
		else if (IsAssetOf(Source, UTexture2D::StaticClass()))
		{
			Icons.Add(&Source);
		}
	}

	TArray<UInventoryItemDefinition*> Created;
	auto Create = [&Created](const FAssetData& Source, const FAssetData* Mesh, const FAssetData* Icon)
	{
		if (UInventoryItemDefinition* Item = CreateItem(Source, LoadAsset<UStaticMesh>(Mesh), LoadAsset<UTexture2D>(Icon)))
		{
			Created.Add(Item);
		}
	};

	if (Meshes.Num() == 1 && Icons.Num() == 1)
	{
		Create(*Meshes[0], Meshes[0], Icons[0]);
	}
	else
	{
		for (const FAssetData* Mesh : Meshes)
		{
			Create(*Mesh, Mesh, nullptr);
		}
		for (const FAssetData* Icon : Icons)
		{
			Create(*Icon, nullptr, Icon);
		}
	}

	if (GetDefault<UInventoryEditorSettings>()->bGenerateIconsForNewItems)
	{
		const TArray<UInventoryItemDefinition*> MeshOnlyItems = Created.FilterByPredicate([](const UInventoryItemDefinition* Item)
		{
			return Item->Cosmetics->Icon.IsNull() && !Item->Cosmetics->WorldMesh.IsNull();
		});
		InventoryIcons::GenerateIcons(MeshOnlyItems);
	}

	const TArray<UObject*> ToSelect(Created);
	FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get().SyncBrowserToAssets(ToSelect);
}

UInventoryItemCosmetics* InventoryItemCreation::CreateCosmetics(const UInventoryItemDefinition& Item)
{
	const FString Folder = FPackageName::GetLongPackagePath(Item.GetPackage()->GetName());
	return CreateCosmeticsAsset(Folder, GetDefault<UInventoryEditorSettings>()->GetBaseName(Item.GetName()));
}
