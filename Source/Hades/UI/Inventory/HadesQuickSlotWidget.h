#pragma once

#include "CoreMinimal.h"

#include "CommonButtonBase.h"

#include "Hades/UI/Inventory/HadesInventoryItemWidget.h"

#include "InventoryUITypes.h"

#include "HadesQuickSlotWidget.generated.h"

class APawn;
class UCommonActionWidget;
class UHadesQuickSlotComponent;
class UInventoryItemWidget;
class UTextBlock;


/**
 * One of the player's quick slots: its key, its item, and whether that item is in hand, such as in the HUD.
 * Putting a held item on it, by dropping it or pressing Accept, assigns the item to the slot. Right-click empties it.
 */
UCLASS(Abstract)
class HADES_API UHadesQuickSlotWidget : public UCommonButtonBase
{
	GENERATED_BODY()

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	//~ End UUserWidget Interface

	/** Highlight the slot here while its item is in hand. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Quick Slot")
	void OnInHandChanged(bool bIsInHand);

	/** 0 is the first slot, bound to Input.QuickSlot.1. */
	UPROPERTY(EditAnywhere, Category = "Quick Slot", meta = (ClampMin = "0"))
	int32 SlotIndex = 0;

	/** Hidden while the slot is empty. */
	UPROPERTY(BlueprintReadOnly, Category = "Quick Slot", meta = (BindWidgetOptional))
	TObjectPtr<UInventoryItemWidget> ItemWidget;

	/** Shows Slot Index + 1, the key that activates it. */
	UPROPERTY(BlueprintReadOnly, Category = "Quick Slot", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NumberText;

	/** The icon of the slot's key, from the quick slot component's Slot Keys. */
	UPROPERTY(BlueprintReadOnly, Category = "Quick Slot", meta = (BindWidgetOptional))
	TObjectPtr<UCommonActionWidget> KeyIcon;

	/** The rounds in a gun on the slot, colored by how full it is. Collapsed for anything else. */
	UPROPERTY(BlueprintReadOnly, Category = "Quick Slot", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AmmoText;

	/** The rounds for that gun in the inventory, such as "/30". */
	UPROPERTY(BlueprintReadOnly, Category = "Quick Slot", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SpareAmmoText;

	UPROPERTY(EditAnywhere, Category = "Quick Slot")
	FHadesAmmoColors AmmoColors;

private:
	/** The HUD is made before the pawn exists, so the slots are found once it's possessed. */
	UFUNCTION()
	void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);

	UFUNCTION()
	void Refresh();

	void HandleClicked();
	bool AssignHeldItem();

	TWeakObjectPtr<UHadesQuickSlotComponent> QuickSlots;
	bool bInHand = false;

	/** Rounds change in the inventory without the quick slots changing. */
	FInventoryRegistryListener RegistryListener;
};
