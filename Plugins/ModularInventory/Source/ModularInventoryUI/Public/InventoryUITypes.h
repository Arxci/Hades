#pragma once

#include "CoreMinimal.h"

#include "InventoryRegistry.h"

#include "InventoryUITypes.generated.h"

class UInventoryItemDefinition;
class UInventorySubsystem;
class USoundBase;


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


/** What just happened to the held item, such as for a sound or an effect. */
UENUM(BlueprintType)
enum class EInventoryHoldEvent : uint8
{
	PickedUp,
	/** Put down on a free spot. */
	Placed,
	/** Put down onto a stack it joined. */
	Merged,
	/** Put down in another item's place, trading places with it. */
	Swapped,
	/** Putting it down was tried where it can't go. */
	Refused,
	Rotated,
	Split,
	/** Put back where it was, without being placed. */
	PutBack
};


/** Sounds an inventory screen plays for what happens to items. Each is optional. Buttons, such as cells and menu rows, play their own from their styles. */
USTRUCT(BlueprintType)
struct MODULARINVENTORYUI_API FInventoryUISounds
{
	GENERATED_BODY()

	/** The mouse or gamepad focus reaches an item, while nothing is held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	TObjectPtr<USoundBase> HoverItem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	TObjectPtr<USoundBase> PickUp;

	/** Put down on a free spot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	TObjectPtr<USoundBase> PutDown;

	/** Put down onto a stack it joined. None plays Put Down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	TObjectPtr<USoundBase> Merge;

	/** Traded places with another item. None plays Put Down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	TObjectPtr<USoundBase> Swap;

	/** Putting it down was tried where it can't go. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	TObjectPtr<USoundBase> CantPlace;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	TObjectPtr<USoundBase> Rotate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	TObjectPtr<USoundBase> Split;

	/** Put back where it was, such as with Back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	TObjectPtr<USoundBase> PutBack;
};


/** What a right-click does while an item is held. */
UENUM(BlueprintType)
enum class EInventoryHeldRightClick : uint8
{
	/** The same as with nothing held: on an item, it asks for that item's menu, which puts the held item back. */
	AsUsual,
	/** Puts the held item back where it was. */
	PutBack,
	/** Turns the held item. */
	Rotate
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

	/**
	 * The item the mouse was last over stays hovered, lit and in details panels, until the mouse reaches another item. Moving across
	 * gaps and empty cells, or off the inventory, doesn't end it. Off, it ends as the mouse leaves the item. Gamepad focus always decides.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	bool bKeepLastHoveredItem = false;

	/** What a right-click does while an item is held. Put Back and Rotate work anywhere on an inventory screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	EInventoryHeldRightClick HeldRightClick = EInventoryHeldRightClick::AsUsual;

	/** The mouse wheel turns the held item, anywhere on an inventory screen except over something that scrolls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	bool bWheelRotatesHeld = false;
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


/**
 * An item drawn without being in any inventory, such as on a grid's designer preview: a definition, and how it looks.
 * Item widgets show it with Show Preview.
 */
USTRUCT(BlueprintType)
struct MODULARINVENTORYUI_API FInventoryItemPreview
{
	GENERATED_BODY()

	/** The value of Tag, from Tag Values or else the definition's. False when neither has it. */
	bool FindTagValue(const FGameplayTag& Tag, float& OutValue) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	TObjectPtr<UInventoryItemDefinition> Definition;

	/** Its top-left cell on a grid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI", meta = (ClampMin = "0"))
	FIntPoint Cell = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	bool bRotated = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI", meta = (ClampMin = "1"))
	int32 Count = 1;

	/** Drawn as if hovered, to see the highlight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	bool bLit = false;

	/** Tag values it has instead of its definition's, such as durability for a meter. A game's item widget can read others for its own marks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI", meta = (TitleProperty = "Tag"))
	TArray<FInventoryTagValue> TagValues;
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
