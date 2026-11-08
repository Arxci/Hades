#pragma once

#include "CoreMinimal.h"

#include "Subsystems/LocalPlayerSubsystem.h"

#include "InventoryTypes.h"
#include "InventoryUITypes.h"

#include "InventoryUISubsystem.generated.h"

class ULocalPlayer;
struct FInventoryChangeSet;
struct FInventoryRegistry;


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInventoryUIHeldItemChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInventoryUITargetChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInventoryUIHoveredItemChangedSignature, FInventoryItemHandle, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInventoryUIItemSignature, FInventoryItemHandle, Item);


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

	// Holding
	/** Count 0 holds the whole stack. Items that can't split are always held whole. */
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	bool BeginHold(FInventoryItemHandle Item, int32 Count = 0);

	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void CancelHold();

	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void RotateHeld();

	/** Halves the held count. From one, goes back to the whole stack. */
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void SplitHeld();

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

	// Placing
	/** Puts the held item there: on a free spot, onto a stack it can join, or by swapping with the item there. */
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	bool PlaceHeld(FInventoryContainerHandle Container, const FInventoryPlacement& Placement);

	/** What a click or Accept does: places the held item there, or picks up the item there. */
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	bool ActivatePlacement(FInventoryContainerHandle Container, const FInventoryPlacement& Placement);

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

	// Hover: the item under the cursor or gamepad focus, for details panels.
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void SetHoveredItem(FInventoryItemHandle Item);

	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	FInventoryItemHandle GetHoveredItem() const { return HoveredItem; }

	// Item menus
	/** Asks whoever shows item menus, usually the screen, to open one. Item widgets call it on right click; call it from your gamepad binding. */
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void RequestContext(FInventoryItemHandle Item);

	// Choosing: for "use this on that" actions, such as combining two items.
	/** Until an item is chosen, a click or Accept on an item fires On Item Chosen instead of picking it up. Puts down anything held. */
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void BeginChoosingItem();

	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void CancelChoosingItem() { bChoosingItem = false; }

	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	bool IsChoosingItem() const { return bChoosingItem; }

	/** Drops the hold, the choosing, the target and the hover. Call it when an inventory screen closes. */
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void ClearInteraction();

	/** Picked up, put down, rotated or split. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory UI")
	FInventoryUIHeldItemChangedSignature OnHeldItemChanged;

	/** The target moved, or what placing there would do changed. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory UI")
	FInventoryUITargetChangedSignature OnTargetChanged;

	UPROPERTY(BlueprintAssignable, Category = "Inventory UI")
	FInventoryUIHoveredItemChangedSignature OnHoveredItemChanged;

	UPROPERTY(BlueprintAssignable, Category = "Inventory UI")
	FInventoryUIItemSignature OnContextRequested;

	/** The item picked after Begin Choosing Item. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory UI")
	FInventoryUIItemSignature OnItemChosen;

private:
	FInventoryRegistry* GetRegistry() const;
	EInventoryDropAction ResolvePlacement(FInventoryContainerHandle Container, const FInventoryPlacement& Placement, bool bApply);
	void SetHeld(FInventoryItemHandle Item, int32 Count, bool bRotated);
	void RefreshTarget();
	void HandleRegistryChanged(const FInventoryChangeSet& Changes);

	FInventoryItemHandle HeldItem;
	int32 HeldCount = 0;
	bool bHeldRotated = false;

	FInventoryContainerHandle TargetContainer;
	FInventoryPlacement TargetPlacement;
	EInventoryDropAction TargetAction = EInventoryDropAction::None;

	FInventoryItemHandle HoveredItem;

	bool bChoosingItem = false;

	/** Only listens while holding, to drop the hold when the item goes away. */
	FInventoryRegistryListener RegistryListener;
};
