#pragma once

#include "CoreMinimal.h"

#include "Subsystems/WorldSubsystem.h"

#include "InventoryRegistry.h"
#include "InventoryTypes.h"

#include "InventorySubsystem.generated.h"

class UInventoryContainerLayout;
class UInventoryItemDefinition;
class UInventoryLootTable;
class UInventoryRecipe;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInventoryContainerChangedSignature, FInventoryContainerHandle, Container);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInventoryItemChangedSignature, FInventoryItemHandle, Item);


/**
 * Owns the world's inventory registry and exposes it to Blueprint.
 * C++ can also use GetRegistry() directly, which returns errors instead of taking them as out parameters.
 */
UCLASS()
class MODULARINVENTORY_API UInventorySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UInventorySubsystem* Get(const UObject* WorldContextObject);

	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	FInventoryRegistry& GetRegistry() { return Registry; }
	const FInventoryRegistry& GetRegistry() const { return Registry; }

	// Items
	UFUNCTION(BlueprintPure, Category = "Inventory|Items")
	bool IsValidItem(FInventoryItemHandle Item) const { return Registry.IsValid(Item); }

	UFUNCTION(BlueprintPure, Category = "Inventory|Items")
	const UInventoryItemDefinition* GetItemDefinition(FInventoryItemHandle Item) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Items")
	int32 GetItemCount(FInventoryItemHandle Item) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Items")
	FInventoryContainerHandle GetItemContainer(FInventoryItemHandle Item) const;

	/** For list containers, Index is the item's position in the list. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Items")
	FInventoryPlacement GetItemPlacement(FInventoryItemHandle Item) const { return Registry.GetItemPlacement(Item); }

	/** Cells taken in its current rotation. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Items")
	FIntPoint GetItemSize(FInventoryItemHandle Item) const { return Registry.GetItemSize(Item); }

	/** The containers the item carries, like a backpack's storage. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Items")
	TArray<FInventoryContainerHandle> GetItemContainers(FInventoryItemHandle Item) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Items")
	FInventoryContainerHandle FindItemContainer(FInventoryItemHandle Item, FGameplayTag ContainerTag) const { return Registry.FindItemContainer(Item, ContainerTag); }

	/** Whether the item sits in an equipment slot. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Items")
	bool IsItemEquipped(FInventoryItemHandle Item) const;

	/** The items in its equipment containers, such as a rifle's scope, and those attached to them in turn. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Items")
	TArray<FInventoryItemHandle> GetAttachments(FInventoryItemHandle Item) const { return Registry.GetAttachments(Item); }

	UFUNCTION(BlueprintPure, Category = "Inventory|Items")
	bool CanStack(FInventoryItemHandle Item, FInventoryItemHandle Other) const { return Registry.CanStack(Item, Other); }

	UFUNCTION(BlueprintPure, Category = "Inventory|Items")
	bool MatchesQuery(FInventoryItemHandle Item, const FInventoryItemQuery& Query) const { return Registry.MatchesQuery(Item, Query); }

	// Tags and values
	/** The item's own tags plus its definition's. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Tags")
	FGameplayTagContainer GetItemTags(FInventoryItemHandle Item) const { return Registry.GetItemTags(Item); }

	UFUNCTION(BlueprintPure, Category = "Inventory|Tags")
	bool HasItemTag(FInventoryItemHandle Item, FGameplayTag Tag) const { return Registry.GetItemTags(Item).HasTag(Tag); }

	/** Item tags plus tags granted by the sockets of neighbouring items. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Tags")
	FGameplayTagContainer GetEffectiveItemTags(FInventoryItemHandle Item) const { return Registry.GetEffectiveTags(Item); }

	/** The item's own value, else its definition's, else 0. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Tags")
	float GetTagValue(FInventoryItemHandle Item, FGameplayTag Tag) const { return Registry.GetTagValue(Item, Tag); }

	/** Tag value plus bonuses from the sockets of neighbouring items and from attachments. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Tags")
	float GetEffectiveTagValue(FInventoryItemHandle Item, FGameplayTag Tag) const { return Registry.GetEffectiveTagValue(Item, Tag); }

	UFUNCTION(BlueprintPure, Category = "Inventory|Tags")
	bool HasTagValue(FInventoryItemHandle Item, FGameplayTag Tag) const { return Registry.HasTagValue(Item, Tag); }

	/** The most Tag's value can be, such as full durability: Max Tag's effective value when the item has one, else the value the definition starts Tag at. 0 when neither. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Tags")
	float GetTagValueMax(FInventoryItemHandle Item, FGameplayTag Tag, FGameplayTag MaxTag) const { return Registry.GetTagValueMax(Item, Tag, MaxTag); }

	/** Tag's value as a part of Get Tag Value Max, such as durability for a bar. False when the item has no Tag value or no maximum. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Tags")
	bool GetTagValueFraction(FInventoryItemHandle Item, FGameplayTag Tag, FGameplayTag MaxTag, float& OutFraction) const;

	/** The sum of a tag value over the items in the containers, each times its count, such as the weight of what they hold. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Tags")
	float GetTotalTagValue(const TArray<FInventoryContainerHandle>& Containers, FGameplayTag Tag, bool bRecursive = true) const { return Registry.GetTotalTagValue(Containers, Tag, bRecursive); }

	/** Items whose sockets currently affect this one. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Tags")
	TArray<FInventoryItemHandle> GetSocketSources(FInventoryItemHandle Item) const { return Registry.GetSocketSources(Item); }

	UFUNCTION(BlueprintCallable, Category = "Inventory|Tags")
	bool SetTagValue(FInventoryItemHandle Item, FGameplayTag Tag, float Value);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Tags")
	bool AddToTagValue(FInventoryItemHandle Item, FGameplayTag Tag, float Delta);

	/** Drops the item's own value, so it reads its definition's again. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Tags")
	bool RemoveTagValue(FInventoryItemHandle Item, FGameplayTag Tag);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Tags")
	bool AddItemTag(FInventoryItemHandle Item, FGameplayTag Tag);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Tags")
	bool RemoveItemTag(FInventoryItemHandle Item, FGameplayTag Tag);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Fragments")
	bool RemoveItemFragment(FInventoryItemHandle Item, const UScriptStruct* FragmentType);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Fragments")
	bool RemoveContainerFragment(FInventoryContainerHandle Container, const UScriptStruct* FragmentType);

	// Containers
	UFUNCTION(BlueprintPure, Category = "Inventory|Containers")
	bool IsValidContainer(FInventoryContainerHandle Container) const { return Registry.IsValid(Container); }

	UFUNCTION(BlueprintPure, Category = "Inventory|Containers")
	const UInventoryContainerLayout* GetContainerLayout(FInventoryContainerHandle Container) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Containers")
	FGameplayTag GetContainerTag(FInventoryContainerHandle Container) const;

	/** List containers return their items in order. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Containers")
	TArray<FInventoryItemHandle> GetContainerItems(FInventoryContainerHandle Container) const;

	/** The item carrying this container; unset for root containers. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Containers")
	FInventoryItemHandle GetContainerOwnerItem(FInventoryContainerHandle Container) const;

	/** Whoever created the root container, usually an inventory component. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Containers")
	UObject* GetContainerOwner(FInventoryContainerHandle Container) const { return Registry.GetOwner(Container); }

	UFUNCTION(BlueprintPure, Category = "Inventory|Containers")
	FInventoryContainerHandle GetRootContainer(FInventoryContainerHandle Container) const { return Registry.GetRootContainer(Container); }

	/** The item covering a grid cell, or in a slot or list position. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Containers")
	FInventoryItemHandle GetItemAt(FInventoryContainerHandle Container, const FInventoryPlacement& Placement) const { return Registry.GetItemAt(Container, Placement); }

	UFUNCTION(BlueprintPure, Category = "Inventory|Containers")
	FInventoryItemHandle GetItemInSlot(FInventoryContainerHandle Container, FGameplayTag Slot) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Containers")
	bool CanPlaceItem(FInventoryItemHandle Item, FInventoryContainerHandle Container, const FInventoryPlacement& Placement, EInventoryError& OutError) const;

	/** The first free spot, trying the item's current rotation first. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Containers")
	bool FindPlacement(FInventoryItemHandle Item, FInventoryContainerHandle Container, FInventoryPlacement& OutPlacement) const { return Registry.FindPlacement(Item, Container, OutPlacement); }

	UFUNCTION(BlueprintPure, Category = "Inventory|Containers")
	TArray<FInventoryItemHandle> FindItems(const TArray<FInventoryContainerHandle>& Containers, const FInventoryItemQuery& Query, bool bRecursive = true) const;

	/** Total stack count of matching items. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Containers")
	int32 CountItems(const TArray<FInventoryContainerHandle>& Containers, const FInventoryItemQuery& Query, bool bRecursive = true) const { return Registry.CountItems(Containers, Query, bRecursive); }

	/**
	 * A container that isn't owned by an item or component, e.g. for world storage. Destroy it when done. Starting Capacity gives it its
	 * own capacity instead of its layout's, as Set Capacity would; -1 follows the layout.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Containers")
	FInventoryContainerHandle CreateContainer(const UInventoryContainerLayout* Layout, FGameplayTag Tag, UObject* Owner, int32 StartingCapacity = -1);

	/** Root containers only, along with everything inside. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Containers")
	bool DestroyContainer(FInventoryContainerHandle Container);

	/**
	 * Another layout of the same type, such as a bigger grid for an upgraded backpack. Items keep their places; fails if one no longer fits.
	 * A capacity set with Set Capacity goes back to following the new layout.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Containers")
	bool SetContainerLayout(FInventoryContainerHandle Container, const UInventoryContainerLayout* Layout, EInventoryError& OutError);

	// Capacity: how much of a container's layout can hold items now. The layout is the most it can ever hold; capacity unlocks part of it.
	/**
	 * How much the container can hold now: unlocked cells for a grid, unlocked slots for slots, the most items for a list, or -1 for a list
	 * without a limit. Its own once it has one, from a Starting Capacity, Set Capacity, Add Capacity or a capacity upgrade; else its layout's.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|Capacity")
	int32 GetCapacity(FInventoryContainerHandle Container) const { return Registry.GetCapacity(Container); }

	/**
	 * The most the container can be unlocked to: every cell of its grid at its layout's Max Rows, or every slot. -1 for a grid that grows
	 * without limit, or a list, which has no most.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|Capacity")
	int32 GetMaxCapacity(FInventoryContainerHandle Container) const { return Registry.GetMaxCapacity(Container); }

	/**
	 * How many rows a grid has now, as grid widgets draw them: its layout's Grid Size, and every row it has grown into, as its capacity
	 * rose or to fit its items. 0 for other types.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|Capacity")
	int32 GetGridRows(FInventoryContainerHandle Container) const { return Registry.GetGridRows(Container); }

	/**
	 * Whether the placement is in the part of the container that can hold items now: an unlocked cell of a grid, an unlocked slot, or a
	 * position in a list below its capacity. Locked cells and slots are the rest.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|Capacity")
	bool IsPlacementUsable(FInventoryContainerHandle Container, const FInventoryPlacement& Placement) const { return Registry.IsPlacementUsable(Container, Placement); }

	/**
	 * Gives the container its own capacity, overriding its layout's: storage that grows as upgrades are found, slots earned during play, a
	 * satchel that holds more, a harder difficulty that starts with less room. Grids: Count cells can hold items, counted row by row from
	 * the top left; past Grid Size, the grid gains the rows they reach into, up to Max Rows. Slots: the first Count slots can. Lists: at
	 * most Count items. A negative Count follows the layout again. Fails, changing nothing, while an item sits where it would lock, or a list
	 * holds more than Count.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Capacity")
	bool SetCapacity(FInventoryContainerHandle Container, int32 Count, EInventoryError& OutError);

	/**
	 * Unlocks Count more of what the container can hold, such as 2 cells for a storage upgrade, one slot for a newly earned ring, or 5 more
	 * items for a bigger satchel, or locks that many again with a negative Count. Fails, changing nothing, with At Max Capacity when there's
	 * nothing left to unlock, as for a list without a limit, so an upgrade can be refused once the container is at its biggest; with Invalid
	 * Count when there's nothing left to lock; and while an item sits where it would lock.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Capacity")
	bool AddCapacity(FInventoryContainerHandle Container, int32 Count, EInventoryError& OutError);

	/** How many of the item one stack holds where it is: its definition's Max Stack Size, and its container's stack bonuses. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Capacity")
	int32 GetMaxStackSize(FInventoryItemHandle Item) const { return Registry.GetMaxStackSize(Item); }

	/**
	 * Lets items matching Item Query stack Amount higher in the container, such as for a stack size upgrade. Bonuses add up, and stay with
	 * the container. A stack bigger than a container allows, such as one moved out of a container with a bonus, stays whole but takes no
	 * more.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Capacity")
	bool AddStackBonus(FInventoryContainerHandle Container, const FInventoryItemQuery& ItemQuery, int32 Amount, EInventoryError& OutError);

	/** Sorts a grid or list: merges partial stacks, then orders the items biggest first in a grid, then by name. All or nothing. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Containers")
	bool SortContainer(FInventoryContainerHandle Container, EInventoryError& OutError);

	/** Sorts a grid or list as Settings say, such as by category, then by name. A grid puts each item on the first spot that fits. All or nothing. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Containers")
	bool SortContainerBy(FInventoryContainerHandle Container, const FInventorySortSettings& Settings, EInventoryError& OutError);

	// Operations
	/** Fills compatible stacks first, then free spots. With Allow Partial, adds what fits; otherwise all or nothing. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Operations")
	int32 AddItem(const FInventoryItemSpec& Spec, FInventoryContainerHandle Container, bool bAllowPartial, TArray<FInventoryItemHandle>& OutItems, EInventoryError& OutError);

	/** Adds a single stack at the placement, or onto the compatible stack already there. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Operations")
	FInventoryItemHandle AddItemAt(const FInventoryItemSpec& Spec, FInventoryContainerHandle Container, const FInventoryPlacement& Placement, EInventoryError& OutError);

	/** Count 0 or more than the stack destroys the item and everything inside it. Returns how many were removed. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Operations")
	int32 RemoveItem(FInventoryItemHandle Item, int32 Count = 0) { return Registry.RemoveItem(Item, Count); }

	/** Tops up compatible stacks, then takes a free spot. Count 0 moves the whole stack. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Operations")
	bool MoveItem(FInventoryItemHandle Item, FInventoryContainerHandle Container, int32 Count, EInventoryError& OutError);

	/** Moves to the placement, joining a compatible stack already there. Count 0 moves the whole stack. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Operations")
	bool MoveItemTo(FInventoryItemHandle Item, FInventoryContainerHandle Container, const FInventoryPlacement& Placement, int32 Count, EInventoryError& OutError);

	/** Tops up stacks in all the containers, then takes the first free spot. Count 0 moves the whole stack. With Allow Partial, the rest stays behind. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Operations")
	bool MoveItemToContainers(FInventoryItemHandle Item, const TArray<FInventoryContainerHandle>& Containers, int32 Count, bool bAllowPartial, EInventoryError& OutError);

	/** Each item takes the other's place, keeping its own rotation. Works across containers and components. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Operations")
	bool SwapItems(FInventoryItemHandle Item, FInventoryItemHandle Other, EInventoryError& OutError);

	/** Moves Count into a new stack on a free spot. An unset container means the item's own. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Operations")
	FInventoryItemHandle SplitStack(FInventoryItemHandle Item, int32 Count, FInventoryContainerHandle Container, EInventoryError& OutError);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Operations")
	FInventoryItemHandle SplitStackTo(FInventoryItemHandle Item, int32 Count, FInventoryContainerHandle Container, const FInventoryPlacement& Placement, EInventoryError& OutError);

	/** Moves as much as fits from Source onto Target. Count 0 tries the whole stack. Returns how many moved. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Operations")
	int32 MergeStacks(FInventoryItemHandle Source, FInventoryItemHandle Target, int32 Count, EInventoryError& OutError);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Operations")
	bool RotateItem(FInventoryItemHandle Item, EInventoryError& OutError);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Operations")
	bool EquipItem(FInventoryItemHandle Item, FInventoryContainerHandle EquipmentContainer, FGameplayTag Slot, EInventoryError& OutError);

	/** Moves the item out of its equipment slot onto a free spot in Container. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Operations")
	bool UnequipItem(FInventoryItemHandle Item, FInventoryContainerHandle Container, EInventoryError& OutError);

	/** Runs the operations in order. If one fails, all of them are undone. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Operations")
	FInventoryBatchResult ExecuteBatch(const TArray<FInventoryOperation>& Operations) { return Registry.ExecuteBatch(Operations); }

	// Crafting
	/** Checks availability and ingredients in Sources and everything nested in them. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Crafting")
	bool CanCraft(const UInventoryRecipe* Recipe, const TArray<FInventoryContainerHandle>& Sources, const FGameplayTagContainer& ContextTags, int32 Times, EInventoryError& OutError) const;

	/** Consumes ingredients from Sources and adds the results to Outputs. All or nothing. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Crafting")
	bool Craft(const UInventoryRecipe* Recipe, const TArray<FInventoryContainerHandle>& Sources, const TArray<FInventoryContainerHandle>& Outputs,
		const FGameplayTagContainer& ContextTags, int32 Times, TArray<FInventoryItemHandle>& OutItems, EInventoryError& OutError);

	// Events
	/** Something in the container changed, including items entering or leaving it. Fires once per container per committed change. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FInventoryContainerChangedSignature OnContainerChanged;

	/** The item was created, changed or destroyed. Fires once per item per committed change. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FInventoryItemChangedSignature OnItemChanged;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void HandleRegistryChanged(const FInventoryChangeSet& Changes);
	EInventoryError CheckOwnerRequirements(const FInventoryRegistry& InRegistry, const FInventoryItemRecord& Item, FInventoryContainerHandle Container) const;

	/** The carry limit of the Inventory Component that owns the container's root, if it has one. */
	EInventoryError CheckCarryLimit(const FInventoryRegistry& InRegistry, const FInventoryItemRecord& Item, FInventoryContainerHandle Container) const;

	UPROPERTY()
	FInventoryRegistry Registry;

	FDelegateHandle RegistryChangedHandle;
};
