#pragma once

#include "CoreMinimal.h"

#include "InventoryComponent.h"
#include "Templates/SubclassOf.h"

#include "HadesInventoryComponent.generated.h"

class AHadesItemPickup;
class UGameplayEffect;
class UInventoryItemDefinition;
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
 * Add to a gun's item definition's Fragments: it makes the item a gun and says what loads it. A gun's numbers are tag values on its
 * definition, where Value Displays, details panels, loot rolls and attachments all see them:
 * - Item.Stat.Capacity: the rounds it holds when full. Attachments add to it, such as an extended magazine's Attachment Value Modifiers.
 * - Item.Stat.Rounds: the rounds it's found with. Each gun then keeps its own.
 * Combining a gun with its ammo loads it, from that stack first.
 */
USTRUCT(BlueprintType)
struct FHadesGun
{
	GENERATED_BODY()

	/**
	 * What loads it: the definition Handgun Ammo, say, or a Tag Query for Item.Ammo.Handgun so every ammo tagged that does. Empty loads
	 * nothing, as it would match every item. Other guns never load it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun")
	FInventoryItemQuery Ammo;
};


/**
 * The player's inventory with the game's rules on top: one item in hand, using, discarding and combining items, guns and their ammo, and
 * pouches that grow the pockets. The item in hand stays where it is in the pockets. Its equip rules decide which hand slot it's equipped to.
 * Expects a Container.Pockets grid and a Container.KeyItems list, plus an Inventory Equipment Component on the same actor. Give the pockets
 * one layout at the biggest size they grow to, with Usable Cells set to what they start with: pouches unlock the rest.
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

	// Guns: items with a Hades Gun fragment. Their rounds and capacity are tag values on the item, so they show and save with it.
	UFUNCTION(BlueprintPure, Category = "Inventory|Guns")
	bool IsGun(FInventoryItemHandle Item) const;

	/** Whether Ammo loads Gun: it matches the gun's Hades Gun Ammo, and isn't a gun itself. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Guns")
	bool IsAmmoFor(FInventoryItemHandle Gun, FInventoryItemHandle Ammo) const;

	/** Rounds in a gun: its Item.Stat.Rounds. 0 for anything that isn't a gun. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Guns")
	int32 GetLoadedRounds(FInventoryItemHandle Gun) const;

	/** Rounds a gun holds when full: its Item.Stat.Capacity, with its attachments' bonuses, such as an extended magazine's. 0 for anything that isn't a gun. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Guns")
	int32 GetCapacity(FInventoryItemHandle Gun) const;

	/** Rounds that would load a gun, held anywhere in this inventory, not counting those in it. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Guns")
	int32 GetSpareRounds(FInventoryItemHandle Gun) const;

	/** Fills a gun from its ammo in the inventory, in one change: the rounds go in only as the ammo comes out. False when it's full or there's none. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Guns")
	bool ReloadGun(FInventoryItemHandle Gun);

	/** Takes rounds out of a gun, such as when it fires. False, taking none, if it hasn't that many. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Guns")
	bool SpendRounds(FInventoryItemHandle Gun, int32 Count = 1);

	// Pockets. They're one layout at the biggest size they grow to, with Usable Cells set to what they start with.
	/** Unlocks Cells more of the pockets' cells, row by row. False, unlocking none, once every cell is unlocked. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Pockets")
	bool GrowPockets(int32 Cells);

	/** An item tagged Item.Type.Pouch: it grows the pockets by its Item.Stat.PocketCells instead of going in them. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Pockets")
	bool IsPouch(FInventoryItemHandle Item) const;

	/**
	 * Grows the pockets by the pouch's Item.Stat.PocketCells, uses the pouch up and tells the player. False, keeping it, when it isn't a
	 * pouch, unlocks nothing, or the pockets are already at their biggest.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Pockets")
	bool UsePouch(FInventoryItemHandle Pouch);

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

private:
	FGameplayTag ChooseHand(FInventoryItemHandle Item) const;
	const FHadesGun* FindGun(FInventoryItemHandle Item) const;

	/** Fills a gun from its ammo, taking from Preferred Ammo first when it loads the gun, such as the stack the player combined it with. */
	bool LoadGun(FInventoryItemHandle Gun, FInventoryItemHandle PreferredAmmo);

	/** Whichever of the two is a gun the other one loads. */
	FInventoryItemHandle FindGunLoadedBy(FInventoryItemHandle Item, FInventoryItemHandle Other) const;
	FTransform FindDropTransform() const;
};
