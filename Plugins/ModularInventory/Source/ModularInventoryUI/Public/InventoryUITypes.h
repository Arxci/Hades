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
	/** Asks for the item's menu with the Inventory UI Subsystem's Request Item Menu, which an inventory screen answers. */
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


/** Colors for a value by how full it is, such as a durability bar going from green to red, or a count that's green only while full. */
USTRUCT(BlueprintType)
struct MODULARINVENTORYUI_API FInventoryMeterColors
{
	GENERATED_BODY()

	/** Low at Low Fraction or emptier, Full at Full Fraction or fuller, and Partly between them, or a blend of the three with Blend. */
	FLinearColor Pick(float Fraction) const;

	/** At Full Fraction or fuller. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	FLinearColor Full = FLinearColor(FColor(140, 193, 82));

	/** Between Low Fraction and Full Fraction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	FLinearColor Partly = FLinearColor(FColor(232, 226, 212));

	/** Empty, or no fuller than Low Fraction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	FLinearColor Low = FLinearColor(FColor(217, 83, 79));

	/** How full still counts as low, such as 0.25 to warn before something breaks. 0 is only when empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI", meta = (ClampMin = "0", ClampMax = "1"))
	float LowFraction = 0.0f;

	/** How full counts as full, such as 0.5 for a bar that stays Full down to half. 1 is only when completely full. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI", meta = (ClampMin = "0", ClampMax = "1"))
	float FullFraction = 1.0f;

	/** Fades smoothly from Low through Partly to Full between Low Fraction and Full Fraction, instead of changing in steps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	bool bBlend = false;
};


/** Where a value display's number comes from. */
UENUM(BlueprintType)
enum class EInventoryValueSource : uint8
{
	/** A tag value, such as ammo, charges or durability. Shown only for items that have it, their definition's included. */
	TagValue,
	/** How many are in the stack. Shown only for items that stack. */
	StackCount,
	/**
	 * A number the item widget works out itself, in Resolve Custom Value, such as from a fragment, or from other items like the spare ammo
	 * held for a weapon. Tag says which number it is, so one function can serve several displays.
	 */
	Custom
};


/**
 * A number an item widget shows, such as ammo, charges, durability or a stack's count, in Text Blocks and a Progress Bar you name in its
 * designer. Each is shown only for the items it applies to. Displays in the same Group take turns, so one spot can show whichever number an
 * item has, each in its own widgets. Displays naming the same widget share it: the first that applies shows there. A named widget no shown
 * display claims is collapsed.
 */
USTRUCT(BlueprintType)
struct MODULARINVENTORYUI_API FInventoryValueDisplay
{
	GENERATED_BODY()

	/**
	 * Whether it applies to a real item, and if so, its number and what's full, Max 0 for no maximum. False for an item it doesn't apply
	 * to, or while it's hidden by Hide Single or Hide When Full. Count is the stack count to show, such as part of a stack being moved.
	 * Custom numbers need the item widget, so they're always false here.
	 */
	bool Resolve(const UInventorySubsystem& Inventory, FInventoryItemHandle Item, int32 Count, float& OutValue, float& OutMax) const;

	/** Whether Hide When Full hides it at this number. */
	bool IsHiddenAt(float Value, float Max) const { return bHideWhenFull && Max > 0.0f && Value >= Max; }

	/** What its Text Block says for a number: Format with {Value}, {Max} and {Percent} filled in. */
	FText FormatText(float Value, float Max) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	EInventoryValueSource Source = EInventoryValueSource::TagValue;

	/** The tag value shown, such as Item.Stat.Durability. With a Custom source, it names the number for Resolve Custom Value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI", meta = (EditCondition = "Source != EInventoryValueSource::StackCount", EditConditionHides))
	FGameplayTag Tag;

	/**
	 * The tag value that's full, such as Item.Stat.Capacity, for the meter, the tint, {Max} and {Percent}, with the bonuses of attachments and
	 * sockets, such as an extended magazine's. Without it, or for items without it, the value the definition starts Tag at is full.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI", meta = (EditCondition = "Source == EInventoryValueSource::TagValue", EditConditionHides))
	FGameplayTag MaxTag;

	/** Adds attachments' and sockets' bonuses to the value, as for a stat such as damage. Off, it's the item's own, as for ammo or durability. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI", meta = (EditCondition = "Source == EInventoryValueSource::TagValue", EditConditionHides))
	bool bIncludeBonuses = false;

	/** Only items that match show it, such as only weapons, by their tags. Empty shows it for every item with the value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	FInventoryItemQuery ItemFilter;

	/**
	 * Displays with the same Group take turns: only the first in the list that applies to the item shows, and every other in the group
	 * hides all its widgets. For one spot that shows whichever number an item has, each in its own widgets, such as a corner showing a
	 * weapon's ammo in one Text Block, else a tool's charge as a bar, else a stack's count in another Text Block. None never takes turns.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	FName Group;

	/** The name of a Text Block in the item widget's designer that shows the number, such as CountText. None shows no number. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	FName TextWidget = TEXT("CountText");

	/** What the Text Block says. {Value} is the number, {Max} what's full, and {Percent} how full it is, out of 100. Such as x{Value}, or {Value}/{Max}. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	FText Format = INVTEXT("{Value}");

	/** Decimal places {Value} and {Max} show at most, such as 1 for a weight of 2.5. 0 rounds them to whole numbers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI", meta = (ClampMin = "0", ClampMax = "3"))
	int32 FractionalDigits = 0;

	/** The name of a Progress Bar in the designer, filled to how full the value is, such as a durability bar. None shows no bar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	FName MeterWidget;

	/** The name of any widget in the designer shown only while this value is, such as a darker patch behind the number. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	FName FrameWidget;

	/** A stack of one shows no count, so only real stacks show a number. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI", meta = (EditCondition = "Source == EInventoryValueSource::StackCount", EditConditionHides))
	bool bHideSingle = false;

	/** Not shown while full, so only spent or worn items show it, such as a knife's durability. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	bool bHideWhenFull = false;

	/** Colors the Text Block and the bar by how full the value is, such as green when full and red when empty. Off, they keep the designer's colors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	bool bTintByFullness = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI", meta = (EditCondition = "bTintByFullness"))
	FInventoryMeterColors Colors;
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

	/** Only items that match list it, such as Fire Rate only for weapons, by their tags. Empty lists it for every item with the value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	FInventoryItemQuery ItemFilter;

	/** Left out while the value is 0, such as a resistance the item doesn't give. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	bool bHideWhenZero = false;

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

	/**
	 * What the stat's Value Text says, such as {Value}% or {Value} kg. {Value} is the value, and with Show Max, {Max} is its maximum and
	 * {Percent} how full it is, out of 100. Empty leaves it to the stat widget: the value, or its Max Format with a maximum.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	FText Format;
};


/** A tag named in words, such as Two-Handed for Item.Trait.TwoHanded, for an item's details to list. */
USTRUCT(BlueprintType)
struct MODULARINVENTORYUI_API FInventoryTagLabel
{
	GENERATED_BODY()

	/** Items with a child tag of it have it too. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	FText Label;
};


/** How a list widget orders its entries. Only how they're shown: the container keeps its own order. */
UENUM(BlueprintType)
enum class EInventoryListSort : uint8
{
	/** As the container holds them. */
	ContainerOrder,
	/** By the items' names. */
	Name,
	/** By their definitions' Category, then by name, so items of a kind sit together. */
	Category,
	/** By a tag value with its bonuses, such as weight, price or damage, then by name. */
	TagValue,
	/** By how many are in each stack, then by name. */
	Count
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
	bool bHighlighted = false;

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
