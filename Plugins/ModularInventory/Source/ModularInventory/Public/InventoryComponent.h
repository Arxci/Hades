#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"
#include "Engine/StreamableManager.h"
#include "GameplayTagContainer.h"

#include "InventoryTypes.h"

#include "InventoryComponent.generated.h"

class UInventoryComponent;
class UInventoryContainerLayout;
class UInventoryItemDefinition;
class UInventoryLootTable;
class UInventoryRecipe;
class UInventoryRecipeBook;
class UInventorySubsystem;
struct FInventoryCapacityUpgrade;
struct FInventoryChangeSet;
struct FInventoryItemRecord;
struct FInventoryRegistry;


/** Whether items an Inventory Component takes in can go into one of its containers. */
UENUM(BlueprintType)
enum class EInventoryNewItems : uint8
{
	/** Storage containers take new items and equipment slots don't, so picked-up items are never equipped by themselves. */
	Auto,
	/**
	 * New items can go into it, equipment slots included: an item whose equip rules allow a free slot goes into it, and is equipped, such as a
	 * single light source slot that the light the player finds goes straight into.
	 */
	Take,
	/** New items never go into it; only moving items into it puts them there, such as a stash the player fills by hand. */
	Skip
};


USTRUCT(BlueprintType)
struct MODULARINVENTORY_API FInventoryContainerConfig
{
	GENERATED_BODY()

	/** Tells this component's containers apart, e.g. Container.Inventory or Container.Equipment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	FGameplayTag Tag;

	/** Its shape and rules. For a grid that grows during play, give it the size it starts at and the Max Rows it can grow to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UInventoryContainerLayout> Layout;

	/**
	 * Whether items the component takes in, with Add Item, Take Item, Take All, loot and crafting, can go into it, in the order of the
	 * component's containers: Auto lets storage containers take them and not equipment slots; Take lets this one take them, equipment slots
	 * included, which equips what goes in; Skip never puts new items in it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	EInventoryNewItems NewItems = EInventoryNewItems::Auto;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (InlineEditConditionToggle))
	bool bOverrideStartingCapacity = false;

	/**
	 * How much this container holds when play begins, instead of its layout's: cells for a grid, counted row by row from the top left;
	 * slots for slots, from the first; items for a list. 0 starts it with nothing usable. Capacity upgrades and Set Capacity grow it from
	 * there, up to the most its layout allows, so one layout serves any number of starting sizes, such as a harder difficulty that starts
	 * with less room.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (EditCondition = "bOverrideStartingCapacity", ClampMin = "0"))
	int32 StartingCapacity = 0;
};


USTRUCT(BlueprintType)
struct MODULARINVENTORY_API FInventoryStartingEquipment
{
	GENERATED_BODY()

	/** A slot in one of the component's equipment containers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	FGameplayTag Slot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	FInventoryItemSpec Item;
};


/** Whether a total of a tag value across everything an Inventory Component holds, such as weight, limits what it can take. */
UENUM(BlueprintType)
enum class EInventoryCarryLimit : uint8
{
	/** Only the room in its containers limits what it takes. */
	None,
	/** Items that would take the total past Max Carried are turned away: by pickups, chests, the inventory UI and code alike. */
	Refuse,
	/** Items are always taken. Past Max Carried it's over its limit, for the game to slow its owner down. */
	AllowOver
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInventoryComponentChangedSignature, UInventoryComponent*, Inventory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FInventoryCapacityUpgradedSignature, UInventoryComponent*, Inventory, FGameplayTag, ContainerTag, const UInventoryItemDefinition*, Upgrade);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInventoryOverCarryLimitSignature, UInventoryComponent*, Inventory, bool, bIsOverCarryLimit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FInventoryItemUsedSignature, UInventoryComponent*, Inventory, FInventoryItemHandle, Item, const UInventoryItemDefinition*, Definition);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FInventoryItemDroppedSignature, UInventoryComponent*, Inventory, const UInventoryItemDefinition*, Definition, int32, Count, AActor*, DroppedActor);


/**
 * Gives an actor its own root containers: a player's inventory and equipment slots, a chest, a pickup lying in the world.
 * The items live in the world's inventory registry; this component creates the containers on begin play and destroys them on end play.
 *
 * This is the gameplay way in: give it items, take items into it, use upgrades, craft, all by container tag, with its rules applied, such
 * as capacity upgrades used as they're taken and its carry limit. The Inventory Subsystem works on any container or item by handle, for
 * what this doesn't cover, such as exact placements, chests' contents or tag values.
 */
UCLASS(Blueprintable, ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class MODULARINVENTORY_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	UFUNCTION(BlueprintPure, Category = "Inventory", meta = (DefaultToSelf = "Actor"))
	static UInventoryComponent* FindInventoryComponent(const AActor* Actor);

	// Containers
	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<FInventoryContainerHandle> GetContainers() const { return RootContainers; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	FInventoryContainerHandle GetContainer(FGameplayTag Tag) const;

	/**
	 * Where new items go, in order: root containers, then containers inside items, breadth first. Root containers go by their New Items, which
	 * by default skips equipment slots; containers inside items always skip them, but what equipped items carry is included, like a worn
	 * backpack's storage.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<FInventoryContainerHandle> GetStorageContainers() const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<FInventoryContainerHandle> GetEquipmentContainers() const;

	/** Whether new items can go into the container, one of this component's at any depth, as its New Items says. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool TakesNewItems(FInventoryContainerHandle Container) const;

	/**
	 * Where items leaving an equipment slot go, such as one unequipped, or one making way for another: the storage containers, in order,
	 * without equipment slots, so an item leaving one slot never lands in another by itself.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<FInventoryContainerHandle> GetStowContainers() const;

	/** Whether the container is one of this component's, or nested anywhere inside them. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool OwnsContainer(FInventoryContainerHandle Container) const;

	/** What begin play will add, for editor previews such as a pickup showing its item. */
	const TArray<FInventoryItemSpec>& GetStartingItems() const { return StartingItems; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool OwnsItem(FInventoryItemHandle Item) const;

	// Items. Taking in a capacity upgrade that's applied when taken grows its container and uses it up, instead of storing it.
	/** Adds to the storage containers, topping up stacks first. Returns how many were added; without Allow Partial that is all or nothing. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 AddItem(const FInventoryItemSpec& Spec, bool bAllowPartial, TArray<FInventoryItemHandle>& OutItems);

	/** Whether Add Item would succeed, without adding anything. With Allow Partial, whether any of it fits. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool CanAddItem(const FInventoryItemSpec& Spec, bool bAllowPartial) const;

	/**
	 * Creates Count of an item kind in storage: Add Item with nothing but a definition and a count. Returns how many were added; without
	 * Allow Partial that is all or nothing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 AddItemByDefinition(UInventoryItemDefinition* Definition, int32 Count = 1, bool bAllowPartial = false);

	/** Rolls the table and adds the results. With Allow Partial, whatever doesn't fit is discarded; otherwise all or nothing. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool AddLoot(const UInventoryLootTable* LootTable, const FGameplayTagContainer& ContextTags, bool bAllowPartial = true);

	/** Moves an item from anywhere into this component's storage, topping up stacks first. Count 0 moves the whole stack. With Allow Partial, what doesn't fit stays behind. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TakeItem(FInventoryItemHandle Item, int32 Count, bool bAllowPartial, EInventoryError& OutError);

	/** Whether Take Item would succeed, without moving anything, such as to grey out a pickup the player has no room for. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool CanTakeItem(FInventoryItemHandle Item, int32 Count, bool bAllowPartial) const;

	/** Moves an item into Target's storage. Count 0 moves the whole stack. With Allow Partial, what doesn't fit stays behind. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TransferItem(FInventoryItemHandle Item, UInventoryComponent* Target, int32 Count, bool bAllowPartial, EInventoryError& OutError);

	/** Takes as much as fits of everything in Container, such as a chest's, into storage. True when nothing is left behind. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TakeAll(FInventoryContainerHandle Container);

	/** Removes up to Count matching items from everything this component holds; 0 removes them all. Returns how many were removed. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 RemoveItems(const FInventoryItemQuery& Query, int32 Count = 0);

	/** Searches everything this component holds, equipment and nested containers included. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<FInventoryItemHandle> FindItems(const FInventoryItemQuery& Query) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 CountItems(const FInventoryItemQuery& Query) const;

	/** The sum of a tag value over everything this component holds, each times its count, such as the weight carried. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	float GetTotalTagValue(FGameplayTag Tag) const;

	// Capacity: how much one of this component's containers holds, by its tag. The Inventory Subsystem's versions take a container handle.
	/** How much the container holds now: usable cells for a grid, unlocked slots, or the most items for a list. -1 for no limit. 0 without that container. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Capacity")
	int32 GetContainerCapacity(FGameplayTag ContainerTag) const;

	/** The most the container can grow to. -1 for no limit. 0 without that container. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Capacity")
	int32 GetContainerMaxCapacity(FGameplayTag ContainerTag) const;

	/**
	 * Adds room to the container, such as two cells for a story reward, as a capacity upgrade does, or takes that much away with a negative
	 * Count. Fails with At Max Capacity when it can't grow any more.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Capacity")
	bool AddContainerCapacity(FGameplayTag ContainerTag, int32 Count, EInventoryError& OutError);

	/** Sets how much the container holds, as Add Container Capacity counts it. -1 goes back to its layout's. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Capacity")
	bool SetContainerCapacity(FGameplayTag ContainerTag, int32 Count, EInventoryError& OutError);

	/** Lets items matching Item Query stack Amount higher in the container, as a stack size upgrade does. Bonuses add up. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Capacity")
	bool AddContainerStackBonus(FGameplayTag ContainerTag, const FInventoryItemQuery& ItemQuery, int32 Amount, EInventoryError& OutError);

	// Capacity upgrades: items with an Inventory Capacity Upgrade fragment, such as a pouch, upgrade the container its Container Tag names.
	/**
	 * Applies the item's Inventory Capacity Upgrade to this component's container with its Container Tag, adding room or letting items
	 * stack higher, and uses one of the item up, wherever the item is: here, in a pickup, or in a chest. Upgrades with Apply When Taken
	 * apply as they're picked up, so this is for the others, such as from an item menu's Apply Capacity Upgrade action. Fails, changing
	 * nothing, when the item isn't a capacity upgrade (Invalid Fragment), this component has no container with its tag (Invalid
	 * Container), or the container can't grow any more (At Max Capacity).
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Capacity")
	bool ApplyCapacityUpgrade(FInventoryItemHandle Item, EInventoryError& OutError);

	/** Whether Apply Capacity Upgrade would work for the item now, such as to offer it in an item menu. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Capacity")
	bool CanApplyCapacityUpgrade(FInventoryItemHandle Item) const;

	// Carry limit: a total of a tag value, such as weight, across everything this component holds. Off unless Carry Limit is set.
	/** The total of Carried Value Tag across everything it holds, equipped items and what's inside other items included. 0 without a carry limit. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Carry Limit")
	float GetCarried() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Carry Limit")
	float GetMaxCarried() const { return MaxCarried; }

	/** Changes Max Carried, such as for a strength bonus or a better backpack. Lowering it never drops anything. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Carry Limit")
	void SetMaxCarried(float NewMaxCarried);

	/** How much more it can carry before it's over its limit. A very large number without a carry limit. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Carry Limit")
	float GetCarryRoom() const;

	/** Whether it carries more than Max Carried: with Allow Over, or after Max Carried was lowered. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Carry Limit")
	bool IsOverCarryLimit() const;

	/** For the inventory registry's entry check: Over Carry Limit when Item entering one of this component's containers would break a Refuse carry limit. */
	EInventoryError CheckCarryLimit(const FInventoryRegistry& Registry, const FInventoryItemRecord& Item) const;

	// Using: items with an Inventory Item Use fragment, such as a herb that heals.
	/** Whether Use Item would work now: the item is held here, it has an Inventory Item Use, there are enough of it, and the owner meets its requirements. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Use")
	virtual bool CanUseItem(FInventoryItemHandle Item) const;

	/**
	 * Uses the item as its Inventory Item Use says: applies its Effect to the owner, sends the owner its Event Tag, and uses up Count Used of
	 * the stack. Then On Item Used fires, for whatever else using it does in your game.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Use")
	virtual bool UseItem(FInventoryItemHandle Item);

	// Dropping: taking items out of the inventory into the world, or away for good.
	/** Whether Drop Item would work now: the item is held here and has none of Undroppable Tags. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Drop")
	virtual bool CanDropItem(FInventoryItemHandle Item) const;

	/**
	 * Takes Count of the item out of the inventory, 0 for the whole stack, into a Dropped Item Class actor spawned in front of the owner, such
	 * as a pickup, so it can be picked up again. Without a Dropped Item Class, it's destroyed. False, changing nothing, when it can't drop
	 * the item, or the dropped actor has no room for it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Drop")
	virtual bool DropItem(FInventoryItemHandle Item, int32 Count = 0);

	// Crafting. Ingredients come from storage, and the owner's ability system tags join the context tags.
	UFUNCTION(BlueprintPure, Category = "Inventory|Crafting")
	bool CanCraft(const UInventoryRecipe* Recipe, const FGameplayTagContainer& ContextTags, int32 Times = 1) const;

	/** All or nothing: if the results don't fit, the ingredients are kept. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Crafting")
	bool Craft(const UInventoryRecipe* Recipe, const FGameplayTagContainer& ContextTags, int32 Times, TArray<FInventoryItemHandle>& OutItems, EInventoryError& OutError);

	UFUNCTION(BlueprintPure, Category = "Inventory|Crafting")
	TArray<UInventoryRecipe*> GetCraftableRecipes(const UInventoryRecipeBook* Book, const FGameplayTagContainer& ContextTags) const;

	/** The recipes in Book with an ingredient this item can be, such as to offer Combine for it, or to show what it makes. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Crafting")
	TArray<UInventoryRecipe*> GetRecipesUsing(const UInventoryRecipeBook* Book, FInventoryItemHandle Item) const;

	/** The first recipe in Book made from exactly these items held here, every one of them used, such as two herbs combined. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Crafting")
	UInventoryRecipe* FindRecipeMadeFrom(const UInventoryRecipeBook* Book, const TArray<FInventoryItemHandle>& Items, const FGameplayTagContainer& ContextTags) const;

	/** Crafts once taking ingredients only from these items held here, such as the ones a player chose to combine. The results go to storage. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Crafting")
	bool CraftFromItems(const UInventoryRecipe* Recipe, const TArray<FInventoryItemHandle>& Items, const FGameplayTagContainer& ContextTags, TArray<FInventoryItemHandle>& OutItems, EInventoryError& OutError);

	/**
	 * Craft, taking ingredients from Preferred Items first, such as the item a player picked a recipe for, then from the rest of storage,
	 * so the stack the player meant is the one used.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Crafting")
	bool CraftPreferring(const UInventoryRecipe* Recipe, const TArray<FInventoryItemHandle>& PreferredItems, const FGameplayTagContainer& ContextTags, int32 Times,
		TArray<FInventoryItemHandle>& OutItems, EInventoryError& OutError);

	/**
	 * Why crafting the recipe Times times would fail now, or None when it would work: Requirements Not Met, Missing Ingredients, or No Space
	 * for what it makes. Unlike Can Craft, it tries the craft and undoes it, so room for the results is checked too, and it never
	 * disagrees with Craft.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|Crafting")
	EInventoryError GetCraftError(const UInventoryRecipe* Recipe, const FGameplayTagContainer& ContextTags, int32 Times = 1) const;

	/** How many times the recipe can be crafted at once now, as Get Craft Error sees it, up to Max Times, such as for a crafting menu. 0 when it can't be. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Crafting")
	int32 GetCraftableTimes(const UInventoryRecipe* Recipe, const FGameplayTagContainer& ContextTags, int32 MaxTimes = 99) const;

	/** Fires once per committed change touching any of this component's containers, nested ones included. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FInventoryComponentChangedSignature OnInventoryChanged;

	/**
	 * A capacity upgrade grew one of this component's containers, once for each one used up: with Apply Capacity Upgrade, or as it was
	 * taken. After On Inventory Changed, so widgets already show the new room. Upgrade is the item's definition, such as for its name in a
	 * message.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Capacity")
	FInventoryCapacityUpgradedSignature OnCapacityUpgraded;

	/** It went over its carry limit, or back under it, such as to slow its owner down while it's over. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Carry Limit")
	FInventoryOverCarryLimitSignature OnOverCarryLimitChanged;

	/** Use Item used an item. Item is no longer valid when that used up the stack; Definition says what it was. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Use")
	FInventoryItemUsedSignature OnItemUsed;

	/** Drop Item took Count of an item out of the inventory, into Dropped Actor, or destroyed without one. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Drop")
	FInventoryItemDroppedSignature OnItemDropped;

protected:
	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

	/** The root containers, created on begin play. Their order is the order new items go into them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (TitleProperty = "Tag"))
	TArray<FInventoryContainerConfig> Containers;

	/**
	 * Keeps the cosmetics of everything held here loaded, so menus and equipping never wait on them.
	 * Turn it on for the player; chests and pickups are better off loading on demand.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	bool bPreloadCosmetics = false;

	/** Added on begin play. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Starting Items")
	TArray<FInventoryItemSpec> StartingItems;

	/** Placed straight into equipment slots on begin play. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Starting Items", meta = (TitleProperty = "Slot"))
	TArray<FInventoryStartingEquipment> StartingEquipment;

	/** Rolled and added on begin play, e.g. what a drawer holds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Starting Items")
	TObjectPtr<UInventoryLootTable> StartingLoot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Starting Items")
	FGameplayTagContainer StartingLootContext;

	/**
	 * Whether a total of Carried Value Tag across everything it holds, such as weight, limits what it takes: not at all, by refusing
	 * what would go past Max Carried, or by taking it and being over its limit.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Carry Limit")
	EInventoryCarryLimit CarryLimit = EInventoryCarryLimit::None;

	/** The tag value that adds up, such as Item.Stat.Weight: each item's value times its count, and everything inside it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Carry Limit", meta = (EditCondition = "CarryLimit != EInventoryCarryLimit::None", EditConditionHides))
	FGameplayTag CarriedValueTag;

	/** The most it can carry before it's over its limit. Set Max Carried changes it at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Carry Limit", meta = (ClampMin = "0", EditCondition = "CarryLimit != EInventoryCarryLimit::None", EditConditionHides))
	float MaxCarried = 100.0f;

	/**
	 * Spawned to hold what Drop Item drops, such as a pickup the player can take it back from. It needs an Inventory Component with room
	 * for the item, and no starting items of its own. Empty destroys what's dropped.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Drop")
	TSubclassOf<AActor> DroppedItemClass;

	/** How far in front of the owner a dropped item lands, on the floor below. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Drop", meta = (ClampMin = "0", Units = "cm"))
	float DropDistance = 60.0f;

	/** Items with any of these tags can't be dropped, such as Item.Trait.KeyItem. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Drop")
	FGameplayTagContainer UndroppableTags;

	/** Where a dropped item lands: Drop Distance in front of the owner, on the floor below, facing the way the owner does. */
	virtual FTransform GetDropTransform() const;

private:
	/** Capacity upgrades used up by one call, to announce once its change is made. */
	struct FAppliedUpgrade
	{
		TWeakObjectPtr<const UInventoryItemDefinition> Definition;
		FGameplayTag ContainerTag;
		int32 Times = 0;
	};

	UInventorySubsystem* GetSubsystem() const { return Subsystem.Get(); }
	FGameplayTagContainer GatherContextTags(const FGameplayTagContainer& ExtraTags) const;

	/** Takes New Items for a container the registry has: its root config's New Items, or for a container inside an item, whether it's storage. */
	bool TakesNewItemsIn(const FInventoryRegistry& Registry, FInventoryContainerHandle Container) const;

	/** Every item in storage a recipe can take ingredients from, those of Preferred Items among them first, in their order. */
	TArray<FInventoryItemHandle> GatherCraftCandidates(TConstArrayView<FInventoryItemHandle> PreferredItems) const;
	void HandleRegistryChanged(const FInventoryChangeSet& Changes);
	void RefreshCosmeticsPreload();

	/** The capacity upgrade of an item kind this component applies as it's taken, and the container of this component it grows. */
	const FInventoryCapacityUpgrade* FindUpgradeAppliedWhenTaken(const UInventoryItemDefinition* Definition, FInventoryContainerHandle& OutContainer) const;

	/**
	 * Grows Container by Upgrade Times times. With bAllowPartial, as many times as it can, failing only when it can't once; otherwise all or
	 * nothing. Call it inside a transaction. Returns how many times it applied.
	 */
	static int32 ApplyUpgrade(FInventoryRegistry& Registry, const FInventoryCapacityUpgrade& Upgrade, FInventoryContainerHandle Container, int32 Times,
		bool bAllowPartial, EInventoryError& OutError);

	/** Add Item into Storage, inside the caller's transaction: a capacity upgrade applied when taken is applied instead. Returns how many were added or applied. */
	int32 AddToStorage(FInventoryRegistry& Registry, const FInventoryItemSpec& Spec, TConstArrayView<FInventoryContainerHandle> Storage, bool bAllowPartial,
		TArray<FInventoryItemHandle>* OutItems, EInventoryError& OutError, TArray<FAppliedUpgrade>& OutApplied) const;

	/** Take Item into Storage, inside the caller's transaction: a capacity upgrade applied when taken is applied, and used up, instead. */
	EInventoryError TakeIntoStorage(FInventoryRegistry& Registry, FInventoryItemHandle Item, int32 Count, bool bAllowPartial,
		TConstArrayView<FInventoryContainerHandle> Storage, TArray<FAppliedUpgrade>& OutApplied) const;

	/** Fires On Capacity Upgraded for each upgrade a committed call used up. */
	void AnnounceUpgrades(const TArray<FAppliedUpgrade>& Applied);

	bool HasCarryLimit() const { return CarryLimit != EInventoryCarryLimit::None && CarriedValueTag.IsValid(); }

	/**
	 * How many of Wanted, each adding Value Per One, a Refuse carry limit lets it take in, so a partial pickup takes what it can carry.
	 * Wanted when nothing limits it.
	 */
	int32 GetCountItCanCarry(float ValuePerOne, int32 Wanted) const;

	/** Fires On Over Carry Limit Changed when it went over its carry limit or back under it. */
	void RefreshCarryLimitState();

	TArray<FInventoryContainerHandle> RootContainers;

	/** Each root container's New Items, in the same order. */
	TArray<EInventoryNewItems> RootNewItems;

	TWeakObjectPtr<UInventorySubsystem> Subsystem;
	FDelegateHandle RegistryChangedHandle;

	TSet<FSoftObjectPath> PreloadedCosmetics;
	TSharedPtr<FStreamableHandle> CosmeticsHandle;

	bool bWasOverCarryLimit = false;
};
