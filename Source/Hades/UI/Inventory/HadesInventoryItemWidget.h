#pragma once

#include "CoreMinimal.h"

#include "Widgets/InventoryItemWidget.h"

#include "HadesInventoryItemWidget.generated.h"

class UCommonActionWidget;
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


/** An item in the inventory with the game's marks on it: the key of the shortcut it's on, whether it's in hand, and a gun's rounds. */
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

	/** The icon of the shortcut's key, from the quick slot component's Slot Keys. It hides itself while the item has no shortcut. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UCommonActionWidget> ShortcutKey;

	/** Shown while the item is in hand. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EquippedMarker;

	/** Covers the item's cells, so a big item reads as one. Hidden on the stand-in for an item being moved, so what's under it shows through. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Background;

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
