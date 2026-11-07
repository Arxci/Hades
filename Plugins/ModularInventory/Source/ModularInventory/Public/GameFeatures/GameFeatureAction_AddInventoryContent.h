#pragma once

#include "CoreMinimal.h"

#include "GameFeatureAction.h"

#include "Loot/InventoryLootTable.h"

#include "GameFeatureAction_AddInventoryContent.generated.h"

class UInventoryRecipe;
class UInventoryRecipeBook;


USTRUCT()
struct FInventoryLootInjection
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Loot")
	TSoftObjectPtr<UInventoryLootTable> Table;

	/** Rolled after the table's own pools while the feature is active. */
	UPROPERTY(EditAnywhere, Category = "Loot")
	TArray<FInventoryLootPool> Pools;
};


USTRUCT()
struct FInventoryRecipeInjection
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Crafting")
	TSoftObjectPtr<UInventoryRecipeBook> Book;

	UPROPERTY(EditAnywhere, Category = "Crafting")
	TArray<TSoftObjectPtr<UInventoryRecipe>> Recipes;
};


/** Adds loot pools to existing loot tables and recipes to existing recipe books while the game feature is active, e.g. for DLC. */
UCLASS(MinimalAPI, meta = (DisplayName = "Add Inventory Content"))
class UGameFeatureAction_AddInventoryContent final : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	//~ Begin UGameFeatureAction Interface
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
	//~ End UGameFeatureAction Interface

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	UPROPERTY(EditAnywhere, Category = "Loot", meta = (TitleProperty = "Table"))
	TArray<FInventoryLootInjection> LootPools;

	UPROPERTY(EditAnywhere, Category = "Crafting", meta = (TitleProperty = "Book"))
	TArray<FInventoryRecipeInjection> Recipes;

	/** Holds what was injected into, so the tables and books aren't unloaded while active. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInventoryLootTable>> ActiveTables;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInventoryRecipeBook>> ActiveBooks;
};
