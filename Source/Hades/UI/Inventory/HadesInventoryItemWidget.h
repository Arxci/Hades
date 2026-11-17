#pragma once

#include "CoreMinimal.h"

#include "InventoryUITypes.h"
#include "Widgets/InventoryItemWidget.h"

#include "HadesInventoryItemWidget.generated.h"

class UHadesInputKeyWidget;
class UHadesQuickSlotComponent;
class UTextBlock;


/**
 * An item in the inventory with the game's marks on it: the key of the quick slot it's on, whether it's in hand, and a gun's rounds.
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

	/** Shown only while Count Text or Ammo Text shows, such as the darker patch behind the number. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CountPatch;

	/** Shown while the item is in hand. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EquippedMarker;

	/** A gun's rounds, colored by how full it is. Collapsed for anything else. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AmmoText;

	UPROPERTY(EditAnywhere, Category = "Inventory|Item")
	FInventoryMeterColors AmmoColors;

private:
	UFUNCTION()
	void RefreshMarks();

	TWeakObjectPtr<UHadesQuickSlotComponent> QuickSlots;
};
