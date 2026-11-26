#pragma once

#include "CoreMinimal.h"

#include "CommonButtonBase.h"

#include "InventoryTypes.h"
#include "Widgets/InventoryItemMenuWidget.h"

#include "InventoryShortcutMenuWidget.generated.h"

class UInventoryItemWidget;
class UInventoryShortcutComponent;
class UModularInputKeyWidget;
class UTextBlock;


/** One shortcut slot in a shortcut menu: its key, its number and name, and the item on it now. */
UCLASS(Abstract)
class MODULARINVENTORYUI_API UInventoryShortcutMenuEntry : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	/** Shows a slot of Shortcuts. The menu calls it for the entries it makes, and for those placed in its designer, with their own Slot Index. */
	void SetShortcutSlot(const UInventoryShortcutComponent* Shortcuts, int32 InSlotIndex);

	int32 GetSlotIndex() const { return SlotIndex; }

protected:
	//~ Begin UUserWidget Interface
	virtual void NativePreConstruct() override;
	//~ End UUserWidget Interface

	/** The slot was shown, such as to mark the slot the item is on already. Item is the slot's item now, unset while it's empty. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Shortcut Menu Entry")
	void OnShortcutSlotShown(int32 ShownSlotIndex, FInventoryItemHandle Item);

	/**
	 * The slot this entry shows, 0 being the first, for entries placed in the shortcut menu's designer, such as a cross laid out like the
	 * D-pad. Entries the menu makes for its Entry Panel are given theirs.
	 */
	UPROPERTY(EditAnywhere, Category = "Shortcut Menu Entry", meta = (ClampMin = "0"))
	int32 SlotIndex = 0;

	/** The key the player has mapped to the slot's input action, which follows rebinding. */
	UPROPERTY(BlueprintReadOnly, Category = "Shortcut Menu Entry", meta = (BindWidgetOptional))
	TObjectPtr<UModularInputKeyWidget> KeyWidget;

	/** Slot Index + 1, such as a number in the slot's corner. */
	UPROPERTY(BlueprintReadOnly, Category = "Shortcut Menu Entry", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NumberText;

	/** The slot's Display Name, such as Quick Use. */
	UPROPERTY(BlueprintReadOnly, Category = "Shortcut Menu Entry", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SlotNameText;

	/**
	 * The item on the slot, upright, laid out on cells of Item Cell Size as in an inventory grid. Hidden, not collapsed, while the slot is
	 * empty, so a Size Box around it keeps its size.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Shortcut Menu Entry", meta = (BindWidgetOptional))
	TObjectPtr<UInventoryItemWidget> ItemWidget;

	/** The name of the item on the slot, or Empty Text. */
	UPROPERTY(BlueprintReadOnly, Category = "Shortcut Menu Entry", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	/** What Name Text says while the slot is empty. */
	UPROPERTY(EditAnywhere, Category = "Shortcut Menu Entry")
	FText EmptyText;

	/**
	 * Lays Item Widget out as a grid does, on cells this size: a 2x1 item is two cells wide, its icon unstretched. Match the inventory
	 * grid's Cell Size. 0 leaves Item Widget at its icon's own size.
	 */
	UPROPERTY(EditAnywhere, Category = "Shortcut Menu Entry", meta = (ClampMin = "0"))
	float ItemCellSize = 0.0f;

	/** Space between those cells, for items bigger than one cell. Match the inventory grid's Cell Gap. */
	UPROPERTY(EditAnywhere, Category = "Shortcut Menu Entry", meta = (ClampMin = "0", EditCondition = "ItemCellSize > 0"))
	float ItemCellGap = 0.0f;
};


/**
 * Picks one of the player's shortcut slots for an item, such as the quick slot keys or a weapon wheel's spokes. The Assign Shortcut item
 * action opens it from an item menu; it opens where the screen opens item menus, replacing that menu.
 * Beside the item, it's a context menu like any other. For a modal picker, as some survival horror games do, set Placement to As Designed,
 * lay Menu Panel out in the middle of the screen, and give the root a dim full-screen backdrop.
 * Picking a slot puts the item on it, moving it from any other slot; picking the slot it's on already takes it off. Its entries are made
 * for Entry Panel, one per slot in order, or placed in the designer, each with its Slot Index, such as a cross laid out like the D-pad.
 * Don't put entries of your own inside Entry Panel: it's emptied each time. It can list item actions too, under the slots.
 */
UCLASS(Abstract, Blueprintable)
class MODULARINVENTORYUI_API UInventoryShortcutMenuWidget : public UInventoryItemMenuWidget
{
	GENERATED_BODY()

public:
	/** Whether a shortcut slot takes the item, or it's on one already, or it has an action to list. */
	virtual bool CanOpenFor(const FInventoryItemActionContext& Context) const override;

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	//~ Begin UCommonActivatableWidget Interface
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	//~ End UCommonActivatableWidget Interface

	//~ Begin UModularContextMenuWidget Interface
	virtual FText GetEntryDescription(UCommonButtonBase* Entry) const override;
	//~ End UModularContextMenuWidget Interface

	//~ Begin UInventoryItemMenuWidget Interface
	virtual void NativeOnItemChanged() override;
	//~ End UInventoryItemMenuWidget Interface

	/** A slot refused the item, such as one only for weapons. The menu stays open. Say why here, after Refused Hint shows. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Shortcut Menu")
	void OnAssignRefused(int32 SlotIndex, FInventoryItemHandle RefusedItem);

	/** The player's shortcut component, found on the owning player's pawn. */
	UInventoryShortcutComponent* GetShortcuts() const;

	/** What the menu makes for Entry Panel, one per slot. */
	UPROPERTY(EditDefaultsOnly, Category = "Shortcut Menu")
	TSubclassOf<UInventoryShortcutMenuEntry> SlotEntryClass;

	/** Picking a slot closes the menu. Off, it stays open showing the slots as they are now, such as to fill several. */
	UPROPERTY(EditDefaultsOnly, Category = "Shortcut Menu")
	bool bCloseOnPick = true;

	/** Said in Description Text, the menu's and the screen's, while an entry is highlighted, such as "Assign to this slot." Empty says nothing. */
	UPROPERTY(EditDefaultsOnly, Category = "Shortcut Menu|Hints")
	FText AssignHint;

	/** Said instead while the highlighted slot holds this item already, such as "Remove from this slot." */
	UPROPERTY(EditDefaultsOnly, Category = "Shortcut Menu|Hints")
	FText ClearHint;

	/** Said when a slot refuses the item. */
	UPROPERTY(EditDefaultsOnly, Category = "Shortcut Menu|Hints")
	FText RefusedHint;

private:
	void HandleEntryClicked(UInventoryShortcutMenuEntry* Entry);

	/** The shown entry for the slot, or null. */
	UInventoryShortcutMenuEntry* FindEntry(int32 SlotIndex) const;

	/** Shows each slot as it is now. */
	void RefreshEntries();

	/** The entries placed in the designer, found once. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInventoryShortcutMenuEntry>> PlacedEntries;

	/** Every entry: those placed in the designer, then those made for Entry Panel. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInventoryShortcutMenuEntry>> SlotEntries;
};
