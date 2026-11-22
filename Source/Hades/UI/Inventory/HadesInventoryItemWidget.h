#pragma once

#include "CoreMinimal.h"

#include "Widgets/InventoryItemWidget.h"

#include "HadesInventoryItemWidget.generated.h"

class UHadesInputKeyWidget;
class UHadesQuickSlotComponent;
class UTextBlock;


/**
 * An item in the inventory with the game's marks on it: the key of the quick slot it's on, and whether it's in hand.
 * Numbers such as a gun's rounds and a stack's count are the plugin's Value Displays, set in Class Defaults:
 * - Tag Value Item.Stat.Rounds, Max Tag Item.Stat.Capacity, Tint By Fullness, Text Widget AmmoText.
 * - Stack Count, Text Widget CountText, Format x{Value}.
 * Both with Group Corner, so only the first that applies to an item shows, and Frame Widget CountPatch, so the patch shows behind it.
 * A display for another number in the same corner, such as a durability bar, goes in Group Corner too.
 * The plugin's item widget highlights it and hides its Background on the stand-in for a moved item.
 */
UCLASS(Abstract)
class HADES_API UHadesInventoryItemWidget : public UInventoryItemWidget
{
	GENERATED_BODY()

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	//~ Begin UInventoryItemWidget Interface
	virtual void NativeOnItemChanged() override;
	//~ End UInventoryItemWidget Interface

	/** The key the player has bound to the item's quick slot, which follows rebinding. It collapses itself while the item has no quick slot. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UHadesInputKeyWidget> QuickSlotKeyWidget;

	/** The number of the quick slot the item is on, 1 for the first, such as on a badge in the corner. Collapsed while it has none. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuickSlotNumberText;

	/** Shown only while the item is on a quick slot, such as the corner Quick Slot Key Widget sits on. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> QuickSlotMarker;

	/** Shown while the item is in hand. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EquippedMarker;

private:
	UFUNCTION()
	void RefreshMarks();

	TWeakObjectPtr<UHadesQuickSlotComponent> QuickSlots;
};
