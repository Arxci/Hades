#include "GameFeatures/GameFeatureAction_AddInventoryContent.h"

#include "Crafting/InventoryRecipe.h"

void UGameFeatureAction_AddInventoryContent::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	Super::OnGameFeatureActivating(Context);

	for (const FInventoryLootInjection& Injection : LootPools)
	{
		if (UInventoryLootTable* Table = Injection.Table.LoadSynchronous())
		{
			Table->AddInjectedPools(this, Injection.Pools);
			ActiveTables.AddUnique(Table);
		}
	}

	for (const FInventoryRecipeInjection& Injection : Recipes)
	{
		UInventoryRecipeBook* Book = Injection.Book.LoadSynchronous();
		if (!Book)
		{
			continue;
		}

		TArray<UInventoryRecipe*> LoadedRecipes;
		for (const TSoftObjectPtr<UInventoryRecipe>& Recipe : Injection.Recipes)
		{
			if (UInventoryRecipe* LoadedRecipe = Recipe.LoadSynchronous())
			{
				LoadedRecipes.Add(LoadedRecipe);
			}
		}
		Book->AddInjectedRecipes(this, LoadedRecipes);
		ActiveBooks.AddUnique(Book);
	}
}

void UGameFeatureAction_AddInventoryContent::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	for (UInventoryLootTable* Table : ActiveTables)
	{
		if (Table)
		{
			Table->RemoveInjectedPools(this);
		}
	}
	for (UInventoryRecipeBook* Book : ActiveBooks)
	{
		if (Book)
		{
			Book->RemoveInjectedRecipes(this);
		}
	}
	ActiveTables.Reset();
	ActiveBooks.Reset();

	Super::OnGameFeatureDeactivating(Context);
}

#if WITH_EDITOR
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "GameFeatureAction_AddInventoryContent"

EDataValidationResult UGameFeatureAction_AddInventoryContent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	for (int32 Index = 0; Index < LootPools.Num(); ++Index)
	{
		if (LootPools[Index].Table.IsNull())
		{
			Context.AddError(FText::Format(LOCTEXT("MissingTable", "Loot Pools [{0}] has no table."), FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
		}
	}

	for (int32 Index = 0; Index < Recipes.Num(); ++Index)
	{
		if (Recipes[Index].Book.IsNull())
		{
			Context.AddError(FText::Format(LOCTEXT("MissingBook", "Recipes [{0}] has no recipe book."), FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE
#endif // WITH_EDITOR
