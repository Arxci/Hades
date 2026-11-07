#include "Crafting/InventoryRecipe.h"

bool UInventoryRecipe::IsAvailable_Implementation(const FGameplayTagContainer& ContextTags) const
{
	return InventoryTags::PassesQuery(Requirements, ContextTags);
}

TArray<UInventoryRecipe*> UInventoryRecipeBook::GetRecipes() const
{
	TArray<UInventoryRecipe*> Result;
	auto AddRecipes = [&Result](const TArray<TObjectPtr<UInventoryRecipe>>& Source)
	{
		for (UInventoryRecipe* Recipe : Source)
		{
			if (Recipe)
			{
				Result.Add(Recipe);
			}
		}
	};

	AddRecipes(Recipes);
	for (const FInventoryInjectedRecipes& Injected : InjectedRecipes)
	{
		AddRecipes(Injected.Recipes);
	}
	return Result;
}

void UInventoryRecipeBook::AddInjectedRecipes(const UObject* Source, const TArray<UInventoryRecipe*>& InRecipes)
{
	FInventoryInjectedRecipes& Injected = InjectedRecipes.AddDefaulted_GetRef();
	Injected.Source = FObjectKey(Source);
	for (UInventoryRecipe* Recipe : InRecipes)
	{
		Injected.Recipes.Add(Recipe);
	}
}

void UInventoryRecipeBook::RemoveInjectedRecipes(const UObject* Source)
{
	const FObjectKey SourceKey(Source);
	InjectedRecipes.RemoveAll([&SourceKey](const FInventoryInjectedRecipes& Injected) { return Injected.Source == SourceKey; });
}

#if WITH_EDITOR
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "InventoryRecipe"

EDataValidationResult UInventoryRecipe::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	if (Outputs.IsEmpty() && !RandomOutputs)
	{
		Context.AddError(LOCTEXT("NoOutputs", "The recipe produces nothing."));
		Result = EDataValidationResult::Invalid;
	}

	for (int32 Index = 0; Index < Ingredients.Num(); ++Index)
	{
		if (Ingredients[Index].Query.IsEmpty())
		{
			Context.AddError(FText::Format(LOCTEXT("EmptyIngredient", "Ingredient {0} has an empty query and would accept any item."), FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
		}
	}

	for (int32 Index = 0; Index < Outputs.Num(); ++Index)
	{
		if (!Outputs[Index].Definition || Outputs[Index].Count < 1)
		{
			Context.AddError(FText::Format(LOCTEXT("InvalidOutput", "Output {0} needs a definition and a count of at least 1."), FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE
#endif // WITH_EDITOR
