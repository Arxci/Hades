#pragma once

#include "CoreMinimal.h"

#include "InventoryRegistry.h"

#include "InventoryUITypes.generated.h"

class UInventorySubsystem;


UENUM(BlueprintType)
enum class EInventoryDropAction : uint8
{
	/** Nothing is held, or nothing is pointed at. */
	None,
	Place,
	/** Joins the stack already there. */
	Merge,
	/** Trades places with the item already there. */
	Swap,
	Invalid
};


/** How the player handles items. Each inventory screen applies its own while it's open; all defaults is drag-and-drop with click-to-move. */
USTRUCT(BlueprintType)
struct MODULARINVENTORYUI_API FInventoryInteractionStyle
{
	GENERATED_BODY()

	/** A click or Accept on an item asks for its item menu instead of picking it up. Items are then picked up by dragging, or with a Move action. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	bool bClickOpensItemMenu = false;

	/** Grids draw the held item on the cells it would land on instead of under the cursor. It stays on the last cells aimed at, and a mouse drag let go anywhere puts it there. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	bool bSnapHeldItem = false;

	/**
	 * Putting the held item down on another one trades their places. Where either sits in a grid, only an item taking the same cells
	 * swaps, so a swap never depends on what space is free. Off, an occupied spot is refused. Equipment slots make way either way.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	bool bSwapItems = true;

	/** A swap picks the other item up from where the held one was, instead of only trading places. Letting go of it finishes the swap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	bool bSwapPicksUpOther = false;

	/**
	 * A mouse release over an item counts as a click even when the press began elsewhere, such as on an item menu that closed when
	 * pressed, so one click goes straight from one item's menu to another's. Off, a click must start and end on the item.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	bool bClickOnAnyRelease = false;

	/** Shift and a click on an item moves it to the other side, such as from a chest into the pockets, while there's a transfer container. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	bool bShiftClickTransfers = false;
};


/** Colors for a value by how full it is, such as rounds left or durability. */
USTRUCT(BlueprintType)
struct MODULARINVENTORYUI_API FInventoryMeterColors
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	FLinearColor Full = FLinearColor(FColor(140, 193, 82));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	FLinearColor Partly = FLinearColor(FColor(232, 226, 212));

	/** Empty, or no fuller than Low Fraction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	FLinearColor Low = FLinearColor(FColor(217, 83, 79));

	/** How full still counts as low, such as 0.25 to warn before something breaks. 0 is only when empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI", meta = (ClampMin = "0", ClampMax = "1"))
	float LowFraction = 0.0f;

	const FLinearColor& Pick(float Fraction) const { return Fraction <= LowFraction ? Low : Fraction >= 1.0f ? Full : Partly; }
};


/** A color for items with a tag, such as a rarity's. */
USTRUCT(BlueprintType)
struct MODULARINVENTORYUI_API FInventoryTagColor
{
	GENERATED_BODY()

	/** Its child tags match too. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	FLinearColor Color = FLinearColor::White;
};


/** A tag value listed in an item's details, such as Damage from Item.Damage. */
USTRUCT(BlueprintType)
struct MODULARINVENTORYUI_API FInventoryStatDisplay
{
	GENERATED_BODY()

	/** Items without this tag value leave the stat out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	FText Label;

	/** Shows the value out of its maximum, such as durability: Max Tag's value, or else the value the definition starts Tag at. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	bool bShowMax = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI", meta = (EditCondition = "bShowMax"))
	FGameplayTag MaxTag;

	/** Less is better, such as weight or reload time, so a comparison colors the difference the other way. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	bool bLowerIsBetter = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI", meta = (ClampMin = "0", ClampMax = "3"))
	int32 FractionalDigits = 0;
};


/** Hears the world's inventory registry while bound. Unbind it before its owner goes away. */
struct MODULARINVENTORYUI_API FInventoryRegistryListener
{
	void Bind(const UObject* WorldContextObject, FOnInventoryRegistryChanged::FDelegate&& Delegate);
	void Unbind();

private:
	TWeakObjectPtr<UInventorySubsystem> Subsystem;
	FDelegateHandle Handle;
};
