#pragma once

#include "CoreMinimal.h"

#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"

#include "InventoryTypes.h"

#include "InventoryItemAction.generated.h"

class APlayerController;
class UInventoryItemMenuWidget;
class UInventoryScreenWidget;
class UInventoryUISubsystem;
class UTexture2D;


/** What an item menu does with an action that can't run for the item now. */
UENUM(BlueprintType)
enum class EInventoryActionUnavailable : uint8
{
	/** Leaves it out, so the menu only lists what can be done. */
	Hide,
	/** Lists it disabled, with Get Unavailable Reason, so the player learns it exists and why it can't run, such as "Not enough space". */
	ShowDisabled
};


/** What an item menu asks before running an action. */
UENUM(BlueprintType)
enum class EInventoryActionPrompt : uint8
{
	/** Runs straight away. */
	None,
	/** Asks the player to confirm first, such as before discarding. */
	Confirm,
	/** Asks how many, such as how many to split off a stack. The action gets the count chosen. */
	Quantity
};


/** Who and what an item action runs for. */
USTRUCT(BlueprintType)
struct MODULARINVENTORYUI_API FInventoryItemActionContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Action")
	FInventoryItemHandle Item;

	/** The player handling the item. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Action")
	TObjectPtr<APlayerController> Player = nullptr;

	/** The inventory screen the item is on, if any. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Action")
	TObjectPtr<UInventoryScreenWidget> Screen = nullptr;

	/** The item menu the action was picked from, if any. It has closed by the time the action runs. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Action")
	TObjectPtr<UInventoryItemMenuWidget> Menu = nullptr;

	/** How many the player chose, with a Quantity prompt. 0 otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Action")
	int32 Count = 0;
};


/**
 * Something the player can do with an item, listed in item menus, such as Use, Equip or Drop. Subclass it in C++ or Blueprint: Can Run
 * says whether it works for an item now, and Run does it. Add actions to an Item Action Set, or straight to an item menu's Actions.
 * Each entry has its own text, icon, hotkey and prompt, so one class can serve several entries.
 * While it's asked about an item, it has that player's world, so Blueprint subclasses can use world functions.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class MODULARINVENTORYUI_API UInventoryItemAction : public UObject
{
	GENERATED_BODY()

public:
	//~ Begin UObject Interface
	virtual UWorld* GetWorld() const override;
	//~ End UObject Interface

	// What menus and screens call. Each gives the action the context's world while it runs.
	bool CanRunFor(const FInventoryItemActionContext& Context) const;
	FText GetUnavailableReasonFor(const FInventoryItemActionContext& Context) const;
	void GetQuantityRangeFor(const FInventoryItemActionContext& Context, int32& OutMin, int32& OutMax, int32& OutDefault) const;
	void RunFor(const FInventoryItemActionContext& Context);

	/** Text with {Item} replaced by the item's name and {Count} by the context's count, such as Prompt Text. */
	FText FormatText(const FText& Text, const FInventoryItemActionContext& Context) const;

	/** The entry's label, such as Use. Buttons made for the action show it; buttons placed in a menu keep their own. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	FText DisplayName;

	/** What it does, shown in the inventory screen's action hint while its entry is highlighted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (MultiLine = true))
	FText Description;

	/** Shown on buttons made for the action that have an Icon. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	TObjectPtr<UTexture2D> Icon;

	/**
	 * A key that runs the action while its menu is open, shown on its button by the button's Input Action Widget: a row of a UI input
	 * action table.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle InputAction;

	/** Whether a menu lists the action disabled while it can't run, or leaves it out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	EInventoryActionUnavailable WhenUnavailable = EInventoryActionUnavailable::Hide;

	/**
	 * What a menu asks before running the action. Prompts need the menu's Prompt Panel and Confirm Button; without them, it runs straight
	 * away, with the count Get Quantity Range starts at. Default actions never prompt.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	EInventoryActionPrompt Prompt = EInventoryActionPrompt::None;

	/** What the prompt asks. {Item} is the item's name, and {Count} the count chosen so far. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (EditCondition = "Prompt != EInventoryActionPrompt::None"))
	FText PromptText;

	/**
	 * The name of a button already in the menu's designer to use for this action, such as UseButton, instead of making one in its Action
	 * Panel. For menus laid out by hand. The button keeps its own label.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	FName ButtonName;

protected:
	/** Whether the action works for the item now. True by default. */
	UFUNCTION(BlueprintNativeEvent, Category = "Inventory|Action")
	bool CanRun(const FInventoryItemActionContext& Context) const;

	/** Why it can't run now, for an entry shown disabled, such as "Not enough space". Empty by default. */
	UFUNCTION(BlueprintNativeEvent, Category = "Inventory|Action")
	FText GetUnavailableReason(const FInventoryItemActionContext& Context) const;

	/** The counts a Quantity prompt offers, and the one it starts at. By default 1 to the item's count, starting at half. */
	UFUNCTION(BlueprintNativeEvent, Category = "Inventory|Action")
	void GetQuantityRange(const FInventoryItemActionContext& Context, int32& OutMin, int32& OutMax, int32& OutDefault) const;

	/** Does it. Only called after Can Run said yes, and after any prompt was answered. */
	UFUNCTION(BlueprintNativeEvent, Category = "Inventory|Action")
	void Run(const FInventoryItemActionContext& Context);

	/** The player's Inventory UI Subsystem, for C++ subclasses. */
	static UInventoryUISubsystem* GetInteraction(const FInventoryItemActionContext& Context);

	/** Something in the context's world, for functions that take a world context. */
	static const UObject* GetContextObject(const FInventoryItemActionContext& Context);

private:
	/** The world of the context it's asked about. Actions live in assets and on menu classes, which have none of their own. */
	mutable TWeakObjectPtr<UWorld> ContextWorld;
};


/**
 * Item actions in the order menus list them, to share between menus and screens, such as everything a game's item menu offers, or an
 * inventory screen's Default Actions.
 */
UCLASS(BlueprintType)
class MODULARINVENTORYUI_API UInventoryItemActionSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Actions")
	TArray<TObjectPtr<UInventoryItemAction>> Actions;
};
