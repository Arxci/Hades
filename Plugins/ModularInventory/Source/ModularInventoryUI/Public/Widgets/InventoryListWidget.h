#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"

#include "InventoryTypes.h"
#include "InventoryUITypes.h"

#include "InventoryListWidget.generated.h"

class UInventorySlotWidget;
class UPanelWidget;
struct FInventoryChangeSet;


/**
 * Shows a list container: an entry per item, then an empty entry while the list has room, so mouse and gamepad can put items there.
 * Entry Panel lays the entries out: a Wrap Box for tiles, a Vertical Box for rows. Put the panel in a Scroll Box for long lists.
 */
UCLASS(Abstract)
class MODULARINVENTORYUI_API UInventoryListWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows a container yourself, such as a chest's, instead of finding Container Tag on the player. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|List")
	void SetContainer(FInventoryContainerHandle InContainer);

	UFUNCTION(BlueprintPure, Category = "Inventory|List")
	FInventoryContainerHandle GetContainer() const { return Container; }

	/** For gamepad focus. The empty entry comes after the items. */
	UFUNCTION(BlueprintPure, Category = "Inventory|List")
	UInventorySlotWidget* GetEntryWidget(int32 Index) const;

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	/** A container of the owning player's inventory component, found on construct, such as Container.KeyItems. */
	UPROPERTY(EditAnywhere, Category = "Inventory|List")
	FGameplayTag ContainerTag;

	/** One per entry. Leave its Container Tag and Slot Tag empty. */
	UPROPERTY(EditAnywhere, Category = "Inventory|List")
	TSubclassOf<UInventorySlotWidget> EntryClass;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|List", meta = (BindWidget))
	TObjectPtr<UPanelWidget> EntryPanel;

private:
	void HandleRegistryChanged(const FInventoryChangeSet& Changes);
	void Refresh();

	FInventoryContainerHandle Container;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInventorySlotWidget>> Entries;

	FInventoryRegistryListener RegistryListener;
};
