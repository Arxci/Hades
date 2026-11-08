#pragma once

#include "CoreMinimal.h"

#include "GameplayTagContainer.h"

#include "InventoryUITypes.h"
#include "Widgets/InventoryCellWidget.h"

#include "InventorySlotWidget.generated.h"

class UInventoryItemWidget;
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

	/** A container of the owning player's inventory component, found on construct, such as Container.Equipment. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Slot")
	FGameplayTag ContainerTag;

	UPROPERTY(EditAnywhere, Category = "Inventory|Slot")
	FGameplayTag SlotTag;

	/** Collapsed while the slot is empty. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Slot", meta = (BindWidgetOptional))
	TObjectPtr<UInventoryItemWidget> ItemWidget;

private:
	UFUNCTION()
	void HandleTargetChanged();

	void HandleRegistryChanged(const FInventoryChangeSet& Changes);
	void Refresh();

	FInventoryItemHandle ShownItem;
	EInventoryDropAction PreviewAction = EInventoryDropAction::None;
	FInventoryRegistryListener RegistryListener;
};
