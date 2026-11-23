#pragma once

#include "CoreMinimal.h"

#include "GameplayTagContainer.h"

#include "InventoryTypes.h"

#include "InventoryCapacityUpgrade.generated.h"


/** What a capacity upgrade grows. */
UENUM(BlueprintType)
enum class EInventoryCapacityUpgradeKind : uint8
{
	/** More room: cells for a grid, slots for slots, or items for a list. */
	Room,
	/** Bigger stacks: matching items stack higher in the container, such as ammo that stacks to 60 instead of 30. */
	StackSize
};


/**
 * Makes an item a capacity upgrade: it lets one of its user's containers hold more, such as a pouch that adds two cells to a grid, a ring
 * slot earned during play, a satchel that holds five more items, or an ammo case that lets ammo stack higher. Add it to an item
 * definition's Fragments.
 *
 * Room grows as far as the container's layout allows: a grid set to grow With Upgrades up to its Max Rows, slots up to every slot the
 * layout has, and a list without end. Inventory widgets show the new room, and the cells or slots still to come disabled.
 * Picking the item up applies it, with Apply When Taken. Otherwise Inventory Component > Apply Capacity Upgrade applies it and uses one up,
 * such as from an item menu.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Inventory Capacity Upgrade"))
struct MODULARINVENTORY_API FInventoryCapacityUpgrade
{
	GENERATED_BODY()

	/** The Inventory Component's container it upgrades, by its tag, such as Container.Inventory. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Capacity Upgrade")
	FGameplayTag ContainerTag;

	/** Whether it adds room, or lets items stack higher. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Capacity Upgrade")
	EInventoryCapacityUpgradeKind Kind = EInventoryCapacityUpgradeKind::Room;

	/**
	 * How much one of the item adds. Room: cells for a grid, added after the last one; slots for slots; items for a list. Stack Size: how
	 * many more of each matching item one stack holds.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Capacity Upgrade", meta = (ClampMin = "1"))
	int32 Amount = 1;

	/**
	 * Stack Size only: which items stack higher, such as a Tag Query of Item.Type.Ammo. Empty means every item that stacks. Items that
	 * don't stack at all never do.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Capacity Upgrade", meta = (EditCondition = "Kind == EInventoryCapacityUpgradeKind::StackSize", EditConditionHides))
	FInventoryItemQuery StackItems;

	/**
	 * Applied, and used up, as soon as an Inventory Component with the container takes it, with Add Item, Take Item, Take All or Add Loot,
	 * so it never needs room: picking up a pouch grows the inventory at once. Off, it's kept like any other item until Apply Capacity
	 * Upgrade applies it, such as from an item menu.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Capacity Upgrade")
	bool bApplyWhenTaken = true;
};
