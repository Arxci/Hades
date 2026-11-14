#pragma once

#include "CoreMinimal.h"

#include "CommonActivatableWidget.h"
#include "Engine/DataTable.h"

#include "InventoryTypes.h"

#include "InventoryItemMenuWidget.generated.h"

class UInventoryItemWidget;
class UInventoryUISubsystem;
class USoundBase;
struct FUIInputConfig;


/** Where an item menu opens. */
UENUM(BlueprintType)
enum class EInventoryMenuPlacement : uint8
{
	/** Beside the item, on the side toward the middle of the screen: level with its top in the upper half, and with its bottom in the lower half. */
	TowardCenter,
	/** On the item's right, level with its top. It goes to the left where only that side has room, and moves up to stay on screen. */
	Right,
	/** On the item's left, level with its top. It goes to the right where only that side has room, and moves up to stay on screen. */
	Left,
	/** Wherever Menu Panel is in the designer, such as a fixed spot for a menu that isn't laid out around the item. */
	AsDesigned
};


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
	 * Opens the menu for InItem beside Item Widget, usually found with the screen's Find Item Widget, as Placement says. Without one,
	 * it opens beside the focused widget on a gamepad, or at the cursor. The menu is placed after On Item Changed, so it's measured
	 * with the buttons shown for this item.
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
	virtual void NativeOnActivated() override;
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

	/** Where the menu opens. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Item Menu")
	EInventoryMenuPlacement Placement = EInventoryMenuPlacement::TowardCenter;

	/** Space between the item and the menu, in pixels. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Item Menu", meta = (ClampMin = "0", EditCondition = "Placement != EInventoryMenuPlacement::AsDesigned"))
	float MenuGap = 0.0f;

	/** Played as the menu opens. Its buttons play their own hover and click sounds from their styles. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Item Menu")
	TObjectPtr<USoundBase> OpenSound;

	/** Played as the menu closes, however it closes. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Item Menu")
	TObjectPtr<USoundBase> CloseSound;

	/** Only shown in an action bar, for what Accept does on the focused button. Accept itself still goes to the button. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle ConfirmAction;

private:
	FBox2D FindAnchor(const UInventoryItemWidget* ItemWidget) const;
	void PlaceBeside(const FBox2D& Box);

	/** Menu Panel's size as it is now, with the buttons shown for this item. Lays the menu out first, as it may not have been drawn yet. */
	FVector2D MeasureMenuPanel();

	FInventoryItemHandle Item;
	TWeakObjectPtr<UInventoryItemWidget> AnchorWidget;
	FBox2D Anchor = FBox2D(ForceInit);
};
