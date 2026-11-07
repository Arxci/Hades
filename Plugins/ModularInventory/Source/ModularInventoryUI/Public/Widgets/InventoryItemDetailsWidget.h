#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "Engine/StreamableManager.h"

#include "InventoryTypes.h"
#include "InventoryUITypes.h"

#include "InventoryItemDetailsWidget.generated.h"

class UImage;
class UInventoryItemDefinition;
class UTextBlock;
struct FInventoryChangeSet;


/** An item's name, description and stats: a tooltip, or a panel that follows whatever the player hovers or focuses. */
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
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	/** The item, or its state, changed. Fill in stats from its tag values or fragments here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Details")
	void OnItemChanged();

	/** Shows whatever the player hovers with the mouse or focuses with a gamepad. Leave off for tooltips. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Details")
	bool bFollowHoveredItem = false;

	UPROPERTY(EditAnywhere, Category = "Inventory|Details")
	bool bCollapseWithoutItem = true;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Details", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Icon;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Details", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Details", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionText;

	/** Collapsed for single items. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Details", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CountText;

private:
	UFUNCTION()
	void HandleHoveredItemChanged(FInventoryItemHandle HoveredItem);

	void HandleRegistryChanged(const FInventoryChangeSet& Changes);
	void Refresh();

	FInventoryItemHandle Item;
	TWeakObjectPtr<const UInventoryItemDefinition> ShownDefinition;
	TSharedPtr<FStreamableHandle> IconHandle;
	FInventoryRegistryListener RegistryListener;
};
