#pragma once

#include "CoreMinimal.h"

#include "GameplayTagContainer.h"
#include "Math/RandomStream.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/Class.h"

#include "InventoryTypes.h"

#include "InventoryRegistry.generated.h"

class UInventoryContainerLayout;
class UInventoryItemDefinition;
class UInventoryRecipe;
struct FInventoryItemSocket;


USTRUCT()
struct MODULARINVENTORY_API FInventoryItemRecord
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	int32 Generation = 1;

	UPROPERTY(SaveGame)
	bool bAlive = false;

	UPROPERTY(SaveGame)
	TObjectPtr<const UInventoryItemDefinition> Definition;

	UPROPERTY(SaveGame)
	int32 Count = 0;

	UPROPERTY(SaveGame)
	FInventoryContainerHandle Container;

	UPROPERTY(SaveGame)
	FInventoryPlacement Placement;

	/** Only the item's own tags; its definition's are added on lookup. */
	UPROPERTY(SaveGame)
	FGameplayTagContainer Tags;

	/** Only values that differ from the definition's. */
	UPROPERTY(SaveGame)
	TArray<FInventoryTagValue> TagValues;

	UPROPERTY(SaveGame)
	TArray<FInstancedStruct> Fragments;

	/** Created from the definition's container list, in the same order. */
	UPROPERTY(SaveGame)
	TArray<FInventoryContainerHandle> Containers;
};


/** Lets matching items stack higher in one container, such as from a stack size upgrade. */
USTRUCT(BlueprintType)
struct MODULARINVENTORY_API FInventoryStackBonus
{
	GENERATED_BODY()

	/** Which items stack higher. Empty matches every item that stacks; items that don't stack never do. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory")
	FInventoryItemQuery Items;

	/** How many more of each matching item one stack holds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory", meta = (ClampMin = "1"))
	int32 Amount = 0;
};


USTRUCT()
struct MODULARINVENTORY_API FInventoryContainerRecord
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	int32 Generation = 1;

	UPROPERTY(SaveGame)
	bool bAlive = false;

	UPROPERTY(SaveGame)
	TObjectPtr<const UInventoryContainerLayout> Layout;

	UPROPERTY(SaveGame)
	FGameplayTag Tag;

	/** The item carrying this container. Unset for root containers. */
	UPROPERTY(SaveGame)
	FInventoryItemHandle OwnerItem;

	/** Root containers only: whoever created it, usually an inventory component. */
	UPROPERTY()
	TWeakObjectPtr<UObject> Owner;

	/** List containers keep these in list order. */
	UPROPERTY(SaveGame)
	TArray<FInventoryItemHandle> Items;

	UPROPERTY(SaveGame)
	TArray<FInstancedStruct> Fragments;

	/**
	 * How much it can hold, when it has its own capacity rather than its layout's: from a Starting Capacity, or set at runtime with
	 * SetCapacity or AddCapacity, such as by a capacity upgrade. Usable cells for a grid, counted row by row from the top left; usable slots
	 * for slots, counted from the first; the most items for a list. INDEX_NONE follows the layout.
	 */
	UPROPERTY(SaveGame)
	int32 CapacityOverride = INDEX_NONE;

	/** How much higher items stack here than their definitions say, from stack size upgrades. They add up. */
	UPROPERTY(SaveGame)
	TArray<FInventoryStackBonus> StackBonuses;

	/**
	 * Grids: the item index on each cell, counted row by row from the top left. Slots: the item index in each slot. INDEX_NONE when empty.
	 * Rebuilt from the items' placements. A grid's cells past the end are empty: they're added, a whole row at a time, as items go there.
	 */
	UPROPERTY()
	TArray<int32> Cells;

	/** Grids: the layout's columns when Cells was built, so cells are never read with another count, such as after the layout changed. */
	UPROPERTY()
	int32 CellColumns = 0;
};


/** What a committed transaction touched, including anything it destroyed. */
struct MODULARINVENTORY_API FInventoryChangeSet
{
	bool IsEmpty() const { return Items.IsEmpty() && Containers.IsEmpty(); }
	void Append(const FInventoryChangeSet& Other);

	TSet<FInventoryItemHandle> Items;
	TSet<FInventoryContainerHandle> Containers;
};


DECLARE_MULTICAST_DELEGATE_OneParam(FOnInventoryRegistryChanged, const FInventoryChangeSet& /*Changes*/);


/**
 * Every container and item in a world, stored as flat records addressed by generation-checked handles.
 * Each change runs in a transaction: when an operation fails, everything it did is undone, and listeners only hear about committed changes.
 * Gameplay code normally goes through UInventorySubsystem, which wraps this for Blueprint.
 */
USTRUCT()
struct MODULARINVENTORY_API FInventoryRegistry
{
	GENERATED_BODY()

public:
	// Records
	bool IsValid(FInventoryItemHandle Item) const;
	bool IsValid(FInventoryContainerHandle Container) const;
	const FInventoryItemRecord* FindItem(FInventoryItemHandle Item) const;
	const FInventoryContainerRecord* FindContainer(FInventoryContainerHandle Container) const;
	int32 GetNumItems() const { return Items.Num() - FreeItems.Num(); }
	int32 GetNumContainers() const { return Containers.Num() - FreeContainers.Num(); }

	// Hierarchy
	FInventoryContainerHandle GetRootContainer(FInventoryContainerHandle Container) const;
	/** Whoever created the root container this one is nested in. */
	UObject* GetOwner(FInventoryContainerHandle Container) const;
	/** Whether Container is one of Item's containers, or nested anywhere inside them. */
	bool IsInsideItem(FInventoryContainerHandle Container, FInventoryItemHandle Item) const;
	FInventoryContainerHandle FindItemContainer(FInventoryItemHandle Item, const FGameplayTag& Tag) const;
	/** The roots and every container nested in their items, breadth first. */
	void CollectContainers(TConstArrayView<FInventoryContainerHandle> Roots, TArray<FInventoryContainerHandle>& OutContainers) const;
	/** The item covering a grid cell, or in a slot or list position. */
	FInventoryItemHandle GetItemAt(FInventoryContainerHandle Container, const FInventoryPlacement& Placement) const;

	// Item state
	/** For list containers, Index is the item's position in the list. */
	FInventoryPlacement GetItemPlacement(FInventoryItemHandle Item) const;
	FIntPoint GetItemSize(FInventoryItemHandle Item) const;
	/** The item's own tags plus its definition's. */
	FGameplayTagContainer GetItemTags(FInventoryItemHandle Item) const;
	/** The item's own value, else its definition's, else 0. */
	float GetTagValue(FInventoryItemHandle Item, const FGameplayTag& Tag) const;
	bool HasTagValue(FInventoryItemHandle Item, const FGameplayTag& Tag) const;
	/** The most Tag's value can be, such as full durability: MaxTag's effective value when the item has one, else the value the definition starts Tag at. 0 when neither. */
	float GetTagValueMax(FInventoryItemHandle Item, const FGameplayTag& Tag, const FGameplayTag& MaxTag) const;
	const FInstancedStruct* FindItemFragment(FInventoryItemHandle Item, const UScriptStruct* Type) const;
	const FInstancedStruct* FindContainerFragment(FInventoryContainerHandle Container, const UScriptStruct* Type) const;
	bool MatchesQuery(FInventoryItemHandle Item, const FInventoryItemQuery& Query) const;
	bool CanStack(FInventoryItemHandle Item, FInventoryItemHandle Other) const;

	// Sockets and attachments
	/** Tag value plus bonuses from the sockets of neighbouring items and from attachments. */
	float GetEffectiveTagValue(FInventoryItemHandle Item, const FGameplayTag& Tag) const;
	/** Item tags plus tags granted by the sockets of neighbouring items and by attachments. */
	FGameplayTagContainer GetEffectiveTags(FInventoryItemHandle Item) const;
	/** Items whose sockets currently affect Item. */
	TArray<FInventoryItemHandle> GetSocketSources(FInventoryItemHandle Item) const;
	/** The items in Item's equipment containers, such as a rifle's scope, and those attached to them in turn. */
	TArray<FInventoryItemHandle> GetAttachments(FInventoryItemHandle Item) const;

	// Search
	void FindItems(TConstArrayView<FInventoryContainerHandle> SearchRoots, const FInventoryItemQuery& Query, bool bRecursive, TArray<FInventoryItemHandle>& OutItems) const;
	/** Total stack count of matching items. */
	int32 CountItems(TConstArrayView<FInventoryContainerHandle> SearchRoots, const FInventoryItemQuery& Query, bool bRecursive) const;
	/** The sum of a tag value over the items, each times its count, such as the weight of everything carried. */
	float GetTotalTagValue(TConstArrayView<FInventoryContainerHandle> SearchRoots, const FGameplayTag& Tag, bool bRecursive) const;
	/**
	 * What an item adds to such a sum, as a carry limit counts it: its value of Tag times its count, and the sum over everything inside it.
	 * Takes a record so items that don't exist yet can be weighed.
	 */
	float GetCarriedValue(const FInventoryItemRecord& Item, const FGameplayTag& Tag) const;

	// Stacks
	/** How many of the item one stack holds where it is: its definition's Max Stack Size, and the stack bonuses of its container. */
	int32 GetMaxStackSize(FInventoryItemHandle Item) const;
	/**
	 * Lets items matching Item Query stack Amount higher in the container, such as for a stack size upgrade. Bonuses add up, and stay with
	 * the container. A stack bigger than a container allows, such as one moved from a container with a bonus, stays whole but takes no more.
	 */
	EInventoryError AddStackBonus(FInventoryContainerHandle Container, const FInventoryItemQuery& ItemQuery, int32 Amount);

	// Placement
	EInventoryError CanPlace(FInventoryItemHandle Item, FInventoryContainerHandle Container, const FInventoryPlacement& Placement) const;
	/** The first free spot, trying the item's current rotation first. */
	bool FindPlacement(FInventoryItemHandle Item, FInventoryContainerHandle Container, FInventoryPlacement& OutPlacement) const;
	/**
	 * Whether a placement is in the part of the container that can hold items now: an unlocked cell of a grid (the placement's cell
	 * only), an unlocked slot, or a position in a list below its capacity.
	 */
	bool IsPlacementUsable(FInventoryContainerHandle Container, const FInventoryPlacement& Placement) const;

	// Capacity: how much a container can hold now, which can grow during play as far as its layout allows.
	/**
	 * How much the container can hold now: usable cells for a grid, unlocked slots for slots, the most items for a list. INDEX_NONE for no
	 * limit, as for a grid that grows to fit without Max Rows. The container's own once it has one, from a Starting Capacity, SetCapacity
	 * or AddCapacity; else its layout's.
	 */
	int32 GetCapacity(FInventoryContainerHandle Container) const;
	/**
	 * The most the container can be unlocked to: every cell of its grid at its layout's Max Rows, or every slot. INDEX_NONE for a grid that
	 * grows without limit, or a list, which has no most.
	 */
	int32 GetMaxCapacity(FInventoryContainerHandle Container) const;
	/**
	 * How many rows a grid has now, as grid widgets draw them: its layout's Grid Size, and every row it has grown into, as its capacity
	 * rose or to fit its items. 0 for other types.
	 */
	int32 GetGridRows(FInventoryContainerHandle Container) const;
	/**
	 * Gives the container its own capacity, overriding its layout's: storage that grows as upgrades are found, slots earned during play, a
	 * satchel that holds more. Grids: Count cells can hold items, counted row by row from the top left; a grid growing With Upgrades gains
	 * the rows they reach into, up to Max Rows, and one growing To Fit Items grows to fit up to Count cells. Slots: the first Count slots
	 * can. Lists: at most Count items. Grids and slots stop at the most they can have. A negative Count follows the layout again. Fails
	 * without changing anything when an item sits where it would lock, or a list holds more than Count.
	 */
	EInventoryError SetCapacity(FInventoryContainerHandle Container, int32 Count);
	/**
	 * Unlocks Amount more of the container, such as 2 cells for a pouch, or locks that many again with a negative Amount, as SetCapacity
	 * does. Unlocking stops at the most the container can hold: when nothing is left to unlock, it fails with AtMaxCapacity, as it does for
	 * a list without a limit. Locking fails with InvalidCount when nothing is left to lock.
	 */
	EInventoryError AddCapacity(FInventoryContainerHandle Container, int32 Amount);

	// Containers
	/**
	 * A root container, owned by whoever created it. Starting Capacity gives it its own capacity from the start instead of its layout's,
	 * as SetCapacity would; INDEX_NONE follows the layout.
	 */
	FInventoryContainerHandle CreateContainer(const UInventoryContainerLayout* Layout, const FGameplayTag& Tag, UObject* Owner, int32 StartingCapacity = INDEX_NONE);
	/** Root containers only, along with everything inside them. Item containers live and die with their item. */
	EInventoryError DestroyContainer(FInventoryContainerHandle Container);
	/**
	 * Gives the container another layout of the same type, such as a bigger grid for an upgraded backpack. Items keep their places.
	 * Fails without changing anything when an item no longer fits or the new filter refuses it. A capacity set with SetCapacity goes back
	 * to following the new layout.
	 */
	EInventoryError SetContainerLayout(FInventoryContainerHandle Container, const UInventoryContainerLayout* Layout);
	/**
	 * Sorts a grid or list: merges partial stacks, then orders the items biggest first in a grid, then by name, so the same items sit
	 * together. A grid puts each on the first spot that fits. All or nothing.
	 */
	EInventoryError SortContainer(FInventoryContainerHandle Container);
	/** Sorts a grid or list as Settings say, such as by category, then by name. A grid puts each item on the first spot that fits. All or nothing. */
	EInventoryError SortContainer(FInventoryContainerHandle Container, const FInventorySortSettings& Settings);

	// Operations. An unset Placement means "find a free spot".
	/** Fills compatible stacks first, then free spots. With an explicit placement, adds a single stack there or onto the stack already there. */
	EInventoryError AddItem(const FInventoryItemSpec& Spec, FInventoryContainerHandle Container, const TOptional<FInventoryPlacement>& Placement = {}, TArray<FInventoryItemHandle>* OutItems = nullptr);
	/** Like AddItem across several containers in order. Returns how many were added; without bAllowPartial that is all or nothing. */
	int32 AddItemToContainers(const FInventoryItemSpec& Spec, TConstArrayView<FInventoryContainerHandle> TargetContainers, bool bAllowPartial, TArray<FInventoryItemHandle>* OutItems = nullptr, EInventoryError* OutError = nullptr);
	/** Count 0 or more than the stack destroys the item and everything inside it. Returns how many were removed. */
	int32 RemoveItem(FInventoryItemHandle Item, int32 Count = 0);
	/** Count 0 moves the whole stack; less splits it. Lands on a compatible stack at the placement, or tops up stacks before taking a free spot. */
	EInventoryError MoveItem(FInventoryItemHandle Item, FInventoryContainerHandle Container, const TOptional<FInventoryPlacement>& Placement = {}, int32 Count = 0);
	/**
	 * Like MoveItem without a placement, across several containers: tops up stacks in all of them, then takes the first free spot.
	 * With bAllowPartial, whatever fits moves and the rest stays in the source stack.
	 */
	EInventoryError MoveItemToContainers(FInventoryItemHandle Item, TConstArrayView<FInventoryContainerHandle> TargetContainers, int32 Count = 0, bool bAllowPartial = false);
	/** Each item takes the other's place, keeping its own rotation. */
	EInventoryError SwapItems(FInventoryItemHandle Item, FInventoryItemHandle Other);
	/** Moves Count into a new stack on a free spot. An unset Container means the item's own container. */
	EInventoryError SplitStack(FInventoryItemHandle Item, int32 Count, FInventoryContainerHandle Container = {}, const TOptional<FInventoryPlacement>& Placement = {}, FInventoryItemHandle* OutNewItem = nullptr);
	/** Moves as much as fits from Source onto Target. Count 0 tries the whole stack. */
	EInventoryError MergeStacks(FInventoryItemHandle Source, FInventoryItemHandle Target, int32 Count = 0, int32* OutMoved = nullptr);
	EInventoryError RotateItem(FInventoryItemHandle Item);
	/**
	 * Moves the item into a slot of an equipment container. With DisplaceTo, whatever keeps it out first makes way: the item in the slot,
	 * and items the equip rules keep apart from it, like an off-hand item for a two-handed weapon. Each tries the spot the item left,
	 * then the first free spot in DisplaceTo. All or nothing.
	 */
	EInventoryError EquipItem(FInventoryItemHandle Item, FInventoryContainerHandle EquipmentContainer, const FGameplayTag& Slot, TConstArrayView<FInventoryContainerHandle> DisplaceTo = {});
	/** Moves an item out of its equipment slot. */
	EInventoryError UnequipItem(FInventoryItemHandle Item, FInventoryContainerHandle Container, const TOptional<FInventoryPlacement>& Placement = {});

	EInventoryError SetTagValue(FInventoryItemHandle Item, const FGameplayTag& Tag, float Value);
	EInventoryError AddToTagValue(FInventoryItemHandle Item, const FGameplayTag& Tag, float Delta);
	/** Drops the item's own value, so it reads its definition's again. */
	EInventoryError RemoveTagValue(FInventoryItemHandle Item, const FGameplayTag& Tag);
	EInventoryError AddItemTag(FInventoryItemHandle Item, const FGameplayTag& Tag);
	EInventoryError RemoveItemTag(FInventoryItemHandle Item, const FGameplayTag& Tag);
	/** Replaces the fragment of the same type, or adds it. */
	EInventoryError SetItemFragment(FInventoryItemHandle Item, const FInstancedStruct& Fragment);
	EInventoryError RemoveItemFragment(FInventoryItemHandle Item, const UScriptStruct* Type);
	EInventoryError SetContainerFragment(FInventoryContainerHandle Container, const FInstancedStruct& Fragment);
	EInventoryError RemoveContainerFragment(FInventoryContainerHandle Container, const UScriptStruct* Type);

	// Crafting
	/** Checks availability and ingredients. Space for the outputs is only known once crafting. */
	EInventoryError CanCraft(const UInventoryRecipe& Recipe, TConstArrayView<FInventoryContainerHandle> Sources, const FGameplayTagContainer& ContextTags, int32 Times = 1) const;
	/** Takes ingredients from Sources and their nested containers, then adds the outputs to Outputs. All or nothing. */
	EInventoryError Craft(const UInventoryRecipe& Recipe, TConstArrayView<FInventoryContainerHandle> Sources, TConstArrayView<FInventoryContainerHandle> Outputs,
		const FGameplayTagContainer& ContextTags, int32 Times, FRandomStream& Stream, TArray<FInventoryItemHandle>* OutItems = nullptr);
	/** Like Craft, taking ingredients only from FromItems, such as the items a player chose to combine. */
	EInventoryError CraftFromItems(const UInventoryRecipe& Recipe, TConstArrayView<FInventoryItemHandle> FromItems, TConstArrayView<FInventoryContainerHandle> Outputs,
		const FGameplayTagContainer& ContextTags, int32 Times, FRandomStream& Stream, TArray<FInventoryItemHandle>* OutItems = nullptr);
	/** Whether the recipe can be made once from FromItems with every one of them used, as when combining exactly those items. */
	bool IsMadeFrom(const UInventoryRecipe& Recipe, TConstArrayView<FInventoryItemHandle> FromItems, const FGameplayTagContainer& ContextTags) const;

	// Batches
	/** Runs the operations in order. If one fails, all of them are undone. */
	FInventoryBatchResult ExecuteBatch(TConstArrayView<FInventoryOperation> Operations);
	EInventoryError ExecuteOperation(const FInventoryOperation& Operation);

	/**
	 * Runs Body as one atomic change: if it returns an error, everything it did is undone.
	 * Nests: an inner failure only undoes the inner part, and listeners hear once the outermost transaction commits.
	 */
	EInventoryError RunTransaction(TFunctionRef<EInventoryError()> Body);
	bool IsInTransaction() const { return !Transactions.IsEmpty(); }

	/** Runs Body and undoes it whatever happens, returning what it would have returned. For previews that must follow the real rules. */
	EInventoryError DryRun(TFunctionRef<EInventoryError()> Body);

public:
	FOnInventoryRegistryChanged OnChanged;

	/** Extra say on whether an item may enter a container, e.g. ability system requirements for equipment. */
	TFunction<EInventoryError(const FInventoryRegistry&, const FInventoryItemRecord&, FInventoryContainerHandle)> CanEnterHook;

private:
	struct FTransaction
	{
		int32 NumItems = 0;
		int32 NumContainers = 0;
		TArray<int32> FreeItems;
		TArray<int32> FreeContainers;
		/** Records as they were before this transaction first changed them. */
		TMap<int32, FInventoryItemRecord> ItemBackups;
		TMap<int32, FInventoryContainerRecord> ContainerBackups;
		FInventoryChangeSet Changes;
	};

	void BeginTransaction();
	void CommitTransaction();
	void RollbackTransaction();

	FInventoryItemHandle MakeHandle(int32 ItemIndex) const { return FInventoryItemHandle(ItemIndex, Items[ItemIndex].Generation); }
	FInventoryContainerHandle MakeContainerHandle(int32 ContainerIndex) const { return FInventoryContainerHandle(ContainerIndex, Containers[ContainerIndex].Generation); }

	// Every change to a record goes through these, so it can be undone and reported.
	FInventoryItemRecord& EditItem(int32 Index);
	FInventoryContainerRecord& EditContainer(int32 Index);
	int32 AllocateItem();
	int32 AllocateContainer();
	void FreeItem(int32 Index);
	void FreeContainer(int32 Index);

	FInventoryItemRecord MakeItemRecord(const FInventoryItemSpec& Spec) const;
	int32 CreateItem(FInventoryItemRecord&& Record);
	FInventoryContainerHandle CreateContainerRecord(const UInventoryContainerLayout* Layout, const FGameplayTag& Tag, UObject* Owner, FInventoryItemHandle OwnerItem, int32 StartingCapacity);
	void DestroyItemRecord(int32 Index);
	void DestroyContainerRecord(int32 Index);

	void AttachItem(int32 ItemIndex, FInventoryContainerHandle Container, const FInventoryPlacement& Placement);
	void DetachItem(int32 ItemIndex);
	void StampCells(int32 ContainerIndex, int32 ItemIndex, int32 Value);
	int32 SplitOff(int32 ItemIndex, int32 Count);
	EInventoryError PlaceWholeStack(int32 ItemIndex, FInventoryContainerHandle Container, const TOptional<FInventoryPlacement>& Placement);
	/** Pours the stack into compatible stacks in the container. Returns true once nothing is left of it. */
	bool TopUpStacks(int32 ItemIndex, FInventoryContainerHandle Container);
	/** For an item that had to make way: Spot if it fits there in its own rotation, else the first free spot in Targets. */
	EInventoryError PlaceDisplaced(int32 ItemIndex, FInventoryContainerHandle SpotContainer, FInventoryPlacement Spot, TConstArrayView<FInventoryContainerHandle> Targets);

	// Checks take a record so items that don't exist yet can be tested; ItemIndex is INDEX_NONE for those.
	// CanEnter covers the container as a whole, CanOccupy one spot in it.
	EInventoryError CanEnter(int32 ItemIndex, const FInventoryItemRecord& Item, FInventoryContainerHandle Container) const;
	EInventoryError CanOccupy(int32 ItemIndex, const FInventoryItemRecord& Item, FInventoryContainerHandle Container, const FInventoryPlacement& Placement) const;
	EInventoryError CanEquipInSlot(int32 ItemIndex, const FInventoryItemRecord& Item, const FInventoryContainerRecord& Container, int32 SlotIndex) const;
	/** Items that keep Item out of the slot: the one in it, and those whose equip rules or Item's keep them apart. */
	TArray<int32> FindEquipConflicts(int32 ItemIndex, const FInventoryItemRecord& Item, const FInventoryContainerRecord& Container, int32 SlotIndex) const;
	bool FindPlacementFor(int32 ItemIndex, const FInventoryItemRecord& Item, FInventoryContainerHandle Container, FInventoryPlacement& OutPlacement) const;
	int32 FindOccupant(const FInventoryContainerRecord& Container, const FInventoryPlacement& Placement) const;
	/** The container's own capacity, set at runtime, or else its layout's. INDEX_NONE for no limit. */
	static int32 GetRecordCapacity(const FInventoryContainerRecord& Container);
	static int32 GetRecordGridRows(const FInventoryContainerRecord& Container);
	/** A grid cell's index in a container's Cells, counted row by row from the top left. INDEX_NONE outside its columns or above it. */
	static int32 GetCellIndex(const UInventoryContainerLayout& Layout, const FIntPoint& Cell);
	/**
	 * Whether an item may cover a grid cell: inside the grid's columns, within its Max Rows, and unlocked, its index below the capacity.
	 * Rows a grid that grows to fit doesn't have yet count, as it adds them for the item.
	 */
	static bool IsRecordCellUsable(const FInventoryContainerRecord& Container, const FIntPoint& Cell);
	static bool IsRecordPlacementUsable(const FInventoryContainerRecord& Container, const FInventoryPlacement& Placement);

	bool RecordMatchesQuery(const FInventoryItemRecord& Item, const FInventoryItemQuery& Query) const;
	bool RecordsCanStack(const FInventoryItemRecord& Item, const FInventoryItemRecord& Other) const;
	/** How many of the item one stack holds in the container: its definition's Max Stack Size and the container's matching stack bonuses. */
	int32 GetRecordMaxStackSize(const FInventoryItemRecord& Item, FInventoryContainerHandle Container) const;
	FGameplayTagContainer GetRecordTags(const FInventoryItemRecord& Item) const;
	float GetRecordTagValue(const FInventoryItemRecord& Item, const FGameplayTag& Tag) const;
	void ForEachSocketAffecting(FInventoryItemHandle Item, TFunctionRef<void(FInventoryItemHandle Source, const FInventoryItemSocket& Socket)> Callback) const;
	void ForEachAttachment(FInventoryItemHandle Item, TFunctionRef<void(FInventoryItemHandle Attachment)> Callback) const;

	struct FIngredientUse
	{
		FInventoryItemHandle Item;
		int32 Count = 0;
		bool bConsume = true;
	};
	bool AllocateIngredients(const UInventoryRecipe& Recipe, TConstArrayView<FInventoryItemHandle> Candidates, int32 Times, TArray<FIngredientUse>& OutUses) const;

	void UpdateTraceCounters() const;

	UPROPERTY()
	TArray<FInventoryItemRecord> Items;

	UPROPERTY()
	TArray<FInventoryContainerRecord> Containers;

	TArray<int32> FreeItems;
	TArray<int32> FreeContainers;
	TArray<FTransaction> Transactions;
};

// Handles index into one registry, so copying it would only produce confusing duplicates.
template <>
struct TStructOpsTypeTraits<FInventoryRegistry> : public TStructOpsTypeTraitsBase2<FInventoryRegistry>
{
	enum
	{
		WithCopy = false
	};
};
