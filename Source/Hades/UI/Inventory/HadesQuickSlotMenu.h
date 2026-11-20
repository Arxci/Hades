#pragma once

#include "CoreMinimal.h"

#include "CommonButtonBase.h"

#include "InventoryTypes.h"
#include "Widgets/InventoryItemMenuWidget.h"

#include "HadesQuickSlotMenu.generated.h"

class UHadesInputKeyWidget;
class UHadesQuickSlotComponent;
class UInputAction;
class UInventoryItemWidget;
class UPanelWidget;
class UTextBlock;


/** One quick slot in the quick slot menu: its key's icon or number, and what's on it. */
UCLASS(Abstract)
class HADES_API UHadesQuickSlotMenuEntry : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	/** Shows a quick slot. Slot Input is the input action that activates it, from the quick slot component's Get Slot Input Action. */
	void SetQuickSlot(int32 InSlotIndex, const UInputAction* SlotInput, FInventoryItemHandle SlotItem);

	int32 GetSlotIndex() const { return SlotIndex; }

protected:
	//~ Begin UUserWidget Interface
	virtual void NativePreConstruct() override;
	//~ End UUserWidget Interface

	/**
	 * The quick slot this entry shows, 0 being the first, for entries placed in the quick slot menu's designer, such as a cross laid out
	 * like the D-pad. Entries the menu makes for its Entry Panel are given theirs.
	 */
	UPROPERTY(EditAnywhere, Category = "Quick Slot", meta = (ClampMin = "0"))
	int32 SlotIndex = 0;

	/** The key the player has bound to the slot, which follows rebinding. */
	UPROPERTY(BlueprintReadOnly, Category = "Quick Slot", meta = (BindWidgetOptional))
	TObjectPtr<UHadesInputKeyWidget> KeyWidget;

	/** Slot Index + 1, such as a number in the slot's corner. */
	UPROPERTY(BlueprintReadOnly, Category = "Quick Slot", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NumberText;

	/**
	 * The item on the slot, upright, laid out on cells of Item Cell Size as in the inventory grid. Hidden, not collapsed, while the slot
	 * is empty, so a Size Box around it keeps its size.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Quick Slot", meta = (BindWidgetOptional))
	TObjectPtr<UInventoryItemWidget> ItemWidget;

	/** The item's name, or Empty Text. */
	UPROPERTY(BlueprintReadOnly, Category = "Quick Slot", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(EditAnywhere, Category = "Quick Slot")
	FText EmptyText;

	/**
	 * Lays Item Widget out as the grid does, on cells this size: a 2x1 item is two cells wide, its icon unstretched. Match the inventory
	 * grid's Cell Size. 0 leaves Item Widget at its icon's own size.
	 */
	UPROPERTY(EditAnywhere, Category = "Quick Slot", meta = (ClampMin = "0"))
	float ItemCellSize = 0.0f;

	/** Space between those cells, for items bigger than one cell. Match the inventory grid's Cell Gap. */
	UPROPERTY(EditAnywhere, Category = "Quick Slot", meta = (ClampMin = "0", EditCondition = "ItemCellSize > 0"))
	float ItemCellGap = 0.0f;
};


/**
 * The quick slots, opened beside an item by the item menu's Assign Quick Slot action. Picking a slot puts the item on it, moving it from
 * any other; picking the slot it's already on takes it off.
 * The entries are either made for Entry Panel, one per quick slot in order, or placed in the designer, each with its Slot Index, such as
 * a cross laid out like the D-pad. Don't put entries of your own inside Entry Panel: it's emptied each time.
 * It comes and goes as the item menu does.
 */
UCLASS(Abstract, Blueprintable)
class HADES_API UHadesQuickSlotMenu : public UInventoryItemMenuWidget
{
	GENERATED_BODY()

public:
	UHadesQuickSlotMenu(const FObjectInitializer& ObjectInitializer);

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
	UPROPERTY(EditDefaultsOnly, Category = "Quick Slot Menu")
	TSubclassOf<UHadesQuickSlotMenuEntry> EntryClass;

	/** Gets an entry per quick slot, in order, such as a Vertical Box. Leave it out when the entries are placed in the designer. */
	UPROPERTY(BlueprintReadOnly, Category = "Quick Slot Menu", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> EntryPanel;

private:
	void HandleEntryClicked(int32 SlotIndex);
	UHadesQuickSlotComponent* GetQuickSlots() const;

	/** The shown entry for the slot, or null. */
	UHadesQuickSlotMenuEntry* FindEntry(int32 SlotIndex) const;

	/** The entries placed in the designer, found once. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UHadesQuickSlotMenuEntry>> PlacedEntries;

	/** Every entry: those placed in the designer, then those made for Entry Panel. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UHadesQuickSlotMenuEntry>> Entries;
};
