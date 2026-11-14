#pragma once

#include "CoreMinimal.h"

#include "CommonButtonBase.h"

#include "InventoryTypes.h"
#include "Widgets/InventoryItemMenuWidget.h"

#include "HadesShortcutMenu.generated.h"

class UCommonActionWidget;
class UHadesQuickSlotComponent;
class UInventoryItemWidget;
class UPanelWidget;
class UTextBlock;


/** One quick slot in the shortcut menu: its key's icon or number, and what's on it. */
UCLASS(Abstract)
class HADES_API UHadesShortcutEntry : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	/** Shows a quick slot. Key is the slot's row from the quick slot component's Slot Keys. */
	void SetShortcut(int32 InSlotIndex, const FDataTableRowHandle& Key, FInventoryItemHandle SlotItem);

	int32 GetSlotIndex() const { return SlotIndex; }

protected:
	//~ Begin UUserWidget Interface
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	//~ End UUserWidget Interface

	//~ Begin UCommonButtonBase Interface
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;
	//~ End UCommonButtonBase Interface

	/**
	 * The quick slot this entry shows, 0 being the first, for entries placed in the shortcut menu's designer, such as a cross laid
	 * out like the D-pad. Entries the menu makes for its Entry Panel are given theirs.
	 */
	UPROPERTY(EditAnywhere, Category = "Shortcut", meta = (ClampMin = "0"))
	int32 SlotIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Shortcut", meta = (BindWidgetOptional))
	TObjectPtr<UCommonActionWidget> KeyIcon;

	/** Slot Index + 1, such as a number in the slot's corner. */
	UPROPERTY(BlueprintReadOnly, Category = "Shortcut", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NumberText;

	/** Hidden while the slot is empty. */
	UPROPERTY(BlueprintReadOnly, Category = "Shortcut", meta = (BindWidgetOptional))
	TObjectPtr<UInventoryItemWidget> ItemWidget;

	/** The item's name, or Empty Text. */
	UPROPERTY(BlueprintReadOnly, Category = "Shortcut", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	/** Shown while the entry is hovered, or focused on a gamepad: a frame drawn over the slot's item, which would hide the button's own hover look. */
	UPROPERTY(BlueprintReadOnly, Category = "Shortcut", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Highlight;

	UPROPERTY(EditAnywhere, Category = "Shortcut")
	FText EmptyText;

private:
	void SetHighlighted(bool bInHighlighted);
};


/**
 * Opened by the item menu's Shortcut button: the quick slots. Picking a slot puts the item on it, moving it from any other;
 * picking the slot it's already on takes it off.
 * The entries are either made for Entry Panel, one per quick slot in order, or placed in the designer, each with its Slot Index,
 * such as a cross laid out like the D-pad, as in the RE2 remake. Don't put entries of your own inside Entry Panel: it's emptied each time.
 */
UCLASS(Abstract, Blueprintable)
class HADES_API UHadesShortcutMenu : public UInventoryItemMenuWidget
{
	GENERATED_BODY()

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	//~ Begin UCommonActivatableWidget Interface
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	//~ End UCommonActivatableWidget Interface

	//~ Begin UInventoryItemMenuWidget Interface
	virtual void NativeOnItemChanged() override;
	//~ End UInventoryItemMenuWidget Interface

	/** What the menu makes for Entry Panel. */
	UPROPERTY(EditDefaultsOnly, Category = "Shortcut Menu")
	TSubclassOf<UHadesShortcutEntry> EntryClass;

	/** Gets an entry per quick slot, in order, such as a Vertical Box. Leave it out when the entries are placed in the designer. */
	UPROPERTY(BlueprintReadOnly, Category = "Shortcut Menu", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> EntryPanel;

private:
	void HandleEntryClicked(int32 SlotIndex);
	UHadesQuickSlotComponent* GetQuickSlots() const;

	/** The shown entry for the slot, or null. */
	UHadesShortcutEntry* FindEntry(int32 SlotIndex) const;

	/** The entries placed in the designer, found once. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UHadesShortcutEntry>> PlacedEntries;

	/** Every entry: those placed in the designer, then those made for Entry Panel. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UHadesShortcutEntry>> Entries;
};
