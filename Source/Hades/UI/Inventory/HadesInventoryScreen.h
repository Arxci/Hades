#pragma once

#include "CoreMinimal.h"

#include "Engine/DataTable.h"
#include "Input/UIActionBindingHandle.h"

#include "InventoryTypes.h"
#include "Widgets/InventoryScreenWidget.h"

#include "HadesInventoryScreen.generated.h"

class UCommonAnimatedSwitcher;
class UHadesInventoryComponent;
class UHadesTabListWidget;
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
	virtual void NativeConstruct() override;
	//~ End UUserWidget Interface

	//~ Begin UCommonActivatableWidget Interface
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	//~ End UCommonActivatableWidget Interface

	//~ Begin UInventoryScreenWidget Interface
	virtual void RefreshActions() override;
	virtual UInventoryItemMenuWidget* PushItemMenu(TSubclassOf<UInventoryItemMenuWidget> MenuClass, TFunctionRef<void(UInventoryItemMenuWidget&)> InitMenu) override;
	//~ End UInventoryScreenWidget Interface

	/** Shows the item box, which turns on Store and Take. Leave it out of the plain inventory screen. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UInventoryListWidget> ItemBoxList;

	/** A tab for each page of Pages, in the same order. */
	UPROPERTY(BlueprintReadOnly, Category = "Tabs", meta = (BindWidgetOptional))
	TObjectPtr<UHadesTabListWidget> TabList;

	UPROPERTY(BlueprintReadOnly, Category = "Tabs", meta = (BindWidgetOptional))
	TObjectPtr<UCommonAnimatedSwitcher> Pages;

	/** The page with the items. The screen opens on it. */
	UPROPERTY(EditDefaultsOnly, Category = "Tabs", meta = (ClampMin = "0"))
	int32 ItemsPage = 1;

	/** A row of the UI input action table that repacks the inventory. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle OrganizeAction;

private:
	UFUNCTION()
	void HandleItemChosen(FInventoryItemHandle Item, FInventoryItemHandle Chosen);

	UFUNCTION()
	void HandleTabSelected(FName TabId);

	UHadesInventoryComponent* GetPlayerInventory() const;
	bool IsItemsPageOpen() const;

	void HandleOrganizeAction();
	void HandleCloseAction();
	void HandleRotateAction();

	FUIActionBindingHandle OrganizeHandle;
	FUIActionBindingHandle PreviousTabRotateHandle;
	FUIActionBindingHandle NextTabRotateHandle;
};
