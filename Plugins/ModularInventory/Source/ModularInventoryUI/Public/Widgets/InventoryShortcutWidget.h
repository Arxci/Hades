#pragma once

#include "CoreMinimal.h"

#include "CommonButtonBase.h"
#include "GameplayTagContainer.h"

#include "InventoryTypes.h"
#include "InventoryUITypes.h"

#include "InventoryShortcutWidget.generated.h"

class APawn;
class UInventoryItemWidget;
class UInventoryShortcutComponent;
class UModularInputKeyWidget;
class UTextBlock;
struct FInventoryChangeSet;


/**
 * One slot of the owning player's Inventory Shortcut Component, such as on a HUD's shortcut bar or an inventory screen: the slot's item,
 * its name, number and key, whether it's the selected slot, and a way to fill it. While an item is held, clicking the slot or pressing
 * Accept on it puts the item on the slot and puts the held item back where it was, as the slot only points at it; dropping a dragged item
 * on the slot does the same. A right-click empties the slot. It's a Common UI button: style it like the cells.
 * Subclass it in C++ to show more of the slot's item, such as a gun's rounds, with Native On Refresh.
 */
UCLASS(Abstract)
class MODULARINVENTORYUI_API UInventoryShortcutWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	/** Shows another slot, such as for each slot of a bar built in a loop. Slot Tag, when set, wins. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shortcut")
	void SetSlotIndex(int32 InSlotIndex);

	/** The slot shown: the one with Slot Tag when it's set, else Slot Index. -1 when no slot has Slot Tag. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Shortcut")
	int32 GetSlotIndex() const;

	/** The slot's item; unset while it's empty. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Shortcut")
	FInventoryItemHandle GetSlotItem() const;

	/** The owning player's shortcut component, found on its pawn. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Shortcut")
	UInventoryShortcutComponent* GetShortcuts() const { return Shortcuts.Get(); }

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	//~ End UUserWidget Interface

	/** The slot's item changed. Also fires with an unset item when the slot empties. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Shortcut")
	void OnShortcutChanged(FInventoryItemHandle Item);

	/** The slot was activated, such as by its key, to flash it. Item is unset for an empty slot. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Shortcut")
	void OnShortcutActivated(FInventoryItemHandle Item);

	/** A held item was put on the slot, but the slot doesn't take it. The item is still held. Say why here, such as with a message. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Shortcut")
	void OnAssignRefused(FInventoryItemHandle Item);

	/** The slot became the selected one, or stopped being it, such as to light it on a hotbar. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Shortcut")
	void OnSelectedChanged(bool bIsSelected);

	/** After the slot was shown again, for C++ subclasses to show more of its item. */
	virtual void NativeOnRefresh(FInventoryItemHandle Item) {}

	/** Before On Assign Refused, for C++ subclasses. */
	virtual void NativeOnAssignRefused(FInventoryItemHandle Item) {}

	/** Whether a change in the inventory changes what the slot shows. By default, only a change to the slot's own item does. */
	virtual bool ShouldRefreshFor(const FInventoryChangeSet& Changes) const;

	/** Which slot of the component it shows, 0 for the first. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Shortcut", meta = (ClampMin = "0"))
	int32 SlotIndex = 0;

	/** Finds the slot by its tag instead, when set, such as Input.Shortcut.1, so it follows the slot if the slots are reordered. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Shortcut")
	FGameplayTag SlotTag;

	/** A right-click on it empties the slot. The item stays where it is. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Shortcut")
	bool bRightClickClears = true;

	/** The slot's item. Hidden while the slot is empty. It never takes clicks itself, so it can't be dragged out of the slot. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Shortcut", meta = (BindWidgetOptional))
	TObjectPtr<UInventoryItemWidget> ItemWidget;

	/** The slot's Display Name. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Shortcut", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	/** The slot's number, 1 for the first, such as the number key that activates it. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Shortcut", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NumberText;

	/** Shown only while the slot is empty, such as a dash or a faded outline. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Shortcut", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EmptyMarker;

	/** The key the player has mapped to the slot's input action, which follows rebinding. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Shortcut", meta = (BindWidgetOptional))
	TObjectPtr<UModularInputKeyWidget> KeyWidget;

	/** Shown only while the slot is the selected one, such as a frame around a hotbar's slot. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Shortcut", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelectedMarker;

private:
	UFUNCTION()
	void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);

	UFUNCTION()
	void HandleShortcutsChanged();

	UFUNCTION()
	void HandleShortcutActivated(int32 ActivatedSlot, FInventoryItemHandle Item);

	UFUNCTION()
	void HandleShortcutSelected(int32 SelectedSlot, FInventoryItemHandle Item);

	void HandleClicked();

	/** Puts the held item on the slot and puts it back where it was. False when nothing is held or the slot doesn't take it. */
	bool AssignHeldItem();

	void BindShortcuts(UInventoryShortcutComponent* NewShortcuts);

protected:
	/** Shows the slot as it is now. */
	void Refresh();

private:

	TWeakObjectPtr<UInventoryShortcutComponent> Shortcuts;
	FInventoryItemHandle ShownItem;
	bool bShownOnce = false;
	bool bShownSelected = false;

	/** The item's state, such as ammo, changes without the slots changing. */
	FInventoryRegistryListener RegistryListener;
};
