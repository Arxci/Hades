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


/** One quick slot in the shortcut menu: its key's icon and what's on it. */
UCLASS(Abstract)
class HADES_API UHadesShortcutEntry : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	/** Key is the slot's row from the quick slot component's Slot Keys. */
	void SetShortcut(const FDataTableRowHandle& Key, FInventoryItemHandle SlotItem);

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Shortcut", meta = (BindWidgetOptional))
	TObjectPtr<UCommonActionWidget> KeyIcon;

	/** Hidden while the slot is empty. */
	UPROPERTY(BlueprintReadOnly, Category = "Shortcut", meta = (BindWidgetOptional))
	TObjectPtr<UInventoryItemWidget> ItemWidget;

	/** The item's name, or Empty Text. */
	UPROPERTY(BlueprintReadOnly, Category = "Shortcut", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(EditAnywhere, Category = "Shortcut")
	FText EmptyText;
};


/**
 * Opened by the item menu's Shortcut button: the quick slots in a list. Picking a slot puts the item on it, moving it from any other;
 * picking the slot it's already on takes it off.
 */
UCLASS(Abstract, Blueprintable)
class HADES_API UHadesShortcutMenu : public UInventoryItemMenuWidget
{
	GENERATED_BODY()

protected:
	//~ Begin UCommonActivatableWidget Interface
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	//~ End UCommonActivatableWidget Interface

	//~ Begin UInventoryItemMenuWidget Interface
	virtual void NativeOnItemChanged() override;
	//~ End UInventoryItemMenuWidget Interface

	UPROPERTY(EditDefaultsOnly, Category = "Shortcut Menu")
	TSubclassOf<UHadesShortcutEntry> EntryClass;

	/** Gets an entry per quick slot, in order, such as a Vertical Box. */
	UPROPERTY(BlueprintReadOnly, Category = "Shortcut Menu", meta = (BindWidget))
	TObjectPtr<UPanelWidget> EntryPanel;

private:
	void HandleEntryClicked(int32 SlotIndex);
	UHadesQuickSlotComponent* GetQuickSlots() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHadesShortcutEntry>> Entries;
};
