#pragma once

#include "CoreMinimal.h"

#include "InventoryUITypes.h"
#include "Widgets/InventoryShortcutWidget.h"

#include "HadesQuickSlotWidget.generated.h"

class UTextBlock;


/**
 * One of the player's quick slots, such as in the HUD: the plugin's shortcut slot, with a gun's rounds and whether its item is in hand.
 * Putting a held item on it, by dropping it or pressing Accept, assigns the item to the slot. Right-click empties it.
 */
UCLASS(Abstract)
class HADES_API UHadesQuickSlotWidget : public UInventoryShortcutWidget
{
	GENERATED_BODY()

protected:
	//~ Begin UInventoryShortcutWidget Interface
	virtual void NativeOnRefresh(FInventoryItemHandle Item) override;
	virtual void NativeOnAssignRefused(FInventoryItemHandle Item) override;
	virtual bool ShouldRefreshFor(const FInventoryChangeSet& Changes) const override;
	//~ End UInventoryShortcutWidget Interface

	/** Highlight the slot here while its item is in hand. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Quick Slot")
	void OnInHandChanged(bool bIsInHand);

	/** The rounds in a gun on the slot, colored by how full it is. Collapsed for anything else. */
	UPROPERTY(BlueprintReadOnly, Category = "Quick Slot", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AmmoText;

	/** The rounds for that gun in the inventory, such as "/30". */
	UPROPERTY(BlueprintReadOnly, Category = "Quick Slot", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SpareAmmoText;

	UPROPERTY(EditAnywhere, Category = "Quick Slot")
	FInventoryMeterColors AmmoColors;

private:
	bool bInHand = false;
};
