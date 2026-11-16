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


/**
 * What pressing on an item does while nothing is held: a left click, a right-click, or Accept on a gamepad or keyboard. While an item is
 * being chosen for another one, such as to combine them, a left click or Accept on an item chooses it instead.
 */
UENUM(BlueprintType)
enum class EInventoryItemPress : uint8
{
	/** Picks the item up, to put down with another click or Accept. */
	PickUp,
	/** Asks for the item's menu: the Inventory UI Subsystem's On Context Requested, which an inventory screen passes to On Item Menu Requested. */
	OpenItemMenu,
	/** Nothing. The item can still be dragged, and a screen's Move and Item Menu actions still work on it. */
	None,
	/**
	 * Runs the item's default action: the first of the inventory screen's Default Actions that can run for it, such as Use, then Equip.
	 * Through the Inventory UI Subsystem's On Default Action Requested.
	 */
	DefaultAction
};


/** What a right-click does while an item is held. */
UENUM(BlueprintType)
enum class EInventoryHeldRightClick : uint8
{
	/**
	 * Right Click decides, as with nothing held: with Open Item Menu, a right-click on an item asks for its menu, which puts the held item
	 * back; with Pick Up, it puts the held item down there, as a click does.
	 */
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

	/** What a left click on an item does while nothing is held. A drag always picks the item up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	EInventoryItemPress LeftClick = EInventoryItemPress::PickUp;

	/** What a right-click on an item does while nothing is held. Held Right Click decides while an item is held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	EInventoryItemPress RightClick = EInventoryItemPress::OpenItemMenu;

	/**
	 * What Accept on an item does while nothing is held: on the focused cell with a gamepad or the keyboard, or on the hovered item with a
	 * screen's Confirm action while the mouse is in use. Separate from Left Click, so a gamepad can open item menus while a click picks
	 * items up, or the other way round.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	EInventoryItemPress Accept = EInventoryItemPress::PickUp;

	/**
	 * What a double-click on an item does while nothing is held, such as Default Action to use or equip it. None leaves the second click a
	 * click like the first. A first click that picked the item up is undone, so pair it with a Left Click that does little on its own.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	EInventoryItemPress DoubleClick = EInventoryItemPress::None;

	/** What holding a finger on an item does on a touch screen, such as open its menu, as a right-click would. A tap is a left click. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	EInventoryItemPress LongPress = EInventoryItemPress::OpenItemMenu;

	/** How long a finger is held on an item for a long press, in seconds. Moving off the item, or starting a drag, cancels it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI", meta = (ClampMin = "0.1", Units = "s"))
	float LongPressTime = 0.5f;

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
