#pragma once

#include "CoreMinimal.h"

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/ObjectKey.h"

#include "InventoryTypes.h"

#include "InventoryRecipe.generated.h"

class UInventoryLootTable;


USTRUCT(BlueprintType)
struct MODULARINVENTORY_API FInventoryRecipeIngredient
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	FInventoryItemQuery Query;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe", meta = (ClampMin = "1"))
	int32 Count = 1;

	/** Off for tools like a knife: required, but kept. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	bool bConsume = true;
};


/** Turns ingredients into items. Subclass in Blueprint and override Is Available for rules tags can't express. */
UCLASS(BlueprintType, Blueprintable, Const)
class MODULARINVENTORY_API UInventoryRecipe : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Whether the context allows this recipe at all, before ingredients are checked. Defaults to testing Requirements. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Crafting")
	bool IsAvailable(const FGameplayTagContainer& ContextTags) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	FGameplayTagContainer Tags;

	/** Tested against the context tags, such as the station's and the crafter's. Empty always passes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	FGameplayTagQuery Requirements;

	/** Each item counts toward one ingredient only. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	TArray<FInventoryRecipeIngredient> Ingredients;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	TArray<FInventoryItemSpec> Outputs;

	/** Rolled on every craft and added to the outputs. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	TObjectPtr<UInventoryLootTable> RandomOutputs;

	/** Whatever else a game needs, such as craft time or sounds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fragments")
	TArray<FInstancedStruct> Fragments;
};


USTRUCT()
struct FInventoryInjectedRecipes
{
	GENERATED_BODY()

	FObjectKey Source;

	UPROPERTY()
	TArray<TObjectPtr<UInventoryRecipe>> Recipes;
};


/** A list of recipes, such as everything a workbench can make. Game features can add recipes while they are active. */
UCLASS(BlueprintType, Const)
class MODULARINVENTORY_API UInventoryRecipeBook : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The book's own recipes, then those added by game features. */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	TArray<UInventoryRecipe*> GetRecipes() const;

	void AddInjectedRecipes(const UObject* Source, const TArray<UInventoryRecipe*>& InRecipes);
	void RemoveInjectedRecipes(const UObject* Source);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting")
	TArray<TObjectPtr<UInventoryRecipe>> Recipes;

private:
	UPROPERTY(Transient)
	TArray<FInventoryInjectedRecipes> InjectedRecipes;
};
