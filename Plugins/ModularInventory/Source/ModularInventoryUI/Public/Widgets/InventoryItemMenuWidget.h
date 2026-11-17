#pragma once

#include "CoreMinimal.h"

#include "CommonActivatableWidget.h"
#include "Containers/Ticker.h"
#include "Engine/DataTable.h"
#include "Input/UIActionBindingHandle.h"

#include "Actions/InventoryItemAction.h"
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


/**
 * A small menu for one item, such as Use, Equip and Drop, opened beside it: set it as an inventory screen's Item Menu Class, or open it
 * with the screen's Open Item Menu. While it's open the item is selected, and nothing is held.
 * It lists the item actions of its Action Set and Actions: each on the button placed in the designer under its Button Name, or else on a
 * button made for it in Action Panel. An action can ask the player to confirm, or how many, in Prompt Panel first. The screen's action
 * hint says what the highlighted one does.
 * Back, or a right press on the menu, closes it. A press outside Menu Panel closes it too, and goes on to the item under it as Left Press
 * Outside and Right Press Outside say. It also closes by itself when its item leaves the inventory, such as when it's used up, and when
 * its screen closes. Name animations OpenAnimation and CloseAnimation to play them as it opens and closes.
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

	/** Closes the menu, after Close Animation if it has one. Use it instead of Deactivate Widget so the animation plays. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item Menu")
	void CloseMenu();

	/** Close Animation is playing, and the menu closes once it ends. Ignore presses on the menu meanwhile. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Item Menu")
	bool IsClosing() const { return bClosing; }

	/** Who and what the menu's actions run for: its item, player and screen. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Item Menu")
	FInventoryItemActionContext GetActionContext() const;

	/** Lists the actions again, such as after something changed what can run. Set Item does it. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item Menu")
	void RefreshActions();

	/**
	 * Whether the menu has anything for the item: an action it would list. A menu without actions, such as one laid out by hand for
	 * something else, always has. Screens ask the menu's class defaults before opening it.
	 */
	virtual bool CanOpenFor(const FInventoryItemActionContext& Context) const;

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
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
	 * Gamepad focus starts on its first visible button, unless Get Desired Focus Target is implemented.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item Menu", meta = (BindWidget))
	TObjectPtr<UWidget> MenuPanel;

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

	/** Played as the menu closes, however it closes. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Item Menu")
	TObjectPtr<USoundBase> CloseSound;

	/**
	 * Played as the menu opens, after it's placed, such as a fade or a slide. Name an animation OpenAnimation to use one. Give it every
	 * property Close Animation changes, as a menu is reused from where it last closed.
	 */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> OpenAnimation;

	/**
	 * Played before the menu closes, when it closes with Back, a press outside it or an action; the menu takes no input meanwhile, and an
	 * action runs once it has closed. A press passed through to an item, or the item or screen going away, closes it at once. Name an
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

	/** Goes back to the actions. Gamepad focus starts on it for a confirmation, so the safe answer is the easy one. */
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
	/** Closes the menu after Close Animation, if it has one, then calls After Close. */
	void CloseThen(TFunction<void()>&& AfterClose);

	/** Closes the menu without its animation, for something that happens right after, such as a press passed through to an item. */
	void CloseNow();
	void FinishClosing();

	// Actions
	void ClearActionButtons();
	void HandleActionClicked(int32 Index);
	void HandleActionHovered(int32 Index);
	void RunAction(UInventoryItemAction* Action, int32 Count);

	/** Says something in the screen's action hint. Empty gives the screen its own hint back. */
	void ShowHint(const FText& Hint);

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

	/** Close Animation is playing, and the menu closes once it ends. */
	bool bClosing = false;
	TFunction<void()> AfterClosing;

	FTSTicker::FDelegateHandle FocusHandle;
};
