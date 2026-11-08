#pragma once

#include "CoreMinimal.h"

#include "InventoryComponent.h"
#include "Templates/SubclassOf.h"

#include "HadesInventoryComponent.generated.h"

class AHadesItemPickup;
class UGameplayEffect;
class UInventoryContainerLayout;
class UInventoryRecipe;
class UInventoryRecipeBook;


/** Add to an item definition's Fragments to make it usable from the item menu or a quick slot, such as a herb that heals. */
USTRUCT(BlueprintType)
struct FHadesItemUse
{
	GENERATED_BODY()

	/** Applied to the player. Using the item uses up one of the stack. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Use")
	TSubclassOf<UGameplayEffect> Effect;
};


/**
 * The player's inventory with the game's rules on top: two hands, using, discarding and combining items, and pouches that grow the pockets.
 * Expects a Container.Pockets grid, a Container.KeyItems list and a Container.Hands equipment layout with Equipment.Slot.Hand.Right and Left,
 * plus an Inventory Equipment Component on the same actor.
 */
UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class HADES_API UHadesInventoryComponent : public UInventoryComponent
{
	GENERATED_BODY()

public:
	// Hands
	/** Takes the item in the hand its equip rules allow, putting away whatever is in the way. Fails when that has no room. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Hands")
	bool DrawItem(FInventoryItemHandle Item);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Hands")
	bool StowItem(FInventoryItemHandle Item);

	UFUNCTION(BlueprintPure, Category = "Inventory|Hands")
	bool CanDraw(FInventoryItemHandle Item) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Hands")
	bool IsInHand(FInventoryItemHandle Item) const;

	// Item actions
	UFUNCTION(BlueprintPure, Category = "Inventory|Actions")
	bool CanUseItem(FInventoryItemHandle Item) const;

	/** Applies the item's Hades Item Use effect to the owner and uses one up. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Actions")
	bool UseItem(FInventoryItemHandle Item);

	/** Key items can't be discarded; they go away by themselves once used up. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Actions")
	bool CanDiscardItem(FInventoryItemHandle Item) const;

	/** Drops the item at the owner's feet, or destroys it when Dropped Pickup Class is empty. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Actions")
	bool DiscardItem(FInventoryItemHandle Item);

	/** Whether any of Combine Recipes uses the item. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Actions")
	bool CanCombine(FInventoryItemHandle Item) const;

	/** Crafts the first of Combine Recipes made from these two items, telling the player when it can't. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Actions")
	bool CombineItems(FInventoryItemHandle Item, FInventoryItemHandle Other);

	UFUNCTION(BlueprintPure, Category = "Inventory|Actions")
	bool IsKeyItem(FInventoryItemHandle Item) const;

	// Pockets
	/** Moves the pockets on to the next of Pocket Upgrades. False once they are the biggest. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Pockets")
	bool UpgradePockets();

	/** Repacks the pockets, biggest items first. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Pockets")
	bool OrganizePockets();

protected:
	/** Spawned with a discarded item inside, so it can be picked up again. Empty destroys discarded items. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Hades")
	TSubclassOf<AHadesItemPickup> DroppedPickupClass;

	/** What Combine can make. Each recipe takes one or two ingredients, such as two green herbs or a green and a red. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Hades")
	TObjectPtr<UInventoryRecipeBook> CombineRecipes;

	/** Each pouch found moves the pockets on to the next layout here. Each must hold the one before, as items keep their cells. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Hades")
	TArray<TObjectPtr<UInventoryContainerLayout>> PocketUpgrades;

private:
	FGameplayTag ChooseHand(FInventoryItemHandle Item) const;
	UInventoryRecipe* FindCombineRecipe(FInventoryItemHandle Item, FInventoryItemHandle Other) const;
	bool IsMadeFrom(const UInventoryRecipe& Recipe, FInventoryItemHandle Item, FInventoryItemHandle Other) const;
	FTransform FindDropTransform() const;
};
