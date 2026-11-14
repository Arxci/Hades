#pragma once

#include "CoreMinimal.h"

#include "Engine/DataTable.h"
#include "Input/UIActionBindingHandle.h"
#include "Misc/EnumRange.h"

#include "InventoryTypes.h"
#include "Widgets/InventoryScreenWidget.h"

#include "HadesInventoryScreen.generated.h"

class UHadesInventoryComponent;
class UHadesItemMenu;
class UHadesShortcutMenu;
class UInventoryListWidget;
class UPanelWidget;
class UTextBlock;
class UWidgetSwitcher;


UENUM(BlueprintType)
enum class EHadesItemAction : uint8
{
	/** Onto a quick slot. */
	Shortcut,
	Use,
	Equip,
	Unequip,
	Combine,
	Discard,
	/** Into the item box. */
	Store,
	/** Out of the item box. */
	Take
};
ENUM_RANGE_BY_FIRST_AND_LAST(EHadesItemAction, EHadesItemAction::Shortcut, EHadesItemAction::Take);


/**
 * The player's inventory, pushed onto the GameMenu layer by the HUD, or by an item box when the screen has an Item Box List.
 * The world stops while it's open. It plays like the RE games: a click or Accept on an item opens its item menu beside it, dragging or Move
 * picks it up, and the held item sits on the cells it would land on. Putting it on another item picks that one up.
 * With a Tab Bar and Pages, Previous Tab and Next Tab change the page, or turn the item being moved; the item actions only work on Items Page.
 */
UCLASS(Abstract, Blueprintable)
class HADES_API UHadesInventoryScreen : public UInventoryScreenWidget
{
	GENERATED_BODY()

public:
	UHadesInventoryScreen(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool CanRunItemAction(EHadesItemAction Action, FInventoryItemHandle Item) const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RunItemAction(EHadesItemAction Action, FInventoryItemHandle Item);

	/** Says what the highlighted option of an open menu does, above the action bar. Empty goes back to the screen's own hint. */
	void ShowActionHint(const FText& Hint);

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	//~ Begin UCommonActivatableWidget Interface
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual bool NativeOnHandleBackAction() override;
	//~ End UCommonActivatableWidget Interface

	//~ Begin UInventoryScreenWidget Interface
	virtual bool IsInventoryShown() const override;
	virtual void RefreshActions() override;
	virtual void NativeOnItemMenuRequested(FInventoryItemHandle Item) override;
	//~ End UInventoryScreenWidget Interface

	/** Pushed onto the Modal layer beside the item the player clicks. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TSubclassOf<UHadesItemMenu> ItemMenuClass;

	/** Pushed onto the Modal layer by the item menu's Shortcut button. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TSubclassOf<UHadesShortcutMenu> ShortcutMenuClass;

	/** Shows the item box, which turns on Store and Take. Leave it out of the plain inventory screen. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UInventoryListWidget> ItemBoxList;

	/** Says what to do now, or what a menu option does. Hidden while there's nothing to say. Put Action Hint Text inside it. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ActionHint;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ActionHintText;

	/** Holds a button per page, in the same order as Pages. Hades Tab Buttons show which one is open. */
	UPROPERTY(BlueprintReadOnly, Category = "Tabs", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> TabBar;

	UPROPERTY(BlueprintReadOnly, Category = "Tabs", meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> Pages;

	/** The page with the items. The screen opens on it. */
	UPROPERTY(EditDefaultsOnly, Category = "Tabs", meta = (ClampMin = "0"))
	int32 ItemsPage = 1;

	/** A row of the UI input action table that repacks the pockets. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle OrganizeAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle PreviousTabAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle NextTabAction;

	/** The action hint while an item is being moved. */
	UPROPERTY(EditDefaultsOnly, Category = "Hints")
	FText MoveHint;

	/** The action hint while choosing the second item to combine. */
	UPROPERTY(EditDefaultsOnly, Category = "Hints")
	FText CombineHint;

	/** The action hint the rest of the time. Empty hides it. */
	UPROPERTY(EditDefaultsOnly, Category = "Hints")
	FText IdleHint;

private:
	UFUNCTION()
	void HandleItemChosen(FInventoryItemHandle Item, FInventoryItemHandle Chosen);

	UFUNCTION()
	void HandleHeldItemChanged();

	UHadesInventoryComponent* GetPlayerInventory() const;
	FInventoryContainerHandle GetItemBox() const;
	void OpenItemMenu(FInventoryItemHandle Item);
	void OpenShortcutMenu(FInventoryItemHandle Item);
	void HandleMenuClosed();

	void SetPage(int32 Index);
	void RefreshActionHint();

	void HandleOrganizeAction();
	void HandleCloseAction();
	void HandlePreviousTab();
	void HandleNextTab();

	/** What the highlighted option of an open menu does. */
	FText MenuHint;

	FUIActionBindingHandle OrganizeHandle;
};
