#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"

#include "InventoryTypes.h"
#include "InventoryUITypes.h"

#include "InventoryListWidget.generated.h"

class APawn;
class UInventoryItemWidget;
class UInventorySlotWidget;
class UInventorySubsystem;
class UInventoryUISubsystem;
class UPanelWidget;
struct FInventoryChangeSet;


/**
 * Shows a list container: an entry per item, then an empty entry while the list has room, so mouse and gamepad can put items there.
 * Without the empty entry, an item dropped anywhere else on the list goes to its end. Read-only lists never have one.
 * Entry Panel lays the entries out: a Wrap Box for tiles, a Vertical Box for rows. Put the panel in a Scroll Box for long lists.
 * Display Filter and Sort By choose which items get entries and in what order, such as one tab per kind of item over a single inventory,
 * sorted by weight. They only change what's shown: the container keeps every item, in its own order.
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

	/** For gamepad focus: the entries in the order shown. The empty entry comes after the items. */
	UFUNCTION(BlueprintPure, Category = "Inventory|List")
	UInventorySlotWidget* GetEntryWidget(int32 Index) const;

	/** The widget showing the item, such as to open a menu beside it. Null when the item isn't in this list, or Display Filter leaves it out. */
	UFUNCTION(BlueprintPure, Category = "Inventory|List")
	UInventoryItemWidget* GetItemWidget(FInventoryItemHandle Item) const;

	/** Shows only items that match, such as when the player picks a tab. An empty query shows every item. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|List")
	void SetDisplayFilter(const FInventoryItemQuery& InDisplayFilter);

	/** Orders the entries another way, such as when the player picks a sort option. Sort Tag is only for Tag Value. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|List")
	void SetSort(EInventoryListSort InSortBy, FGameplayTag InSortTag, bool bInSortDescending);

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	//~ End UUserWidget Interface

	/** An empty entry after the items while the list has room, for gamepads to put items on. */
	UPROPERTY(EditAnywhere, Category = "Inventory|List")
	bool bShowEmptyEntry = true;

	/** A container of the owning player's inventory component, such as Container.KeyItems. Found on construct, and again when the player's pawn changes. */
	UPROPERTY(EditAnywhere, Category = "Inventory|List")
	FGameplayTag ContainerTag;

	/** One per entry. Leave its Container Tag and Slot Tag empty. */
	UPROPERTY(EditAnywhere, Category = "Inventory|List")
	TSubclassOf<UInventorySlotWidget> EntryClass;

	/**
	 * Only items that match get an entry, such as Tag Query Item.Type.Weapon on a weapons tab. Empty shows every item. Items left out stay in
	 * the container, and an item put on the list goes into it whether or not it matches. Set Display Filter changes it at runtime.
	 */
	UPROPERTY(EditAnywhere, Category = "Inventory|List|Display")
	FInventoryItemQuery DisplayFilter;

	/** How the entries are ordered. Only how they're shown: the container keeps its own order. Set Sort changes it at runtime. */
	UPROPERTY(EditAnywhere, Category = "Inventory|List|Display")
	EInventoryListSort SortBy = EInventoryListSort::ContainerOrder;

	/** The tag value Tag Value sorting orders by, such as Item.Stat.Weight. */
	UPROPERTY(EditAnywhere, Category = "Inventory|List|Display", meta = (EditCondition = "SortBy == EInventoryListSort::TagValue", EditConditionHides))
	FGameplayTag SortTag;

	/** Highest first, or names from Z to A. */
	UPROPERTY(EditAnywhere, Category = "Inventory|List|Display", meta = (EditCondition = "SortBy != EInventoryListSort::ContainerOrder", EditConditionHides))
	bool bSortDescending = false;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|List", meta = (BindWidget))
	TObjectPtr<UPanelWidget> EntryPanel;

private:
	UFUNCTION()
	void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);

	void HandleRegistryChanged(const FInventoryChangeSet& Changes);
	void Refresh();

	/** Orders the positions of items in the container as Sort By says. */
	void SortPositions(const UInventorySubsystem& Inventory, const TArray<FInventoryItemHandle>& Items, TArray<int32>& InOutPositions) const;
	UInventoryUISubsystem* GetDropInteraction(const UDragDropOperation* Operation) const;
	FInventoryPlacement GetEndPlacement() const;

	FInventoryContainerHandle Container;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInventorySlotWidget>> Entries;

	FInventoryRegistryListener RegistryListener;
};
