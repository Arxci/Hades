#include "Assets/InventoryAssetFactories.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"

#include "Assets/InventoryAssetManagerSetup.h"
#include "Crafting/InventoryRecipe.h"
#include "Data/InventoryContainerLayout.h"
#include "Data/InventoryEquipRules.h"
#include "Data/InventoryItemCosmetics.h"
#include "Data/InventoryItemDefinition.h"
#include "Data/InventoryStackRules.h"
#include "Loot/InventoryLootTable.h"

#define LOCTEXT_NAMESPACE "InventoryAssetFactories"

UInventoryAssetFactory::UInventoryAssetFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UInventoryAssetFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UObject>(InParent, InClass, InName, Flags);
}

FText UInventoryAssetFactory::GetDisplayName() const
{
	// The menu already says Inventory, so "Item Definition" rather than "Inventory Item Definition".
	FString ClassName = SupportedClass->GetDisplayNameText().ToString();
	ClassName.RemoveFromStart(TEXT("Inventory "));
	return FText::FromString(ClassName);
}

uint32 UInventoryAssetFactory::GetMenuCategories() const
{
	// Registering again returns the category registered the first time.
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	return AssetTools.RegisterAdvancedAssetCategory(TEXT("Inventory"), LOCTEXT("InventoryCategory", "Inventory"));
}

UInventoryItemDefinitionFactory::UInventoryItemDefinitionFactory()
{
	SupportedClass = UInventoryItemDefinition::StaticClass();
}

UObject* UInventoryItemDefinitionFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	InventoryAssetManagerSetup::PromptIfUnregistered();
	return Super::FactoryCreateNew(InClass, InParent, InName, Flags, Context, Warn);
}

UInventoryItemCosmeticsFactory::UInventoryItemCosmeticsFactory()
{
	SupportedClass = UInventoryItemCosmetics::StaticClass();
}

UInventoryContainerLayoutFactory::UInventoryContainerLayoutFactory()
{
	SupportedClass = UInventoryContainerLayout::StaticClass();
}

UInventoryStackRulesFactory::UInventoryStackRulesFactory()
{
	SupportedClass = UInventoryStackRules::StaticClass();
}

UInventoryEquipRulesFactory::UInventoryEquipRulesFactory()
{
	SupportedClass = UInventoryEquipRules::StaticClass();
}

UInventoryLootTableFactory::UInventoryLootTableFactory()
{
	SupportedClass = UInventoryLootTable::StaticClass();
}

UInventoryRecipeFactory::UInventoryRecipeFactory()
{
	SupportedClass = UInventoryRecipe::StaticClass();
}

UInventoryRecipeBookFactory::UInventoryRecipeBookFactory()
{
	SupportedClass = UInventoryRecipeBook::StaticClass();
}

#undef LOCTEXT_NAMESPACE
