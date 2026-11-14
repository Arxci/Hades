#pragma once

#include "CoreMinimal.h"

#include "GameplayTagContainer.h"

#include "InventoryUITypes.h"
#include "Widgets/InventoryCellWidget.h"

#include "InventorySlotWidget.generated.h"

class APawn;
class UInventoryItemWidget;
class UTextBlock;
struct FInventoryChangeSet;


/**
 * One slot of a slot container, such as the main hand in the equipment slots. Place these by hand to lay out an equipment screen.
 * List widgets use it for their entries too.
 */
UCLASS(Abstract)
class MODULARINVENTORYUI_API UInventorySlotWidget : public UInventoryCellWidget
{
	GENERATED_BODY()

public:
	/** Points the slot at a container yourself, instead of finding Container Tag on the player. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Slot")
	void SetContainer(FInventoryContainerHandle InContainer);

	/** Shows any position of any container, ignoring Slot Tag, such as one entry of a list. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Slot")
	void SetContainerPlacement(FInventoryContainerHandle InContainer, const FInventoryPlacement& InPlacement);

	/** Item Widget while it shows the item, such as to open a menu beside it; otherwise null. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Slot")
	UInventoryItemWidget* GetItemWidget(FInventoryItemHandle Item) const;

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	//~ End UUserWidget Interface

	/** Also fires with an unset item when the slot empties. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Slot")
	void OnSlotItemChanged(FInventoryItemHandle NewItem);

	/** What dropping the held item here would do, while it's aimed here; None otherwise. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Slot")
	void OnPlacementPreviewChanged(EInventoryDropAction Action);

	/** A container of the owning player's inventory component, such as Container.Equipment. Found on construct, and again when the player's pawn changes. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Slot")
	FGameplayTag ContainerTag;

	UPROPERTY(EditAnywhere, Category = "Inventory|Slot")
	FGameplayTag SlotTag;

	/** Hidden while the slot is empty. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Slot", meta = (BindWidgetOptional))
	TObjectPtr<UInventoryItemWidget> ItemWidget;

	/** The item's name, for rows of a list such as a storage box. Empty while the slot is. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Slot", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

private:
	UFUNCTION()
	void HandleTargetChanged();

	UFUNCTION()
	void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);

	void HandleRegistryChanged(const FInventoryChangeSet& Changes);
	void Refresh();

	FInventoryItemHandle ShownItem;
	EInventoryDropAction PreviewAction = EInventoryDropAction::None;
	FInventoryRegistryListener RegistryListener;
};
