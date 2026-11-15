#pragma once

#include "CoreMinimal.h"

#include "Subsystems/LocalPlayerSubsystem.h"

#include "InventoryTypes.h"
#include "InventoryUITypes.h"

#include "InventoryUISubsystem.generated.h"

class UInventoryComponent;
class ULocalPlayer;
struct FInventoryChangeSet;
struct FInventoryRegistry;


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInventoryUIHeldItemChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInventoryUITargetChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInventoryUIHoveredItemChangedSignature, FInventoryItemHandle, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInventoryUIItemSignature, FInventoryItemHandle, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInventoryUIItemChosenSignature, FInventoryItemHandle, Item, FInventoryItemHandle, Chosen);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInventoryUIDroppedOutsideSignature, FInventoryItemHandle, Item, int32, Count);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInventoryUIHoldEventSignature, EInventoryHoldEvent, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInventoryUIPutDownSignature, FInventoryItemHandle, Item, EInventoryHoldEvent, Event);


/**
 * The player's inventory interaction, shared by every inventory widget: the item being held, where it would land, and what's under the cursor.
 * Mouse drag-and-drop and gamepad pick-up-and-place both go through here, so they follow the same rules.
 * Holding an item changes nothing in the inventory; only placing it does, so cancelling is always safe.
 */
UCLASS()
class MODULARINVENTORYUI_API UInventoryUISubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	static UInventoryUISubsystem* Get(const ULocalPlayer* LocalPlayer);

	//~ Begin USubsystem Interface
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	// Style
	/** Inventory screens set their own while they're open. */
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void SetStyle(const FInventoryInteractionStyle& InStyle);

	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	FInventoryInteractionStyle GetStyle() const { return Style; }

	// Holding
	/**
	 * Count 0 holds the whole stack. Items that can't split are always held whole.
	 * In a grid, the item is held by the cell aimed at, when that's one of its cells, such as the focused one or the one under the cursor;
	 * otherwise by its middle cell. That cell then goes wherever the held item is aimed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	bool BeginHold(FInventoryItemHandle Item, int32 Count = 0);

	/** False for items in read-only containers, and while choosing an item. */
	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	bool CanBeginHold(FInventoryItemHandle Item) const;

	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void CancelHold();

	/** Turns the held item a quarter turn around the cell it's held by, if Can Rotate Held. */
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void RotateHeld();

	/** Whether the held item can turn, and could still fit the grid it's over turned: the targeted one, or else the one it's in. */
	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	bool CanRotateHeld() const;

	/** Halves the held count. From one, goes back to the whole stack. */
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void SplitHeld();

	/** Whether the held item is a stack of more than one that can split. */
	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	bool CanSplitHeld() const;

	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void SetHeldCount(int32 Count);

	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	bool IsHolding() const { return HeldItem.IsSet(); }

	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	FInventoryItemHandle GetHeldItem() const { return HeldItem; }

	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	int32 GetHeldCount() const { return HeldCount; }

	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	bool IsHeldRotated() const { return bHeldRotated; }

	/** Cells the held item takes in its held rotation. */
	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	FIntPoint GetHeldSize() const;

	// Placing. In a grid, a placement passed in is the cell aimed at: the held item goes with the cell it's held by there.
	/** Puts the held item there: on a free spot, onto a stack it can join, or by swapping with an item of its size there. */
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	bool PlaceHeld(FInventoryContainerHandle Container, const FInventoryPlacement& Placement);

	/** What a click or Accept does: places the held item, or picks up the item there or asks for its item menu. */
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	bool ActivatePlacement(FInventoryContainerHandle Container, const FInventoryPlacement& Placement);

	/** What a Move action does, whatever the style: puts the held item down where it's aimed, or picks up the hovered item. */
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	bool PickUpOrPlace();

	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	bool CanPickUpOrPlace() const;

	/**
	 * What a Confirm key does with the mouse, where keyboard focus isn't on what the mouse is: what a click on the hovered item does, or
	 * puts the held item down where it's aimed. On the focused cell, a gamepad's Accept does the same.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	bool ActivateAimed();

	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	bool CanActivateAimed() const;

	/** What placing the held item there would do, worked out with the real rules but without changing anything. */
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	EInventoryDropAction PreviewPlacement(FInventoryContainerHandle Container, const FInventoryPlacement& Placement);

	// Target: where the cursor or gamepad focus points. Widgets preview the held item there.
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void SetTarget(FInventoryContainerHandle Container, const FInventoryPlacement& Placement);

	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void ClearTarget();

	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	FInventoryContainerHandle GetTargetContainer() const { return TargetContainer; }

	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	FInventoryPlacement GetTargetPlacement() const { return TargetPlacement; }

	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	EInventoryDropAction GetTargetAction() const { return TargetAction; }

	/**
	 * Why the held item can't go where it's aimed, such as Occupied for another item in the way, Out Of Bounds for locked cells, or
	 * Rejected for a container that won't take it. None unless Get Target Action is Invalid.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	EInventoryError GetTargetError() const { return TargetError; }

	/** Where the held item would land at the target. In a grid: exactly on an item of its size there, or moved in from the edge so it fits. */
	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	FInventoryPlacement GetLandingPlacement() const { return FitInGrid(TargetContainer, TargetPlacement); }

	// Hover: the item under the cursor or gamepad focus, for details panels.
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void SetHoveredItem(FInventoryItemHandle Item);

	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	FInventoryItemHandle GetHoveredItem() const { return HoveredItem; }

	// Selection: the item being acted on, such as the one whose item menu is open. Item widgets show it with On Selected Changed.
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void SetSelectedItem(FInventoryItemHandle Item);

	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	FInventoryItemHandle GetSelectedItem() const { return SelectedItem; }

	// Item menus
	/**
	 * Asks whoever shows item menus, usually the screen, to open one. Item widgets call it on right click, unless the style turns that
	 * off, and clicks and Accept do with Click Opens Item Menu; call it from your own binding otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void RequestContext(FInventoryItemHandle Item);

	// Choosing: for "use this on that" actions, such as combining two items.
	/**
	 * Until another item is chosen, a click or Accept on an item fires On Item Chosen instead of picking it up. Puts down anything held,
	 * and selects Item, the one being used, until the choosing ends.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void BeginChoosingItem(FInventoryItemHandle Item);

	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void CancelChoosingItem();

	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	bool IsChoosingItem() const { return bChoosingItem; }

	/** The item Begin Choosing Item was called for. */
	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	FInventoryItemHandle GetChoosingFor() const { return ChoosingFor; }

	// Transfer: moving items straight between the player and another container, such as an open chest.
	/** The other side, such as an open chest's container. Inventory screens set their own while they're open. */
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void SetTransferContainer(FInventoryContainerHandle Container) { TransferContainer = Container; }

	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	FInventoryContainerHandle GetTransferContainer() const { return TransferContainer; }

	/** Moves as much of the item as fits to the other side: into the transfer container, or out of it into the player's storage. */
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	bool TransferItem(FInventoryItemHandle Item);

	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	bool CanTransferItem(FInventoryItemHandle Item) const;

	/** Moves as much as fits of everything in the transfer container into the player's storage. True when nothing is left behind. */
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	bool TakeAll();

	/** Whether the transfer container holds anything to take. */
	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	bool CanTakeAll() const;

	/** Drops the hold, the choosing, the target, the hover and the selection. Call it when an inventory screen closes. */
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void ClearInteraction();

	/** Picked up, put down, rotated or split. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory UI")
	FInventoryUIHeldItemChangedSignature OnHeldItemChanged;

	/**
	 * What just happened to the held item, for sounds and effects: picked up, placed, merged, swapped, refused, rotated, split or put back.
	 * After On Held Item Changed. Holds that end because the item went away, was transferred, or the screen closed send nothing.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Inventory UI")
	FInventoryUIHoldEventSignature OnHoldEvent;

	/**
	 * The held item went down, with the item now there: the one placed, the stack it joined, or the item put back. Event is Placed,
	 * Merged, Swapped or Put Back. After On Hold Event, once grids and lists show the item where it went. Item widgets showing it get
	 * On Put Down.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Inventory UI")
	FInventoryUIPutDownSignature OnItemPutDown;

	/** The target moved, or what placing there would do changed. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory UI")
	FInventoryUITargetChangedSignature OnTargetChanged;

	UPROPERTY(BlueprintAssignable, Category = "Inventory UI")
	FInventoryUIHoveredItemChangedSignature OnHoveredItemChanged;

	UPROPERTY(BlueprintAssignable, Category = "Inventory UI")
	FInventoryUIItemSignature OnSelectedItemChanged;

	UPROPERTY(BlueprintAssignable, Category = "Inventory UI")
	FInventoryUIItemSignature OnContextRequested;

	/** Chosen was picked after Begin Choosing Item was called for Item. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory UI")
	FInventoryUIItemChosenSignature OnItemChosen;

	/**
	 * A mouse drag of the held item ended outside every inventory widget, such as to drop it in the world, as many games do.
	 * Never with Snap Held Item, which puts the item where it's drawn instead. The hold is dropped after.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Inventory UI")
	FInventoryUIDroppedOutsideSignature OnDroppedOutside;

private:
	FInventoryRegistry* GetRegistry() const;
	UInventoryComponent* GetPlayerInventory() const;
	TArray<FInventoryContainerHandle> GetTransferTargets(FInventoryItemHandle Item) const;
	FInventoryPlacement FitInGrid(FInventoryContainerHandle Container, const FInventoryPlacement& Placement) const;
	EInventoryDropAction ResolvePlacement(FInventoryContainerHandle Container, const FInventoryPlacement& Placement, bool bApply);

	/** Also says why, when it's Invalid. */
	EInventoryDropAction ResolvePlacement(FInventoryContainerHandle Container, const FInventoryPlacement& Placement, bool bApply, EInventoryError& OutError);
	/** Picked Up is announced unless bAnnounce is false, such as for the item a swap picks up. */
	bool BeginHoldBy(FInventoryItemHandle Item, int32 Count, const FIntPoint& GrabCell, bool bAnnounce = true);

	/** Cancel Hold without Put Back, for holds that end some other way. */
	void EndHold();
	void SetHeld(FInventoryItemHandle Item, int32 Count, bool bRotated);

	/** Which of the item's cells is aimed at, from its top-left. Unset unless the target is one of its cells in a grid. */
	TOptional<FIntPoint> GetTargetedCell(FInventoryItemHandle Item) const;
	void RefreshTarget();
	void HandleRegistryChanged(const FInventoryChangeSet& Changes);

	FInventoryInteractionStyle Style;

	FInventoryItemHandle HeldItem;
	int32 HeldCount = 0;
	bool bHeldRotated = false;

	/** The cell of the held item it's held by, from its top-left as it's turned now. In a grid, it goes on the cell aimed at. */
	FIntPoint HeldGrabCell = FIntPoint::ZeroValue;

	FInventoryContainerHandle TargetContainer;
	FInventoryPlacement TargetPlacement;
	EInventoryDropAction TargetAction = EInventoryDropAction::None;
	EInventoryError TargetError = EInventoryError::None;

	FInventoryItemHandle HoveredItem;
	FInventoryItemHandle SelectedItem;

	bool bChoosingItem = false;
	FInventoryItemHandle ChoosingFor;

	FInventoryContainerHandle TransferContainer;

	/** Only listens while holding, to drop the hold when the item goes away. */
	FInventoryRegistryListener RegistryListener;
};
