#pragma once

#include "CoreMinimal.h"

#include "CommonActivatableWidget.h"
#include "Engine/DataTable.h"
#include "Input/UIActionBindingHandle.h"

#include "InventoryTypes.h"
#include "InventoryUITypes.h"

#include "InventoryScreenWidget.generated.h"

class UInventoryGridWidget;
class UInventoryItemWidget;
class UInventoryUISubsystem;
class USoundBase;
struct FUIInputConfig;
enum class ECommonInputType : uint8;


/**
 * A Common UI screen for an inventory: push it onto a layer stack to open it, and deactivate it to close it. Lay out its grids, lists,
 * slots and details in the designer.
 * While it's active, Interaction Style decides how items are handled, and each action is bound only while it can work, so an action
 * bar shows what works now. Back stops choosing an item, or puts back the held item, before it closes the screen, and closing it
 * drops whatever was held or chosen. A right-click or the mouse wheel anywhere on it can put back or turn the held item, as the style says.
 */
UCLASS(Abstract, Blueprintable)
class MODULARINVENTORYUI_API UInventoryScreenWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UInventoryScreenWidget(const FObjectInitializer& ObjectInitializer);

	//~ Begin UCommonActivatableWidget Interface
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	//~ End UCommonActivatableWidget Interface

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

	/** Whether the items are on screen, such as while a tabbed screen shows their page. Item menus and the actions only work while they are. */
	virtual bool IsInventoryShown() const { return true; }

	/** Binds each action while it can work. Override to bind your own with Set Action Bound, after calling Super. */
	virtual void RefreshActions();

	/** Registers Row while bBound and unregisters it otherwise. */
	void SetActionBound(FUIActionBindingHandle& Handle, const FDataTableRowHandle& Row, bool bBound, const FSimpleDelegate& Handler);

	/** Before On Item Menu Requested, for C++ subclasses. */
	virtual void NativeOnItemMenuRequested(FInventoryItemHandle Item) {}

	/**
	 * An item asked for its menu: clicked with Click Opens Item Menu on, right-clicked, or passed to Request Context. Open your item menu
	 * here, such as an Inventory Item Menu Widget pushed onto a modal layer, then given the item and its Find Item Widget.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Screen")
	void OnItemMenuRequested(FInventoryItemHandle Item);

	UInventoryUISubsystem* GetInteraction() const;

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
	/** Picks up the item under the cursor or focus, or puts the held item down where it's aimed. How keys pick items up when clicks open item menus. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle MoveAction;

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

	void PlayInventorySound(USoundBase* Sound) const;

	void HandleInputMethodChanged(ECommonInputType InputType);

	/** Binds Confirm only to show it, without taking Accept from the focused cell, while bShown. */
	void SetConfirmShown(bool bShown);

	void HandleConfirmAction();
	void HandleMoveAction();
	void HandleRotateAction();
	void HandleSplitAction();
	void HandleTransferAction();
	void HandleTakeAllAction();

	FInventoryInteractionStyle PreviousStyle;
	FInventoryContainerHandle TransferContainer;

	/** The spot last aimed at while the screen is open, which focus goes back to. */
	FInventoryContainerHandle LastAimedContainer;
	FInventoryPlacement LastAimedPlacement;

	/** The item hovered as the screen last closed, for Start On Last Item. */
	FInventoryItemHandle LastHoveredItem;

	/** Aim At Start is hovering the item the screen starts on, which plays no hover sound. */
	bool bAimingAtStart = false;

	FUIActionBindingHandle ConfirmHandle;
	FUIActionBindingHandle ConfirmShownHandle;
	FUIActionBindingHandle MoveHandle;
	FUIActionBindingHandle RotateHandle;
	FUIActionBindingHandle SplitHandle;
	FUIActionBindingHandle TransferHandle;
	FUIActionBindingHandle TakeAllHandle;
};
