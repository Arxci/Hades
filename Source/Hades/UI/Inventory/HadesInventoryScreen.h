#pragma once

#include "CoreMinimal.h"

#include "InventoryTypes.h"
#include "Widgets/InventoryScreenWidget.h"

#include "HadesInventoryScreen.generated.h"

class UHadesInventoryComponent;
class UInventoryListWidget;


/**
 * The player's inventory, pushed onto the GameMenu layer by the HUD, or by an item box when the screen has an Item Box List.
 * A right-click or Accept on an item opens Item Menu Class beside it, on the Modal layer, and dragging or Move picks it up, with the held
 * item drawn on the cells it would land on. Putting it on another item picks that one up.
 * With a Tab List linked to Pages, the items only work on Items Page, and while an item is held the tab keys turn it instead.
 */
UCLASS(Abstract, Blueprintable)
class HADES_API UHadesInventoryScreen : public UInventoryScreenWidget
{
	GENERATED_BODY()

public:
	UHadesInventoryScreen(const FObjectInitializer& ObjectInitializer);

	/** The item box's storage while this screen shows it in Item Box List, for Store and Take. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	FInventoryContainerHandle GetItemBox() const;

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	//~ Begin UCommonActivatableWidget Interface
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	//~ End UCommonActivatableWidget Interface

	//~ Begin UInventoryScreenWidget Interface
	virtual void NativeOnSorted(bool bSorted) override;
	//~ End UInventoryScreenWidget Interface

	/** Shows the item box, which turns on Store and Take. Leave it out of the plain inventory screen. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UInventoryListWidget> ItemBoxList;

private:
	UFUNCTION()
	void HandleItemChosen(FInventoryItemHandle Item, FInventoryItemHandle Chosen);

	UHadesInventoryComponent* GetPlayerInventory() const;

	void HandleCloseAction();
};
