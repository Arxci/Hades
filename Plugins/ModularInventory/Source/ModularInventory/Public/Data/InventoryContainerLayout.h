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

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout")
	EInventoryContainerType Type = EInventoryContainerType::Grid;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "1", EditCondition = "Type == EInventoryContainerType::Grid", EditConditionHides))
	FIntPoint GridSize = FIntPoint(4, 4);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout", meta = (TitleProperty = "Tag", EditCondition = "Type == EInventoryContainerType::Slots", EditConditionHides))
	TArray<FInventorySlotDefinition> Slots;

	/** 0 for no limit. */
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer Tags;

	/** Static data shared by every container with this layout. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fragments")
	TArray<FInstancedStruct> Fragments;

	/** Copied onto each new container as its own state. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fragments")
	TArray<FInstancedStruct> InstanceFragments;
};
