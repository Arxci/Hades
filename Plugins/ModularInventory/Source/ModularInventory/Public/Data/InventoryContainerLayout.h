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


/** The shape and rules of a container. Shared by every container built from it, on actors or inside items. */
UCLASS(BlueprintType, Const)
class MODULARINVENTORY_API UInventoryContainerLayout : public UDataAsset
{
	GENERATED_BODY()

public:
	/** INDEX_NONE when the layout has no such slot. */
	int32 FindSlot(const FGameplayTag& SlotTag) const;
	int32 GetSlotCount() const { return Type == EInventoryContainerType::Slots ? Slots.Num() : 0; }
	int32 GetCellCount() const { return Type == EInventoryContainerType::Grid ? GridSize.X * GridSize.Y : 0; }

	/** Whether a grid cell is inside the grid and not locked by Usable Cells. */
	bool IsCellUsable(const FIntPoint& Cell) const;

	/** How many of the grid's cells can hold items. */
	int32 GetUsableCellCount() const;

	/** How many of the slots can hold items, counted from the first. */
	int32 GetUsableSlotCount() const;

	/**
	 * What each container built from it can hold to start with: usable cells for a grid, usable slots for slots, and Capacity for a list,
	 * INDEX_NONE for a list without a limit. The Inventory Subsystem's Set Capacity changes it for one container at runtime.
	 */
	int32 GetStartingCapacity() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout")
	EInventoryContainerType Type = EInventoryContainerType::Grid;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "1", EditCondition = "Type == EInventoryContainerType::Grid", EditConditionHides))
	FIntPoint GridSize = FIntPoint(4, 4);

	/**
	 * How many cells can hold items, counted row by row from the top left. The rest are locked: nothing can go in them, and grid
	 * widgets draw them disabled. 0 makes every cell usable. For storage that grows a few cells at a time: a 4 x 3 grid with 10 usable
	 * cells has its last two cells locked. This is where each container starts: the Inventory Subsystem's Set Capacity and Add Capacity
	 * unlock more of one container's cells at runtime, so one layout at the biggest size serves every stage of an upgrade path.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0", EditCondition = "Type == EInventoryContainerType::Grid", EditConditionHides))
	int32 UsableCells = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout", meta = (TitleProperty = "Tag", EditCondition = "Type == EInventoryContainerType::Slots", EditConditionHides))
	TArray<FInventorySlotDefinition> Slots;

	/**
	 * How many slots can hold items, counted from the first. The rest are locked: nothing can go in them, and slot widgets draw them
	 * disabled. 0 makes every slot usable. For slots earned during play, such as a second ring or a fourth quick slot. This is where each
	 * container starts: the Inventory Subsystem's Set Capacity and Add Capacity unlock more of one container's slots at runtime.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0", EditCondition = "Type == EInventoryContainerType::Slots", EditConditionHides))
	int32 UsableSlots = 0;

	/**
	 * The most items it holds. 0 for no limit. This is where each container starts: the Inventory Subsystem's Set Capacity and Add Capacity
	 * change it for one container at runtime, such as a satchel that holds more once upgraded.
	 */
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

	/** The inventory UI shows these items but can't pick them up or put anything here. Code still can, as with quest items that come and go by themselves. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules", meta = (DisplayName = "Read Only In UI"))
	bool bReadOnlyInUI = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer Tags;

	/** Static data shared by every container with this layout. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fragments")
	TArray<FInstancedStruct> Fragments;

	/** Copied onto each new container as its own state. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fragments")
	TArray<FInstancedStruct> InstanceFragments;
};
