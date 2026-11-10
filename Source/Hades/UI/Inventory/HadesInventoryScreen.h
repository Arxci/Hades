#pragma once

#include "CoreMinimal.h"

#include "Engine/DataTable.h"
#include "Input/UIActionBindingHandle.h"
#include "Misc/EnumRange.h"

#include "Hades/UI/Foundation/HadesActivatableWidget.h"

#include "InventoryTypes.h"

#include "HadesInventoryScreen.generated.h"

class UDragDropOperation;
class UHadesInventoryComponent;
class UHadesItemMenu;
class UHadesShortcutMenu;
class UInventoryGridWidget;
class UInventoryListWidget;
class UInventoryUISubsystem;
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
class HADES_API UHadesInventoryScreen : public UHadesActivatableWidget
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
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	//~ End UCommonActivatableWidget Interface

	/** Pushed onto the Modal layer beside the item the player clicks. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TSubclassOf<UHadesItemMenu> ItemMenuClass;

	/** Pushed onto the Modal layer by the item menu's Shortcut button. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TSubclassOf<UHadesShortcutMenu> ShortcutMenuClass;

	/** Its first cell takes gamepad focus when the screen opens, unless the Blueprint implements Get Desired Focus Target. Menus open beside its items. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UInventoryGridWidget> MainGrid;

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

	// Rows of the UI input action table, so a Common Bound Action Bar shows them with their key icons.
	/** Picks up the item under the cursor or focus, and puts the held item down. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle MoveAction;

	/** Repacks the pockets. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle OrganizeAction;

	/** Only works, and shows, while holding something that turns. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle RotateAction;

	/** Only works, and shows, while holding a stack of more than one. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle SplitAction;

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
	void OpenItemMenu(FInventoryItemHandle Item);

	UFUNCTION()
	void HandleItemChosen(FInventoryItemHandle Item);

	UFUNCTION()
	void HandleHeldItemChanged();

	UFUNCTION()
	void HandleHoveredItemChanged(FInventoryItemHandle Item);

	/** Bound only while they can work, so the action bar shows what works now. */
	UFUNCTION()
	void RefreshItemActions();

	UInventoryUISubsystem* GetInteraction() const;
	UHadesInventoryComponent* GetPlayerInventory() const;
	FInventoryContainerHandle GetItemBox() const;
	FBox2D FindItemAnchor(FInventoryItemHandle Item) const;
	bool IsOnItemsPage() const;
	void OpenShortcutMenu(FInventoryItemHandle Item);
	void HandleMenuClosed();

	/** For a drag that starts on a menu beside the item. */
	UDragDropOperation* BeginItemDrag(FInventoryItemHandle Item);

	void SetPage(int32 Index);
	void RefreshActionHint();
	void SetActionBound(FUIActionBindingHandle& Handle, const FDataTableRowHandle& Row, bool bBound, void (UHadesInventoryScreen::*Handler)());

	void HandleMoveAction();
	void HandleRotateAction();
	void HandleSplitAction();
	void HandleOrganizeAction();
	void HandleCloseAction();
	void HandlePreviousTab();
	void HandleNextTab();

	/** The first of two items to combine, while the player chooses the second. */
	FInventoryItemHandle CombineItem;

	/** What the highlighted option of an open menu does. */
	FText MenuHint;

	FUIActionBindingHandle MoveHandle;
	FUIActionBindingHandle OrganizeHandle;
	FUIActionBindingHandle RotateHandle;
	FUIActionBindingHandle SplitHandle;
};
