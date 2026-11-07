#pragma once

#include "CoreMinimal.h"

#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

#include "InventoryTypes.generated.h"

class UInventoryItemDefinition;


namespace InventoryTags
{
	/** Unlike FGameplayTagQuery::Matches, an empty query passes. */
	inline bool PassesQuery(const FGameplayTagQuery& Query, const FGameplayTagContainer& Tags)
	{
		return Query.IsEmpty() || Query.Matches(Tags);
	}
}


/** Refers to an item in the registry. Goes stale once the item is destroyed, even if its slot is reused. */
USTRUCT(BlueprintType)
struct MODULARINVENTORY_API FInventoryItemHandle
{
	GENERATED_BODY()

	FInventoryItemHandle() = default;
	FInventoryItemHandle(int32 InIndex, int32 InGeneration) : Index(InIndex), Generation(InGeneration) {}

	/** Only whether the handle was ever set. Ask the registry or subsystem whether the item still exists. */
	bool IsSet() const { return Index != INDEX_NONE; }
	void Reset() { *this = FInventoryItemHandle(); }
	FString ToString() const { return FString::Printf(TEXT("Item(%d.%d)"), Index, Generation); }

	bool operator==(const FInventoryItemHandle& Other) const { return Index == Other.Index && Generation == Other.Generation; }
	bool operator!=(const FInventoryItemHandle& Other) const { return !(*this == Other); }
	friend uint32 GetTypeHash(const FInventoryItemHandle& Handle) { return HashCombine(::GetTypeHash(Handle.Index), ::GetTypeHash(Handle.Generation)); }

	UPROPERTY(SaveGame)
	int32 Index = INDEX_NONE;

	UPROPERTY(SaveGame)
	int32 Generation = 0;
};


/** Refers to a container in the registry. Goes stale once the container is destroyed, even if its slot is reused. */
USTRUCT(BlueprintType)
struct MODULARINVENTORY_API FInventoryContainerHandle
{
	GENERATED_BODY()

	FInventoryContainerHandle() = default;
	FInventoryContainerHandle(int32 InIndex, int32 InGeneration) : Index(InIndex), Generation(InGeneration) {}

	/** Only whether the handle was ever set. Ask the registry or subsystem whether the container still exists. */
	bool IsSet() const { return Index != INDEX_NONE; }
	void Reset() { *this = FInventoryContainerHandle(); }
	FString ToString() const { return FString::Printf(TEXT("Container(%d.%d)"), Index, Generation); }

	bool operator==(const FInventoryContainerHandle& Other) const { return Index == Other.Index && Generation == Other.Generation; }
	bool operator!=(const FInventoryContainerHandle& Other) const { return !(*this == Other); }
	friend uint32 GetTypeHash(const FInventoryContainerHandle& Handle) { return HashCombine(::GetTypeHash(Handle.Index), ::GetTypeHash(Handle.Generation)); }

	UPROPERTY(SaveGame)
	int32 Index = INDEX_NONE;

	UPROPERTY(SaveGame)
	int32 Generation = 0;
};


/** Where an item sits in its container. Which fields matter depends on the container type. */
USTRUCT(BlueprintType)
struct MODULARINVENTORY_API FInventoryPlacement
{
	GENERATED_BODY()

	static FInventoryPlacement AtCell(FIntPoint InCell, bool bInRotated = false);
	static FInventoryPlacement AtIndex(int32 InIndex);

	bool operator==(const FInventoryPlacement& Other) const { return Cell == Other.Cell && Index == Other.Index && bRotated == Other.bRotated; }
	bool operator!=(const FInventoryPlacement& Other) const { return !(*this == Other); }

	/** Top-left cell in grid containers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory")
	FIntPoint Cell = FIntPoint::ZeroValue;

	/** Slot index in slot containers. In list containers, the position to insert at; past the end appends. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory")
	int32 Index = 0;

	/** Turned 90 degrees clockwise. Grid containers only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory")
	bool bRotated = false;
};


USTRUCT(BlueprintType)
struct MODULARINVENTORY_API FInventoryTagValue
{
	GENERATED_BODY()

	FInventoryTagValue() = default;
	FInventoryTagValue(FGameplayTag InTag, float InValue) : Tag(InTag), Value(InValue) {}

	bool operator==(const FInventoryTagValue& Other) const { return Tag == Other.Tag && Value == Other.Value; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory")
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory")
	float Value = 0.0f;
};


UENUM(BlueprintType)
enum class EInventoryComparison : uint8
{
	Equal,
	NotEqual,
	Less,
	LessOrEqual,
	Greater,
	GreaterOrEqual
};


USTRUCT(BlueprintType)
struct MODULARINVENTORY_API FInventoryTagValueCondition
{
	GENERATED_BODY()

	bool Test(float InValue) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	EInventoryComparison Comparison = EInventoryComparison::GreaterOrEqual;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	float Value = 0.0f;
};


/**
 * Describes which items something accepts: container and slot filters, recipe ingredients, socket targets, searches.
 * Tests the item's own tags and tag values, without socket bonuses, so placing an item never changes what its neighbours accept.
 */
USTRUCT(BlueprintType)
struct MODULARINVENTORY_API FInventoryItemQuery
{
	GENERATED_BODY()

	bool IsEmpty() const { return Definitions.IsEmpty() && TagQuery.IsEmpty() && ValueConditions.IsEmpty(); }

	/** The item must be one of these. Empty allows any definition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TArray<TObjectPtr<UInventoryItemDefinition>> Definitions;

	/** Tested against the item's tags, its definition's included. Empty allows any tags. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	FGameplayTagQuery TagQuery;

	/** Every condition must pass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (TitleProperty = "Tag"))
	TArray<FInventoryTagValueCondition> ValueConditions;
};


/** Everything needed to create an item. */
USTRUCT(BlueprintType)
struct MODULARINVENTORY_API FInventoryItemSpec
{
	GENERATED_BODY()

	FInventoryItemSpec() = default;
	FInventoryItemSpec(UInventoryItemDefinition* InDefinition, int32 InCount = 1) : Definition(InDefinition), Count(InCount) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TObjectPtr<UInventoryItemDefinition> Definition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (ClampMin = "1"))
	int32 Count = 1;

	/** Set on the item, replacing the definition's defaults. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (TitleProperty = "Tag"))
	TArray<FInventoryTagValue> TagValues;

	/** Added to the item's own tags. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	FGameplayTagContainer Tags;

	/** Replaces the definition's instance fragment of the same type, or adds a new one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TArray<FInstancedStruct> Fragments;
};


UENUM(BlueprintType)
enum class EInventoryError : uint8
{
	None,
	InvalidItem,
	InvalidContainer,
	InvalidDefinition,
	InvalidCount,
	InvalidTag,
	InvalidFragment,
	/** Outside the grid, or no such slot. */
	OutOfBounds,
	Occupied,
	/** The container or slot filter refused the item. */
	Rejected,
	/** The equip rules don't allow the slot, another equipped item blocks it, or the owner doesn't meet the requirements. */
	NotEquippable,
	CannotRotate,
	/** Not stackable, different instance state, or the stack is full. */
	CannotStack,
	/** The target container is inside the item being moved. */
	WouldContainItself,
	NoSpace,
	MissingIngredients,
	RequirementsNotMet
};


UENUM(BlueprintType)
enum class EInventoryOperation : uint8
{
	Add,
	Remove,
	Move,
	Swap,
	Split,
	Merge,
	Rotate,
	Equip,
	Unequip
};


/** One step of a batch. Only the fields the operation reads need setting. */
USTRUCT(BlueprintType)
struct MODULARINVENTORY_API FInventoryOperation
{
	GENERATED_BODY()

	static FInventoryOperation MakeAdd(const FInventoryItemSpec& InSpec, FInventoryContainerHandle InContainer);
	static FInventoryOperation MakeRemove(FInventoryItemHandle InItem, int32 InCount = 0);
	static FInventoryOperation MakeMove(FInventoryItemHandle InItem, FInventoryContainerHandle InContainer, int32 InCount = 0);
	static FInventoryOperation MakeMoveTo(FInventoryItemHandle InItem, FInventoryContainerHandle InContainer, const FInventoryPlacement& InPlacement, int32 InCount = 0);
	static FInventoryOperation MakeSwap(FInventoryItemHandle InItem, FInventoryItemHandle InOtherItem);
	static FInventoryOperation MakeSplit(FInventoryItemHandle InItem, int32 InCount, FInventoryContainerHandle InContainer);
	static FInventoryOperation MakeMerge(FInventoryItemHandle InSource, FInventoryItemHandle InTarget, int32 InCount = 0);
	static FInventoryOperation MakeRotate(FInventoryItemHandle InItem);
	static FInventoryOperation MakeEquip(FInventoryItemHandle InItem, FInventoryContainerHandle InEquipmentContainer, FGameplayTag InSlot);
	static FInventoryOperation MakeUnequip(FInventoryItemHandle InItem, FInventoryContainerHandle InContainer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	EInventoryOperation Operation = EInventoryOperation::Move;

	/** The item operated on. Swap: the first item. Merge: the stack to take from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	FInventoryItemHandle Item;

	/** Swap: the second item. Merge: the stack to add to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	FInventoryItemHandle OtherItem;

	/** Add, Move, Split, Unequip: the destination. Equip: the equipment container. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	FInventoryContainerHandle Container;

	/** Used instead of finding a free spot when Auto Place is off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	FInventoryPlacement Placement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	bool bAutoPlace = true;

	/** Equip: the slot to equip into. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	FGameplayTag Slot;

	/** Remove, Move, Split, Merge: how many to take. 0 takes the whole stack. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (ClampMin = "0"))
	int32 Count = 0;

	/** Add: the item to create. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	FInventoryItemSpec Spec;
};


USTRUCT(BlueprintType)
struct MODULARINVENTORY_API FInventoryBatchResult
{
	GENERATED_BODY()

	bool Succeeded() const { return Error == EInventoryError::None; }

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	EInventoryError Error = EInventoryError::None;

	/** The operation that failed and rolled the batch back, or INDEX_NONE. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 FailedOperation = INDEX_NONE;
};
