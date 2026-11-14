#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "Engine/StreamableManager.h"

#include "InventoryTypes.h"
#include "InventoryUITypes.h"

#include "InventoryItemDetailsWidget.generated.h"

class UImage;
class UInventoryEquipmentComponent;
class UInventoryItemDefinition;
class UInventoryStatWidget;
class UInventorySubsystem;
class UPanelWidget;
class UTextBlock;
struct FInventoryChangeSet;


/**
 * An item's name, description and stats: a tooltip, or a panel that follows whatever the player hovers or focuses.
 * Stats are tag values listed as rows in Stat Panel, and can show how the item compares with what's equipped.
 */
UCLASS(Abstract)
class MODULARINVENTORYUI_API UInventoryItemDetailsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Inventory|Details")
	void SetItem(FInventoryItemHandle InItem);

	UFUNCTION(BlueprintPure, Category = "Inventory|Details")
	FInventoryItemHandle GetItem() const { return Item; }

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	/** The item, or its state, changed. Fill in what the base doesn't here, such as stats from fragments. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Details")
	void OnItemChanged();

	/** Shows whatever the player hovers with the mouse or focuses with a gamepad, or the item being moved while one is held. Leave off for tooltips. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Details")
	bool bFollowHoveredItem = false;

	/** With Follow Hovered Item, keeps showing the last item while nothing is hovered, until it leaves the inventory. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Details", meta = (EditCondition = "bFollowHoveredItem"))
	bool bKeepLastItem = false;

	/** Hidden rather than collapsed, so it keeps its size and nothing around it moves. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Details")
	bool bHideWithoutItem = true;

	/** Colors Name Text by the item's tags, such as its rarity: the first entry whose tag the item has. Without one, Name Text keeps its own color. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Details")
	TArray<FInventoryTagColor> TagColors;

	/** The tag values listed in Stat Panel, in order, such as damage and durability. Each item lists the ones it has. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Details|Stats")
	TArray<FInventoryStatDisplay> Stats;

	UPROPERTY(EditAnywhere, Category = "Inventory|Details|Stats")
	TSubclassOf<UInventoryStatWidget> StatClass;

	/** Each stat also shows how much better or worse the item is than the one equipped where it would go, such as the gun in hand. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Details|Stats")
	bool bCompareWithEquipped = false;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Details", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Icon;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Details", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Details", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionText;

	/** Collapsed for single items. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Details", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CountText;

	/** Gets a Stat Class row for each stat the item has, such as a Vertical Box. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Details", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> StatPanel;

private:
	UFUNCTION()
	void HandleHoveredItemChanged(FInventoryItemHandle HoveredItem);

	UFUNCTION()
	void FollowInteraction();

	UFUNCTION()
	void HandleEquipmentChanged(FInventoryItemHandle EquippedItem, FGameplayTag EquipSlot);

	void HandleRegistryChanged(const FInventoryChangeSet& Changes);
	void Refresh();
	void RefreshStats(const UInventorySubsystem* Inventory);

	FInventoryItemHandle Item;
	FInventoryItemHandle ComparedItem;
	FSlateColor DefaultNameColor;
	TWeakObjectPtr<const UInventoryItemDefinition> ShownDefinition;
	TWeakObjectPtr<UInventoryEquipmentComponent> Equipment;
	TSharedPtr<FStreamableHandle> IconHandle;
	FInventoryRegistryListener RegistryListener;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInventoryStatWidget>> StatRows;
};
