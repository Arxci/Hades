#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"

#include "InventoryTypes.h"
#include "InventoryUITypes.h"

#include "InventoryEquippedItemWidget.generated.h"

class APawn;
class UInventoryEquipmentComponent;
class UInventoryItemWidget;
class UTextBlock;
struct FInventoryChangeSet;


/**
 * What the owning player has equipped, such as the weapon in hand or the light carried, on an inventory screen or a HUD: the item, its name,
 * and whether it's active, such as a light that's on. Slots lists the equipment slots it shows, in order: the first one with an item equipped
 * is shown, such as a right hand then a left hand for whatever is in hand. It follows equipping, unequipping and the item's own changes, and
 * finds the owning player's Inventory Equipment Component again when the player's pawn changes.
 * Numbers on the item, such as a gun's rounds or a light's charge, are Item Widget's Value Displays. Subclass it in C++ to show more, such as
 * spare ammo, with Native On Refresh.
 */
UCLASS(Abstract)
class MODULARINVENTORYUI_API UInventoryEquippedItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows other slots, such as for a widget made in a loop, one per slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Equipped Item")
	void SetSlots(const TArray<FGameplayTag>& InSlots);

	/** The item shown, the first of Slots with one equipped; unset while none has. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Equipped Item")
	FInventoryItemHandle GetShownItem() const { return ShownItem; }

	/** Which of Slots the shown item is equipped in; none while nothing is shown. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Equipped Item")
	FGameplayTag GetShownSlot() const { return ShownSlot; }

	/** Whether the shown item is active: it has Active Tag, or a tag value above 0 for it. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Equipped Item")
	bool IsShownItemActive() const { return bShownActive; }

	/** The owning player's equipment component, found on its pawn. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Equipped Item")
	UInventoryEquipmentComponent* GetEquipmentComponent() const { return Equipment.Get(); }

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	/** The item shown changed, or the slot it's in, including as the widget first shows. Item is unset while nothing is equipped. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Equipped Item")
	void OnEquippedItemChanged(FInventoryItemHandle Item, FGameplayTag EquipSlot);

	/** The shown item became active or stopped being, such as a light turned on, including as the widget first shows. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Equipped Item")
	void OnActiveChanged(bool bIsActive);

	/** After the widget was shown again, for C++ subclasses to show more of the item. Item is unset while nothing is equipped. */
	virtual void NativeOnRefresh(FInventoryItemHandle Item) {}

	/** Whether a change in the inventory changes what the widget shows. By default, a change to the shown item or to a container of Slots does. */
	virtual bool ShouldRefreshFor(const FInventoryChangeSet& Changes) const;

	/** Shows the equipped item as it is now. */
	void Refresh();

	/** The equipment slots it shows, in order: the first one with an item equipped is shown, such as a right hand then a left hand. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Equipped Item")
	TArray<FGameplayTag> Slots;

	/**
	 * Marks the shown item as active while it has this tag, or a tag value above 0 for it, such as a state tag a light sets to 1 while it's
	 * on. Active Marker shows and On Active Changed fires. None never marks it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Equipped Item")
	FGameplayTag ActiveTag;

	/** What Name Text says while nothing is equipped, such as Empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Equipped Item")
	FText EmptyText;

	/**
	 * The item, with its Value Displays, such as rounds or charge. Hidden, not collapsed, while nothing is equipped, so a Size Box around it
	 * keeps its size. It never takes clicks itself, so it can't be dragged out of its slot.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Equipped Item", meta = (BindWidgetOptional))
	TObjectPtr<UInventoryItemWidget> ItemWidget;

	/** The item's Display Name, or Empty Text. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Equipped Item", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	/** Shown only while nothing is equipped, such as a line across the empty box. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Equipped Item", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EmptyMarker;

	/** Shown only while the item is active, as Active Tag says, such as a lit dot beside a light that's on. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Equipped Item", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ActiveMarker;

private:
	UFUNCTION()
	void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);

	UFUNCTION()
	void HandleEquipmentChanged(FInventoryItemHandle Item, FGameplayTag EquipSlot);

	void BindEquipment(UInventoryEquipmentComponent* NewEquipment);
	bool IsItemActive(FInventoryItemHandle Item) const;

	TWeakObjectPtr<UInventoryEquipmentComponent> Equipment;
	FInventoryItemHandle ShownItem;
	FGameplayTag ShownSlot;
	bool bShownActive = false;
	bool bShownOnce = false;

	/** The item's state, such as a light turned on, changes without it being equipped again. */
	FInventoryRegistryListener RegistryListener;
};
