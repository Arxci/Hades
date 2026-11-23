#pragma once

#include "CoreMinimal.h"

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

#include "InventoryTypes.h"

#include "InventoryContainerLayout.generated.h"


UENUM(BlueprintType)
enum class EInventoryContainerType : uint8
{
	/** Items take Width x Height cells and may rotate. */
	Grid,
	/** A fixed set of tagged slots holding one item each. */
	Slots,
	/** An ordered list with an optional capacity. Item sizes are ignored. */
	List
};


/** How a grid grows past its Grid Size. */
UENUM(BlueprintType)
enum class EInventoryGridGrowth : uint8
{
	/** It stays at Grid Size. An item that doesn't fit is turned away. */
	None,
	/**
	 * Capacity upgrades add cells to it, such as a pouch that adds two, and an item that doesn't fit is turned away. When added cells
	 * start a new row, the whole row is shown, with the cells not added yet disabled, until more upgrades add them too.
	 */
	WithUpgrades,
	/**
	 * It adds rows whenever an item doesn't fit, so it never turns an item away, and loses empty rows at the bottom again, down to Grid
	 * Size. Grid widgets draw a free row below the last for the player to put items in.
	 */
	ToFitItems
};


/**
 * What the player may do to a container's items through the inventory UI. Only the UI follows it: code, such as a pickup, a quest
 * script or a crafting station, still adds, moves and removes items freely.
 */
UENUM(BlueprintType)
enum class EInventoryPlayerAccess : uint8
{
	/** Items can be taken out, put in and moved around. */
	Full,
	/** Items can be taken out and moved around, but nothing can be put in, such as a corpse's loot or a shop's stock. */
	TakeOnly,
	/** Items can be put in, but not taken back out or moved, such as a donation box, a quest turn-in or an offering. */
	PutOnly,
	/**
	 * Items are shown and their item menus open, but nothing can be picked up, moved, taken out or put in, such as key items that come
	 * and go by themselves.
	 */
	ReadOnly
};


USTRUCT(BlueprintType)
struct MODULARINVENTORY_API FInventorySlotDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slot")
	FGameplayTag Tag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slot")
	FText DisplayName;

	/** On top of the container's filter. Empty accepts any item. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slot")
	FInventoryItemQuery Filter;
};


/**
 * The shape and rules of a container. Shared by every container built from it, on actors or inside items.
 *
 * A grid starts at Grid Size, and Grid Growth decides whether it grows past it: as capacity upgrades add cells, or to fit whatever is put
 * in it, up to Max Rows or without limit. A slot layout lists every slot it will ever have, and Usable Slots says how many start
 * unlocked. A list holds up to its Capacity. Capacity upgrades and Set Capacity change how much a container holds at runtime, and a
 * Starting Capacity where the layout is used can start one container with less.
 */
UCLASS(BlueprintType, Const)
class MODULARINVENTORY_API UInventoryContainerLayout : public UDataAsset
{
	GENERATED_BODY()

public:
	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface

	/** INDEX_NONE when the layout has no such slot. */
	int32 FindSlot(const FGameplayTag& SlotTag) const;
	int32 GetSlotCount() const { return Type == EInventoryContainerType::Slots ? Slots.Num() : 0; }

	/** Every cell of Grid Size. 0 for other types. */
	int32 GetCellCount() const { return Type == EInventoryContainerType::Grid ? FMath::Max(0, GridSize.X) * FMath::Max(0, GridSize.Y) : 0; }

	/** Whether a grid built from it can grow past Grid Size. */
	bool CanGridGrow() const { return Type == EInventoryContainerType::Grid && GridGrowth != EInventoryGridGrowth::None; }

	/** The most rows a grid built from it can have: Grid Size's, or Max Rows when it grows. INDEX_NONE when it grows without limit. 0 for other types. */
	int32 GetMaxRows() const;

	/**
	 * How many rows a grid that grows with upgrades, or not at all, has at a capacity: Grid Size's rows, and as many more as the capacity
	 * reaches into, up to Max Rows. A grid that grows to fit has the rows its items reach instead; the registry's GetGridRows tells a
	 * container's rows whatever it does. 0 for other types.
	 */
	int32 GetGridRowsAtCapacity(int32 AtCapacity) const;

	/**
	 * The most a container built from it can hold: every cell of a grid at Max Rows, or every slot. INDEX_NONE for a grid that grows
	 * without limit, or a list, which can always be given a bigger capacity.
	 */
	int32 GetMaxCapacity() const;

	/**
	 * What each container built from it can hold to start with: every cell of Grid Size, or as much as Max Rows allows for a grid that
	 * grows to fit; Usable Slots for slots, or else every slot; and Capacity for a list. INDEX_NONE for no limit.
	 */
	int32 GetStartingCapacity() const;

	/** A capacity as a container with this layout holds it: no more than Get Max Capacity, and INDEX_NONE for a negative count. */
	int32 ClampCapacity(int32 Count) const;

	/** Whether the inventory UI lets the player take items out of containers built from it, and move them around inside. */
	bool CanPlayerTakeOut() const { return PlayerAccess == EInventoryPlayerAccess::Full || PlayerAccess == EInventoryPlayerAccess::TakeOnly; }

	/** Whether the inventory UI lets the player put items into containers built from it. */
	bool CanPlayerPutIn() const { return PlayerAccess == EInventoryPlayerAccess::Full || PlayerAccess == EInventoryPlayerAccess::PutOnly; }

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout")
	EInventoryContainerType Type = EInventoryContainerType::Grid;

	/** Columns x rows: the grid each container starts with. Every cell of it is usable. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "1", EditCondition = "Type == EInventoryContainerType::Grid", EditConditionHides))
	FIntPoint GridSize = FIntPoint(4, 4);

	/**
	 * Whether the grid grows past Grid Size: None; With Upgrades, as capacity upgrades add cells, turning away what doesn't fit; or To Fit
	 * Items, adding rows whenever an item doesn't fit. With Upgrades, a 4 x 2 grid given 2 more cells shows a third row: two of its cells
	 * usable, the other two disabled until more are added.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout", meta = (EditCondition = "Type == EInventoryContainerType::Grid", EditConditionHides))
	EInventoryGridGrowth GridGrowth = EInventoryGridGrowth::None;

	/** The most rows the grid can grow to. 0 for no limit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0", EditCondition = "Type == EInventoryContainerType::Grid && GridGrowth != EInventoryGridGrowth::None", EditConditionHides))
	int32 MaxRows = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout", meta = (TitleProperty = "Tag", EditCondition = "Type == EInventoryContainerType::Slots", EditConditionHides))
	TArray<FInventorySlotDefinition> Slots;

	/**
	 * How many slots each container starts with unlocked, counted from the first. The rest are locked: nothing can go in them, and slot
	 * widgets draw them disabled. 0 unlocks every slot. For slots earned during play, such as a second ring or a fourth quick slot.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0", EditCondition = "Type == EInventoryContainerType::Slots", EditConditionHides))
	int32 UsableSlots = 0;

	/** The most items each container starts able to hold. 0 for no limit. A list's capacity can grow past it at runtime. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0", EditCondition = "Type == EInventoryContainerType::List", EditConditionHides))
	int32 Capacity = 0;

	/**
	 * Slots only accept items whose equip rules allow the slot's tag, and an equipment component equips what they hold.
	 * Inside an item, e.g. a weapon's attachment slots, the contents are equipped while that item is.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout", meta = (EditCondition = "Type == EInventoryContainerType::Slots", EditConditionHides))
	bool bEquipment = false;

	/** Every item must match. Empty accepts any item. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules")
	FInventoryItemQuery Filter;

	/**
	 * What the player may do to the items through the inventory UI: anything, only take them out, only put them in, or only look at them.
	 * Code is never limited by it.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules")
	EInventoryPlayerAccess PlayerAccess = EInventoryPlayerAccess::Full;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer Tags;

	/** Static data shared by every container with this layout. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fragments")
	TArray<FInstancedStruct> Fragments;

	/** Copied onto each new container as its own state. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fragments")
	TArray<FInstancedStruct> InstanceFragments;

private:
#if WITH_EDITORONLY_DATA
	/** Replaced by Player Access. Loaded only so PostLoad can carry a layout's setting over. */
	UPROPERTY()
	bool bReadOnlyInUI_DEPRECATED = false;
#endif
};
