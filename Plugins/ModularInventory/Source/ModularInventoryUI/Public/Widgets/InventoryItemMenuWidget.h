#pragma once

#include "CoreMinimal.h"

#include "CommonActivatableWidget.h"
#include "Containers/Ticker.h"
#include "Engine/DataTable.h"
#include "Input/UIActionBindingHandle.h"

#include "Actions/InventoryItemAction.h"
#include "InventoryMenuTransition.h"
#include "InventoryTypes.h"
#include "InventoryUITypes.h"

#include "InventoryItemMenuWidget.generated.h"

class UCommonButtonBase;
class UInventoryItemActionButton;
class UInventoryItemWidget;
class UInventoryScreenWidget;
class UInventoryUISubsystem;
class UPanelWidget;
class USlider;
class USoundBase;
class UTextBlock;
class UWidgetAnimation;
struct FInventoryChangeSet;
struct FUIInputConfig;
enum class ECommonInputType : uint8;


/** Where an item menu opens. Each but As Designed then moves in as far as it takes to stay on screen. */
UENUM(BlueprintType)
enum class EInventoryMenuPlacement : uint8
{
	/**
	 * Beside the item, on the side toward the middle of the screen: level with the item's top in the upper half, and with its bottom in
	 * the lower half.
	 */
	TowardCenter,
	/** On the item's right, level with its top. It goes to the left where only that side has room. */
	Right,
	/** On the item's left, level with its top. It goes to the right where only that side has room. */
	Left,
	/** Wherever Menu Panel is in the designer, such as a fixed spot for a menu that isn't laid out around the item. */
	AsDesigned,
	/**
	 * At the cursor, down and to the right of it as desktop context menus open, or up or to the left where only that side has room. Without
	 * the mouse, such as on a gamepad, it opens as Toward Center does.
	 */
	AtCursor
};


/** What a press outside an open item menu does. Either way, the menu closes. */
UENUM(BlueprintType)
enum class EInventoryMenuOutsidePress : uint8
{
	/** Only closes the menu, wherever the press is, even on an item. */
	Close,
	/**
	 * Closes the menu and goes on to the item under the press, as if the menu weren't open: a click or right-click does what it does there,
	 * and a drag picks the item up. A press that would open a menu for this menu's own item only closes it, as pressing what opened a menu
	 * again does. Presses only pass through to items on the screen the menu's item is on.
	 */
	PassThrough
};


/** When an item menu runs an action picked in it, against its close. */
UENUM(BlueprintType)
enum class EInventoryMenuActionTiming : uint8
{
	/**
	 * At once, as the menu starts to close, so what the action does shows while the menu goes, such as the inventory dimming to choose an
	 * item to combine with. Another menu the action opens in the same stack replaces this one at once.
	 */
	AsMenuCloses,
	/**
	 * Once the menu has closed, after its close transition and Close Animation, so nothing the action shows overlaps the menu. The longer
	 * the close, the later the action.
	 */
	AfterMenuCloses
};


/**
 * A small menu for one item, such as Use, Equip and Drop, opened beside it: set it as an inventory screen's Item Menu Class, or open it
 * with the screen's Open Item Menu. While it's open the item is selected, and nothing is held.
 * It lists the item actions of its Action Set and Actions: each on the button placed in the designer under its Button Name, or else on a
 * button made for it in Action Panel. An action can ask the player to confirm, or how many, in Prompt Panel first. As the menu opens,
 * its first entry is highlighted and takes focus; the mouse and focus move the highlight, and the screen's action hint says what the
 * highlighted one does.
 * Back, or a right press on the menu, closes it. A press outside Menu Panel closes it too, and goes on to the item under it as Left Press
 * Outside and Right Press Outside say. It also closes by itself when its item leaves the inventory, such as when it's used up, and when
 * its screen closes.
 * It comes in with Open Transition and goes with Close Transition, such as a fade and a slide out from the item and back, and plays
 * animations named OpenAnimation and CloseAnimation too.
 * Give the root a full-screen border that is hit-testable, so those presses reach the menu and nothing behind it can be hovered while it's open.
 */
UCLASS(Abstract, Blueprintable)
class MODULARINVENTORYUI_API UInventoryItemMenuWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UInventoryItemMenuWidget(const FObjectInitializer& ObjectInitializer);

	//~ Begin UCommonActivatableWidget Interface
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	//~ End UCommonActivatableWidget Interface

	/**
	 * Opens the menu for InItem beside Item Widget, usually found with the screen's Find Item Widget, as Placement says. Without one,
	 * it opens beside the focused widget on a gamepad, or at the cursor. The menu is placed after On Item Changed, so it's measured
	 * with the buttons shown for this item.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item Menu")
	void SetItem(FInventoryItemHandle InItem, UInventoryItemWidget* ItemWidget);

	UFUNCTION(BlueprintPure, Category = "Inventory|Item Menu")
	FInventoryItemHandle GetItem() const { return Item; }

	/**
	 * The inventory screen the menu is for: where its hints show, what its actions run on, and what closes it by closing. Set Item finds it
	 * from Item Widget; set it first for a menu opened without one.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item Menu")
	void SetScreen(UInventoryScreenWidget* InScreen);

	UFUNCTION(BlueprintPure, Category = "Inventory|Item Menu")
	UInventoryScreenWidget* GetScreen() const;

	/** Closes the menu, after its close transition and Close Animation. Use it instead of Deactivate Widget so they play. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item Menu")
	void CloseMenu();

	/**
	 * Closes the menu at once, without its close transition or Close Animation, or cuts a close short. An action waiting for the menu to
	 * close doesn't run. For something that has to happen right away, such as another menu opening in the same stack.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item Menu")
	void CloseMenuNow();

	/**
	 * The menu is closing: its close transition or Close Animation plays, and it closes once they end. It takes no presses or keys
	 * meanwhile; ignore any that reach widgets of your own.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|Item Menu")
	bool IsClosing() const { return bClosing; }

	/** Who and what the menu's actions run for: its item, player and screen. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Item Menu")
	FInventoryItemActionContext GetActionContext() const;

	/** Lists the actions again, such as after something changed what can run. Set Item does it. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item Menu")
	void RefreshActions();

	/**
	 * The entry highlighted now: the first as the menu opens, then whichever the mouse or focus reached last. Entries are the menu's
	 * buttons, the prompt's included.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|Item Menu")
	UCommonButtonBase* GetHighlightedEntry() const { return HighlightedEntry.Get(); }

	/**
	 * Whether the menu has anything for the item: an action it would list. A menu without actions, such as one laid out by hand for
	 * something else, always has. Screens ask the menu's class defaults before opening it.
	 */
	virtual bool CanOpenFor(const FInventoryItemActionContext& Context) const;

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeOnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual void OnAnimationFinished_Implementation(const UWidgetAnimation* Animation) override;
	//~ End UUserWidget Interface

	//~ Begin UCommonActivatableWidget Interface
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnHandleBackAction() override;
	//~ End UCommonActivatableWidget Interface

	/** Before On Item Changed, for C++ subclasses. */
	virtual void NativeOnItemChanged() {}

	/** The actions to list, in order: Action Set's, then Actions. Override to add your own, such as ones made in code. */
	virtual void GatherActions(TArray<UInventoryItemAction*>& OutActions) const;

	/** Set Item was called, after the actions were listed. Fill in anything else the menu shows for the item here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Item Menu")
	void OnItemChanged();

	UInventoryUISubsystem* GetInteraction() const;

	/**
	 * The menu itself. Put it in a Canvas Panel that fills the screen, with Size To Content ticked, so it can be moved beside the item.
	 * As the menu opens, its first visible button is highlighted and takes focus, unless Get Desired Focus Target is implemented.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item Menu", meta = (BindWidget))
	TObjectPtr<UWidget> MenuPanel;

	/**
	 * One entry is always highlighted, as console-style menus keep one: the first as the menu opens, or as a prompt asks, then whichever
	 * the mouse or focus reached last, so Accept runs the one the player sees. It's shown with its button style's Selected look: give
	 * Selected Base and Selected Hovered your highlight, and Selected Text Style its text, the prompt's buttons too. Off, an entry looks
	 * highlighted only while it's hovered, or focused on a gamepad, though the first still takes focus.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Item Menu")
	bool bKeepEntryHighlighted = false;

	/** The item's name. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item Menu", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemNameText;

	/** Where the menu opens. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Item Menu")
	EInventoryMenuPlacement Placement = EInventoryMenuPlacement::TowardCenter;

	/** Space between the item, or the cursor, and the menu, in pixels. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Item Menu", meta = (ClampMin = "0", EditCondition = "Placement != EInventoryMenuPlacement::AsDesigned"))
	float MenuGap = 0.0f;

	/** Space kept between the menu and each edge of the screen, in pixels. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Item Menu", meta = (EditCondition = "Placement != EInventoryMenuPlacement::AsDesigned"))
	FMargin ScreenPadding;

	/**
	 * What a left press outside Menu Panel does. Close suits a menu that has the player's attention, so a stray click only closes it.
	 * Pass Through lets the player carry on as if it weren't open, such as dragging another item straight away.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Item Menu")
	EInventoryMenuOutsidePress LeftPressOutside = EInventoryMenuOutsidePress::Close;

	/**
	 * What a right press outside Menu Panel does. With Pass Through, while right-clicks open item menus, a right-click on another item opens
	 * that item's menu in place of this one, as desktop context menus do. A right press on the menu itself always closes it, as Back does.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Item Menu")
	EInventoryMenuOutsidePress RightPressOutside = EInventoryMenuOutsidePress::PassThrough;

	/** Played as the menu opens. Its buttons play their own hover and click sounds from their styles. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Item Menu")
	TObjectPtr<USoundBase> OpenSound;

	/** Played as the menu starts to close, however it closes. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Item Menu")
	TObjectPtr<USoundBase> CloseSound;

	// Transition
	/**
	 * How the menu comes in as it opens: a fade, a slide and a growth, each optional. The whole menu fades, and Menu Panel slides and grows
	 * from wherever it opened, such as out from the item. It adds to the render opacity and transform they have in the designer. Leave
	 * Item Menu Stack's own transition at 0, so the two don't play over each other.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Item Menu|Transition")
	FInventoryMenuTransition OpenTransition;

	/**
	 * The menu closes by playing Open Transition backwards, from wherever it has got to, as if going back where it came from. Off, it plays
	 * Close Transition.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Item Menu|Transition")
	bool bCloseReversesOpen = true;

	/** How the menu goes as it closes, from however it looks then, such as a quicker fade without a slide. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Item Menu|Transition", meta = (EditCondition = "!bCloseReversesOpen", EditConditionHides))
	FInventoryMenuTransition CloseTransition;

	/**
	 * Played as the menu opens, after it's placed, along with Open Transition. Name an animation OpenAnimation to use one. Give it every
	 * property Close Animation changes, as a menu is reused from where it last closed. Leave the menu's render opacity, and Menu Panel's
	 * render transform, to the transitions.
	 */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> OpenAnimation;

	/**
	 * Played as the menu closes with Back, a press outside it or an action, along with its close transition. The menu closes once both
	 * have ended, and takes no input meanwhile. A press passed through to an item, or the screen closing, closes it at once. Name an
	 * animation CloseAnimation to use one.
	 */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> CloseAnimation;

	// Actions
	/** Actions to list, such as everything the game's item menus offer, shared with other menus. Listed before Actions. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Item Menu|Actions")
	TObjectPtr<UInventoryItemActionSet> ActionSet;

	/** Actions to list after Action Set's, for actions only this menu has. */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Inventory|Item Menu|Actions")
	TArray<TObjectPtr<UInventoryItemAction>> Actions;

	/** Made in Action Panel for each action without a button placed for it. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Item Menu|Actions")
	TSubclassOf<UInventoryItemActionButton> ActionButtonClass;

	/** When an action picked in the menu runs: at once as the menu starts to close, or once it has closed. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Item Menu|Actions")
	EInventoryMenuActionTiming ActionTiming = EInventoryMenuActionTiming::AsMenuCloses;

	/**
	 * Holds the action buttons, such as a Vertical Box. Buttons made for actions go at its end, after any you placed in it. Hidden while a
	 * prompt shows.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item Menu", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> ActionPanel;

	// Prompt
	/**
	 * Shown in place of Action Panel while an action asks to confirm, or how many. Put Prompt Text, Confirm Button and Cancel Button inside
	 * it, and for counts, Quantity Slider and Quantity Text. Back, or Cancel Button, goes back to the actions.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item Menu|Prompt", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PromptPanel;

	/** The action's Prompt Text. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item Menu|Prompt", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PromptText;

	/** Runs the action. Needed for prompts. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item Menu|Prompt", meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> ConfirmButton;

	/** Goes back to the actions, highlighting the one that asked. A confirmation can start on it, as Confirm Starts On Cancel says. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item Menu|Prompt", meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> CancelButton;

	/** Chooses how many, for actions that ask. Gamepad focus starts on it, so left and right change the count. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item Menu|Prompt", meta = (BindWidgetOptional))
	TObjectPtr<USlider> QuantitySlider;

	/** The count chosen, in Quantity Format. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item Menu|Prompt", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText;

	/** What Quantity Text says. {Count} is the count chosen, and {Max} the most there is to choose. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Item Menu|Prompt")
	FText QuantityFormat = INVTEXT("{Count}");

	/**
	 * A confirmation starts highlighted on Cancel Button, so the quick answer is the safe one, such as before discarding. Off, it starts on
	 * the prompt's first button, as the menu starts on its first entry. A count to choose starts on Quantity Slider either way.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Item Menu|Prompt")
	bool bConfirmStartsOnCancel = true;

	/**
	 * What Accept does: shown in an action bar, while Accept itself goes to the focused button. With the mouse on a button, where
	 * keyboard focus may be on another, its key clicks the button under the mouse instead.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle ConfirmAction;

private:
	/** An action listed for the item, and the button showing it. */
	struct FShownAction
	{
		TWeakObjectPtr<UInventoryItemAction> Action;
		TWeakObjectPtr<UCommonButtonBase> Button;
		bool bAvailable = false;
		FText UnavailableReason;
	};

	FBox2D FindAnchor(const UInventoryItemWidget* ItemWidget) const;
	void PlaceBeside(const FBox2D& Box);

	/** An absolute (desktop) box in viewport space, where Menu Panel's canvas positions are. */
	FBox2D AbsoluteBoxToViewport(const FBox2D& AbsoluteBox);

	/** Where the cursor is in the viewport, while the mouse is in use. Unset on a gamepad, where the cursor is hidden. */
	TOptional<FVector2D> GetCursorInViewport();

	/** Menu Panel's size as it is now, with the buttons shown for this item. Lays the menu out first, as it may not have been drawn yet. */
	FVector2D MeasureMenuPanel();

	/** The item widget a press outside Menu Panel is on: the item the menu is beside, or any other item on its screen. */
	UInventoryItemWidget* FindItemWidgetUnder(const FVector2D& ScreenPosition) const;

	/** Whether Press on the item widget's item would only open a menu for this menu's item again, so passing it through only closes this one. */
	bool WouldReopen(const UInventoryItemWidget* ItemWidget, EInventoryItemPress Press) const;

	/** The item widget the last left press was on, if it still shows the same item, and forgets it: a press ends in a click or a drag, not both. */
	UInventoryItemWidget* TakePressedItemWidget();

	/** Closes the menu once its item is gone, such as used up, or destroyed by a game that keeps running behind the menu. */
	void HandleRegistryChanged(const FInventoryChangeSet& Changes);

	/** Follows the screen, so the menu closes with it. Only while the menu is open. */
	void BindScreen(UInventoryScreenWidget* NewScreen);
	void HandleScreenDeactivated();

	// Closing
	/** Closes the menu after its close transition and Close Animation, then calls After Close. */
	void CloseThen(TFunction<void()>&& AfterClose);

	/** Closes the menu once its close transition and Close Animation have both ended. */
	void FinishClosingWhenDone();
	void FinishClosing();
	void PlayCloseSound();

	// Transition
	/**
	 * Moves the transition on each frame while one plays, from the core ticker, so it plays whether or not the menu ticks, with the game
	 * paused too. A close finishes once it ends.
	 */
	void StartTransitionTicker();

	// Actions
	void ClearActionButtons();
	void HandleActionClicked(int32 Index);
	void HandleActionHovered(int32 Index);
	void RunAction(UInventoryItemAction* Action, int32 Count);

	/** Says something in the screen's action hint. Empty gives the screen its own hint back. */
	void ShowHint(const FText& Hint);

	/** Says what the shown action does, or why it can't run, in the screen's action hint. */
	void ShowHintFor(int32 Index);

	// Highlight
	/**
	 * Highlights an entry, so the hint says what its action does, and with Keep Entry Highlighted, it's selected. With bFocus, it takes
	 * focus too, so Accept runs it, as for the mouse. Anything but an entry leaves the highlight where it was.
	 */
	void HighlightEntry(UCommonButtonBase* Entry, bool bFocus);
	void ClearHighlight();

	/** Whether the button is an entry: one of the menu's buttons, the prompt's included. */
	bool IsEntry(UCommonButtonBase* Button) const;

	/** Whether the button is one of the prompt's, such as Confirm Button. */
	bool IsPromptButton(UCommonButtonBase* Button) const;

	// Prompt
	bool IsPrompting() const { return PromptIndex != INDEX_NONE; }
	void ShowPrompt(int32 Index);
	void HidePrompt();
	void RefreshPrompt();
	int32 GetChosenCount() const;
	void HandlePromptConfirmed();
	void HandlePromptCancelled();

	UFUNCTION()
	void HandleQuantityChanged(float Value);

	/** Moves focus to Get Desired Focus Target on the next frame, once what just showed is laid out and can take it. */
	void FocusNextFrame();

	void HandleInputMethodChanged(ECommonInputType InputType);

	/** Binds Confirm to click the button under the mouse while there is one, with the mouse in use, and otherwise only to show it. */
	void RefreshConfirm();
	void HandleConfirmAction();

	/** The button in Menu Panel the mouse is on, if it can be clicked. */
	UCommonButtonBase* FindHoveredButton() const;

	FUIActionBindingHandle ConfirmHandle;
	bool bConfirmClicks = false;

	FInventoryItemHandle Item;
	TWeakObjectPtr<UInventoryItemWidget> AnchorWidget;
	FBox2D Anchor = FBox2D(ForceInit);

	/** Where the cursor was as the menu opened, for At Cursor, so the menu stays there as its size changes. */
	TOptional<FVector2D> OpeningCursor;

	TWeakObjectPtr<UInventoryScreenWidget> Screen;

	TWeakObjectPtr<UInventoryItemWidget> PressedItemWidget;
	FInventoryItemHandle PressedItem;

	/** Only listens while the menu is open. */
	FInventoryRegistryListener RegistryListener;

	TArray<FShownAction> ShownActions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInventoryItemActionButton>> MadeButtons;

	/** The shown action a prompt asks about, or none. */
	int32 PromptIndex = INDEX_NONE;
	bool bPromptQuantity = false;
	int32 PromptDefaultCount = 1;
	int32 PromptMaxCount = 1;

	/** How the designer left the panels, to show them again as they were. */
	ESlateVisibility ActionPanelVisibility = ESlateVisibility::SelfHitTestInvisible;
	ESlateVisibility PromptPanelVisibility = ESlateVisibility::SelfHitTestInvisible;

	/** The menu is closing: its close transition or Close Animation plays, and it closes once they end. */
	bool bClosing = false;
	TFunction<void()> AfterClosing;

	/** Close Animation is playing as the menu closes, so it waits for that too. */
	bool bWaitingForCloseAnimation = false;

	/** Fades the whole menu, and slides and grows Menu Panel. */
	FInventoryMenuTransitionPlayer TransitionPlayer;
	FTSTicker::FDelegateHandle TransitionHandle;

	FTSTicker::FDelegateHandle FocusHandle;

	/** The entry highlighted now. Kept as the menu closes, and cleared as Set Item lists the actions again. */
	TWeakObjectPtr<UCommonButtonBase> HighlightedEntry;

	/** The entry whose action the prompt asks about, highlighted again as the prompt goes. */
	TWeakObjectPtr<UCommonButtonBase> PromptEntry;
};
