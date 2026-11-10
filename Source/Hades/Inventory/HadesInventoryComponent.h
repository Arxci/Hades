#pragma once

#include "CoreMinimal.h"

#include "InventoryComponent.h"
#include "Templates/SubclassOf.h"

#include "HadesInventoryComponent.generated.h"

class AHadesItemPickup;
class UGameplayEffect;
class UInventoryContainerLayout;
class UInventoryItemDefinition;
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


/** Add to a gun's item definition's Fragments. Each gun keeps its own rounds. Combining a gun with its ammo loads it, as in the RE games. */
USTRUCT(BlueprintType)
struct FHadesGun
{
	GENERATED_BODY()

	/** Rounds it holds when full. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun", meta = (ClampMin = "1"))
	int32 Capacity = 10;

	/** Rounds in it when it's found, up to Capacity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun", meta = (ClampMin = "0"))
	int32 StartingRounds = 10;

	/** The item it loads, such as Handgun Ammo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun")
	TObjectPtr<UInventoryItemDefinition> Ammo;
};


/**
 * The player's inventory with the game's rules on top: one item in hand, using, discarding and combining items, and pouches that grow the pockets.
 * The item in hand stays where it is in the pockets, as in the RE games. Its equip rules decide which hand slot it is equipped to.
 * Expects a Container.Pockets grid and a Container.KeyItems list, plus an Inventory Equipment Component on the same actor.
 */
UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class HADES_API UHadesInventoryComponent : public UInventoryComponent
{
	GENERATED_BODY()

public:
	// Hands
	/** Takes the item in hand, putting away whatever was held. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Hands")
	bool DrawItem(FInventoryItemHandle Item);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Hands")
	bool StowItem(FInventoryItemHandle Item);

	UFUNCTION(BlueprintPure, Category = "Inventory|Hands")
	bool CanDraw(FInventoryItemHandle Item) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Hands")
	bool IsInHand(FInventoryItemHandle Item) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Hands")
	FInventoryItemHandle GetItemInHand() const;

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

	/** Whether any of Combine Recipes uses the item, or it's a gun or ammo whose partner is in the inventory. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Actions")
	bool CanCombine(FInventoryItemHandle Item) const;

	/** Loads a gun from its ammo, or crafts the first of Combine Recipes made from these two items, telling the player when it can't. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Actions")
	bool CombineItems(FInventoryItemHandle Item, FInventoryItemHandle Other);

	UFUNCTION(BlueprintPure, Category = "Inventory|Actions")
	bool IsKeyItem(FInventoryItemHandle Item) const;

	// Guns
	/** Rounds in a gun. Anything without a Hades Gun fragment has none. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Guns")
	int32 GetLoadedRounds(FInventoryItemHandle Gun) const;

	/** Rounds a gun holds when full, or 0 for anything that isn't a gun. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Guns")
	int32 GetCapacity(FInventoryItemHandle Gun) const;

	/** Rounds for a gun in the inventory, not counting those in it. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Guns")
	int32 GetSpareRounds(FInventoryItemHandle Gun) const;

	/** Fills a gun from its ammo in the inventory. False if it's full or there's none. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Guns")
	bool ReloadGun(FInventoryItemHandle Gun);

	/** Takes rounds out of a gun, such as when it fires. False, taking none, if it hasn't that many. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Guns")
	bool SpendRounds(FInventoryItemHandle Gun, int32 Count = 1);

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
	const FHadesGun* FindGun(FInventoryItemHandle Item) const;
	FInventoryItemQuery MakeAmmoQuery(const FHadesGun& Gun) const;

	/** Whichever of the two is a gun the other one loads. */
	FInventoryItemHandle FindGunLoadedBy(FInventoryItemHandle Item, FInventoryItemHandle Other) const;
	UInventoryRecipe* FindCombineRecipe(FInventoryItemHandle Item, FInventoryItemHandle Other) const;
	bool IsMadeFrom(const UInventoryRecipe& Recipe, FInventoryItemHandle Item, FInventoryItemHandle Other) const;
	FTransform FindDropTransform() const;
};
