#pragma once

#include "CoreMinimal.h"

#include "Engine/DataTable.h"
#include "Input/UIActionBindingHandle.h"

#include "InventoryTypes.h"
#include "InventoryUITypes.h"
#include "ModularActivatableWidget.h"

#include "InventoryScreenWidget.generated.h"

class UCommonActivatableWidgetStack;
class UInventoryGridWidget;
class UInventoryItemAction;
class UInventoryItemActionSet;
class UInventoryItemMenuWidget;
class UInventoryItemWidget;
class UInventoryUISubsystem;
class USoundBase;
class UTextBlock;
struct FInventoryItemActionContext;
struct FUIInputConfig;
enum class ECommonInputType : uint8;


/**
 * A Common UI screen for an inventory: push it onto a layer stack to open it, and close it with Close Widget. Lay out its grids, lists,
 * slots and details in the designer. It can pause the game, and come in and go with Open Reveal, as any Modular Activatable Widget.
 * While it's active, Interaction Style decides how items are handled, and each action is bound only while it can work, so an action
 * bar shows what works now. Back stops choosing an item, or puts back the held item, before it closes the screen, and closing it
 * drops whatever was held or chosen. A right-click or the mouse wheel anywhere on it can put back or turn the held item, as the style says.
 * An item asking for its menu opens Item Menu Class in Item Menu Stack.
 */
UCLASS(Abstract, Blueprintable)
class MODULARINVENTORYUI_API UInventoryScreenWidget : public UModularActivatableWidget
{
	GENERATED_BODY()

public:
	UInventoryScreenWidget(const FObjectInitializer& ObjectInitializer);

	//~ Begin UCommonActivatableWidget Interface
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	//~ End UCommonActivatableWidget Interface

#if WITH_EDITOR
	virtual void ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree, class IWidgetCompilerLog& CompileLog) const override;
#endif

	/**
	 * Opens Menu Class beside the item, unless the menu has nothing for it, such as no action that can run. The screen opens Item Menu
	 * Class this way; an item action can open another, such as a menu to pick a quick slot. The menu it opened last goes at once if it's
	 * still closing, such as the one whose action opens this one.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Screen")
	UInventoryItemMenuWidget* OpenItemMenu(FInventoryItemHandle Item, TSubclassOf<UInventoryItemMenuWidget> MenuClass);

	/**
	 * Whether the items are on screen, such as on a tabbed screen while their tab is open. While they aren't, item menus and the screen's
	 * actions don't work, and whatever was held or chosen is dropped. Showing them again aims at the start, as opening the screen does.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Screen")
	void SetInventoryShown(bool bShown);

	UFUNCTION(BlueprintPure, Category = "Inventory|Screen")
	bool IsInventoryShown() const { return bInventoryShown; }

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
	 * does as the screen opens. Call it as the items show again, such as on going back to their tab. Nothing while an item is held.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Screen")
	void AimAtStart();

	/**
	 * Says what the highlighted option of an open menu does, in Action Hint Text. Empty gives the screen its own hint back: what to do with
	 * the held item, why it can't go where it's aimed, what to choose, or Idle Hint. Item menus call it for their actions.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Screen")
	void ShowActionHint(const FText& Hint);

	/** The hint Action Hint Text shows now. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Screen")
	FText GetActionHint() const { return ShownHint; }

	/** Runs an item action for an item on this screen if it can run, such as from a button of your own. It doesn't prompt. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Screen")
	bool RunAction(UInventoryItemAction* Action, FInventoryItemHandle Item, int32 Count = 0);

	/** Runs the first of Default Actions that can run for the item, as a press set to Default Action does. False when none can. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Screen")
	bool RunDefaultAction(FInventoryItemHandle Item);

protected:
	//~ Begin UUserWidget Interface
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
	//~ End UModularActivatableWidget Interface

	/** Binds each action while it can work, and none while the screen closes. Override to bind your own with Set Action Bound, after calling Super. */
	virtual void RefreshActions();

	/** Registers Row while bBound and unregisters it otherwise. */
	void SetActionBound(FUIActionBindingHandle& Handle, const FDataTableRowHandle& Row, bool bBound, const FSimpleDelegate& Handler, bool bShowInActionBar = true);

	/** Adds a menu Open Item Menu made to Item Menu Stack. Override to open it elsewhere, such as on a layer of the game's root layout. */
	virtual UInventoryItemMenuWidget* PushItemMenu(TSubclassOf<UInventoryItemMenuWidget> MenuClass, TFunctionRef<void(UInventoryItemMenuWidget&)> InitMenu);

	/** Before On Item Menu Requested, for C++ subclasses. */
	virtual void NativeOnItemMenuRequested(FInventoryItemHandle Item) {}

	/**
	 * An item asked for its menu: pressed as the style's Left Click, Right Click or Accept says, with the Item Menu action, or with Request
	 * Item Menu. After Item Menu Class opens. Without one, open a menu of your own here, then call its Set Screen and Set Item.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Screen")
	void OnItemMenuRequested(FInventoryItemHandle Item);

	/** The action hint changed, such as to animate it. After Action Hint Text shows it. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Screen")
	void OnActionHintChanged(const FText& Hint);

	UInventoryUISubsystem* GetInteraction() const;

	/** The menu an item opens when it asks for one, such as with a right-click. None leaves item menus to On Item Menu Requested. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Screen")
	TSubclassOf<UInventoryItemMenuWidget> ItemMenuClass;

	/** Where item menus open: a Common Activatable Widget Stack drawn over the rest of the screen and filling it. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Screen", meta = (BindWidgetOptional))
	TObjectPtr<UCommonActivatableWidgetStack> ItemMenuStack;

	/**
	 * What a press set to Default Action runs, such as a double-click: the first of these that can run for the item, such as Use, then
	 * Equip. It doesn't prompt; a count is the one a prompt would start at.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Screen")
	TObjectPtr<UInventoryItemActionSet> DefaultActions;

	/** Shows what to do now, or what a menu option does, and is hidden while there's nothing to say. Put Action Hint Text inside it. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Screen|Hints", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ActionHint;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Screen|Hints", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ActionHintText;

	/** The hint while an item is held. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Screen|Hints")
	FText MoveHint;

	/** The hint while the held item is aimed where it can't go, by why it can't. Reasons not listed use Invalid Hint. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Screen|Hints")
	TMap<EInventoryError, FText> InvalidHints;

	/** The hint while the held item is aimed where it can't go, for reasons not in Invalid Hints. Empty keeps Move Hint. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Screen|Hints")
	FText InvalidHint;

	/** The hint while an item is being chosen for another, such as to combine them. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Screen|Hints")
	FText ChooseHint;

	/** The hint the rest of the time. Empty hides the hint. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Screen|Hints")
	FText IdleHint;

	/** How items are handled while this screen is active. The style from before comes back when it closes. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Screen")
	FInventoryInteractionStyle InteractionStyle;

	/** Played while this screen is active, for hovering items and for what happens to the held item. Each is optional. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Screen")
	FInventoryUISounds Sounds;

	/**
	 * Gamepad focus starts on its first cell, unless Get Desired Focus Target is implemented. When focus comes back to the screen, such
	 * as after an item menu closes, it goes to the cell, entry or slot last aimed at instead, in any of the screen's grids, lists and slots.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Screen", meta = (BindWidgetOptional))
	TObjectPtr<UInventoryGridWidget> MainGrid;

	/**
	 * As the screen opens, the spot focus starts on is aimed at and its item hovered, so an item is lit and shown in details panels with
	 * the mouse too, as with a gamepad. The mouse takes over as it reaches another item. Off, with the mouse nothing is lit until then.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Screen")
	bool bAimOnOpen = false;

	/**
	 * Focus starts on the item that was hovered when the screen last closed, while it's still on the screen, rather than on Main Grid's
	 * first cell, so the player picks up where they left off.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Screen")
	bool bStartOnLastItem = false;

	/** The game keeps getting input while this is open, such as to keep walking. Off, only the UI gets it. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	bool bGameKeepsInput = false;

	// Rows of a UI input action table, so a Common Bound Action Bar shows them with their keys. Each is optional.
	/** Picks up the item under the cursor or focus, or puts the held item down where it's aimed. How keys pick items up when Accept opens item menus. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle MoveAction;

	/**
	 * Opens the menu of the item under the cursor or focus, while nothing is held, such as a gamepad face button or the keyboard's Menu key.
	 * How gamepads and keys reach item menus when Accept picks items up.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle ItemMenuAction;

	/** Turns the held item, while it can turn and still fit. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle RotateAction;

	/** Halves the held stack, while it's a stack of more than one that can split. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle SplitAction;

	/** Moves the item under the cursor or focus to the other side, while there's a transfer container and it fits. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle TransferAction;

	/** Takes everything that fits from the transfer container into the player's storage, while it holds anything. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle TakeAllAction;

	/**
	 * What Accept does. With a gamepad, it's only shown in an action bar, as Accept itself goes to the focused cell. With the mouse,
	 * where keyboard focus isn't on what the mouse is, its key does what a click on the hovered item does, or puts the held item down
	 * where it's aimed, and it's bound only while it can.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle ConfirmAction;

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

	FInventoryItemActionContext MakeActionContext(FInventoryItemHandle Item) const;

	/** Shows the menu's hint while there is one, or else what to do now, in Action Hint Text. */
	void RefreshActionHint();

	/** Gamepad focus goes back to the items. With the mouse, the spot focus would start on is aimed at, as Aim On Open does. */
	void ReturnToItems();

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

	FInventoryInteractionStyle PreviousStyle;
	FInventoryContainerHandle TransferContainer;
	bool bInventoryShown = true;

	/** The spot last aimed at while the screen is open, which focus goes back to. */
	FInventoryContainerHandle LastAimedContainer;
	FInventoryPlacement LastAimedPlacement;

	/** The item hovered as the screen last closed, for Start On Last Item. */
	FInventoryItemHandle LastHoveredItem;

	/** Aim At Start is hovering the item the screen starts on, which plays no hover sound. */
	bool bAimingAtStart = false;

	/** What an open menu's highlighted option does, from Show Action Hint. */
	FText MenuHint;

	/** The menu Open Item Menu opened last, wherever Push Item Menu put it, to close at once if it's still closing as another opens. */
	TWeakObjectPtr<UInventoryItemMenuWidget> OpenedMenu;

	/** What Action Hint Text shows now. */
	FText ShownHint;

	FUIActionBindingHandle ConfirmHandle;
	FUIActionBindingHandle ConfirmShownHandle;
	FUIActionBindingHandle MoveHandle;
	FUIActionBindingHandle ItemMenuHandle;
	FUIActionBindingHandle RotateHandle;
	FUIActionBindingHandle SplitHandle;
	FUIActionBindingHandle TransferHandle;
	FUIActionBindingHandle TakeAllHandle;
};
