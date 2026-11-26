#pragma once

#include "CoreMinimal.h"

#include "GameplayTagContainer.h"

#include "Actions/InventoryItemAction.h"

#include "InventoryItemActions.generated.h"

class UInventoryRecipeBook;
class UInventoryRecipeMenuWidget;
class UInventoryShortcutMenuWidget;


/** Picks the item up to put it down somewhere else, as a drag or a screen's Move action does. Gives item menus a way to move items with a gamepad. */
UCLASS(meta = (DisplayName = "Move"))
class MODULARINVENTORYUI_API UInventoryItemAction_Move : public UInventoryItemAction
{
	GENERATED_BODY()

public:
	UInventoryItemAction_Move();

protected:
	virtual bool CanRun_Implementation(const FInventoryItemActionContext& Context) const override;
	virtual void Run_Implementation(const FInventoryItemActionContext& Context) override;
};


/** Picks up part of a stack to put down somewhere else. It asks how many by default, offering 1 to all but one. */
UCLASS(meta = (DisplayName = "Split"))
class MODULARINVENTORYUI_API UInventoryItemAction_Split : public UInventoryItemAction
{
	GENERATED_BODY()

public:
	UInventoryItemAction_Split();

protected:
	virtual bool CanRun_Implementation(const FInventoryItemActionContext& Context) const override;
	virtual void GetQuantityRange_Implementation(const FInventoryItemActionContext& Context, int32& OutMin, int32& OutMax, int32& OutDefault) const override;
	virtual void Run_Implementation(const FInventoryItemActionContext& Context) override;
};


/** Moves the item to the other side, such as from a chest into the player's storage, while the screen has a transfer container. */
UCLASS(meta = (DisplayName = "Transfer"))
class MODULARINVENTORYUI_API UInventoryItemAction_Transfer : public UInventoryItemAction
{
	GENERATED_BODY()

public:
	UInventoryItemAction_Transfer();

protected:
	virtual bool CanRun_Implementation(const FInventoryItemActionContext& Context) const override;
	virtual void Run_Implementation(const FInventoryItemActionContext& Context) override;
};


/**
 * Uses a capacity upgrade the player carries, such as a pouch without Apply When Taken: its Inventory Capacity Upgrade adds room to the
 * player's container, or lets items stack higher there, and one of it is used up. Only offered for items with one, and shown disabled
 * once the container can't grow any more if When Unavailable says so.
 */
UCLASS(meta = (DisplayName = "Apply Capacity Upgrade"))
class MODULARINVENTORYUI_API UInventoryItemAction_ApplyCapacityUpgrade : public UInventoryItemAction
{
	GENERATED_BODY()

public:
	UInventoryItemAction_ApplyCapacityUpgrade();

	/** Only items with an Inventory Capacity Upgrade, on top of Item Filter. */
	virtual bool AppliesTo(const FInventoryItemActionContext& Context) const override;

protected:
	virtual bool CanRun_Implementation(const FInventoryItemActionContext& Context) const override;
	virtual FText GetUnavailableReason_Implementation(const FInventoryItemActionContext& Context) const override;
	virtual void Run_Implementation(const FInventoryItemActionContext& Context) override;

	/** Why it can't run while the container can't grow any more, for an entry shown disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	FText AtMaxCapacityReason;
};


/** Uses the item, as its Inventory Item Use fragment says, such as a herb that heals. Only offered for items with one. */
UCLASS(meta = (DisplayName = "Use"))
class MODULARINVENTORYUI_API UInventoryItemAction_Use : public UInventoryItemAction
{
	GENERATED_BODY()

public:
	UInventoryItemAction_Use();

	/** Only items with an Inventory Item Use, on top of Item Filter. */
	virtual bool AppliesTo(const FInventoryItemActionContext& Context) const override;

protected:
	virtual bool CanRun_Implementation(const FInventoryItemActionContext& Context) const override;
	virtual void Run_Implementation(const FInventoryItemActionContext& Context) override;
};


/**
 * Takes the item out of the player's inventory with the Inventory Component's Drop Item: into its Dropped Item Class, such as a pickup in
 * front of the player, or destroyed without one, as a Discard. It asks to confirm first by default; set Prompt to Quantity to ask how many
 * of a stack.
 */
UCLASS(meta = (DisplayName = "Drop"))
class MODULARINVENTORYUI_API UInventoryItemAction_Drop : public UInventoryItemAction
{
	GENERATED_BODY()

public:
	UInventoryItemAction_Drop();

protected:
	virtual bool CanRun_Implementation(const FInventoryItemActionContext& Context) const override;
	virtual void Run_Implementation(const FInventoryItemActionContext& Context) override;
};


/**
 * Equips the item with the player's Inventory Equipment Component: into one of its equipment slots, or in place, where it stays in the
 * inventory, such as a weapon held in hand that keeps its spot in the grid. Only offered for items with equip rules.
 */
UCLASS(meta = (DisplayName = "Equip"))
class MODULARINVENTORYUI_API UInventoryItemAction_Equip : public UInventoryItemAction
{
	GENERATED_BODY()

public:
	UInventoryItemAction_Equip();

	/** Only items with equip rules, on top of Item Filter. */
	virtual bool AppliesTo(const FInventoryItemActionContext& Context) const override;

protected:
	virtual bool CanRun_Implementation(const FInventoryItemActionContext& Context) const override;
	virtual void Run_Implementation(const FInventoryItemActionContext& Context) override;

	/** The item stays where it is in the inventory, equipped in place. Off, it moves into an equipment container's slot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	bool bEquipInPlace = false;

	/** The slot to equip it to. None picks one its equip rules allow, an empty one first. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	FGameplayTag Slot;

private:
	FGameplayTag ChooseSlot(const FInventoryItemActionContext& Context) const;
};


/** Unequips the item with the player's Inventory Equipment Component, back into storage, or no longer equipped in place. */
UCLASS(meta = (DisplayName = "Unequip"))
class MODULARINVENTORYUI_API UInventoryItemAction_Unequip : public UInventoryItemAction
{
	GENERATED_BODY()

public:
	UInventoryItemAction_Unequip();

protected:
	virtual bool CanRun_Implementation(const FInventoryItemActionContext& Context) const override;
	virtual void Run_Implementation(const FInventoryItemActionContext& Context) override;
};


/**
 * Opens Menu Class for the item, to put it on one of the player's shortcut slots, such as the quick slot keys: beside the item, or as a
 * modal picker, as the menu's Placement says. It opens where the screen opens item menus, replacing this one. Only offered while some slot
 * takes the item, or it's on one already.
 */
UCLASS(meta = (DisplayName = "Assign Shortcut"))
class MODULARINVENTORYUI_API UInventoryItemAction_AssignShortcut : public UInventoryItemAction
{
	GENERATED_BODY()

public:
	UInventoryItemAction_AssignShortcut();

	TSubclassOf<UInventoryShortcutMenuWidget> GetMenuClass() const { return MenuClass; }

protected:
	virtual bool CanRun_Implementation(const FInventoryItemActionContext& Context) const override;
	virtual void Run_Implementation(const FInventoryItemActionContext& Context) override;

	/** The menu that lists the shortcut slots. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	TSubclassOf<UInventoryShortcutMenuWidget> MenuClass;
};


/**
 * Opens Menu Class for the item: the recipes of Recipe Book that take it, each with what it makes, how many of that the player has, and what
 * it takes, to pick one and craft it, as some games' Combine does. It opens where the screen opens item menus, replacing the item menu it
 * was picked from. Only offered while the item takes part in one of the recipes, or with Only When Craftable, while one can be made now.
 */
UCLASS(meta = (DisplayName = "Craft"))
class MODULARINVENTORYUI_API UInventoryItemAction_Craft : public UInventoryItemAction
{
	GENERATED_BODY()

public:
	UInventoryItemAction_Craft();

	TSubclassOf<UInventoryRecipeMenuWidget> GetMenuClass() const { return MenuClass; }

protected:
	virtual bool CanRun_Implementation(const FInventoryItemActionContext& Context) const override;
	virtual void Run_Implementation(const FInventoryItemActionContext& Context) override;

	/**
	 * The recipes offered for the item: Recipe Book, or else the menu's own. Override it in C++ to find them elsewhere, such as recipes the
	 * player has learned.
	 */
	virtual const UInventoryRecipeBook* GetRecipeBook(const FInventoryItemActionContext& Context) const;

	/** The menu that lists the recipes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	TSubclassOf<UInventoryRecipeMenuWidget> MenuClass;

	/** The recipes to offer. None offers the menu's own Recipe Book. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	TObjectPtr<UInventoryRecipeBook> RecipeBook;

	/** Context tags the recipes are checked and crafted with, such as the station the screen is open at. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	FGameplayTagContainer ContextTags;

	/** Only offered while one of the item's recipes can be made now. Off, it's offered while the item takes part in any, so the player sees what it could make. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	bool bOnlyWhenCraftable = false;
};
