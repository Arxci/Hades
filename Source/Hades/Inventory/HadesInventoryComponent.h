#pragma once

#include "CoreMinimal.h"

#include "InventoryComponent.h"
#include "Templates/SubclassOf.h"

#include "HadesInventoryComponent.generated.h"

class UInventoryItemDefinition;
class UInventoryRecipeBook;


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
 * The player's inventory with the game's rules on top: one item in hand, using, discarding and combining items, and guns and their ammo.
 * The item in hand stays where it is in the inventory. Its equip rules decide which hand slot it's equipped to.
 * Expects a Container.Inventory grid and a Container.KeyItems list, plus an Inventory Equipment Component on the same actor.
 * Give the inventory's layout the grid it starts with as its Grid Size, Grid Growth With Upgrades, and the most rows it grows to as its
 * Max Rows. Pouches are items with an Inventory Capacity Upgrade for Container.Inventory: picking one up adds cells, and this component
 * tells the player so.
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

	// Item actions. Using items is the plugin's: items with an Inventory Item Use fragment, such as a herb that heals.
	/** Key items can't be dropped; they go away by themselves once used up. */
	virtual bool CanDropItem(FInventoryItemHandle Item) const override;

	/** Whether Discard Item would work: Can Drop Item. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Actions")
	bool CanDiscardItem(FInventoryItemHandle Item) const { return CanDropItem(Item); }

	/** Drops the whole stack at the owner's feet, in a Dropped Item Class pickup, or destroys it without one. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Actions")
	bool DiscardItem(FInventoryItemHandle Item) { return DropItem(Item, 0); }

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

	// Organizing
	/** Repacks the inventory grid, biggest items first. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool OrganizeInventory();

protected:
	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	//~ End UActorComponent Interface

	/** What Combine can make. Each recipe takes one or two ingredients, such as two green herbs or a green and a red. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Hades")
	TObjectPtr<UInventoryRecipeBook> CombineRecipes;

private:
	/** Tells the player a pouch, or any other capacity upgrade, made room for more. */
	UFUNCTION()
	void HandleCapacityUpgraded(UInventoryComponent* Inventory, FGameplayTag ContainerTag, const UInventoryItemDefinition* Upgrade);

	FGameplayTag ChooseHand(FInventoryItemHandle Item) const;
	const FHadesGun* FindGun(FInventoryItemHandle Item) const;

	/** Fills a gun from its ammo, taking from Preferred Ammo first when it loads the gun, such as the stack the player combined it with. */
	bool LoadGun(FInventoryItemHandle Gun, FInventoryItemHandle PreferredAmmo);

	/** Whichever of the two is a gun the other one loads. */
	FInventoryItemHandle FindGunLoadedBy(FInventoryItemHandle Item, FInventoryItemHandle Other) const;
};
