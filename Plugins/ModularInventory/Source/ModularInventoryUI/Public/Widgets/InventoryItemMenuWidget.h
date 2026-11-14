#pragma once

#include "CoreMinimal.h"

#include "CommonActivatableWidget.h"
#include "Engine/DataTable.h"

#include "InventoryTypes.h"

#include "InventoryItemMenuWidget.generated.h"

class UInventoryItemWidget;
class UInventoryUISubsystem;
struct FUIInputConfig;


/**
 * A small menu for one item, such as Use, Equip and Drop, opened beside it: push it onto a modal layer from an inventory screen's
 * On Item Menu Requested, then call Set Item. While it's open the item is selected, and nothing is held.
 * Back, or a click anywhere outside Menu Panel, closes it. On the item it's beside, a click does nothing and a drag picks the item up.
 * Give the root a full-screen border that is hit-testable, so those clicks reach the menu and nothing behind it can be hovered while it's open.
 */
UCLASS(Abstract, Blueprintable)
class MODULARINVENTORYUI_API UInventoryItemMenuWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UInventoryItemMenuWidget(const FObjectInitializer& ObjectInitializer);

	//~ Begin UCommonActivatableWidget Interface
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	//~ End UCommonActivatableWidget Interface

	/**
	 * Opens the menu for InItem beside Item Widget, usually found with the screen's Find Item Widget. Without one, it opens beside the
	 * focused widget on a gamepad, or at the cursor.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item Menu")
	void SetItem(FInventoryItemHandle InItem, UInventoryItemWidget* ItemWidget);

	UFUNCTION(BlueprintPure, Category = "Inventory|Item Menu")
	FInventoryItemHandle GetItem() const { return Item; }

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	//~ End UUserWidget Interface

	//~ Begin UCommonActivatableWidget Interface
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	//~ End UCommonActivatableWidget Interface

	/** Before On Item Changed, for C++ subclasses. */
	virtual void NativeOnItemChanged() {}

	/** Set Item was called. Fill the menu for the item here, such as showing only the buttons whose actions can run. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Item Menu")
	void OnItemChanged();

	UInventoryUISubsystem* GetInteraction() const;

	/**
	 * The menu itself. Put it in a Canvas Panel that fills the screen, with Size To Content ticked, so it can be moved beside the item.
	 * Gamepad focus starts on its first visible button, unless Get Desired Focus Target is implemented.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item Menu", meta = (BindWidget))
	TObjectPtr<UWidget> MenuPanel;

	/** Only shown in an action bar, for what Accept does on the focused button. Accept itself still goes to the button. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle ConfirmAction;

private:
	FBox2D FindAnchor(const UInventoryItemWidget* ItemWidget) const;
	void PlaceBeside(const FBox2D& Box);

	FInventoryItemHandle Item;
	TWeakObjectPtr<UInventoryItemWidget> AnchorWidget;
	FBox2D Anchor = FBox2D(ForceInit);
};
