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
struct FUIInputConfig;


/**
 * A Common UI screen for an inventory: push it onto a layer stack to open it, and deactivate it to close it. Lay out its grids, lists,
 * slots and details in the designer.
 * While it's active, Interaction Style decides how items are handled, and each action is bound only while it can work, so an action
 * bar shows what works now. Back stops choosing an item, or puts back the held item, before it closes the screen, and closing it
 * drops whatever was held or chosen.
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

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
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

	/** Gamepad focus starts on its first cell, unless Get Desired Focus Target is implemented. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Screen", meta = (BindWidgetOptional))
	TObjectPtr<UInventoryGridWidget> MainGrid;

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

	/** Only shown in an action bar, for what Accept does on the focused cell. Accept itself still goes to the cell. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle ConfirmAction;

private:
	UFUNCTION()
	void HandleItemMenuRequested(FInventoryItemHandle Item);

	UFUNCTION()
	void HandleInteractionChanged();

	UFUNCTION()
	void HandleHoveredItemChanged(FInventoryItemHandle Item);

	void HandleMoveAction();
	void HandleRotateAction();
	void HandleSplitAction();
	void HandleTransferAction();
	void HandleTakeAllAction();

	FInventoryInteractionStyle PreviousStyle;
	FInventoryContainerHandle TransferContainer;

	FUIActionBindingHandle MoveHandle;
	FUIActionBindingHandle RotateHandle;
	FUIActionBindingHandle SplitHandle;
	FUIActionBindingHandle TransferHandle;
	FUIActionBindingHandle TakeAllHandle;
};
