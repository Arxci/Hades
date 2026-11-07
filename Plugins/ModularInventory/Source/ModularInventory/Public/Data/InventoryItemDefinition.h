#pragma once

#include "CoreMinimal.h"

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

#include "InventoryTypes.h"

#include "InventoryItemDefinition.generated.h"

class UInventoryContainerLayout;
class UInventoryEquipRules;
class UInventoryItemCosmetics;
class UInventoryStackRules;


/** A container every item of a kind carries, like a backpack's storage or a rifle's attachment slots. */
USTRUCT(BlueprintType)
struct MODULARINVENTORY_API FInventoryItemContainer
{
	GENERATED_BODY()

	/** Tells the item's containers apart. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Container")
	FGameplayTag Tag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Container")
	TObjectPtr<UInventoryContainerLayout> Layout;
};


/** Buffs items next to this one in a grid, such as a torch lighting the sword above it. */
USTRUCT(BlueprintType)
struct MODULARINVENTORY_API FInventoryItemSocket
{
	GENERATED_BODY()

	/** Cells the socket reaches, relative to the item's top-left cell before rotation: (0,-1) is above it, (Width,0) right of it. They turn with the item. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Socket")
	TArray<FIntPoint> Cells;

	/** Items in those cells that the socket affects. Empty affects any item. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Socket")
	FInventoryItemQuery Filter;

	/** Added to the effective tag values of affected items. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Socket", meta = (TitleProperty = "Tag"))
	TArray<FInventoryTagValue> ValueModifiers;

	/** Added to the effective tags of affected items. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Socket")
	FGameplayTagContainer GrantedTags;
};


/**
 * What an item is. Gameplay rules are hard references; everything visual lives in Cosmetics behind soft references.
 * Items themselves are records in the inventory registry, not objects.
 */
UCLASS(BlueprintType, Const)
class MODULARINVENTORY_API UInventoryItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The Asset Manager type of every item definition, whatever its class. */
	static const FPrimaryAssetType AssetType;

	//~ Begin UObject Interface
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	//~ End UObject Interface

	int32 GetMaxStackSize() const;
	bool CanSplit() const;
	bool IsEquippable() const { return EquipRules != nullptr; }
	FIntPoint GetSize(bool bRotated) const { return bRotated ? FIntPoint(Size.Y, Size.X) : Size; }

	/** Where a cell given relative to the unrotated item lands once the item turns a quarter clockwise. */
	FIntPoint RotateCell(const FIntPoint& Cell, bool bRotated) const { return bRotated ? FIntPoint(Size.Y - 1 - Cell.Y, Cell.X) : Cell; }

	const FInventoryTagValue* FindTagValue(const FGameplayTag& Tag) const;
	const FInstancedStruct* FindFragment(const UScriptStruct* Type) const;

	template <typename T>
	const T* FindFragment() const
	{
		const FInstancedStruct* Fragment = FindFragment(T::StaticStruct());
		return Fragment ? Fragment->GetPtr<T>() : nullptr;
	}

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	TObjectPtr<UInventoryItemCosmetics> Cosmetics;

	/** Cells taken in grid containers. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "1"))
	FIntPoint Size = FIntPoint(1, 1);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shape")
	bool bCanRotate = true;

	/** None for items that never stack. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules")
	TObjectPtr<UInventoryStackRules> StackRules;

	/** None for items that can't be equipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules")
	TObjectPtr<UInventoryEquipRules> EquipRules;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer Tags;

	/** Every item starts with these. An item only stores its own value once it changes, so tuning these updates existing items. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags", meta = (TitleProperty = "Tag"))
	TArray<FInventoryTagValue> TagValues;

	/** Items carrying containers never stack. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Containers", meta = (TitleProperty = "Tag"))
	TArray<FInventoryItemContainer> Containers;

	/** Only active in grid containers. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sockets")
	TArray<FInventoryItemSocket> Sockets;

	/** Static data shared by every item of this kind. Any struct works, Blueprint structs included. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fragments")
	TArray<FInstancedStruct> Fragments;

	/** Copied onto each new item as its own state. Mark the struct's properties SaveGame so they can be saved. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fragments")
	TArray<FInstancedStruct> InstanceFragments;
};
