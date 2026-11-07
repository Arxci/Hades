#pragma once

#include "CoreMinimal.h"

#include "Factories/Factory.h"

#include "InventoryAssetFactories.generated.h"


/** Lists a plugin asset type in the Content Browser's Add menu, under Inventory. */
UCLASS(Abstract)
class UInventoryAssetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UInventoryAssetFactory();

	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual FText GetDisplayName() const override;
	virtual uint32 GetMenuCategories() const override;
	//~ End UFactory Interface
};


UCLASS()
class UInventoryItemDefinitionFactory : public UInventoryAssetFactory
{
	GENERATED_BODY()

public:
	UInventoryItemDefinitionFactory();

	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	//~ End UFactory Interface
};


UCLASS()
class UInventoryItemCosmeticsFactory : public UInventoryAssetFactory
{
	GENERATED_BODY()

public:
	UInventoryItemCosmeticsFactory();
};


UCLASS()
class UInventoryContainerLayoutFactory : public UInventoryAssetFactory
{
	GENERATED_BODY()

public:
	UInventoryContainerLayoutFactory();
};


UCLASS()
class UInventoryStackRulesFactory : public UInventoryAssetFactory
{
	GENERATED_BODY()

public:
	UInventoryStackRulesFactory();
};


UCLASS()
class UInventoryEquipRulesFactory : public UInventoryAssetFactory
{
	GENERATED_BODY()

public:
	UInventoryEquipRulesFactory();
};


UCLASS()
class UInventoryLootTableFactory : public UInventoryAssetFactory
{
	GENERATED_BODY()

public:
	UInventoryLootTableFactory();
};


UCLASS()
class UInventoryRecipeFactory : public UInventoryAssetFactory
{
	GENERATED_BODY()

public:
	UInventoryRecipeFactory();
};


UCLASS()
class UInventoryRecipeBookFactory : public UInventoryAssetFactory
{
	GENERATED_BODY()

public:
	UInventoryRecipeBookFactory();
};
