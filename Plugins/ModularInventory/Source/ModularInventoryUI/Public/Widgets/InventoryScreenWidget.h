#pragma once

#include "CoreMinimal.h"

#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Input/UIActionBindingHandle.h"

#include "InventoryTypes.h"
#include "InventoryUITypes.h"
#include "ModularActivatableWidget.h"

#include "InventoryScreenWidget.generated.h"

class UCommonActivatableWidgetStack;
class UCommonAnimatedSwitcher;
class UInventoryGridWidget;
class UInventoryItemAction;
class UInventoryItemActionSet;
class UInventoryItemMenuWidget;
class UInventoryItemWidget;
class UInventoryUISubsystem;
class UModularTabListWidget;
class USoundBase;
class UTextBlock;
struct FInventoryItemActionContext;
enum class ECommonInputType : uint8;


/** Which of an inventory screen's actions are bound, and so shown in action bars, at a time. */
UENUM(BlueprintType)
enum class EInventoryActionAvailability : uint8
{
	/**
	 * The actions for what the player is doing: browsing the items, holding one, or choosing one. Action bars stay as they are while the
	 * cursor moves, as console games keep them, and an action that can't be done for what's aimed at, such as Move over an empty cell,
	 * does nothing.
	 */
	ByMode,
	/**
	 * Only the actions that can be done for what's aimed at now, such as Move only over an item. Action bars change as the cursor moves,
	 * and a Common Bound Action Bar rebuilds its buttons each time.
	 */
	ByTarget
};


/**
 * A Common UI screen for an inventory: push it onto a layer to open it, and close it with Close Widget. Lay out its grids, lists, slots and
 * details in the designer. It sets its input mode, can pause the game, and comes in and goes with Open Reveal, as any Modular Activatable
 * Widget.
 * While it's active, Interaction Style decides how items are handled, and its actions are bound as Action Availability says, for action
 * bars to show. Back stops choosing an item, or puts back the held item, before it closes the screen, and closing it drops whatever was
 * held or chosen. A right-click or the mouse wheel anywhere on it can put back or turn the held item, as the style says.
 * An item asking for its menu opens Item Menu Class on Item Menu Layer, or in Item Menu Stack.
 * Description Text, the line along the bottom of the screen, says what the player can do now: what the highlighted option of an item menu
 * open over it does, or what to do with the held item or the item being chosen for, such as where the held item can't go and why, or what
 * a highlighted button does. Browsing the items, it says nothing: what an item is belongs in a details panel.
 * With pages, such as Map, Items and Crafting, link a Tab List to Pages, best a Modular Page Switcher: the items only work while Items Page
 * shows, and coming back to it focuses the items again and hovers the item hovered as they were left, as other pages get focus back where
 * it was. The screen opens on Items Page, or on the page Set Start Page names, such as for a key that opens it on its crafting page.
 */
UCLASS(Abstract, Blueprintable)
class MODULARINVENTORYUI_API UInventoryScreenWidget : public UModularActivatableWidget
{
	GENERATED_BODY()

public:
	UInventoryScreenWidget(const FObjectInitializer& ObjectInitializer);

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface

#if WITH_EDITOR
	virtual void ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree, class IWidgetCompilerLog& CompileLog) const override;
#endif

	/**
	 * Opens Menu Class beside the item, unless the menu has nothing for it, such as no action that can run. The screen opens Item Menu
	 * Class this way; an item action can open another, such as a menu to pick a shortcut slot. The menu it opened last goes at once if it's
	 * still closing, such as the one whose action opens this one.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Screen")
	UInventoryItemMenuWidget* OpenItemMenu(FInventoryItemHandle Item, TSubclassOf<UInventoryItemMenuWidget> MenuClass);

	/** Open Item Menu, with Init Menu setting the menu up before it's given its item and shows, such as the recipes a recipe menu lists. */
	UInventoryItemMenuWidget* OpenItemMenuWith(FInventoryItemHandle Item, TSubclassOf<UInventoryItemMenuWidget> MenuClass, TFunctionRef<void(UInventoryItemMenuWidget&)> InitMenu);

	/**
	 * Whether the items are on screen, such as on a tabbed screen while their tab is open. While they aren't, item menus and the screen's
	 * actions don't work, and whatever was held or chosen is dropped. Showing them again gives them focus back once they're drawn, and with
	 * the mouse hovers the item hovered as they were left, or else aims at the start, as opening the screen does. Pages and Tab List call it
	 * as pages change.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Screen")
	void SetInventoryShown(bool bShown);

	UFUNCTION(BlueprintPure, Category = "Inventory|Screen")
	bool IsInventoryShown() const { return bInventoryShown; }

	/** Opens a page of Pages, by its index, through Tab List when there is one, so its tab shows as selected. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Screen")
	void OpenPage(int32 PageIndex);

	/** The index of the page shown in Pages, or -1 without Pages. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Screen")
	int32 GetOpenPage() const;

	/**
	 * The page the screen opens on the next time it opens, once, instead of Items Page, such as its crafting page for a key that opens the
	 * screen there. Call it as the screen is pushed, before it shows, such as from a push's Init Func; called while it's open, it opens the
	 * page now. -1 goes back to Items Page.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Screen")
	void SetStartPage(int32 PageIndex);

	/** Set Start Page, by the Tab Id of the page's tab in Tab List. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Screen")
	void SetStartTab(FName TabId);

	/**
	 * The other side while this screen is open, such as a chest's container: Transfer, Take All and shift-clicks move items between
	 * it and the player. Set it each time the screen opens for a chest; closing the screen clears it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Screen")
	void SetTransferContainer(FInventoryContainerHandle Container);

	/** The widget showing the item among this screen's grids, lists and slots, such as to open a menu beside it. Null when none does. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Screen")
	UInventoryItemWidget* FindItemWidget(FInventoryItemHandle Item) const;

	/**
	 * The item widget at a point in absolute space, such as a mouse event's Screen Space Position, among this screen's grids, lists and
	 * slots. Only one drawn there now that a press there would reach counts: not one on a page that isn't shown. Null over an empty spot.
	 * Item menus use it to pass a press outside them to the item under it.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|Screen")
	UInventoryItemWidget* FindItemWidgetAt(const FVector2D& ScreenPosition) const;

	/**
	 * Aims at the spot focus starts on and hovers its item, so it's lit and shown in details panels with the mouse too, as Aim On Open
	 * does as the screen opens. Showing the items again does it, when there's no item to hover again. Nothing while an item is held.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Screen")
	void AimAtStart();

	/** Runs an item action for an item on this screen if it can run, such as from a button of your own. It doesn't prompt. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Screen")
	bool RunAction(UInventoryItemAction* Action, FInventoryItemHandle Item, int32 Count = 0);

	/** Runs the first of Default Actions that can run for the item, as a press set to Default Action does. False when none can. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Screen")
	bool RunDefaultAction(FInventoryItemHandle Item);

	/**
	 * Sorts the container Get Sort Container says, Main Grid's by default, as Sort Settings say, putting back the held item first, as Sort
	 * Action does. On Sorted says whether it could: a grid can be too full to fit everything again, and then nothing moves.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Screen")
	bool SortItems();

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	//~ End UUserWidget Interface

	//~ Begin UCommonActivatableWidget Interface
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual bool NativeOnHandleBackAction() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	//~ End UCommonActivatableWidget Interface

	//~ Begin UModularActivatableWidget Interface
	virtual void NativeOnCloseStarted() override;
	virtual void RegisterConfirmAction() override;
	virtual FText GetHint() const override;
	//~ End UModularActivatableWidget Interface

	/**
	 * Binds the actions as Action Availability says, and none while the screen closes. Override to bind your own with Set Action Bound,
	 * after calling Super.
	 */
	virtual void RefreshActions();

	/** Registers Row while bBound and unregisters it otherwise. */
	void SetActionBound(FUIActionBindingHandle& Handle, const FDataTableRowHandle& Row, bool bBound, const FSimpleDelegate& Handler, bool bShowInActionBar = true);

	/**
	 * Adds a menu Open Item Menu made to Item Menu Layer, or else to Item Menu Stack. Override to open it somewhere else, such as a stack of
	 * your own.
	 */
	virtual UInventoryItemMenuWidget* PushItemMenu(TSubclassOf<UInventoryItemMenuWidget> MenuClass, TFunctionRef<void(UInventoryItemMenuWidget&)> InitMenu);

	/** The container Sort Items sorts: Main Grid's. Override to sort another, such as the player's backpack by its tag. */
	virtual FInventoryContainerHandle GetSortContainer() const;

	/** Before On Item Menu Requested, for C++ subclasses. */
	virtual void NativeOnItemMenuRequested(FInventoryItemHandle Item) {}

	/** Before On Sorted, for C++ subclasses. */
	virtual void NativeOnSorted(bool bSorted) {}

	/**
	 * An item asked for its menu: pressed as the style's Left Click, Right Click or Accept says, with the Item Menu action, or with Request
	 * Item Menu. After Item Menu Class opens. Without one, open a menu of your own here, then call its Set Screen and Set Item.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Screen")
	void OnItemMenuRequested(FInventoryItemHandle Item);

	/** Sort Items ran: whether everything was sorted, or nothing moved because it couldn't all fit again, such as to tell the player. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Screen")
	void OnSorted(bool bSorted);

	UInventoryUISubsystem* GetInteraction() const;

	// Item Menus
	/** The menu an item opens when it asks for one, such as with a right-click. None leaves item menus to On Item Menu Requested. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory Screen|Item Menus")
	TSubclassOf<UInventoryItemMenuWidget> ItemMenuClass;

	/**
	 * The Modular UI layer item menus open on, such as UI.Layer.Modal, above the screen's own layer, so they never put the screen aside.
	 * None opens them in Item Menu Stack instead.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory Screen|Item Menus", meta = (Categories = "UI.Layer"))
	FGameplayTag ItemMenuLayer;

	/** Where item menus open without an Item Menu Layer: a Common Activatable Widget Stack drawn over the rest of the screen and filling it. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory Screen|Item Menus", meta = (BindWidgetOptional))
	TObjectPtr<UCommonActivatableWidgetStack> ItemMenuStack;

	// Interaction
	/** How items are handled while this screen is active. The style from before comes back when it closes. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory Screen|Interaction")
	FInventoryInteractionStyle InteractionStyle;

	/**
	 * What a press set to Default Action runs, such as a double-click: the first of these that can run for the item, such as Use, then
	 * Equip. It doesn't prompt; a count is the one a prompt would start at.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory Screen|Interaction")
	TObjectPtr<UInventoryItemActionSet> DefaultActions;

	// Focus
	/**
	 * Gamepad focus starts on its first cell, unless Get Desired Focus Target is implemented. When focus comes back to the screen, such
	 * as after an item menu closes, it goes to the cell, entry or slot last aimed at instead, in any of the screen's grids, lists and slots.
	 * Sort Items sorts its container.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory Screen|Focus", meta = (BindWidgetOptional))
	TObjectPtr<UInventoryGridWidget> MainGrid;

	/**
	 * As the screen opens, the spot focus starts on is aimed at and its item hovered, so an item is lit and shown in details panels with
	 * the mouse too, as with a gamepad. The mouse takes over as it reaches another item. Off, with the mouse nothing is lit until then.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory Screen|Focus")
	bool bAimOnOpen = false;

	/**
	 * Focus starts on the item that was hovered when the screen last closed, while it's still on the screen, rather than on Main Grid's
	 * first cell, so the player picks up where they left off.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory Screen|Focus")
	bool bStartOnLastItem = false;

	// Pages
	/** A tab for each page of Pages, in the same order. The screen links them as it's constructed. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory Screen|Pages", meta = (BindWidgetOptional))
	TObjectPtr<UModularTabListWidget> TabList;

	/** The screen's pages, such as a map, the items, and files, one of which holds the items. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory Screen|Pages", meta = (BindWidgetOptional))
	TObjectPtr<UCommonAnimatedSwitcher> Pages;

	/** The index of the page with the items in Pages. The screen opens on it. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory Screen|Pages", meta = (ClampMin = "0"))
	int32 ItemsPage = 0;

	/** While an item is held, the tab list's Next Tab and Previous Tab keys turn it instead of changing pages. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory Screen|Pages")
	bool bTabKeysTurnHeldItem = false;

	// Hints: what Description Text says to do now, ahead of what's highlighted. What the highlighted option of an item menu open over the
	// screen does comes first. Browsing the items, it says nothing.
	/** What to do while an item is held, such as "Select an area to place this item." */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory Screen|Hints")
	FText MoveHint;

	/** Why the held item can't go where it's aimed, by the reason. Reasons not listed say Invalid Hint. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory Screen|Hints")
	TMap<EInventoryError, FText> InvalidHints;

	/** Why the held item can't go where it's aimed, for reasons not in Invalid Hints. Empty keeps Move Hint. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory Screen|Hints")
	FText InvalidHint;

	/** What to do while an item is being chosen for another, such as "Select an item to combine with." */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory Screen|Hints")
	FText ChooseHint;

	// Sorting
	/** How Sort Items orders the items, such as biggest first and then by name, or by category. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory Screen|Sorting")
	FInventorySortSettings SortSettings;

	// Sounds
	/** Played while this screen is active, for hovering items and for what happens to the held item. Each is optional. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory Screen|Sounds")
	FInventoryUISounds Sounds;

	/**
	 * Which actions are bound, and so shown in action bars, at a time: By Mode, those for what the player is doing, browsing, holding an
	 * item or choosing one, so action bars stay still as the cursor moves; By Target, only those that work for what's aimed at now.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Inventory Actions")
	EInventoryActionAvailability ActionAvailability = EInventoryActionAvailability::ByMode;

	// Rows of a UI input action table, so a Common Bound Action Bar shows them with their keys. Each is optional. Confirm Action, with the
	// screen's other input settings, is under Input: with a gamepad it's only shown, as Accept goes to the focused cell; with the mouse, its
	// key does what a click on the hovered item does, or puts the held item down where it's aimed.
	/** Picks up the item under the cursor or focus, or puts the held item down where it's aimed. How keys pick items up when Accept opens item menus. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Inventory Actions", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle MoveAction;

	/**
	 * Opens the menu of the item under the cursor or focus, while nothing is held, such as a gamepad face button or the keyboard's Menu key.
	 * How gamepads and keys reach item menus when Accept picks items up.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Inventory Actions", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle ItemMenuAction;

	/** Turns the held item, while it can turn and still fit. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Inventory Actions", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle RotateAction;

	/** Halves the held stack, while it's a stack of more than one that can split. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Inventory Actions", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle SplitAction;

	/** Moves the item under the cursor or focus to the other side, while there's a transfer container and it fits. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Inventory Actions", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle TransferAction;

	/** Takes everything that fits from the transfer container into the player's storage, while it holds anything. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Inventory Actions", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle TakeAllAction;

	/** Sorts the items with Sort Items, while they show. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Inventory Actions", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle SortAction;

private:
	UFUNCTION()
	void HandleItemMenuRequested(FInventoryItemHandle Item);

	UFUNCTION()
	void HandleInteractionChanged();

	UFUNCTION()
	void HandleHoveredItemChanged(FInventoryItemHandle Item);

	UFUNCTION()
	void HandleHoldEvent(EInventoryHoldEvent Event);

	UFUNCTION()
	void HandleSelectedItemChanged(FInventoryItemHandle Item);

	UFUNCTION()
	void HandleDefaultActionRequested(FInventoryItemHandle Item);

	UFUNCTION()
	void HandleTabSelected(FName TabId);

	FInventoryItemActionContext MakeActionContext(FInventoryItemHandle Item) const;

	/**
	 * Focus goes back to the items once they're drawn, such as when their page shows again, which aims at the cell focused with a gamepad.
	 * With the mouse, the item hovered as the items were left is hovered again, or else the spot focus starts on is aimed at, as Aim On
	 * Open does.
	 */
	void ReturnToItems();

	/** With the mouse, hovers the item hovered as the items were last left, while it's on the screen, or else Aim At Start. */
	void RestoreHover();

	/** Whether the page with the items is the one open, or there are no pages. */
	bool IsItemsPageOpen() const;

	void PlayInventorySound(USoundBase* Sound) const;

	void HandleInputMethodChanged(ECommonInputType InputType);

	/** Binds Confirm only to show it, without taking Accept from the focused cell, while bShown. */
	void SetConfirmShown(bool bShown);

	void HandleConfirmAction();
	void HandleMoveAction();
	void HandleItemMenuAction();
	void HandleRotateAction();
	void HandleSplitAction();
	void HandleTransferAction();
	void HandleTakeAllAction();
	void HandleSortAction();

	FInventoryInteractionStyle PreviousStyle;
	FInventoryContainerHandle TransferContainer;
	bool bInventoryShown = true;

	/** The spot last aimed at while the screen is open, which focus goes back to. */
	FInventoryContainerHandle LastAimedContainer;
	FInventoryPlacement LastAimedPlacement;

	/**
	 * The item hovered as the items were last left: as the screen closed, for Start On Last Item, or as another page showed, to hover it
	 * again as they show again.
	 */
	FInventoryItemHandle LastHoveredItem;

	/** The page the screen opens on next, from Set Start Page, or -1 for Items Page. */
	int32 StartPage = INDEX_NONE;

	/** Aim At Start, or Restore Hover, is hovering an item without the player, which plays no hover sound. */
	bool bAimingAtStart = false;

	/** The menu Open Item Menu opened last, wherever Push Item Menu put it, to close at once if it's still closing as another opens. */
	TWeakObjectPtr<UInventoryItemMenuWidget> OpenedMenu;

	FUIActionBindingHandle ConfirmHandle;
	FUIActionBindingHandle ConfirmShownHandle;
	FUIActionBindingHandle MoveHandle;
	FUIActionBindingHandle ItemMenuHandle;
	FUIActionBindingHandle RotateHandle;
	FUIActionBindingHandle SplitHandle;
	FUIActionBindingHandle TransferHandle;
	FUIActionBindingHandle TakeAllHandle;
	FUIActionBindingHandle SortHandle;
	FUIActionBindingHandle PreviousTabRotateHandle;
	FUIActionBindingHandle NextTabRotateHandle;

#if WITH_EDITORONLY_DATA
	// Replaced by the base's Input Config. Loaded only so PostLoad can carry a screen's setting over.
	UPROPERTY()
	bool bGameKeepsInput_DEPRECATED = false;

	// Replaced by the base's Default Description. Loaded only so PostLoad can carry a screen's text over.
	UPROPERTY()
	FText IdleHint_DEPRECATED;
#endif
};
