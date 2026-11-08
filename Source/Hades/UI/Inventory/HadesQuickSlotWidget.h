#pragma once

#include "CoreMinimal.h"

#include "CommonButtonBase.h"

#include "HadesQuickSlotWidget.generated.h"

class APawn;
class UHadesQuickSlotComponent;
class UInventoryItemWidget;
class UTextBlock;


/**
 * One of the player's quick slots: its number, its item, and whether that item is in hand. Use it in the HUD and the inventory screen.
 * In the inventory, putting a held item on it, by dropping it or pressing Accept, assigns the item to the slot. Right-click empties it.
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

	/** Collapsed while the slot is empty. */
	UPROPERTY(BlueprintReadOnly, Category = "Quick Slot", meta = (BindWidgetOptional))
	TObjectPtr<UInventoryItemWidget> ItemWidget;

	/** Shows Slot Index + 1, the key that activates it. */
	UPROPERTY(BlueprintReadOnly, Category = "Quick Slot", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NumberText;

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
};
