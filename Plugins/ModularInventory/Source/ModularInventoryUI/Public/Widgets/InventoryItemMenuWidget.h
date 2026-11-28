#pragma once

#include "CoreMinimal.h"

#include "Actions/InventoryItemAction.h"
#include "InventoryTypes.h"
#include "InventoryUITypes.h"
#include "Widgets/ModularContextMenuWidget.h"

#include "InventoryItemMenuWidget.generated.h"

class UCommonButtonBase;
class UInventoryItemActionButton;
class UInventoryItemWidget;
class UInventoryScreenWidget;
class UInventoryUISubsystem;
class UPanelWidget;
class UTextBlock;
struct FInventoryChangeSet;


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
 * A context menu for one item, such as Use, Equip and Drop, opened beside it: set it as an inventory screen's Item Menu Class, or open it
 * with the screen's Open Item Menu. While it's open the item is selected, and nothing is held.
 * It lists the item actions of its Action Set and Actions: each on the button placed in the designer under its Button Name, or else on a
 * button made for it in Action Panel. An action can ask first, to confirm or how many, as Prompt Presentation says: inline in place of the
 * actions, over the menu, or in a dialog of its own. Description Text, the menu's and the screen's, says what the highlighted action does,
 * or why it can't run.
 * Running an action closes the menu, unless the action keeps it open. It also closes by itself when its item leaves the inventory, such as
 * when it's used up, and when its screen closes. A press passed through to another item reaches that item, as if the menu weren't open.
 * Everything else, its placement, dismissing, highlight, prompts and transitions, it gets from Modular Context Menu.
 */
UCLASS(Abstract, Blueprintable)
class MODULARINVENTORYUI_API UInventoryItemMenuWidget : public UModularContextMenuWidget
{
	GENERATED_BODY()

public:
	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface

#if WITH_EDITOR
	virtual void ValidateCompiledDefaults(class IWidgetCompilerLog& CompileLog) const override;
#endif

	/**
	 * Opens the menu for InItem beside Item Widget, usually found with the screen's Find Item Widget, as Placement says. Without one, it
	 * opens beside the focused widget on a gamepad, or at the cursor. The menu is placed after On Item Changed, so it's measured with the
	 * actions shown for this item.
	 */
	UFUNCTION(BlueprintCallable, Category = "Item Menu")
	void SetItem(FInventoryItemHandle InItem, UInventoryItemWidget* ItemWidget);

	UFUNCTION(BlueprintPure, Category = "Item Menu")
	FInventoryItemHandle GetItem() const { return Item; }

	/**
	 * The inventory screen the menu is for: where its hints show, what its actions run on, and what closes it by closing. Set Item finds it
	 * from Item Widget; set it first for a menu opened without one.
	 */
	UFUNCTION(BlueprintCallable, Category = "Item Menu")
	void SetScreen(UInventoryScreenWidget* InScreen);

	UFUNCTION(BlueprintPure, Category = "Item Menu")
	UInventoryScreenWidget* GetScreen() const;

	/** Who and what the menu's actions run for: its item, player and screen. */
	UFUNCTION(BlueprintPure, Category = "Item Menu")
	FInventoryItemActionContext GetActionContext() const;

	/** Lists the actions again, such as after something changed what can run. Set Item does it. */
	UFUNCTION(BlueprintCallable, Category = "Item Menu")
	void RefreshActions();

	/**
	 * Whether the menu has anything for the item: an action it would list. A menu without actions, such as one laid out by hand for
	 * something else, always has. Screens ask the menu's class defaults before opening it.
	 */
	virtual bool CanOpenFor(const FInventoryItemActionContext& Context) const;

protected:
	//~ Begin UUserWidget Interface
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	//~ End UUserWidget Interface

	//~ Begin UCommonActivatableWidget Interface
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	//~ End UCommonActivatableWidget Interface

	//~ Begin UModularContextMenuWidget Interface
	virtual bool PassPressThrough(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, FReply& OutReply) override;
	virtual FText GetEntryDescription(UCommonButtonBase* Entry) const override;
	virtual UWidget* GetEntriesWidget() const override;
	virtual UModularActivatableWidget* FindDescriptionScreen() const override;
	//~ End UModularContextMenuWidget Interface

	/** Before On Item Changed, for C++ subclasses. */
	virtual void NativeOnItemChanged() {}

	/** The actions to list, in order: Action Set's, then Actions. Override to add your own, such as ones made in code. */
	virtual void GatherActions(TArray<UInventoryItemAction*>& OutActions) const;

	/** Set Item was called, after the actions were listed. Fill in anything else the menu shows for the item here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Item Menu")
	void OnItemChanged();

	UInventoryUISubsystem* GetInteraction() const;

	// Item
	/** The item's name. */
	UPROPERTY(BlueprintReadOnly, Category = "Item Menu|Item", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemNameText;

	// Actions
	/** Actions to list, such as everything the game's item menus offer, shared with other menus. Listed before Actions. */
	UPROPERTY(EditDefaultsOnly, Category = "Item Menu|Actions")
	TObjectPtr<UInventoryItemActionSet> ActionSet;

	/** Actions to list after Action Set's, for actions only this menu has. */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Item Menu|Actions")
	TArray<TObjectPtr<UInventoryItemAction>> Actions;

	/** Made in Action Panel for each action without a button placed for it. */
	UPROPERTY(EditDefaultsOnly, Category = "Item Menu|Actions")
	TSubclassOf<UInventoryItemActionButton> ActionButtonClass;

	/**
	 * When an action picked in the menu runs: at once as the menu starts to close, or once it has closed. One that keeps the menu open
	 * runs at once.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Item Menu|Actions")
	EInventoryMenuActionTiming ActionTiming = EInventoryMenuActionTiming::AsMenuCloses;

	/**
	 * Holds the action buttons, such as a Vertical Box. Buttons made for actions go at its end, after any you placed in it. Collapsed while
	 * an inline prompt asks.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Item Menu|Actions", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> ActionPanel;

private:
	/** An action listed for the item, and the button showing it. */
	struct FShownAction
	{
		TWeakObjectPtr<UInventoryItemAction> Action;
		TWeakObjectPtr<UCommonButtonBase> Button;
		bool bAvailable = false;
		FText UnavailableReason;
	};

	/** The item widget a press off the menu is on: the item the menu is beside, or any other item on its screen. */
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

	// Actions
	void ClearActionButtons();
	void HandleActionClicked(int32 Index);
	void RunAction(UInventoryItemAction* Action, int32 Count);

	/** What an action asks before it runs, with {Item} filled in, and {Count} and {Max} left for the prompt to fill in as the count changes. */
	FModularPromptRequest MakePromptRequest(const UInventoryItemAction& Action, const FInventoryItemActionContext& Context) const;
	const FShownAction* FindShownAction(const UCommonButtonBase* Button) const;

	FInventoryItemHandle Item;
	TWeakObjectPtr<UInventoryScreenWidget> Screen;

	TWeakObjectPtr<UInventoryItemWidget> PressedItemWidget;
	FInventoryItemHandle PressedItem;

	/** Only listens while the menu is open. */
	FInventoryRegistryListener RegistryListener;

	TArray<FShownAction> ShownActions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInventoryItemActionButton>> MadeActionButtons;

#if WITH_EDITORONLY_DATA
	// Replaced by the context menu's Right Click and Input Config. Loaded only so PostLoad can carry a menu's settings over.
	UPROPERTY()
	EModularMenuOutsideClick RightPressOutside_DEPRECATED = EModularMenuOutsideClick::PassThrough;

	UPROPERTY()
	bool bCloseOnRightPress_DEPRECATED = true;

	UPROPERTY()
	bool bGameKeepsInput_DEPRECATED = false;

	// Renamed Show Description On Screen, now the context menu's. Loaded only so PostLoad can carry a menu's setting over.
	UPROPERTY()
	bool bShowHintOnScreen_DEPRECATED = true;
#endif
};
