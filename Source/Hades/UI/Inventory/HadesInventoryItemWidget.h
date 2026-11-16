#pragma once

#include "CoreMinimal.h"

#include "Widgets/InventoryItemWidget.h"

#include "HadesInventoryItemWidget.generated.h"

class UHadesInputKeyWidget;
class UHadesQuickSlotComponent;
class UTextBlock;


/** Colors a gun's rounds by how full it is, as in Requiem. */
USTRUCT(BlueprintType)
struct FHadesAmmoColors
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Ammo")
	FSlateColor Full = FSlateColor(FLinearColor(FColor(140, 193, 82)));

	UPROPERTY(EditAnywhere, Category = "Ammo")
	FSlateColor Partly = FSlateColor(FLinearColor(FColor(232, 226, 212)));

	UPROPERTY(EditAnywhere, Category = "Ammo")
	FSlateColor Empty = FSlateColor(FLinearColor(FColor(217, 83, 79)));

	const FSlateColor& Pick(int32 Rounds, int32 Capacity) const { return Rounds <= 0 ? Empty : Rounds >= Capacity ? Full : Partly; }
};


/**
 * An item in the inventory with the game's marks on it: the key of the shortcut it's on, whether it's in hand, and a gun's rounds.
 * The plugin's item widget lights it up and hides its Background on the stand-in for a moved item.
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

	/** The key the player has bound to the item's shortcut, which follows rebinding. It collapses itself while the item has no shortcut. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UHadesInputKeyWidget> ShortcutKeyWidget;

	/** The number of the shortcut the item is on, 1 for the first, such as on a badge in the corner. Collapsed while it has none. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ShortcutNumberText;

	/** Shown only while the item is on a shortcut, such as the corner Shortcut Key sits on. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ShortcutMarker;

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
	FHadesAmmoColors AmmoColors;

private:
	UFUNCTION()
	void RefreshMarks();

	TWeakObjectPtr<UHadesQuickSlotComponent> QuickSlots;
};
